/*
 *  ucinternal.cc - Interpreter for usecode.
 *
 *  Copyright (C) 1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <limits>
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "Audio.h"
#include "Gump.h"
#include "Gump_manager.h"
#include "Notebook_gump.h"
#include "Text_gump.h"
#include "actions.h"
#include "actors.h"
#include "animate.h"
#include "barge.h"
#include "chunks.h"
#include "conversation.h"
#include "databuf.h"
#include "effects.h"
#include "egg.h"
#include "exult.h"
#include "game.h"
#include "gamemap.h"
#include "gamewin.h"
#include "ios_state.hpp"
#include "keyring.h"
#include "miscinf.h"
#include "monsters.h"
#include "mouse.h"
#include "opcodes.h"
#include "party.h"
#include "schedule.h"
#include "stackframe.h"
#include "touchui.h"
#include "tqueue.h"
#include "ucfunction.h"
#include "ucinternal.h"
#include "ucsched.h"
#include "ucsymtbl.h"
#include "usefuns.h"
#include "useval.h"

#if (defined(USE_EXULTSTUDIO) && defined(USECODE_DEBUGGER))
#	include "debugmsg.h"
#	include "debugserver.h"
#	include "servemsg.h"
#	include "server.h"
#endif

#include <algorithm>    // STL function things
#include <cstdio>       /* Debugging.           */
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <map>

#ifdef XWIN
#	include <csignal>
#endif

using std::cerr;
using std::cout;
using std::dec;
using std::endl;
using std::exit;
using std::hex;
using std::ifstream;
using std::ios;
using std::istream;
using std::ofstream;
using std::ostream;
using std::setfill;
using std::setw;
using std::size_t;
using std::strchr;
using std::string;
using std::vector;

// External globals..

extern bool intrinsic_trace;
extern int  usecode_trace;

#ifdef USECODE_DEBUGGER

extern bool      usecode_debugging;
std::vector<int> intrinsic_breakpoints;

void initialise_usecode_debugger() {
	// Summon up the configuration file

	// List all the keys.

	// Render intrinsic names to numbers (unless already given as
	// a number (which might be hex. Convert from that.

	// push them all onto the list
}

#endif

void Usecode_internal::stack_trace(ostream& out) {
	if (call_stack.empty()) {
		return;
	}

	auto iter = call_stack.begin();

	const boost::io::ios_flags_saver flags(out);
	const boost::io::ios_fill_saver  fill(out);
	out << std::hex << std::setfill('0');
	do {
		out << *(*iter);
		auto it = except_stack.find(*iter);
		if (it != except_stack.end()) {
			out << ", active catch at 0x" << std::setw(8) << it->second;
		}
		out << endl;
		if ((*iter)->call_depth == 0) {
			break;
		}
		++iter;
	} while (true);
}

Usecode_function* Usecode_internal::find_function(int funcid) {
	Usecode_function* fun;
	// locate function
	const unsigned int slotnum = funcid / 0x100;
	if (slotnum >= funs.size()) {
		fun = nullptr;
	} else {
		Funs256&     slot  = funs[slotnum];
		const size_t index = funcid % 0x100;
		fun                = index < slot.size() ? slot[index] : nullptr;
	}
	if (!fun) {
#ifdef DEBUG
		cout << "Usecode " << funcid << " not found." << endl;
#endif
	}
	return fun;
}

inline Usecode_class_symbol* Usecode_internal::get_class(int n) {
	return symtbl->get_class(n);
}

inline Usecode_class_symbol* Usecode_internal::get_class(const char* nm) {
	return symtbl->get_class(nm);
}

inline int Usecode_internal::get_shape_fun(int n) {
	return n < 0x400 ? n
					 : (symtbl ? symtbl->get_high_shape_fun(n)
							   // Default to 'old-style' high shape functions.
							   : 0x1000 + (n - 0x400));
}

inline bool Usecode_internal::is_object_fun(int n) {
	if (!symtbl) {
		return n < 0x800;
	}
	return symtbl->is_object_fun(n);
}

bool Usecode_internal::call_function(
		int funcid, int eventid, Game_object* caller, bool entrypoint,
		bool orig, int givenargs) {
	Usecode_function* fun = find_function(funcid);
	if (!fun) {
		return false;
	}
	if (orig) {
		if (!(fun = fun->orig)) {
#ifdef DEBUG
			cout << "Original usecode " << funcid << " not found." << endl;
#endif
			return false;
		}
	}

	int depth;
	int oldstack;
	int chain;

	if (entrypoint) {
		depth    = 0;
		oldstack = 0;
		chain    = Stack_frame::getCallChainID();

	} else {
		Stack_frame* parent = call_stack.front();

		// find new depth
		depth = parent->call_depth + 1;

		// find number of elements available to pop from stack (as arguments)
		oldstack = sp - parent->save_sp;

		chain = parent->call_chain;

		if (caller == nullptr) {
			caller = parent->caller_item.get();    // use parent's
		}
	}

	auto* frame = new Stack_frame(fun, eventid, caller, chain, depth);

	int num_args = std::max(frame->num_args, givenargs);
	// Many functions have 'itemref' as a 'phantom' arg.
	// In the originals, this was probably so that the games
	// could know how much memory the function would need.
	// In any case, do this only if this was not an indirect call.
	if (givenargs == 0 && is_object_fun(funcid)) {
		if (--num_args < 0) {
			// Backwards compatibility with older mods.
			cerr << "Called usecode function " << hex << setfill('0') << funcid
				 << dec << setfill(' ');
			cerr << " with negative number of arguments." << endl
				 << "The mod/game was likely compiled with an outdated version "
					"of UCC"
				 << endl;
			num_args = 0;
		}
	}
#ifdef DEBUG
	const int added_args = num_args - oldstack;
#endif
	while (num_args > oldstack) {    // Not enough args pushed?
		pushi(0);                    // add zeroes
		oldstack++;
	}

	// Store args in first num_args locals
	int i;
	for (i = 0; i < num_args; i++) {
		const Usecode_value val         = pop();
		frame->locals[num_args - i - 1] = val;
	}

	// save stack pointer
	frame->save_sp = sp;

	// add new stack frame to top of stack
	call_stack.push_front(frame);

#ifdef DEBUG
	Usecode_symbol* fsym = symtbl ? (*symtbl)[funcid] : nullptr;
	cout << "Running usecode ";
	if (fsym) {
		cout << fsym->get_name() << " [";
	}
	cout << setw(4) << hex << setfill('0') << funcid << dec << setfill(' ');
	if (fsym) {
		cout << "]";
	}
	cout << " (";
	for (i = 0; i < num_args; i++) {
		if (i) {
			cout << ", ";
		}
		frame->locals[i].print(cout);
	}
	cout << ") with event " << eventid << ", depth " << frame->call_depth
		 << endl;
	if (added_args > 0) {
		cout << added_args << (added_args > 1 ? " args" : " arg")
			 << " had to be added to the stack for this call" << endl;
	}
#endif

	return true;
}

void Usecode_internal::previous_stack_frame() {
	// remove current frame from stack
	Stack_frame* frame = call_stack.front();
	call_stack.pop_front();

	// restore stack pointer
	sp = frame->save_sp;

	// Get rid of exception handler for the current function (say, due to return
	// in a try block).
	auto it = except_stack.find(frame);
	if (it != except_stack.end()) {
		except_stack.erase(it);
	}

	if (frame->call_depth == 0) {
		// this was the function called from 'the outside'
		// push a marker (nullptr) for the interpreter onto the call stack,
		// so it knows it has to return instead of continuing
		// further up the call stack
		call_stack.push_front(nullptr);
	}

	delete frame;
}

static ostream& print_usecode_function(
		Usecode_symbol_table* symtbl, const int function) {
	Usecode_symbol* fsym = symtbl ? (*symtbl)[function] : nullptr;
	if (fsym) {
		cout << fsym->get_name() << " (";
	}
	cout << hex << setw(4) << setfill('0') << function << dec << setfill(' ');
	if (fsym) {
		cout << ')';
	}
	return cout;
}

void Usecode_internal::return_from_function(Usecode_value& retval) {
#ifdef DEBUG
	// store old function ID for debugging output
	const int oldfunction = call_stack.front()->function->id;
#endif

	// back up a stack frame
	previous_stack_frame();

	// push the return value
	push(retval);

#ifdef DEBUG
	Stack_frame* parent_frame = call_stack.front();

	cout << "Returning (";
	retval.print(cout);
	cout << ") from usecode ";
	print_usecode_function(symtbl, oldfunction) << endl;

	if (parent_frame) {
		const int newfunction = call_stack.front()->function->id;
		cout << "...back into usecode ";
		print_usecode_function(symtbl, newfunction) << endl;
	}
#endif
}

void Usecode_internal::return_from_procedure() {
#ifdef DEBUG
	// store old function ID for debugging output
	const int oldfunction = call_stack.front()->function->id;
#endif

	// back up a stack frame
	previous_stack_frame();

#ifdef DEBUG
	Stack_frame* parent_frame = call_stack.front();

	cout << "Returning from usecode ";
	print_usecode_function(symtbl, oldfunction) << endl;

	if (parent_frame) {
		const int newfunction = call_stack.front()->function->id;
		cout << "...back into usecode ";
		print_usecode_function(symtbl, newfunction) << endl;
	}
#endif
}

void Usecode_internal::abort_function(Usecode_value& retval) {
#ifdef DEBUG
	const int functionid = call_stack.front()->function->id;

	cout << "Aborting from usecode " << hex << setw(4) << setfill('0')
		 << functionid << dec << setfill(' ') << endl;
#endif

	// clear the entire call stack up to either a catch or the entry point
	while (call_stack.front() != nullptr) {
		previous_stack_frame();
		Stack_frame* frame = call_stack.front();
		auto         it    = except_stack.find(frame);
		if (it != except_stack.end()) {
			const uint8* target = it->second;
#ifdef DEBUG
			const int functionid = frame->function->id;

			cout << "Abort caught in usecode " << hex << setw(4) << setfill('0')
				 << functionid << " at location " << setw(8) << setfill('0')
				 << target << dec << setfill(' ') << endl;
#endif
			except_stack.erase(it);
			frame->ip = target;
			// push the return value
			push(retval);
			break;
		}
	}
}

/*
 *  Append a string.
 */

void Usecode_internal::append_string(const char* str) {
	if (!str) {
		return;
	}
	// Figure new length.
	int len = String ? strlen(String) : 0;
	len += strlen(str);
	char* newstr = new char[len + 1];
	if (String) {
		strcpy(newstr, String);
		delete[] String;
		String = strcat(newstr, str);
	} else {
		String = strcpy(newstr, str);
	}
}

// Push/pop stack.
inline void Usecode_internal::push(const Usecode_value& val) {
	*sp++ = val;
}

inline Usecode_value Usecode_internal::pop() {
	if (sp <= stack) {
		// Happens in SI #0x939
		cerr << "Stack underflow on function ";
		print_usecode_function(symtbl, call_stack.front()->function->id);
		cerr << " at IP ";
		cout << hex << setw(4) << setfill('0') << (frame->ins_ip - frame->code)
			 << dec << setfill(' ') << std::endl;
		return Usecode_value(0);
	}

	// +++++SHARED:  Shouldn't we reset *sp.
	return *--sp;
}

inline Usecode_value Usecode_internal::peek() {
	return sp[-1];
}

inline void Usecode_internal::pushref(Game_object* obj) {
	const Usecode_value v(obj);
	push(v);
}

inline void Usecode_internal::pushref(Game_object_shared obj) {
	const Usecode_value v(std::move(obj));
	push(v);
}

inline void Usecode_internal::pushi(long val) {    // Push/pop integers.
	const Usecode_value v(val);
	push(v);
}

inline int Usecode_internal::popi() {
	const Usecode_value val = pop();
	return val.need_int_value();
}

// Push/pop strings.
inline void Usecode_internal::pushs(const char* s) {
	const Usecode_value val(s);
	push(val);
}

/*
 *  Get a game object from an "itemref", which might be the actual
 *  pointer, or might be -(npc number).
 *
 *  Output: ->game object.
 */

Game_object* Usecode_internal::get_item(Usecode_value& itemref) {
	// If array, take 1st element.
	const Usecode_value& elemval = itemref.get_elem0();

	if (elemval.is_ptr()) {
		return elemval.get_ptr_value();
	}

	const long val = elemval.get_int_value();
	if (!val) {
		return nullptr;
	}
	Game_object* obj = nullptr;
	if (val == -356) {    // Avatar.
		return gwin->get_main_actor();
	} else if (val < -356 && val > -360) {    // Special cases.
		return nullptr;
	}
	if (val < 0 && val > -gwin->get_num_npcs()) {
		obj = gwin->get_npc(-val);
	} else if (val >= 0) {
		// Special case:  palace guards, Time Lord.
		if (val < 0x400 && !itemref.is_array() && caller_item
			&& ((GAME_BG && val == 0x269)
				|| val == caller_item->get_shapenum())) {
			obj = caller_item.get();
		} else {
			return nullptr;
		}
	}
	return obj;
}

/*
 *  Check for an actor.
 */

Actor* Usecode_internal::as_actor(Game_object* obj) {
	if (!obj) {
		return nullptr;
	}
	return obj->as_actor();
}

/*
 *  Get a position.
 */

Tile_coord Usecode_internal::get_position(Usecode_value& itemval) {
	Game_object* obj;    // An object?
	if ((itemval.get_array_size() == 1 || !itemval.get_array_size())
		&& (obj = get_item(itemval))) {
		return obj->get_outermost()->get_tile();
	} else if (itemval.get_array_size() == 3) {
		// An array of coords.?
		return Tile_coord(
				itemval.get_elem(0).get_int_value(),
				itemval.get_elem(1).get_int_value(),
				itemval.get_elem(2).get_int_value());
	} else if (itemval.get_array_size() == 4) {
		// Result of click_on_item() with
		//  array = (null, tx, ty, tz)?
		return Tile_coord(
				itemval.get_elem(1).get_int_value(),
				itemval.get_elem(2).get_int_value(),
				itemval.get_elem(3).get_int_value());
	} else {    // Else assume caller_item.
		return caller_item->get_tile();
	}
}

/*
 *  Make sure pending text has been seen.
 */

void Usecode_internal::show_pending_text() {
	if (book) {    // Book mode?
		int x;
		int y;
		while (book->show_next_page()
			   && Get_click(x, y, Mouse::hand, nullptr, false, book, true))
			;
		gwin->paint();
	}
	// Normal conversation:
	else if (conv->is_npc_text_pending()) {
		click_to_continue();
	}
}

/*
 *  Show book or scroll text.
 */

void Usecode_internal::show_book() {
	char* str = String;
	book->add_text(str);
	delete[] String;
	String = nullptr;
}

/*
 *  Say the current string and empty it.
 */

void Usecode_internal::say_string() {
	//  user_choice = 0;        // Clear user's response.
	if (!String) {
		return;
	}
	if (book) {    // Displaying a book?
		show_book();
		return;
	}
	show_pending_text();    // Make sure prev. text was seen.
	char* str = String;
	while (*str) {            // Look for stopping points ("~~").
		if (*str == '*') {    // Just gets an extra click.
			click_to_continue();
			str++;
			continue;
		}
		char* eol = strchr(str, '~');
		if (!eol) {    // Not found?
			conv->show_npc_message(str);
			click_to_continue();
			break;
		}
		*eol = 0;
		conv->show_npc_message(str);
		click_to_continue();
		str = eol + 1;
		if (*str == '~') {
			str++;    // 2 in a row.
		}
	}
	delete[] String;
	String = nullptr;
}

/*
 *  Gets the face for an NPC.
 */
int Usecode_internal::get_face_shape(
		Usecode_value& arg1, Actor*& npc, int& frame) {
	npc       = as_actor(get_item(arg1));
	int shape = -1;
	if (arg1.is_int()) {
		shape = std::abs(arg1.get_int_value());
		if (shape == 356) {    // Avatar.
			shape = 0;
		}
	} else if (npc) {
		shape = npc->get_face_shapenum();
	}

	if (shape < 0) {    // No need to do anything else.
		return shape;
	}

	// Checks for Petra flag.
	shape = Shapeinfo_lookup::GetFaceReplacement(shape);

	if (Game::get_game_type() == SERPENT_ISLE) {
		// Special case: Nightmare Smith.
		//   (But don't mess up Guardian.)
		Actor* iact;
		if (shape == 296 && this->frame->caller_item
			&& (iact = this->frame->caller_item->as_actor()) != nullptr
			&& iact->get_npc_num() == 277) {
			shape = 277;
		}
	}

	// Another special case: map face shape 0 to
	// the avatar's correct face shape and frame:
	if (shape == 0) {
		Actor*     ava      = gwin->get_main_actor();
		const bool sishapes = Shape_manager::get_instance()->have_si_shapes();
		Skin_data* skin     = Shapeinfo_lookup::GetSkinInfoSafe(
                ava->get_skin_color(),
                npc ? (npc->get_type_flag(Actor::tf_sex))
						: (ava->get_type_flag(Actor::tf_sex)),
                sishapes);
		if (gwin->get_main_actor()->get_flag(Obj_flags::tattooed)) {
			shape = skin->alter_face_shape;
			frame = skin->alter_face_frame;
		} else {
			shape = skin->face_shape;
			frame = skin->face_frame;
		}
	}
	return shape;
}

/*
 *  Show an NPC's face.
 */

void Usecode_internal::show_npc_face(
		Usecode_value& arg1,    // Shape (NPC #).
		Usecode_value& arg2,    // Frame.
		int            slot     // 0, 1, or -1 to find free spot.
) {
	show_pending_text();
	Actor*    npc;
	int       frame = arg2.get_int_value();
	const int shape = get_face_shape(arg1, npc, frame);

	if (shape < 0) {
		return;
	}

	if (Game::get_game_type() == BLACK_GATE && npc) {
		// Only do this if the NPC is the caller item.
		if (npc->get_npc_num() != -1) {
			npc->set_flag(Obj_flags::met);
		}
	}

	if (!conv->get_num_faces_on_screen()) {
		gwin->get_effects()->remove_text_effects();
	}
	// Only non persistent
	if (gumpman->showing_gumps(true)) {
		gumpman->close_all_gumps();
		gwin->set_all_dirty();
		init_conversation();    // jsf-Added 4/20/01 for SI-Lydia.
	}
	gwin->paint_dirty();
	conv->show_face(shape, frame, slot);
	//	user_choice = 0;     // Seems like a good idea.
	// Also seems to create a conversation bug in Test of Love :-(
}

/*
 *  Remove an NPC's face.
 */

void Usecode_internal::remove_npc_face(Usecode_value& arg1    // Shape (NPC #).
) {
	show_pending_text();
	Actor*    npc;
	const int shape = get_face_shape(arg1, npc);
	if (shape < 0) {
		return;
	}
	conv->remove_face(shape);
}

/*
 *  Set an item's shape.
 */

void Usecode_internal::set_item_shape(
		Usecode_value& item_arg, Usecode_value& shape_arg) {
	const int    shape = shape_arg.get_int_value();
	Game_object* item  = get_item(item_arg);
	if (!item) {
		return;
	}
	// See if light turned on/off.
	const bool light_changed = item->get_info().is_light_source()
							   != ShapeID::get_info(shape).is_light_source();
	if (item->get_owner()) {    // Inside something?
		item->get_owner()->change_member_shape(item, shape);
		if (light_changed) {    // Maybe we should repaint all.
			gwin->paint();      // Repaint finds all lights.
		} else {
			Gump* gump = gumpman->find_gump(item);
			if (gump) {
				gump->paint();
			}
		}
		return;
	}
	gwin->add_dirty(item);
	// Get chunk it's in.
	Map_chunk* chunk = item->get_chunk();
	if (!chunk) {
		CERR("Object " << item
					   << " not in chunk and not owned by another object");
		item->set_shape(shape);
		return;
	}
	const Game_object_shared keep = item->shared_from_this();
	chunk->remove(item);    // Remove and add to update cache.
	item->set_shape(shape);
	chunk->add(item);
	gwin->add_dirty(item);
	//	rect = gwin->get_shape_rect(item).add(rect);
	//	rect.enlarge(8);
	//	rect = gwin->clip_to_win(rect);
	if (light_changed) {
		gwin->paint();    // Complete repaint refigures lights.
	}
	//	else
	//		gwin->paint(rect);  // Not sure...
	//	gwin->show();            // Not sure if this is needed.
}

/*
 *  Set an item's frame.
 */

void Usecode_internal::set_item_frame(
		Game_object* item, int frame,
		int check_empty,    // If 1, don't set empty frame.
		int set_rotated     // Set 'rotated' bit to one in 'frame'.
) {
	if (!item) {
		return;
	}
	// Added 9/16/2001:
	if (!set_rotated) {    // Leave bit alone?
		frame = (item->get_framenum() & 32) | (frame & 31);
	}
	if (frame == item->get_framenum()) {
		return;    // Already set to that.
	}
	Actor* act = as_actor(item);
	// Actors have frame replacements for empty frames:
	if (act) {
		act->change_frame(frame);
	} else {
		// Check for empty frame.
		const ShapeID sid(item->get_shapenum(), frame, item->get_shapefile());
		Shape_frame*  shape = sid.get_shape();
		if (!shape || (check_empty && shape->is_empty())) {
			return;
		}
		// cout << "Set_item_frame: " << item->get_shapenum()
		//              << ", " << frame << endl;
		// (Don't mess up rotated frames.)
		if ((frame & 0xf) < item->get_num_frames()) {
			item->change_frame(frame);
		}
	}
	gwin->set_painted();    // Make sure paint gets done.
}

/*
 *  Set to repaint an object.
 */

void Usecode_internal::add_dirty(Game_object* obj) {
	gwin->add_dirty(obj);
}

/*
 *  Remove an item from the world.
 */

void Usecode_internal::remove_item(Game_object* obj) {
	if (!obj) {
		return;
	}
	if (!last_created.empty() && obj == last_created.back().get()) {
		last_created.pop_back();
	}
	add_dirty(obj);
	if (GAME_SI && frame && frame->function->id == 0x70e
		&& obj->get_shapenum() == 0x113 && obj->get_quality() == 0xd) {
		// Hack to fix broken trap switch in SI temple of Discipline.
		// This works better than the original, in that the trap will stay
		// disabled if you go away and come back, instead of returning and being
		// impossible to disarm.
		Egg_vector vec;    // Gets list.
		// Same parameters used in the egg to activate the trap.
		if (obj->find_nearby_eggs(vec, 0xc8, 0xf)) {
			for (auto* egg : vec) {
				egg->remove_this(nullptr);
			}
		}
	}
	Game_object_shared keep;
	obj->remove_this(obj->as_actor() ? &keep : nullptr);
}

/*
 *  Return an array containing the party, with the Avatar first.
 */

Usecode_value Usecode_internal::get_party() {
	const int     cnt = partyman->get_count();
	Usecode_value arr(1 + cnt, nullptr);
	// Add avatar.
	Usecode_value aval(gwin->get_main_actor());
	arr.put_elem(0, aval);
	int num_added = 1;
	for (int i = 0; i < cnt; i++) {
		Game_object* obj = gwin->get_npc(partyman->get_member(i));
		if (!obj) {
			continue;
		}
		Usecode_value val(obj);
		arr.put_elem(num_added++, val);
	}
	// cout << "Party:  "; arr.print(cout); cout << endl;
	return arr;
}

/*
 *  Put text near an item.
 */

void Usecode_internal::item_say(Usecode_value& objval, Usecode_value& strval) {
	Game_object* obj = get_item(objval);
	const char*  str = strval.get_str_value();
	if (obj && str && *str) {
		Effects_manager* eman = gwin->get_effects();
		// Added Nov01,01 to fix 'locate':
		eman->remove_text_effect(obj);
		if (gwin->failed_copy_protection()) {
			str = "Oink!";
		}
		eman->add_text(str, obj);
	}
}

/*
 *  Activate all cached-in usecode eggs near a given spot.
 */

void Usecode_internal::activate_cached(const Tile_coord& pos) {
	if (Game::get_game_type() != BLACK_GATE) {
		return;    // ++++Since we're not sure about it.
	}
	const int  dist = 16;
	Egg_vector vec;    // Find all usecode eggs.
	Game_object::find_nearby_eggs(vec, pos, 275, dist, c_any_qual, 7);
	for (auto* egg : vec) {
		if (egg->get_criteria() == Egg_object::cached_in) {
			egg->activate();
		}
	}
}

/*
 *  For sorting up-to-down, right-to-left, and near-to-far:
 */
class Object_reverse_sorter {
public:
	bool operator()(const Game_object* o1, const Game_object* o2) {
		const Tile_coord t1 = o1->get_tile();
		const Tile_coord t2 = o2->get_tile();
		if (t1.ty > t2.ty) {
			return true;
		} else if (t1.ty == t2.ty) {
			if (t1.tx > t2.tx) {
				return true;
			} else {
				return t1.tx == t2.tx && t1.tz > t2.tz;
			}
		} else {
			return false;
		}
	}
};

/*
 *  Return an array of nearby objects.
 */

Usecode_value Usecode_internal::find_nearby(
		Usecode_value& objval,      // Find them near this.
		Usecode_value& shapeval,    // Shape to find, or -1 for any,
		//  c_any_shapenum for any npc.
		Usecode_value& distval,    // Distance in tiles?
		Usecode_value& mval        // Some kind of mask?  Guessing:
								   //   4 == party members only.
								   //   8 == non-party NPC's only.
								   //  16 == something with eggs???
								   //  32 == monsters? invisible?
) {
	Game_object_vector vec;    // Gets list.

	int shapenum;

	if (shapeval.is_array()) {
		// fixes 'lightning whip sacrifice' in Silver Seed
		shapenum = shapeval.get_elem(0).get_int_value();
		if (shapeval.get_array_size() > 1) {
			cerr << "Calling find_nearby with an array > 1 !!!!" << endl;
		}
	} else {
		shapenum = shapeval.get_int_value();
	}

	// It might be (tx, ty, tz).
	const int arraysize = objval.get_array_size();
	if (arraysize == 4) {    // Passed result of click_on_item.
		Game_object::find_nearby(
				vec,
				Tile_coord(
						objval.get_elem(1).get_int_value(),
						objval.get_elem(2).get_int_value(),
						objval.get_elem(3).get_int_value()),
				shapenum, distval.get_int_value(), mval.get_int_value());
	} else if (arraysize == 3 || arraysize == 5) {
		// Coords(x,y,z) [qual, frame]
		// Qual is 4th if there.
		const int qual = arraysize == 5 ? objval.get_elem(3).get_int_value()
										: c_any_qual;
		// Frame is 5th if there.
		const int frnum = arraysize == 5 ? objval.get_elem(4).get_int_value()
										 : c_any_framenum;
		Game_object::find_nearby(
				vec,
				Tile_coord(
						objval.get_elem(0).get_int_value(),
						objval.get_elem(1).get_int_value(),
						objval.get_elem(2).get_int_value()),
				shapenum, distval.get_int_value(), mval.get_int_value(), qual,
				frnum);
	} else {
		Game_object* obj = get_item(objval);
		if (!obj) {
			return Usecode_value(0, nullptr);
		}
		obj = obj->get_outermost();    // Might be inside something.
		obj->find_nearby(
				vec, shapenum, distval.get_int_value(), mval.get_int_value());
	}
	if (vec.size() > 1) {    // Sort right-left, near-far to fix
		//   SI/SS cask bug.
		std::sort(vec.begin(), vec.end(), Object_reverse_sorter());
	}
	Usecode_value nearby(vec.size(), nullptr);    // Create return array.
	int           i = 0;
	for (auto* each : vec) {
		Usecode_value val(each);
		nearby.put_elem(i++, val);
	}
	return nearby;
}

/*
 *  Look for a barge that an object is a part of, or on, using the same
 *  sort (right-left, front-back) as ::find_nearby().  If there are more
 *  than one beneath 'obj', the highest is returned.
 *
 *  Output: ->barge if found, else 0.
 */

Barge_object* Get_barge(Game_object* obj) {
	// Check object itself.
	Barge_object* barge = obj->as_barge();
	if (barge) {
		return barge;
	}
	Game_object_vector vec;    // Find it within 20 tiles (egglike).
	obj->find_nearby(vec, 961, 20, 0x10);
	if (vec.size() > 1) {    // Sort right-left, near-far.
		std::sort(vec.begin(), vec.end(), Object_reverse_sorter());
	}
	// Object must be inside it.
	const Tile_coord pos  = obj->get_tile();
	Barge_object*    best = nullptr;
	for (auto* it : vec) {
		barge = it->as_barge();
		if (barge
			&& barge->get_tile_footprint().has_world_point(pos.tx, pos.ty)) {
			const int lift = barge->get_lift();
			if (!best ||    // First qualifying?
							// First beneath obj.?
				(best->get_lift() > pos.tz && lift <= pos.tz) ||
				// Highest beneath?
				(lift <= pos.tz && lift > best->get_lift())) {
				best = barge;
			}
		}
	}
	return best;
}

/*
 *  Return object of given shape nearest given obj.
 */

Usecode_value Usecode_internal::find_nearest(
		Usecode_value& objval,      // Find near this.
		Usecode_value& shapeval,    // Shape to find
		Usecode_value& distval      // Guessing it's distance.
) {
	Game_object* obj = get_item(objval);
	if (!obj) {
		return Usecode_value(static_cast<Game_object*>(nullptr));
	}
	Game_object_vector vec;                    // Gets list.
	obj             = obj->get_outermost();    // Might be inside something.
	int       dist  = distval.get_int_value();
	const int shnum = shapeval.get_int_value();
	// Kludge for Test of Courage:
	if (frame->function->id == 0x70a && shnum == 0x9a && dist == 0) {
		dist = 16;    // Mage may have wandered.
	}
	obj->find_nearby(vec, shnum, dist, 0);
	Game_object*     closest  = nullptr;
	uint32           bestdist = 100000;    // Distance-squared in tiles.
	const Tile_coord t1       = obj->get_tile();
	for (auto* each : vec) {
		const Tile_coord t2   = each->get_tile();
		const int        dx   = t1.tx - t2.tx;
		const int        dy   = t1.ty - t2.ty;
		const int        dz   = t1.tz - t2.tz;
		const uint32     dist = dx * dx + dy * dy + dz * dz;
		if (dist < bestdist) {
			bestdist = dist;
			closest  = each;
		}
	}
	return Usecode_value(closest);
}

/*
 *  Find the angle (0-7) from one object to another.
 */

Usecode_value Usecode_internal::find_direction(
		Usecode_value& from, Usecode_value& to) {
	unsigned         angle;    // Gets angle 0-7 (north - northwest)
	const Tile_coord t1 = get_position(from);
	const Tile_coord t2 = get_position(to);
	// Treat as cartesian coords.
	angle = static_cast<int>(Get_direction(t1.ty - t2.ty, t2.tx - t1.tx));
	return Usecode_value(angle);
}

/*
 *  Count objects of a given shape in a container, or in the whole party.
 */

Usecode_value Usecode_internal::count_objects(
		Usecode_value& objval,    // The container, or -357 for party.
		Usecode_value&
				shapeval,    // Object shape to count (c_any_shapenum=any).
		Usecode_value& qualval,    // Quality (c_any_qual=any).
		Usecode_value& frameval    // Frame (c_any_framenum=any).
) {
	const long oval     = objval.get_int_value();
	const int  shapenum = shapeval.get_int_value();
	const int  qualnum  = qualval.get_int_value();
	const int  framenum = frameval.get_int_value();
	if (oval != -357) {
		Game_object* obj = get_item(objval);
		return Usecode_value(
				!obj ? 0 : obj->count_objects(shapenum, qualnum, framenum));
	}
	// Look through whole party.
	Usecode_value party = get_party();
	const int     cnt   = party.get_array_size();
	int           total = 0;
	for (int i = 0; i < cnt; i++) {
		Game_object* obj = get_item(party.get_elem(i));
		if (obj) {
			total += obj->count_objects(shapenum, qualnum, framenum);
		}
	}
	return Usecode_value(total);
}

/*
 *  Get objects of a given shape in a container.
 */

Usecode_value Usecode_internal::get_objects(
		Usecode_value& objval,    // The container.
		Usecode_value&
				shapeval,    // Object shape to get or c_any_shapenum for any.
		Usecode_value& qualval,    // Quality (c_any_qual=any).
		Usecode_value& frameval    // Frame (c_any_framenum=any).
) {
	Game_object* obj = get_item(objval);
	if (!obj) {
		return Usecode_value(static_cast<Game_object*>(nullptr));
	}
	const int          shapenum = shapeval.get_int_value();
	const int          framenum = frameval.get_int_value();
	const int          qual     = qualval.get_int_value();
	Game_object_vector vec;    // Gets list.
	obj->get_objects(vec, shapenum, qual, framenum);

	//	cout << "Container objects found:  " << cnt << << endl;
	Usecode_value within(vec.size(), nullptr);    // Create return array.
	int           i = 0;
	for (auto* each : vec) {
		Usecode_value val(each);
		within.put_elem(i++, val);
	}
	return within;
}

/*
 *  Remove a quantity of an item from the party.
 *
 *  Output: 1 (or the object) if successful, else 0.
 */

Usecode_value Usecode_internal::remove_party_items(
		Usecode_value& quantval,    // Quantity to remove.
		Usecode_value& shapeval,    // Shape.
		Usecode_value& qualval,     // Quality??
		Usecode_value& frameval,    // Frame.
		Usecode_value& flagval      // Flag??
) {
	ignore_unused_variable_warning(flagval);
	int                 quantity = quantval.need_int_value();
	const int           shapenum = shapeval.get_int_value();
	const int           framenum = frameval.get_int_value();
	const int           quality  = qualval.get_int_value();
	Usecode_value       party    = get_party();
	const int           cnt      = party.get_array_size();
	Usecode_value       all(-357);    // See if they exist.
	const Usecode_value avail = count_objects(all, shapeval, qualval, frameval);
	// Verified. Originally SI-only, allowing for BG too.
	if (quantity == c_any_quantity) {
		quantity = avail.get_int_value();
	} else if (avail.get_int_value() < quantity) {
		return Usecode_value(0);
	}
	const int orig_cnt = quantity;
	// Look through whole party.
	for (int i = 0; i < cnt && quantity > 0; i++) {
		Game_object* obj = get_item(party.get_elem(i));
		if (obj) {
			quantity = obj->remove_quantity(
					quantity, shapenum, quality, framenum);
		}
	}
	return Usecode_value(quantity != orig_cnt);
}

/*
 *  Add a quantity of an item to the party.
 *
 *  Output: List of members who got objects.
 */

Usecode_value Usecode_internal::add_party_items(
		Usecode_value& quantval,    // Quantity to add.
		Usecode_value& shapeval,    // Shape.
		Usecode_value& qualval,     // Quality.
		Usecode_value& frameval,    // Frame.
		Usecode_value& temporary    // If the objects are to be temporary or not
) {
	int quantity = quantval.get_int_value();
	// ++++++First see if there's room.
	const int shapenum = shapeval.get_int_value();
	int       framenum = frameval.get_int_value();
	const int quality  = qualval.get_int_value();
	// Note: the temporary flag only applies to items placed on the
	// ground in SI.
	const bool temp = temporary.get_int_value() != 0;
	// Look through whole party.
	Usecode_value party = get_party();
	const int     cnt   = party.get_array_size();
	Usecode_value result(0, nullptr);    // Start with empty array.
	for (int i = 0; i < cnt && quantity > 0; i++) {
		Game_object* obj = get_item(party.get_elem(i));
		if (!obj) {
			continue;
		}
		const int prev = quantity;
		quantity       = obj->add_quantity(
                quantity, shapenum, quality, framenum, false, GAME_BG && temp);
		if (quantity < prev) {    // Added to this NPC.
			result.concat(party.get_elem(i));
		}
	}
	if (GAME_BG || GAME_SIB) {    // Black gate or Beta SI?  Just return result.
		return result;
	}
	int todo = quantity;    // SI:  Put remaining on the ground.
	if (framenum == c_any_framenum) {
		framenum = 0;
	}
	while (todo > 0) {
		const Tile_coord pos = Map_chunk::find_spot(
				gwin->get_main_actor()->get_tile(), 3, shapenum, framenum, 2);
		if (pos.tx == -1) {    // Hope this rarely happens.
			break;
		}
		const Shape_info& info = ShapeID::get_info(shapenum);
		// Create and place.
		const Game_object_shared newobj
				= gmap->create_ireg_object(info, shapenum, framenum, 0, 0, 0);
		if (quality != c_any_qual) {
			newobj->set_quality(quality);    // set quality
		}
		newobj->set_flag(Obj_flags::okay_to_take);
		if (temp) {    // Mark as temporary.
			newobj->set_flag(Obj_flags::is_temporary);
		}
		newobj->move(pos);
		todo--;
		if (todo > 0) {    // Create quantity if possible.
			todo = newobj->modify_quantity(todo);
		}
	}
	// SI?  Append # left on ground.
	Usecode_value ground(quantity - todo);
	result.concat(ground);
	return result;
}

/*
 *  Add a quantity of an item to a container
 *
 *  Output: Num created
 */

Usecode_value Usecode_internal::add_cont_items(
		Usecode_value& container,    // What do we add to
		Usecode_value& quantval,     // Quantity to add.
		Usecode_value& shapeval,     // Shape.
		Usecode_value& qualval,      // Quality.
		Usecode_value& frameval,     // Frame.
		Usecode_value& temporary    // If the objects are to be temporary or not
) {
	const int  quantity = quantval.get_int_value();
	const int  shapenum = shapeval.get_int_value();
	const int  framenum = frameval.get_int_value();
	int        quality  = qualval.get_int_value();
	const bool temp     = temporary.get_int_value() != 0;
	// e.g., Knight's Test wolf meat.
	if (quality == -359) {
		quality = 0;
	}

	Game_object* obj = get_item(container);
	if (obj) {
		// This fixes teleport storm in SI Beta.
		const int numleft = obj->add_quantity(
				quantity, shapenum, quality, framenum, false, temp);
		if (GAME_SIB) {
			return Usecode_value(quantity - numleft);
		} else {
			return Usecode_value(numleft);
		}
	}
	return Usecode_value(0);
}

/*
 *  Remove a quantity of an item to a container
 *
 *  Output: Num removed
 */

Usecode_value Usecode_internal::remove_cont_items(
		Usecode_value& container,    // What do we add to
		Usecode_value& quantval,     // Quantity to add.
		Usecode_value& shapeval,     // Shape.
		Usecode_value& qualval,      // Quality.
		Usecode_value& frameval,     // Frame.
		Usecode_value& flagval       // Flag??
) {
	Game_object* obj = get_item(container);
	if (!obj) {
		if (container.get_int_value() == -357) {
			return remove_party_items(
					quantval, shapeval, qualval, frameval, flagval);
		}
		return Usecode_value(0);
	}

	int       quantity = quantval.get_int_value();
	const int shapenum = shapeval.get_int_value();
	const int framenum = frameval.get_int_value();
	auto      quality  = static_cast<unsigned int>(qualval.get_int_value());

	if (quantity == c_any_quantity) {
		quantval = count_objects(container, shapeval, qualval, frameval);
		quantity = quantval.get_int_value();
	}

	return Usecode_value(
			quantity
			- obj->remove_quantity(quantity, shapenum, quality, framenum));
}

/*
 *  Create a new object and push it onto the last_created stack.
 */

Game_object_shared Usecode_internal::create_object(
		int  shapenum,
		bool equip    // Equip monsters.
) {
	Game_object_shared obj;    // Create to be written to Ireg.
	const Shape_info&  info = ShapeID::get_info(shapenum);
	modified_map            = true;
	// +++Not sure if 1st test is needed.
	if (info.get_monster_info() || info.is_npc()) {
		// (Wait sched. added for FOV.)
		// don't add equipment (Erethian's transform sequence)
		Game_object_shared new_monster = Monster_actor::create(
				shapenum, Tile_coord(-1, -1, -1), Schedule::wait,
				static_cast<int>(Actor::neutral), true, equip);
		auto* monster = static_cast<Monster_actor*>(new_monster.get());
		// FORCE it to be neutral (dec04,01).
		monster->set_alignment(static_cast<int>(Actor::neutral));
		gwin->add_dirty(monster);
		gwin->add_nearby_npc(monster);
		gwin->show();
		last_created.push_back(monster->shared_from_this());
		return new_monster;
	} else {
		if (info.is_body_shape()) {
			obj = std::make_shared<Dead_body>(shapenum, 0, 0, 0, 0, -1);
		} else {
			obj = gmap->create_ireg_object(shapenum, 0);
			// Be liberal about taking stuff.
			obj->set_flag(Obj_flags::okay_to_take);
		}
	}
	obj->set_invalid();    // Not in world yet.
	obj->set_flag(Obj_flags::okay_to_take);
	last_created.push_back(obj->shared_from_this());
	return obj;
}

/*
 *  Have an NPC walk somewhere and then execute usecode.
 *
 *  Output: true if successful, else false.
 */

bool Usecode_internal::path_run_usecode(
		Usecode_value& npcval,       // # or ref.
		Usecode_value& locval,       // Where to walk to.
		Usecode_value& useval,       // Usecode #.
		Usecode_value& itemval,      // Use as itemref in Usecode fun.
		Usecode_value& eventval,     // Eventid.
		bool           find_free,    // Not sure.  For SI.
		bool           always,       // Always run function, even if failed.
		bool           companions    // For SI:  companions should follow.
) {
	Actor* npc = as_actor(get_item(npcval));
	if (!npc) {
		return false;
	}
	path_npc            = npc;
	const int    usefun = useval.get_elem0().get_int_value();
	Game_object* obj    = get_item(itemval);
	const int    sz     = locval.get_array_size();
	if (!npc || sz < 2) {
		CERR("Path_run_usecode: bad inputs");
		return false;
	}
	const Tile_coord src = npc->get_tile();
	Tile_coord       dest(
            locval.get_elem(0).get_int_value(),
            locval.get_elem(1).get_int_value(),
            sz == 3 ? locval.get_elem(2).get_int_value() : 0);
	if (dest.tz < 0) {    // ++++Don't understand this.
		dest.tz = 0;
	}
	if (find_free) {
		/* Now works with SI lightning platform */
		// Allow rise of 3 (for SI lightning).
		Tile_coord d = Map_chunk::find_spot(dest, 3, npc, 3);
		if (d.tx == -1) {    // No?  Try at source level.
			d = Map_chunk::find_spot(
					Tile_coord(dest.tx, dest.ty, src.tz), 3, npc, 0);
		}
		if (d.tx != -1) {    // Found it?
			dest = d;
		}
		if (usefun == 0x60a &&    // ++++Added 7/21/01 to fix Iron
			src.distance(dest) <= 1) {
			return true;    // Maiden loop in SI.  Kludge+++++++
		}
	}
	if (!obj) {    // Just skip the usecode part.
		const int res = npc->walk_path_to_tile(dest, gwin->get_std_delay(), 0);
		if (res && companions && npc->get_action()) {
			npc->get_action()->set_get_party();
		}
		return res;
	}
	// Walk there and execute.
	auto* action = new If_else_path_actor_action(
			npc, dest,
			new Usecode_actor_action(usefun, obj, eventval.get_int_value()));
	if (companions) {
		action->set_get_party();
	}
	if (always) {    // Set failure to same thing.
		// Note: si_path_run_usecode always uses event 14 for this. Even when
		// it is overridden by UI_set_path_failure. This causes a few bugs in
		// SI usecode because the script developers assumed that the failure
		// event could be overriden by UI_set_path_failure. Most of the time,
		// they just use this value anyway so nothing is lost.
		action->set_failure(new Usecode_actor_action(
				usefun, obj, Usecode_machine::si_path_fail));
	}
	npc->set_action(action);    // Get into time queue.
	npc->start(gwin->get_std_delay(), 0);
	return !action->done_and_failed();
}

/*
 *  See if an actor can go to a given location.
 */

bool Usecode_internal::is_dest_reachable(Actor* npc, const Tile_coord& dest) {
	if (dest.tz < 0) {
		return false;
	}
	if (npc->distance(dest) <= 1) {    // Already OK.
		return true;
	}
	Path_walking_actor_action action(nullptr, 6);
	return action.walk_to_tile(npc, npc->get_tile(), dest, 1) != nullptr;
}

/*
 *  Schedule a script.
 */

void Usecode_internal::create_script(
		Usecode_value& objval, Usecode_value& codeval,
		long delay    // Delay from current time.
) {
	Game_object* obj = get_item(objval);
	// Pure kludge for SI wells:
	if (objval.get_array_size() == 2 && Game::get_game_type() == SERPENT_ISLE
		&& obj && obj->get_shapenum() == 470 && obj->get_lift() == 0) {
		// We want the TOP of the well.
		Usecode_value& v2 = objval.get_elem(1);
		Game_object*   o2 = get_item(v2);
		if (o2->get_shapenum() == obj->get_shapenum() && o2->get_lift() == 2) {
			objval = v2;
			obj    = o2;
		}
	}
	if (!obj) {
		cerr << "Can't create script for nullptr object" << endl;
		return;
	}
	auto* code = new Usecode_value();
	code->steal_array(codeval);    // codeval is undefined after this.
	auto* script = new Usecode_script(obj, code);
	script->start(delay);
}

#ifdef DEBUG
/*
 *  Report unhandled intrinsic.
 */

static void Usecode_Trace(
		const char* name, int intrinsic, int num_parms,
		Usecode_value parms[12]) {
	const boost::io::ios_flags_saver flags(cout);
	const boost::io::ios_fill_saver  fill(cout);
	cout << hex << "    [0x" << setfill('0') << setw(2) << intrinsic
		 << "]: " << name << "(";
	for (int i = 0; i < num_parms; i++) {
		parms[i].print(cout);
		if (i != num_parms - 1) {
			cout << ", ";
		}
	}
	cout << ") = ";
}

static void Usecode_TraceReturn(Usecode_value& v) {
	v.print(cout);
	cout << dec << endl;
}

#endif

Usecode_value no_ret;

Usecode_value Usecode_internal::Execute_Intrinsic(
		UsecodeIntrinsicFn func, const char* name, int intrinsic, int num_parms,
		Usecode_value parms[12]) {
#ifdef XWIN
#	ifdef USECODE_DEBUGGER
	if (usecode_debugging) {
		// Examine the list of intrinsics for function breakpoints.
		if (std::find(
					intrinsic_breakpoints.begin(), intrinsic_breakpoints.end(),
					intrinsic)
			!= intrinsic_breakpoints.end()) {
			raise(SIGIO);    // Breakpoint
		}
	}
#	endif
#endif
#ifndef DEBUG
	ignore_unused_variable_warning(name, intrinsic);
#else
	if (intrinsic_trace) {
		Usecode_Trace(name, intrinsic, num_parms, parms);
		cout.flush();
		Usecode_value u = (this->*func)(num_parms, parms);
		Usecode_TraceReturn(u);
		return u;
	}
#endif
	return (this->*func)(num_parms, parms);
}

using UsecodeIntrinsicFn = Usecode_value (Usecode_internal::*)(
		int num_parms, Usecode_value parms[12]);

// missing from mingw32 header files, so included manually
#ifndef TO_STRING
#	if defined __STDC__ && __STDC__
#		define TO_STRING(x) #x
#	else
#		define TO_STRING(x) "x"
#	endif
#endif

#define USECODE_INTRINSIC_PTR(NAME) \
	{ &Usecode_internal::UI_##NAME, TO_STRING(NAME) }

// Black Gate Intrinsic Function Table
Usecode_internal::IntrinsicTableEntry Usecode_internal::intrinsics_bg[] = {
#include "bgintrinsics.h"
};

// Serpent Isle Intrinsic Function Table
Usecode_internal::IntrinsicTableEntry Usecode_internal::intrinsics_si[] = {
#include "siintrinsics.h"
};

// Serpent Isle Beta Intrinsic Function Table
Usecode_internal::IntrinsicTableEntry Usecode_internal::intrinsics_sib[] = {
#include "sibetaintrinsics.h"
};

/*
 *  Call an intrinsic function.
 */

Usecode_value Usecode_internal::call_intrinsic(
		int intrinsic,    // The ID.
		int num_parms     // # parms on stack.
) {
	static_assert(
			std::size(intrinsics_bg) <= std::numeric_limits<uint16>::max());
	static_assert(
			std::size(intrinsics_si) <= std::numeric_limits<uint16>::max());
	static_assert(
			std::size(intrinsics_sib) <= std::numeric_limits<uint16>::max());
	Usecode_value parms[13];    // Get parms.
	for (int i = 0; i < num_parms; i++) {
		const Usecode_value val = pop();
		parms[i]                = val;
	}
	tcb::span<Usecode_internal::IntrinsicTableEntry> table;
	if (Game::get_game_type() == SERPENT_ISLE) {
		if (Game::is_si_beta()) {
			table = intrinsics_sib;
		} else {
			table = intrinsics_si;
		}
	} else {
		table = intrinsics_bg;
	}
	if (static_cast<size_t>(intrinsic) <= table.size()) {
		auto&                    table_entry = table[intrinsic];
		const UsecodeIntrinsicFn func        = table_entry.func;
		const char*              name        = table_entry.name;
		return Execute_Intrinsic(func, name, intrinsic, num_parms, parms);
	}
	return no_ret;
}

/*
 *  Wait for user to click inside a conversation.
 */

void Usecode_internal::click_to_continue() {
	if (!gwin->get_pal()->is_faded_out()) {    // If black screen, skip!
		int  xx;
		int  yy;
		char c;
		gwin->paint();    // Repaint scenery.
		Get_click(xx, yy, Mouse::hand, &c, false, conv, true);
	}
	conv->clear_text_pending();
	//  user_choice = 0;        // Clear it.
}

/*
 *  Set book/scroll to display.
 */

void Usecode_internal::set_book(Text_gump* b    // Book/scroll.
) {
	if (touchui != nullptr) {
		Gump_manager* gumpman = gwin->get_gump_man();
		if ((book && !b) && !gumpman->gump_mode()) {
			touchui->showGameControls();
		} else if (!book && b) {
			touchui->hideGameControls();
		}
	}
	delete book;
	book = b;
}

/*
 *  Get user's choice from among the possible responses.
 *
 *  Output: ->user choice string.
 *      0 if no possible choices or user quit.
 */

const char* Usecode_internal::get_user_choice() {
	if (!conv->get_num_answers()) {
		return nullptr;    // This does happen (Emps-honey).
	}

	//  if (!user_choice)       // May have already been done.
	// (breaks conversation with Cyclops on Dagger Isle ('foul magic' option))

	get_user_choice_num();
	return user_choice;
}

/*
 *  Get user's choice from among the possible responses.
 *
 *  Output: User choice is set, with choice # returned.
 *      -1 if no possible choices.
 */

int Usecode_internal::get_user_choice_num() {
	delete[] user_choice;
	user_choice = nullptr;
	conv->show_avatar_choices();
	int x;
	int y;    // Get click.
	int choice_num;
	do {
		char chr;         // Allow '1', '2', etc.
		gwin->paint();    // Paint scenery.
		const bool result
				= Get_click(x, y, Mouse::hand, &chr, false, conv, true);
		if (!result) {    // ESC pressed, select 'bye' if poss.
			choice_num = conv->locate_answer("bye");
		} else if (chr) {       // key pressed
			choice_num = -1;    // invalid key
			if (std::isalnum(static_cast<unsigned char>(chr))) {
				constexpr static const char optionKeys[]
						= "123456789abcdefghijklmnopqrstuvwxyz";
				const auto* it = std::find(
						std::cbegin(optionKeys), std::cend(optionKeys), chr);
				auto dist = std::distance(std::cbegin(optionKeys), it);
				if (it != std::cend(optionKeys)
					&& dist < conv->get_num_answers()) {
					choice_num = dist;
				}
			}
		} else {
			choice_num = conv->conversation_choice(x, y);
		}
	}
	// Wait for valid choice.
	while (choice_num < 0 || choice_num >= conv->get_num_answers());

	conv->clear_avatar_choices();
	// Store ->answer string.
	user_choice = newstrdup(conv->get_answer(choice_num));
	return choice_num;    // Return choice #.
}

/*
 *  Create for the outside world.
 */

Usecode_machine* Usecode_machine::create() {
	return new Usecode_internal();
}

/*
 *  Create machine from a 'usecode' file.
 */

Usecode_internal::Usecode_internal() : stack(new Usecode_value[1024]) {
	sp = stack;
	// Read in usecode.
	std::cout << "Reading usecode file." << std::endl;
	try {
		auto pFile = U7open_in(USECODE);
		if (!pFile) {
			throw file_open_exception(USECODE);
		}
		auto& file = *pFile;
		read_usecode(file);
	} catch (const file_exception& /*f*/) {
		if (!Game::is_editing()) {    // Ok if map-editing.
			throw;
		}
		std::cerr << "Warning (map-editing): Couldn't open '" << USECODE << "'"
				  << endl;
	}

	// Get custom usecode functions.
	if (is_system_path_defined("<PATCH>") && U7exists(PATCH_USECODE)) {
		auto pFile = U7open_in(PATCH_USECODE);
		if (!pFile) {
			throw file_open_exception(PATCH_USECODE);
		}
		auto& file = *pFile;
		read_usecode(file, true);
	}

	//  set_breakpoint();
}

/*
 *  Read in usecode functions.  These may override previously read
 *  functions.
 */

void Usecode_internal::read_usecode(
		istream& file,
		bool     patch    // True if reading from 'patch'.
) {
	file.seekg(0, ios::end);
	const int size = file.tellg();    // Get file size.
	file.seekg(0);
	if (Usecode_symbol_table::has_symbol_table(file)) {
		delete symtbl;
		symtbl = new Usecode_symbol_table();
		symtbl->read(file);
	}
	// Read in all the functions.
	while (file.tellg() < size) {
		auto*              fun     = new Usecode_function(file);
		const unsigned int slotnum = fun->id / 0x100;
		if (slotnum >= funs.size()) {
			funs.resize(slotnum < 10 ? 10 : slotnum + 1);
		}
		Funs256&           vec = funs[slotnum];
		const unsigned int i   = fun->id % 0x100;
		if (i >= vec.size()) {
			vec.resize(i + 1);
		} else if (vec[i]) {
			// Already have one there.
			if (patch) {    // Patching?
				if (vec[i]->orig) {
					// Patching a patch.
					fun->orig = vec[i]->orig;
					delete vec[i];
				} else {    // Patching fun. from static.
					fun->orig = vec[i];
				}
			} else {
				delete vec[i]->orig;
				delete vec[i];
			}
		}
		vec[i] = fun;
	}
}

/*
 *  Delete.
 */

Usecode_internal::~Usecode_internal() {
	delete[] stack;
	delete[] String;
	delete symtbl;
	const int num_slots = funs.size();
	for (int i = 0; i < num_slots; i++) {
		Funs256&  slot = funs[i];
		const int cnt  = slot.size();
		for (int j = 0; j < cnt; j++) {
			delete slot[j];
		}
	}
	delete book;
}

#ifdef DEBUG
int        debug    = 2;                   // 2 for more stuff.
static int ucbp_fun = -1, ucbp_ip = -1;    // Breakpoint.

void Setbreak(int fun, int ip) {
	ucbp_fun = fun;
	ucbp_ip  = ip;
}

void Clearbreak() {
	ucbp_fun = ucbp_ip = -1;
}
#endif

#define CERR_CURRENT_IP()                                         \
	cerr << " (at function = " << hex << setw(4) << setfill('0')  \
		 << frame->function->id << ", ip = " << current_IP << dec \
		 << setfill(' ') << ")" << endl

#define LOCAL_VAR_ERROR(x)                                 \
	cerr << "Local variable #" << (x) << " out of range!"; \
	CERR_CURRENT_IP()

#define DATA_SEGMENT_ERROR()              \
	cerr << "Data pointer out of range!"; \
	CERR_CURRENT_IP()

#define EXTERN_ERROR()                     \
	cerr << "Extern offset out of range!"; \
	CERR_CURRENT_IP()

#define FLAG_ERROR(x)                                   \
	cerr << "Global flag #" << (x) << " out of range!"; \
	CERR_CURRENT_IP()

#define THIS_ERROR()                  \
	cerr << "nullptr class pointer!"; \
	CERR_CURRENT_IP()

/*
 *  The main usecode interpreter
 *
 *  Output:
 */

int Usecode_internal::run() {
	bool aborted           = false;
	bool initializing_loop = false;

	while ((frame = call_stack.front())) {
		const int num_locals = frame->num_vars + frame->num_args;
		int       offset;
		int       sval;

		bool frame_changed = false;

		// set some variables for use in other member functions
		caller_item = frame->caller_item;

		/*
		 *  Main loop.
		 */
		while (!frame_changed) {
			if ((frame->ip >= frame->endp) || (frame->ip < frame->code)) {
				cerr << "Usecode: jumped outside of code segment of "
					 << "function " << hex << setw(4) << setfill('0')
					 << frame->function->id << dec << setfill(' ')
					 << " ! Aborting." << endl;

				Usecode_value msg("Out of bounds usecode execution!");
				abort_function(msg);
				frame_changed = true;
				continue;
			}

			const auto current_IP = frame->ip - frame->code;
			frame->ins_ip         = frame->ip;

			auto opcode = static_cast<UsecodeOps>(*(frame->ip));

			if (frame->ip + get_opcode_length(static_cast<int>(opcode))
				> frame->endp) {
				cerr << "Operands lie outside of code segment. ";
				CERR_CURRENT_IP();
				continue;
			}

#ifdef DEBUG
			if (usecode_trace == 2) {
				uc_trace_disasm(frame);
			}
#endif

#ifdef USECODE_DEBUGGER
			// check breakpoints

			const int bp = breakpoints.check(frame);
			if (bp != -1) {
				// we hit a breakpoint

				// allow handling extra debugging messages
				on_breakpoint = true;

				cout << "On breakpoint" << endl;

				// signal remote client that we hit a breakpoint
				auto c = static_cast<unsigned char>(
						Exult_server::dbg_on_breakpoint);
				if (client_socket >= 0) {
					Exult_server::Send_data(
							client_socket, Exult_server::usecode_debugging, &c,
							1);
				}

#	ifdef XWIN
				raise(SIGUSR1);    // to allow trapping it in gdb too
#	endif

#	ifdef USECODE_CONSOLE_DEBUGGER
				// little console mode "debugger" (if you can call it that...)
				bool done = false;
				while (!done) {
					char userinput;
					cout << "s=step into, o=step over, f=finish, c=continue, "
						 << "b=stacktrace: ";
					std::cin >> userinput;

					if (userinput == 's') {
						breakpoints.add(new AnywhereBreakpoint());
						cout << "Stepping into..." << endl;
						done = true;
					} else if (userinput == 'o') {
						breakpoints.add(new StepoverBreakpoint(frame));
						cout << "Stepping over..." << endl;
						done = true;
					} else if (userinput == 'f') {
						breakpoints.add(new FinishBreakpoint(frame));
						cout << "Finishing function..." << endl;
						done = true;
					} else if (userinput == 'c') {
						done = true;
					} else if (userinput == 'b') {
						stack_trace(cout);
					}
				}
#	elif (defined(USE_EXULTSTUDIO))
				breakpoint_action = -1;
				while (breakpoint_action == -1) {
					SDL_Delay(20);
					Server_delay(Handle_client_debug_message);
				}
#	endif

				c = static_cast<unsigned char>(Exult_server::dbg_continuing);
				if (client_socket >= 0) {
					Exult_server::Send_data(
							client_socket, Exult_server::usecode_debugging, &c,
							1);
				}
				// disable extra debugging messages again
				on_breakpoint = false;
			}
#endif

			frame->ip++;

			switch (opcode) {
			case UC_CONVERSE:        // start conversation
			case UC_CONVERSE32: {    // (32 bit version)
				if (opcode < UC_EXTOPCODE) {
					offset = little_endian::Read2s(frame->ip);
				} else {
					offset = little_endian::Read4s(frame->ip);
				}

				found_answer = false;
				if (!get_user_choice()) {    // Exit conv. if no choices.
					frame->ip += offset;     // (Emps and honey.)
				}
				break;
			}
			case UC_JNE:        // JNE.
			case UC_JNE32: {    // JNE32
				if (opcode < UC_EXTOPCODE) {
					offset = little_endian::Read2s(frame->ip);
				} else {
					offset = little_endian::Read4s(frame->ip);
				}
				const Usecode_value val = pop();
				if (val.is_false()) {
					frame->ip += offset;
				}
				break;
			}
			case UC_JMP:      // JMP.
			case UC_JMP32:    // JMP32
				if (opcode < UC_EXTOPCODE) {
					offset = little_endian::Read2s(frame->ip);
				} else {
					offset = little_endian::Read4s(frame->ip);
				}
				frame->ip += offset;
				break;
			case UC_CMPS:        // CMPS.
			case UC_CMPS32: {    // (32 bit version)
				int cnt = little_endian::Read2(frame->ip);    // # strings.
				if (opcode < UC_EXTOPCODE) {
					offset = little_endian::Read2s(frame->ip);
				} else {
					offset = little_endian::Read4s(frame->ip);
				}

				bool matched = false;

				// only try to match if we haven't found an answer yet
				while (!matched && !found_answer && cnt-- > 0) {
					const Usecode_value s   = pop();
					const char*         str = s.get_str_value();
					if (str && strcmp(str, user_choice) == 0) {
						matched      = true;
						found_answer = true;
					}
				}
				while (cnt-- > 0) {    // Pop rest of stack.
					pop();
				}
				if (!matched) {    // Jump if no match.
					frame->ip += offset;
				}
			} break;
			case UC_ADD: {    // ADD.
				const Usecode_value v2     = pop();
				const Usecode_value v1     = pop();
				const Usecode_value retval = v1 + v2;
				push(retval);
				break;
			}
			case UC_SUB: {    // SUB.
				const Usecode_value v2     = pop();
				const Usecode_value v1     = pop();
				const Usecode_value retval = v1 - v2;
				push(retval);
				break;
			}
			case UC_DIV: {    // DIV.
				const Usecode_value v2     = pop();
				const Usecode_value v1     = pop();
				const Usecode_value retval = v1 / v2;
				push(retval);
				break;
			}
			case UC_MUL: {    // MUL.
				const Usecode_value v2     = pop();
				const Usecode_value v1     = pop();
				const Usecode_value retval = v1 * v2;
				push(retval);
				break;
			}
			case UC_MOD: {    // MOD.
				const Usecode_value v2     = pop();
				const Usecode_value v1     = pop();
				const Usecode_value retval = v1 % v2;
				push(retval);
				break;
			}
			case UC_AND: {    // AND.
				const Usecode_value v1     = pop();
				const Usecode_value v2     = pop();
				const bool          result = v1.is_true() && v2.is_true();
				pushi(result);
				break;
			}
			case UC_OR: {    // OR.
				const Usecode_value v1     = pop();
				const Usecode_value v2     = pop();
				const bool          result = v1.is_true() || v2.is_true();
				pushi(result);
				break;
			}
			case UC_NOT:    // NOT.
				pushi(!pop().is_true());
				break;
			case UC_POP: {    // POP into a variable.
				offset = little_endian::Read2(frame->ip);
				// Get value.
				const Usecode_value val = pop();
				if (offset < 0 || offset >= num_locals) {
					LOCAL_VAR_ERROR(offset);
				} else {
					frame->locals[offset] = val;
				}
				break;
			}
			case UC_PUSHTRUE:    // PUSH true.
				pushi(1);
				break;
			case UC_PUSHFALSE:    // PUSH false.
				pushi(0);
				break;
			case UC_CMPGT:    // CMPGT.
				sval = popi();
				pushi(popi() > sval);    // Order?
				break;
			case UC_CMPLT:    // CMPLT.
				sval = popi();
				pushi(popi() < sval);
				break;
			case UC_CMPGE:    // CMPGE.
				sval = popi();
				pushi(popi() >= sval);
				break;
			case UC_CMPLE:    // CMPLE.
				sval = popi();
				pushi(popi() <= sval);
				break;
			case UC_CMPNE: {    // CMPNE.
				const Usecode_value val1 = pop();
				const Usecode_value val2 = pop();
				pushi(!(val1 == val2));
				break;
			}
			case UC_ADDSI:      // ADDSI.
			case UC_ADDSI32:    // ADDSI32
				if (opcode < UC_EXTOPCODE) {
					offset = little_endian::Read2(frame->ip);
				} else {
					offset = little_endian::Read4s(frame->ip);
				}
				if (offset < 0 || frame->data + offset >= frame->externs - 6) {
					DATA_SEGMENT_ERROR();
					break;
				}
				append_string(frame->data + offset);
				break;
			case UC_PUSHS:      // PUSHS.
			case UC_PUSHS32:    // PUSHS32
				if (opcode < UC_EXTOPCODE) {
					offset = little_endian::Read2(frame->ip);
				} else {
					offset = little_endian::Read4s(frame->ip);
				}
				if (offset < 0 || frame->data + offset >= frame->externs - 6) {
					DATA_SEGMENT_ERROR();
					break;
				}
				pushs(frame->data + offset);
				break;
			case UC_ARRC: {    // ARRC.
				// Get # values to pop into array.
				const int     num = little_endian::Read2(frame->ip);
				int           cnt = num;
				Usecode_value arr(num, nullptr);
				int           to = 0;    // Store at this index.
				while (cnt--) {
					Usecode_value val = pop();
					to += arr.add_values(to, val);
				}
				if (to < num) {    // 1 or more vals empty arrays?
					arr.resize(to);
				}
				push(arr);
				break;
			}
			case UC_PUSHI:        // PUSHI.
			case UC_PUSHI32: {    // PUSHI32
				// Might be negative.
				int ival;
				if (opcode < UC_EXTOPCODE) {
					ival = little_endian::Read2s(frame->ip);
				} else {
					ival = little_endian::Read4s(frame->ip);
				}
				pushi(ival);
				break;
			}
			case UC_PUSH:    // PUSH.
				offset = little_endian::Read2(frame->ip);
				if (offset < 0 || offset >= num_locals) {
					LOCAL_VAR_ERROR(offset);
					pushi(0);
				} else {
					push(frame->locals[offset]);
				}
				break;
			case UC_CMPEQ: {    // CMPEQ.
				const Usecode_value val1 = pop();
				const Usecode_value val2 = pop();
				pushi(val1 == val2);
				break;
			}
			case UC_CALL: {    // CALL.
				offset = little_endian::Read2(frame->ip);
				if (offset < 0 || offset >= frame->num_externs) {
					EXTERN_ERROR();
					break;
				}

				const uint8* tempptr = frame->externs + 2 * offset;
				const int    funcid  = little_endian::Read2(tempptr);

				call_function(funcid, frame->eventid);
				frame_changed = true;
				break;
			}
			case UC_CALL32: {    // 32-bit CALL.
				offset = little_endian::Read4s(frame->ip);
				call_function(offset, frame->eventid);
				frame_changed = true;
				break;
			}
			case UC_RET:     // RET. (End of procedure reached)
			case UC_RET2:    // RET. (Return from procedure)
				show_pending_text();

				return_from_procedure();
				frame_changed = true;
				break;
			case UC_AIDX:         // AIDX.
			case UC_AIDXS:        // AIDXS.
			case UC_AIDXTHV: {    // AIDXTHV.
				sval = popi();    // Get index into array.
				sval--;           // It's 1 based.
				// Get # of local to index.
				Usecode_value* val;
				if (opcode == UC_AIDX) {
					offset = little_endian::Read2(frame->ip);
					if (offset < 0 || offset >= num_locals) {
						LOCAL_VAR_ERROR(offset);
						pushi(0);
						break;
					}
					val = &(frame->locals[offset]);
				} else if (opcode == UC_AIDXTHV) {
					offset             = little_endian::Read2(frame->ip);
					Usecode_value& ths = frame->get_this();
					if (offset < 0 || offset >= ths.get_class_var_count()) {
						cerr << "Class variable #" << (offset)
							 << " out of range!";
						CERR_CURRENT_IP();
						break;
					}
					val = &(ths.nth_class_var(offset));
				} else {
					offset = little_endian::Read2s(frame->ip);
					if (offset < 0) {    // Global static.
						if (static_cast<unsigned>(-offset) < statics.size()) {
							val = &(statics[-offset]);
						} else {
							cerr << "Global static variable #" << (offset)
								 << " out of range!";
							pushi(0);
							break;
						}
					} else {
						if (static_cast<unsigned>(offset)
							< frame->function->statics.size()) {
							val = &(frame->function->statics[offset]);
						} else {
							cerr << "Local static variable #" << (offset)
								 << " out of range!";
							pushi(0);
							break;
						}
					}
				}
				if (sval < 0) {
					cerr << "AIDX: Negative array index: " << sval << endl;
					pushi(0);
					break;
				}
				if (val->is_array() && size_t(sval) >= val->get_array_size()) {
					pushi(0);              // Matches originals.
				} else if (sval == 0) {    // needed for SS keyring (among
										   // others)
					push(val->get_elem0());
				} else {
					push(val->get_elem(sval));
				}
				break;
			}
			case UC_RETV: {    // RET. (Return from function)
				// ++++ Testing.
				show_pending_text();
				Usecode_value r = pop();

				return_from_function(r);
				frame_changed = true;
				break;
			}
			case UC_LOOP:        // INITLOOP (1st byte of loop)
			case UC_LOOP32: {    // (32 bit version)
				int nextopcode = *(frame->ip);
				// No real reason to have 32-bit version of this instruction;
				// keeping it for backward compatibility only.
				nextopcode &= 0x7f;
				if (nextopcode != UC_LOOPTOP && nextopcode != UC_LOOPTOPS
					&& nextopcode != UC_LOOPTOPTHV) {
					cerr << "Invalid 2nd byte in loop!" << endl;
					break;
				} else {
					initializing_loop = true;
				}
				break;
			}
			case UC_LOOPTOP:       // LOOP (2nd byte of loop)
			case UC_LOOPTOP32:     // (32 bit version)
			case UC_LOOPTOPS:      // LOOP (2nd byte of loop) using static array
			case UC_LOOPTOPS32:    // (32 bit version)
			case UC_LOOPTOPTHV:    // LOOP (2nd byte of loop) using class member
								   // array
			case UC_LOOPTOPTHV32: {    // (32 bit version)
				// Counter (1-based).
				const int local1 = little_endian::Read2(frame->ip);
				// Total count.
				const int local2 = little_endian::Read2(frame->ip);
				// Current value of loop var.
				const int local3 = little_endian::Read2(frame->ip);
				// Array of values to loop over.
				int        local4;
				const bool is_32bit = (opcode >= UC_EXTOPCODE);
				// Mask off 32bit flag.
				opcode &= 0x7f;
				if (opcode == UC_LOOPTOPS) {
					local4 = little_endian::Read2s(frame->ip);
				} else {
					local4 = little_endian::Read2(frame->ip);
				}
				// Get offset to end of loop.
				if (is_32bit) {
					offset = little_endian::Read4s(
							frame->ip);    // 32 bit offset
				} else {
					offset = little_endian::Read2s(frame->ip);
				}

				if (local1 < 0 || local1 >= num_locals) {
					LOCAL_VAR_ERROR(local1);
					break;
				}
				if (local2 < 0 || local2 >= num_locals) {
					LOCAL_VAR_ERROR(local2);
					break;
				}
				if (local3 < 0 || local3 >= num_locals) {
					LOCAL_VAR_ERROR(local3);
					break;
				}
				if (opcode == UC_LOOPTOPS) {
					if (local4 < 0) {    // Global static.
						if (static_cast<unsigned>(-local4) >= statics.size()) {
							cerr << "Global static variable #" << (-local4)
								 << " out of range!";
							CERR_CURRENT_IP();
							break;
						}
					} else {
						if (static_cast<unsigned>(local4)
							>= frame->function->statics.size()) {
							cerr << "Local static variable #" << (local4)
								 << " out of range!";
							CERR_CURRENT_IP();
							break;
						}
					}
				} else if (opcode == UC_LOOPTOPTHV) {
					Usecode_value& ths = frame->get_this();
					if (local4 < 0 || local4 >= ths.get_class_var_count()) {
						cerr << "Class variable #" << (local4)
							 << " out of range!";
						CERR_CURRENT_IP();
						break;
					}
				} else {
					if (local4 < 0 || local4 >= num_locals) {
						LOCAL_VAR_ERROR(local4);
						break;
					}
				}

				// Get array to loop over.
				Usecode_value& arr
						= opcode == UC_LOOPTOPS
								  ? (local4 < 0
											 ? statics[-local4]
											 : frame->function->statics[local4])
								  : (opcode == UC_LOOPTOPTHV
											 ? frame->get_this().nth_class_var(
													   local4)
											 : frame->locals[local4]);
				if (initializing_loop && arr.is_undefined()) {
					// If the local 'array' is not initialized, do not loop
					// (verified in FoV and SS):
					initializing_loop = false;
					frame->ip += offset;
					break;
				}

				int next = frame->locals[local1].get_int_value();

				if (initializing_loop) {
					// Initialize loop.
					initializing_loop = false;
					const int cnt = arr.is_array() ? arr.get_array_size() : 1;
					frame->locals[local2] = Usecode_value(cnt);
					frame->locals[local1] = Usecode_value(0);

					next = 0;
				}

				// in SI, the loop-array can be modified in-loop, it seems
				// (conv. with Spektran, 044D:00BE)

				// so, check for changes of the array size, and adjust
				// total count and next value accordingly.

				// Allowing this for BG too.

				const int cnt = arr.is_array() ? arr.get_array_size() : 1;

				if (cnt != frame->locals[local2].get_int_value()) {
					// update new total count
					frame->locals[local2] = Usecode_value(cnt);

					if (std::abs(cnt - frame->locals[local2].get_int_value())
						== 1) {
						// small change... we can fix this
						const Usecode_value& curval
								= arr.is_array() ? arr.get_elem(next - 1) : arr;

						if (curval != frame->locals[local3]) {
							if (cnt > frame->locals[local2].get_int_value()) {
								// array got bigger, it seems
								// addition occured before the current value
								next++;
							} else {
								// array got smaller
								// deletion occured before the current value
								next--;
							}
						} else {
							// addition/deletion was after the current value
							// so don't need to update 'next'
						}
					} else {
						// big change...
						// just update total count to make sure
						// we don't crash
					}
				}

				if (cnt != frame->locals[local2].get_int_value()) {
					// update new total count
					frame->locals[local2] = Usecode_value(cnt);

					const Usecode_value& curval
							= arr.is_array() ? arr.get_elem(next - 1) : arr;

					if (curval != frame->locals[local3]) {
						if (cnt > frame->locals[local2].get_int_value()) {
							// array got bigger, it seems
							// addition occured before the current value
							next++;
						} else {
							// array got smaller
							// deletion occured before the current value
							next--;
						}
					} else {
						// addition/deletion was after the current value
						// so don't need to update 'next'
					}
				}

				// End of loop?
				if (next >= frame->locals[local2].get_int_value()) {
					frame->ip += offset;
				} else {    // Get next element.
					frame->locals[local3]
							= arr.is_array() ? arr.get_elem(next) : arr;
					frame->locals[local1] = Usecode_value(next + 1);
				}
				break;
			}
			case UC_ADDSV: {    // ADDSV.
				offset = little_endian::Read2(frame->ip);
				if (offset < 0 || offset >= num_locals) {
					LOCAL_VAR_ERROR(offset);
					break;
				}

				const char* str = frame->locals[offset].get_str_value();
				if (str) {
					append_string(str);
				} else {    // Convert integer.
					// 25-09-2001 - Changed to >= 0 to fix money-counting in SI.
					//              if (locals[offset].get_int_value() != 0) {
					if (frame->locals[offset].get_int_value() >= 0) {
						char buf[20];
						snprintf(
								buf, sizeof(buf), "%ld",
								frame->locals[offset].get_int_value());
						append_string(buf);
					}
				}
				break;
			}
			case UC_IN: {    // IN.  Is a val. in an array?
				Usecode_value arr = pop();
				// If an array, use 1st elem.
				const Usecode_value val = pop().get_elem0();
				pushi(arr.find_elem(val) >= 0);
				break;
			}
			case UC_DEFAULT:      // Conversation default.
			case UC_DEFAULT32:    // (32 bit version)
				// This opcode only occurs in the 'audition' usecode function
				// (BG) It is a version of CMPS that pops no values from the
				// stack (ignores the count parameter) and either branches to
				// the next case if an answer has been picked already or marks
				// an answer as being found, so that no more answers can be
				// found after this opcode runs until the next converse loop.
				// This means that it can be dangerous to place it before other
				// CMPS cases, as they will never match.
				frame->ip += 2;
				if (opcode < UC_EXTOPCODE) {
					offset = little_endian::Read2s(frame->ip);
				} else {
					offset = little_endian::Read4s(frame->ip);
				}

				if (!found_answer) {
					found_answer = true;
				} else {
					frame->ip += offset;
				}
				break;

			case UC_RETZ: {    // RET. (End of function reached)
				show_pending_text();

				Usecode_value zero(0);
				return_from_function(zero);
				frame_changed = true;
				break;
			}
			case UC_SAY:    // SAY.
				say_string();
				break;
			case UC_CALLIS: {    // CALLIS.
				offset = little_endian::Read2(frame->ip);
				sval   = *(frame->ip)++;    // # of parameters.
				const Usecode_value ival = call_intrinsic(offset, sval);
				push(ival);
				frame_changed = true;
				break;
			}
			case UC_CALLI:    // CALLI.
				offset = little_endian::Read2(frame->ip);
				sval   = *(frame->ip)++;    // # of parameters.
				call_intrinsic(offset, sval);
				frame_changed = true;
				break;
			case UC_PUSHITEMREF:    // PUSH ITEMREF.
				pushref(frame->caller_item);
				break;
			case UC_ABRT: {    // ABRT.
				show_pending_text();

				Usecode_value msg("abort executed");
				abort_function(msg);
				frame_changed = true;
				aborted       = true;
				break;
			}
			case UC_THROW: {    // THROW.
				show_pending_text();

				Usecode_value r = pop();
				abort_function(r);
				frame_changed = true;
				aborted       = true;
				break;
			}
			case UC_TRYSTART:
			case UC_TRYSTART32:
				if (opcode < UC_EXTOPCODE) {
					offset = little_endian::Read2s(frame->ip);
				} else {
					offset = little_endian::Read4s(frame->ip);
				}
				except_stack[frame] = frame->ip + offset;
				break;
			case UC_TRYEND: {
				auto it = except_stack.find(frame);
				if (it != except_stack.end()) {
					except_stack.erase(it);
				}
				break;
			}
			case UC_CONVERSELOC:    // end conversation
				found_answer = true;
				break;
			case UC_PUSHF:       // PUSHF.
			case UC_PUSHFVAR:    // PUSHF2.
				if (opcode >= UC_EXTOPCODE) {
					offset = popi();
				} else {
					offset = little_endian::Read2(frame->ip);
				}
				if (offset < 0
					|| static_cast<unsigned>(offset) >= sizeof(gflags)) {
					FLAG_ERROR(offset);
					pushi(0);
				} else {
					pushi(gflags[offset]);
				}
				break;
			case UC_POPF:       // POPF.
			case UC_POPFVAR:    // POPF2.
				if (opcode >= UC_EXTOPCODE) {
					offset = popi();
				} else {
					offset = little_endian::Read2(frame->ip);
				}
				if (offset < 0
					|| static_cast<unsigned>(offset) >= sizeof(gflags)) {
					FLAG_ERROR(offset);
				} else {
					gflags[offset] = static_cast<unsigned char>(popi());
					if (gflags[offset]) {
						Notebook_gump::add_gflag_text(offset);
#ifdef DEBUG
						cout << "Setting global flag: " << offset << endl;
#endif
					}
					// ++++KLUDGE for Monk Isle:
					if (offset == 0x272 && GAME_SI) {
						gflags[offset] = 0;
					}
				}
				break;
			case UC_PUSHB:    // PUSHB.
				pushi(*(frame->ip)++);
				break;
			case UC_POPARR:         // Set array element.
			case UC_POPARRS:        // Set static array element.
			case UC_POPARRTHV: {    // Set class member array element.
				Usecode_value* arr;
				if (opcode == UC_POPARR) {
					offset = little_endian::Read2(frame->ip);
					// Get # of local array.
					if (offset < 0 || offset >= num_locals) {
						LOCAL_VAR_ERROR(offset);
						break;
					}
					arr = &(frame->locals[offset]);
				} else if (opcode == UC_POPARRTHV) {
					offset             = little_endian::Read2(frame->ip);
					Usecode_value& ths = frame->get_this();
					if (offset < 0 || offset >= ths.get_class_var_count()) {
						cerr << "Class variable #" << (offset)
							 << " out of range!";
						CERR_CURRENT_IP();
						break;
					}
					arr = &(ths.nth_class_var(offset));
				} else {
					offset = little_endian::Read2s(frame->ip);
					if (offset < 0) {    // Global static.
						if (static_cast<unsigned>(-offset) < statics.size()) {
							arr = &(statics[-offset]);
						} else {
							cerr << "Global static variable #" << (offset)
								 << " out of range!";
							CERR_CURRENT_IP();
							break;
						}
					} else {
						if (static_cast<unsigned>(offset)
							< frame->function->statics.size()) {
							arr = &(frame->function->statics[offset]);
						} else {
							cerr << "Local static variable #" << (offset)
								 << " out of range!";
							CERR_CURRENT_IP();
							break;
						}
					}
				}
				short index = popi();
				index--;    // It's 1-based.
				Usecode_value val  = pop();
				const int     size = arr->get_array_size();
				if (index >= 0 && (index < size || arr->resize(index + 1))) {
					arr->put_elem(index, val);
				}
				break;
			}
			case UC_CALLE:        // CALLE.  Stack has caller_item.
			case UC_CALLE32: {    // 32-bit version.
				Usecode_value ival   = pop();
				Game_object*  caller = get_item(ival);
				if (opcode < UC_EXTOPCODE) {
					offset = little_endian::Read2(frame->ip);
				} else {
					offset = little_endian::Read4s(frame->ip);
				}
				call_function(offset, frame->eventid, caller);
				frame_changed = true;
				break;
			}
			case UC_PUSHEVENTID:    // PUSH EVENTID.
				pushi(frame->eventid);
				break;
			case UC_ARRA: {    // ARRA.
				Usecode_value val = pop();
				Usecode_value arr = pop();
				push(arr.concat(val));
				break;
			}
			case UC_POPEVENTID:    // POP EVENTID.
				frame->eventid = popi();
				break;
			case UC_DBGLINE: {    // debugging opcode from spanish SI (line
								  // number)
				frame->line_number = little_endian::Read2(frame->ip);
				break;
			}
			case UC_DBGFUNC:    // debugging opcode from spanish SI (function
								// init)
			case UC_DBGFUNC32: {    // 32 bit debugging function init
				int funcname;
				int paramnames;
				if (opcode < UC_EXTOPCODE) {
					funcname   = little_endian::Read2(frame->ip);
					paramnames = little_endian::Read2(frame->ip);
				} else {
					funcname   = little_endian::Read4s(frame->ip);
					paramnames = little_endian::Read4s(frame->ip);
				}
				if (funcname < 0
					|| frame->data + funcname >= frame->externs - 6) {
					DATA_SEGMENT_ERROR();
					break;
				}
				if (paramnames < 0
					|| frame->data + paramnames >= frame->externs - 6) {
					DATA_SEGMENT_ERROR();
					break;
				}
				cout << "Debug opcode found at function = " << hex << setw(4)
					 << setfill('0') << frame->function->id
					 << ", ip = " << current_IP << dec << setfill(' ') << "."
					 << endl;
				cout << "Information is: funcname = '"
					 // This is a complete guess:
					 << (frame->data + funcname) << "'." << endl;
				const char* ptr = reinterpret_cast<const char*>(
						frame->data + paramnames);
				// This is an even bigger complete guess:
				if (*ptr) {
					int nargs = frame->num_args;
					if (is_object_fun(frame->function->id)) {
						nargs--;    // Function has an 'item'.
					}
					if (nargs < 0) {    // Just in case.
						nargs = 0;
					}
					std::vector<std::string> names;
					names.resize(nargs);
					int i;
					// Reversed to match the order in which they are
					// passed in UCC.
					for (i = nargs - 1; i >= 0 && *ptr; i--) {
						const std::string name(ptr);
						names[i] = name;
						ptr += name.length() + 1;
					}
					cout << "Parameter names follow: ";
					for (i = 0; i < nargs; i++) {
						cout << "#" << hex << setw(4) << setfill('0') << i
							 << " = ";
						if (names[i].length()) {
							cout << "'" << names[i] << "'";
						} else {
							cout << "(missing)";
						}
						if (i < nargs) {
							cout << ", ";
						}
					}
					cout << endl << "Variable names follow: ";
					for (i = 0; i < frame->num_vars && *ptr; i++) {
						const std::string name(ptr);
						ptr += name.length() + 1;
						cout << "#" << hex << setw(4) << setfill('0')
							 << (i + nargs) << " = ";
						if (name.length()) {
							cout << "'" << name << "'";
						} else {
							cout << "(missing)";
						}
						if (i < frame->num_vars) {
							cout << ", ";
						}
					}
					for (; i < frame->num_vars; i++) {
						cout << "#" << hex << setw(4) << setfill('0')
							 << (i + nargs) << " = (missing)";
						if (i < frame->num_vars) {
							cout << ", ";
						}
					}
				} else {
					cout << endl;
				}
				break;
			}
			case UC_PUSHSTATIC:    // PUSH static.
				offset = little_endian::Read2s(frame->ip);
				if (offset < 0) {    // Global static.
					if (static_cast<unsigned>(-offset) < statics.size()) {
						push(statics[-offset]);
					} else {
						pushi(0);
					}
				} else {
					if (static_cast<unsigned>(offset)
						< frame->function->statics.size()) {
						push(frame->function->statics[offset]);
					} else {
						pushi(0);
					}
				}
				break;
			case UC_POPSTATIC: {    // POP static.
				offset = little_endian::Read2s(frame->ip);
				// Get value.
				const Usecode_value val = pop();
				if (offset < 0) {
					if (static_cast<unsigned>(-offset) >= statics.size()) {
						statics.resize(-offset + 1);
					}
					statics[-offset] = val;
				} else {
					if (static_cast<unsigned>(offset)
						>= frame->function->statics.size()) {
						frame->function->statics.resize(offset + 1);
					}
					frame->function->statics[offset] = val;
				}
				break;
			}
			case UC_CALLO: {    // CALLO (call original).
				// Otherwise, like CALLE.
				Usecode_value ival   = pop();
				Game_object*  caller = get_item(ival);
				offset               = little_endian::Read2(frame->ip);
				call_function(offset, frame->eventid, caller, false, true);
				frame_changed = true;
				break;
			}
			case UC_CALLIND:          // CALLIND:  call indirect.
			case UC_CALLINDEX_OLD:    // CALLINDEX_OLD:  call indirect with
									  // arguments.
			case UC_CALLINDEX: {    // CALLINDEX: call indirect with arguments.
				//  Function # is on stack.
				const Usecode_value funval = pop();
				const int           offset = funval.get_int_value();
				Usecode_value       ival   = pop();
				Game_object*        caller = get_item(ival);
				int                 numargs;
				if (opcode < UC_EXTOPCODE) {
					numargs = 0;
				} else if (opcode == UC_CALLINDEX_OLD) {
					numargs = popi();
				} else {
					numargs = *(frame->ip)++;
				}
				call_function(
						offset, frame->eventid, caller, false, false, numargs);
				frame_changed = true;
				break;
			}
			case UC_PUSHTHV: {    // PUSH class this->var.
				offset             = little_endian::Read2(frame->ip);
				Usecode_value& ths = frame->get_this();
				push(ths.nth_class_var(offset));
				break;
			}
			case UC_POPTHV: {    // POP class this->var.
				// Get value.
				const Usecode_value val   = pop();
				offset                    = little_endian::Read2(frame->ip);
				Usecode_value& ths        = frame->get_this();
				ths.nth_class_var(offset) = val;
				break;
			}
			case UC_CALLM:       // CALLM - call method, use pushed var vtable.
			case UC_CALLMS: {    // CALLMS - call method, use parameter vtable.
				offset = little_endian::Read2(frame->ip);
				Usecode_class_symbol* c;
				if (opcode == UC_CALLM) {
					const Usecode_value thisptr = peek();
					c                           = thisptr.get_class_ptr();
				} else {
					c = get_class(little_endian::Read2(frame->ip));
				}
				if (!c) {
					THIS_ERROR();
					(void)pop();
					break;
				}
				const int index = c->get_method_id(offset);
				call_function(index, frame->eventid);
				frame_changed = true;
				break;
			}
			case UC_CLSCREATE: {    // CLSCREATE
				const int             cnum = little_endian::Read2(frame->ip);
				Usecode_class_symbol* cls  = symtbl->get_class(cnum);
				if (!cls) {
					cerr << "Can't create obj. for class #" << cnum << endl;
					pushi(0);
					break;
				}
				int           cnt       = cls->get_num_vars();
				Usecode_value new_class = Usecode_value(0);
				new_class.class_new(cls, cnt);

				int to = 0;    // Store at this index.
				// We are trusting UCC output here.
				while (cnt--) {
					const Usecode_value val       = pop();
					new_class.nth_class_var(to++) = val;
				}
				push(new_class);
				break;
			}
			case UC_CLASSDEL: {    // CLASSDEL
				Usecode_value cls = pop();
				cls.class_delete();
				break;
			}
			case UC_PUSHCHOICE:    // PUSHCHOICE
				pushs(user_choice);
				break;
			default:
				cerr << "Opcode " << opcode << " not known. ";
				CERR_CURRENT_IP();
				break;
			}
		}
	}

	if (call_stack.front() == nullptr) {
		// pop the nullptr frame from the stack
		call_stack.pop_front();
	}
	if (aborted) {
		return 0;
	}

	return 1;
}

/*
 *  This is the main entry for outside callers.
 *
 *  Output: -1 if not found.
 *      0 if can't execute now or if aborted.
 *      1 otherwise.
 */

int Usecode_internal::call_usecode(
		int            id,      // Function #.
		Game_object*   item,    // Item ref.
		Usecode_events event) {
	conv->clear_answers();

	int ret;
	if (call_function(id, event, item, true)) {
		ret = run();
	} else {
		ret = -1;    // failed to call the function
	}

	set_book(nullptr);

	// Left hanging (BG)?
	if (conv->get_num_faces_on_screen() > 0) {
		conv->init_faces();       // Remove them.
		gwin->set_all_dirty();    // Force repaint.
	}
	if (modified_map) {
		// On a barge, and we changed the map.
		Barge_object* barge = gwin->get_moving_barge();
		if (barge) {
			barge->set_to_gather();    // Refigure what's on barge.
		}
		modified_map = false;
	}
	return ret;
}

/*
 *  Call a 'method'.
 *  Output: Same as input, unless it's 'new', in which we return the new
 *      instance.
 */

bool Usecode_internal::call_method(
		Usecode_value* inst,    // Instance, or nullptr.
		int            id,      // Function # or -1 for free inst.
		Game_object*   item     // Item ref.
) {
	if (id == -1) {
		// Only delete the class for now
		inst->class_delete();
		return false;
	}
	Usecode_function* fun = find_function(id);
	if (!fun) {
		return false;
	}

	auto* frame
			= new Stack_frame(fun, 0, item, Stack_frame::getCallChainID(), 0);

	int oldstack = 0;
	while (frame->num_args > oldstack) {    // Not enough args pushed?
		pushi(0);                           // add zeroes
		oldstack++;
	}

	// Store args in first num_args locals
	int i;
	for (i = 0; i < frame->num_args; i++) {
		const Usecode_value val                = pop();
		frame->locals[frame->num_args - i - 1] = val;
	}

	// save stack pointer
	frame->save_sp = sp;

	// add new stack frame to top of stack
	call_stack.push_front(frame);

#ifdef DEBUG
	Usecode_class_symbol* cls  = inst->get_class_ptr();
	Usecode_symbol*       fsym = cls ? (*cls)[id] : nullptr;
	cout << "Running usecode " << setw(4);
	if (cls) {
		cout << cls->get_name();
	} else {    // Shouldn't happen.
		cout << "Unknown class";
	}
	cout << "::";
	if (fsym) {
		cout << fsym->get_name();
	} else {
		cout << hex << setfill('0') << id << dec << setfill(' ');
	}
	cout << " (";
	for (i = 0; i < frame->num_args; i++) {
		if (i) {
			cout << ", ";
		}
		frame->locals[i].print(cout);
	}
	cout << endl;
#endif

	return true;
}

/*
 *  Lookup function name in symbol table.  Prints error if not found.
 */

int Usecode_internal::find_function(const char* nm, bool noerr) {
	Usecode_symbol* ucsym = symtbl ? (*symtbl)[nm] : nullptr;
	if (!ucsym) {
		if (!noerr) {
			cerr << "Failed to find Usecode symbol '" << nm << "'." << endl;
		}
		return -1;
	}
	return ucsym->get_val();
}

/*
 *  Lookup function id in symbol table.
 */

const char* Usecode_internal::find_function_name(int funcid) {
	Usecode_symbol* ucsym = symtbl ? (*symtbl)[funcid] : nullptr;
	if (!ucsym) {
		return nullptr;
	}
	return ucsym->get_name();
}

/*
 *  Start speech, or show text if speech isn't enabled.
 */

void Usecode_internal::do_speech(int num) {
	speech_track = num;    // Used in Usecode function.
	if (!Audio::get_ptr()->start_speech(num)
		|| Audio::get_ptr()->is_speech_with_subs()) {
		// No speech?  Call text function.
		call_usecode(SpeechUsecode, nullptr, double_click);
	}
}

/*
 *  Are we in a usecode function for a given item and event?
 */

bool Usecode_internal::in_usecode_for(Game_object* item, Usecode_events event) {
	for (auto* frame : call_stack) {
		if (frame->eventid == event && frame->caller_item.get() == item) {
			return true;
		}
	}
	return false;
}

/*
 *  Write out global data to 'gamedat/usecode.dat'.
 *  (and 'gamedat/keyring.dat')
 *
 *  Output: 0 if error.
 */

void Usecode_internal::write() {
	// Assume new games will have keyring.
	if (Game::get_game_type() != BLACK_GATE && !Game::is_si_beta()) {
		keyring->write();    // write keyring data
	}

	{
		OFileDataSource out(FLAGINIT);
		out.write(gflags, sizeof(gflags));
	}
	{
		OFileDataSource out(USEDAT);
		out.write2(partyman->get_count());    // Write party.
		for (int i = 0; i < EXULT_PARTY_MAX; i++) {
			out.write2(partyman->get_member(i));
		}
		// Timers.
		out.write4(0xffffffffU);
		for (auto& timer : timers) {
			if (!timer.second) {    // Don't write unused timers.
				continue;
			}
			out.write2(timer.first);
			out.write4(timer.second);
		}
		out.write2(0xffff);
		out.write2(saved_pos.tx);    // Write saved pos.
		out.write2(saved_pos.ty);
		out.write2(saved_pos.tz);
		out.write2(saved_map);    // Write saved map.
	}
	// Static variables. 1st, globals.
	OFileDataSource nfile(USEVARS);
	nfile.write4(statics.size());    // # globals.
	for (auto& it : statics) {
		if (!it.save(&nfile)) {
			throw file_exception("Could not write static usecode value");
		}
	}
	// Now do the local statics.
	const int num_slots = funs.size();
	for (int i = 0; i < num_slots; i++) {
		const Funs256& slot = funs[i];
		for (auto* fun : slot) {
			if (!fun || fun->statics.empty()) {
				continue;
			}
			Usecode_symbol* fsym = symtbl ? (*symtbl)[fun->id] : nullptr;
			if (fsym) {
				const char* nm = fsym->get_name();
				nfile.write4(0xfffffffeU);
				nfile.write2(static_cast<uint16>(strlen(nm)));
				nfile.write(nm, strlen(nm));
			} else {
				nfile.write4(fun->id);
			}
			nfile.write4(fun->statics.size());
			for (auto& it : fun->statics) {
				if (!it.save(&nfile)) {
					throw file_exception(
							"Could not write static usecode value");
				}
			}
		}
	}
	nfile.write4(0xffffffffU);    // End with -1.
}

/*
 *  Read in global data from 'gamedat/usecode.dat'.
 *  (and 'gamedat/keyring.dat')
 *
 *  Output: 0 if error.
 */

void Usecode_internal::read() {
	if (Game::get_game_type() != BLACK_GATE && !Game::is_si_beta()) {
		keyring->read();    // read keyring data
	}

	try {
		auto pIn = U7open_in(FLAGINIT);    // Read global flags.
		if (!pIn) {
			throw file_read_exception(FLAGINIT);
		}
		auto& in = *pIn;
		in.seekg(0, ios::end);    // Get filesize.
		size_t filesize = in.tellg();
		in.seekg(0, ios::beg);
		if (filesize > sizeof(gflags)) {
			filesize = sizeof(gflags);
		}
		memset(&gflags[0], 0, sizeof(gflags));
		in.read(reinterpret_cast<char*>(gflags), filesize);
	} catch (const exult_exception& /*e*/) {
		if (!Game::is_editing()) {
			throw;
		}
		memset(&gflags[0], 0, sizeof(gflags));
	}

	clear_usevars();    // first clear all statics
	read_usevars();
	std::unique_ptr<std::istream> pIn;
	try {
		pIn = U7open_in(USEDAT);
	} catch (exult_exception& /*e*/) {
		partyman->set_count(0);
		partyman->link_party();    // Still need to do this.
		return;                    // Not an error if no saved game yet.
	}
	if (!pIn) {
		throw file_read_exception(USEDAT);
	}
	auto& in = *pIn;
	partyman->set_count(little_endian::Read2(in));    // Read party.
	size_t i;                                         // Blame MSVC
	for (i = 0; i < EXULT_PARTY_MAX; i++) {
		partyman->set_member(i, little_endian::Read2(in));
	}
	partyman->link_party();
	// Timers.
	const int cnt = little_endian::Read4(in);
	if (cnt == -1) {
		int tmr = 0;
		while ((tmr = little_endian::Read2(in)) != 0xffff) {
			timers[tmr] = little_endian::Read4(in);
		}
	} else {
		timers[0] = cnt;
		for (size_t t = 1; t < 20; t++) {
			timers[t] = little_endian::Read4(in);
		}
	}
	if (!in.good()) {
		throw file_read_exception(USEDAT);
	}
	saved_pos.tx = little_endian::Read2(in);    // Read in saved position.
	saved_pos.ty = little_endian::Read2(in);
	saved_pos.tz = little_endian::Read2(in);
	if (!in.good() ||    // Failed.+++++Can remove this later.
		saved_pos.tz < 0 || saved_pos.tz > 13) {
		saved_pos = Tile_coord(-1, -1, -1);
	}
	saved_map = little_endian::Read2(in);
	if (!in.good()) {    // For compat. with older saves.
		saved_map = -1;
	}
}

/*
 *  Read in static variables from USEVARS.
 */

void Usecode_internal::read_usevars() {
	IFileDataSource nfile(USEVARS);
	if (!nfile.good()) {
		// Okay if this doesn't exist.
		return;
	}
	const int cnt = nfile.read4();    // Global statics.
	statics.resize(cnt);
	int i;
	for (i = 0; i < cnt; i++) {
		statics[i].restore(&nfile);
	}
	unsigned long funid;
	while (!nfile.eof() && (funid = nfile.read4()) != 0xffffffffU) {
		if (funid == 0xfffffffeU) {
			// ++++ FIXME: Write code for the cases when symtbl == 0 or
			// fsym == 0 (neither of which *should* happen...)
			const int len = nfile.read2();
			char*     nm  = new char[len + 1];
			nfile.read(nm, len);
			nm[len]              = 0;
			Usecode_symbol* fsym = symtbl ? (*symtbl)[nm] : nullptr;
			if (fsym) {
				funid = fsym->get_val();
			}
			delete[] nm;
		}
		const int         cnt = nfile.read4();
		Usecode_function* fun = find_function(funid);
		if (!fun) {
			continue;
		}
		fun->statics.resize(cnt);
		for (i = 0; i < cnt; i++) {
			fun->statics[i].restore(&nfile);
		}
	}
}

void Usecode_internal::clear_usevars() {
	statics.clear();
	const int nslots = funs.size();
	for (int i = 0; i < nslots; ++i) {
		const vector<Usecode_function*>& slot = funs[i];
		for (auto* fun : slot) {
			if (fun) {
				fun->statics.clear();
			}
		}
	}
}

#ifdef USECODE_DEBUGGER

int Usecode_internal::get_callstack_size() const {
	return call_stack.size();
}

Stack_frame* Usecode_internal::get_stackframe(int i) {
	if (i >= 0 && static_cast<unsigned>(i) < call_stack.size()) {
		return call_stack[i];
	} else {
		return nullptr;
	}
}

// return current size of the stack
int Usecode_internal::get_stack_size() const {
	return static_cast<int>(sp - stack);
}

// get an(y) element from the stack. (depth == 0 is top element)
Usecode_value* Usecode_internal::peek_stack(int depth) const {
	if (depth < 0 || depth >= get_stack_size()) {
		return nullptr;
	}

	return sp - depth - 1;
}

// modify an(y) element on the stack. (depth == 0 is top element)
void Usecode_internal::poke_stack(int depth, Usecode_value& val) {
	if (depth < 0 || (sp - depth) < stack) {
		return;
	}

	*(sp - depth) = val;
}

void Usecode_internal::set_breakpoint() {
	breakpoints.add(new AnywhereBreakpoint());
}

void Usecode_internal::dbg_stepover() {
	if (on_breakpoint) {
		breakpoints.add(new StepoverBreakpoint(call_stack.front()));
	}
}

void Usecode_internal::dbg_finish() {
	if (on_breakpoint) {
		breakpoints.add(new FinishBreakpoint(call_stack.front()));
	}
}

int Usecode_internal::set_location_breakpoint(int funcid, int ip) {
	Breakpoint* bp = new LocationBreakpoint(funcid, ip);
	breakpoints.add(bp);

	return bp->id;
}

#endif
