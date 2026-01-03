/*
 *  contain.cc - Container objects.
 *
 *  Copyright (C) 1998-1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2025  The Exult Team
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "contain.h"

#include "Gump_manager.h"
#include "ShortcutBar_gump.h"
#include "cheat.h"
#include "databuf.h"
#include "endianio.h"
#include "exult.h"
#include "frflags.h"
#include "game.h"
#include "gamemap.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "keyring.h"
#include "objiter.h"
#include "ready.h"
#include "ucmachine.h"
#include "ucsched.h"
#include "weaponinf.h"

#ifdef USE_EXULTSTUDIO
#	include "objserial.h"
#	include "servemsg.h"
#	include "server.h"
#endif

using std::cout;
using std::endl;
using std::ostream;

/*
 *  Determines if a shape can be added/found inside a container.
 */
static inline bool Can_be_added(
		const Container_game_object* cont, int shapenum,
		bool allow_locked = false) {
	const Shape_info& continfo = cont->get_info();
	const Shape_info& add_info = ShapeID::get_info(shapenum);
	return cont->get_shapenum() != shapenum    // Shape can't be inside itself.
		   && (allow_locked
			   || !continfo.is_container_locked())    // Locked container.
		   && continfo.is_shape_accepted(shapenum)    // Shape can't be inside.
		   && (add_info.is_spell()
			   || !add_info.is_on_fire());    // Object is on fire, and can't be
											  // inside others
}

// Max. we'll hold.  (Guessing).
int Container_game_object::get_max_volume() const {
	return get_info().has_extradimensional_storage() ? 0 : get_volume();
}

/*
 *  Remove an object.  The object's (cx, cy) fields are set to invalid
 *  #'s (255, 255).
 */

void Container_game_object::remove(Game_object* obj) {
	if (objects.is_empty()) {
		return;
	}
	const int shapenum = obj->get_shapenum();
	volume_used -= obj->get_volume();
	obj->set_owner(nullptr);
	obj->set_invalid();    // No longer part of world.
	objects.remove(obj->shared_from_this());
	if (g_shortcutBar) {
		g_shortcutBar->check_for_updates(shapenum);
	}
}

/*
 *  Add an object.
 *
 *  Output: 1, meaning object is completely contained in this.  Obj may
 *          be deleted in this case if combine==true.
 *      0 if not enough space, although obj's quantity may be
 *          reduced if combine==true.
 */

bool Container_game_object::add(
		Game_object* obj,
		bool         dont_check,    // True to skip volume/recursion check.
		bool         combine,       // True to try to combine obj.  MAY
		//   cause obj to be deleted.
		bool noset    // True to prevent actors from setting sched. weapon.
) {
	ignore_unused_variable_warning(noset);
	// Prevent dragging the avatar into a container.
	// Casting to void* to avoid including actors.h.
	if (obj == static_cast<void*>(gwin->get_main_actor())) {
		return false;
	}
	const Shape_info& info = get_info();
	if (!dont_check
		&& (get_shapenum() == obj->get_shapenum()    // Shape can't be inside
													 // itself (not sure
													 // originals check this).
			|| info.is_container_locked())) {        // Locked container.
		return false;
	}
	if (!info.is_shape_accepted(
				obj->get_shapenum())) {    // Shape can't be inside.
		return false;
	}
	const Shape_info& add_info = obj->get_info();
	if (!dont_check && !cheat.in_map_editor() && !cheat.in_hack_mover()
		&& (!add_info.is_spell() && add_info.is_on_fire())) {
		return false;
	}

	// Always check this. ALWAYS!
	Game_object* parent = this;
	do {    // Watch for snake eating itself.
		if (obj == parent) {
			return false;
		}
	} while ((parent = parent->get_owner()) != nullptr);

	if (combine) {    // Should we try to combine?
		const Shape_info& info  = obj->get_info();
		const int         quant = obj->get_quantity();
		// Combine, but don't add.
		const int newquant = add_quantity(
				quant, obj->get_shapenum(),
				info.has_quality() ? obj->get_quality() : c_any_qual,
				obj->get_framenum(), true);
		if (newquant == 0) {    // All added?
			const int shape_num = obj->get_shapenum();
			obj->remove_this();
			if (g_shortcutBar) {
				g_shortcutBar->check_for_updates(shape_num);
			}
			return true;
		} else if (newquant < quant) {    // Partly successful.
			obj->modify_quantity(newquant - quant);
		}
	}
	const int objvol = obj->get_volume();
	if (!cheat.in_hack_mover() && !dont_check) {
		const int maxvol = get_max_volume();
		// maxvol = 0 means infinite (ship's hold, hollow tree, etc...)
		if (maxvol > 0 && objvol + volume_used > maxvol) {
			return false;    // Doesn't fit.
		}
	}
	volume_used += objvol;
	obj->set_owner(this);                       // Set us as the owner.
	objects.append(obj->shared_from_this());    // Append to chain.
	// Guessing:
	if (get_flag(Obj_flags::okay_to_take)) {
		obj->set_flag(Obj_flags::okay_to_take);
	}
	if (g_shortcutBar) {
		g_shortcutBar->check_for_updates(obj->get_shapenum());
	}
	return true;
}

/*
 *  Change shape of a member.
 */

void Container_game_object::change_member_shape(
		Game_object* obj, int newshape) {
	const int oldvol = obj->get_volume();
	obj->set_shape(newshape);
	// Update total volume.
	volume_used += obj->get_volume() - oldvol;
}

/*
 *  Add a key (by quality) to the SI keyring.
 *
 *  Output: 1 if successful, else 0 (and this does need to be an int).
 */

static int Add2keyring(int qual, int framenum) {
	if (framenum >= 21 && framenum <= 23) {
		return 0;    // Fire, ice, blackrock in SI.
	}
	Keyring* ring = Game_window::get_instance()->get_usecode()->getKeyring();
	// Valid quality & not already there?
	if (qual != c_any_qual && !ring->checkkey(qual)) {
		ring->addkey(qual);
		return 1;
	}
	return 0;
}

/*
 *  Recursively add a quantity of an item to those existing in
 *  this container, and create new objects if necessary.
 *
 *  Output: Delta decremented # added.
 */

int Container_game_object::add_quantity(
		int  delta,         // Quantity to add.
		int  shapenum,      // Shape #.
		int  qual,          // Quality, or c_any_qual for any.
		int  framenum,      // Frame, or c_any_framenum for any.
		bool dontcreate,    // If true, don't create new objs.
		bool temporary      // If objects should be temporary
) {
	if (delta <= 0 || !Can_be_added(this, shapenum)) {
		return delta;
	}

	int cant_add  = 0;                   // # we can't add due to weight.
	int maxweight = get_max_weight();    // Check weight.
	if (maxweight) {
		maxweight *= 10;    // Work in .1 stones.
		const int avail     = maxweight - get_outermost()->get_weight();
		const int objweight = Ireg_game_object::get_weight(shapenum, delta);
		if (objweight && objweight > avail) {
			// Limit what we can add.
			// Work in 1/100ths.
			const int weight1 = (10 * objweight) / delta;
			cant_add          = delta - (10 * avail) / (weight1 ? weight1 : 1);
			if (cant_add >= delta) {
				return delta;    // Can't add any.
			}
			delta -= cant_add;
		}
	}
	const Shape_info& info  = ShapeID::get_info(shapenum);
	const bool has_quantity = info.has_quantity();    // Quantity-type shape?
	const bool has_quantity_frame
			= has_quantity ? info.has_quantity_frames() : false;
	// Note:  quantity is ignored for
	//   figuring volume.
	Game_object* obj;
	if (!objects.is_empty()) {
		// First try existing items.
		Object_iterator next(objects);
		while (delta && (obj = next.get_next()) != nullptr) {
			if (has_quantity && obj->get_shapenum() == shapenum
				&& (framenum == c_any_framenum || has_quantity_frame
					|| obj->get_framenum() == framenum)) {
				delta = obj->modify_quantity(delta);
			}
			// Adding key to SI keyring?
			else if (
					GAME_SI && shapenum == 641 && obj->get_shapenum() == 485
					&& delta == 1) {
				delta -= Add2keyring(qual, framenum);
			}
		}
		next.reset();    // Now try recursively.
		while ((obj = next.get_next()) != nullptr) {
			delta = obj->add_quantity(
					delta, shapenum, qual, framenum, true, temporary);
		}
	}
	if (!delta || dontcreate) {    // All added?
		return delta + cant_add;
	} else {
		return cant_add
			   + create_quantity(
					   delta, shapenum, qual,
					   framenum == c_any_framenum ? 0 : framenum, temporary);
	}
}

/*
 *  Recursively create a quantity of an item.  Assumes weight check has
 *  already been done.
 *
 *  Output: Delta decremented # added.
 */

int Container_game_object::create_quantity(
		int  delta,       // Quantity to add.
		int  shnum,       // Shape #.
		int  qual,        // Quality, or c_any_qual for any.
		int  frnum,       // Frame.
		bool temporary    // Create temporary quantity
) {
	if (!Can_be_added(this, shnum)) {
		return delta;
	}
	// Usecode container?
	const Shape_info& info = ShapeID::get_info(get_shapenum());
	if (info.get_ready_type() == ucont) {
		return delta;
	}
	const Shape_info& shp_info = ShapeID::get_info(shnum);
	if (!shp_info.has_quality()) {    // Not a quality object?
		qual = c_any_qual;            // Then don't set it.
	}
	while (delta) {    // Create them here first.
		Game_object_shared newobj
				= gmap->create_ireg_object(shp_info, shnum, frnum, 0, 0, 0);
		if (!add(newobj.get())) {
			newobj.reset();
			break;
		}

		// Set temporary
		if (temporary) {
			newobj->set_flag(Obj_flags::is_temporary);
		}

		if (qual != c_any_qual) {    // Set desired quality.
			newobj->set_quality(qual);
		}
		delta--;
		if (delta > 0) {
			delta = newobj->modify_quantity(delta);
		}
	}
	if (!delta) {    // All done?
		return 0;
	}
	// Now try those below.
	Game_object* obj;
	if (objects.is_empty()) {
		return delta;
	}
	Object_iterator next(objects);
	while ((obj = next.get_next()) != nullptr) {
		delta = obj->create_quantity(delta, shnum, qual, frnum);
	}
	return delta;
}

/*
 *  Recursively remove a quantity of an item from those existing in
 *  this container.
 *
 *  Output: Delta decremented by # removed.
 */

int Container_game_object::remove_quantity(
		int delta,       // Quantity to remove.
		int shapenum,    // Shape #.
		int qual,        // Quality, or c_any_qual for any.
		int framenum     // Frame, or c_any_framenum for any.
) {
	if (objects.is_empty() || !Can_be_added(this, shapenum)) {
		return delta;    // Empty.
	}
	Game_object* obj  = objects.get_first();
	Game_object* last = obj->get_prev();    // Save last.
	Game_object* next;
	while (obj && delta) {
		// Might be deleting obj.
		next     = obj == last ? nullptr : obj->get_next();
		bool del = false;    // Gets 'deleted' flag.
		if (obj->get_shapenum() == shapenum
			&& (qual == c_any_qual || obj->get_quality() == qual)
			&& (framenum == c_any_framenum
				|| (obj->get_framenum() & 31) == framenum)) {
			delta = -obj->modify_quantity(-delta, &del);
		}

		if (!del) {    // Still there?
			// Do it recursively.
			delta = obj->remove_quantity(delta, shapenum, qual, framenum);
		}
		obj = next;
	}
	return delta;
}

/*
 *  Find and return a desired item.
 *
 *  Output: ->object if found, else nullptr.
 */

Game_object* Container_game_object::find_item(
		int shapenum,    // Shape #.
		int qual,        // Quality, or c_any_qual for any.
		int framenum     // Frame, or c_any_framenum for any.
) {
	if (objects.is_empty() || !Can_be_added(this, shapenum, true)) {
		return nullptr;    // Empty.
	}
	Game_object*    obj;
	Object_iterator next(objects);
	while ((obj = next.get_next()) != nullptr) {
		if (obj->get_shapenum() == shapenum
			&& (framenum == c_any_framenum
				|| (obj->get_framenum() & 31) == framenum)
			&& (qual == c_any_qual || obj->get_quality() == qual)) {
			return obj;
		}

		// Do it recursively.
		Game_object* found = obj->find_item(shapenum, qual, framenum);
		if (found) {
			return found;
		}
	}
	return nullptr;
}

/*
 *  Displays the object's gump.
 *  Returns true if the gump has been handled.
 */

bool Container_game_object::show_gump(int event) {
	ignore_unused_variable_warning(event);
	const Shape_info& inf = get_info();
	int               gump;
	if (cheat.in_map_editor()) {
		return true;    // Do nothing.
	} else if (inf.has_object_flag(
					   get_framenum(), inf.has_quality() ? get_quality() : -1,
					   Frame_flags::fp_force_usecode)) {
		// Run normal usecode fun.
		return false;
	} else if ((gump = inf.get_gump_shape()) >= 0) {
		Gump_manager* gump_man = gumpman;
		gump_man->add_gump(this, gump);
		return true;
	} else if (inf.is_container_locked() && cheat.in_pickpocket()) {
		// Container is locked, showing first gump.
		Gump_manager* gump_man = gumpman;
		gump_man->add_gump(this, 1);
		return true;
	}
	return false;
}

/*
 *  Run usecode when double-clicked.
 */

void Container_game_object::activate(int event) {
	// If event==2, open basic properties instead of container editor
	if (event == 2 && edit_basic_properties()) {
		return;
	}
	if (edit()) {
		return;    // Map-editing.
	}
	if (!show_gump(event)) {
		// Try to run normal usecode fun.
		ucmachine->call_usecode(
				get_usecode(), this,
				static_cast<Usecode_machine::Usecode_events>(event));
	}
}

/*
 *  Edit in ExultStudio.
 */

bool Container_game_object::edit() {
#ifdef USE_EXULTSTUDIO
	if (client_socket >= 0 &&    // Talking to ExultStudio?
		cheat.in_map_editor()) {
		editing.reset();
		const Tile_coord  t    = get_tile();
		const std::string name = get_name();
		if (Container_out(
					client_socket, this, t.tx, t.ty, t.tz, get_shapenum(),
					get_framenum(), get_quality(), name, get_obj_hp(),
					get_flag(Obj_flags::invisible),
					get_flag(Obj_flags::okay_to_take))
			!= -1) {
			cout << "Sent object data to ExultStudio" << endl;
			editing = shared_from_this();
		} else {
			cout << "Error sending object to ExultStudio" << endl;
		}
		return true;
	}
#endif
	return false;
}

bool Container_game_object::edit_basic_properties() {
	// Call the base Game_object::edit() to open basic properties
	// instead of the Container-specific editor
	return Game_object::edit();
}

/*
 *  Message to update from ExultStudio.
 */

void Container_game_object::update_from_studio(
		unsigned char* data, int datalen) {
#ifdef USE_EXULTSTUDIO
	Container_game_object* obj;
	int                    tx;
	int                    ty;
	int                    tz;
	int                    shape;
	int                    frame;
	int                    quality;
	unsigned char          res;
	bool                   invis;
	bool                   can_take;
	std::string            name;
	if (!Container_in(
				data, datalen, obj, tx, ty, tz, shape, frame, quality, name,
				res, invis, can_take)) {
		cout << "Error decoding object" << endl;
		return;
	}
	if (!editing || obj != editing.get()) {
		cout << "Obj from ExultStudio is not being edited" << endl;
		return;
	}
	// Keeps NPC alive until end of function
	// Game_object_shared keep = std::move(editing); // He may have chosen
	// 'Apply', so still editing.
	if (invis) {
		obj->set_flag(Obj_flags::invisible);
	} else {
		obj->clear_flag(Obj_flags::invisible);
	}
	if (can_take) {
		obj->set_flag(Obj_flags::okay_to_take);
	} else {
		obj->clear_flag(Obj_flags::okay_to_take);
	}
	gwin->add_dirty(obj);
	obj->set_shape(shape, frame);
	gwin->add_dirty(obj);
	obj->set_quality(quality);
	obj->set_obj_hp(res);
	Container_game_object* owner = obj->get_owner();
	if (!owner) {
		// See if it moved -- but only if not inside something!
		const Tile_coord oldt = obj->get_tile();
		if (oldt.tx != tx || oldt.ty != ty || oldt.tz != tz) {
			obj->move(tx, ty, tz);
		}
	}
	cout << "Object updated" << endl;
#else
	ignore_unused_variable_warning(data, datalen);
#endif
}

/*
 *  Get (total) weight.
 */

int Container_game_object::get_weight() {
	int               wt   = Ireg_game_object::get_weight();
	const Shape_info& info = get_info();
	if (info.has_extradimensional_storage()) {
		return wt;
	}
	Game_object*    obj;
	Object_iterator next(objects);
	while ((obj = next.get_next()) != nullptr) {
		wt += obj->get_weight();
	}
	return wt;
}

/*
 *  Drop another onto this.
 *
 *  Output: false to reject, true to accept.
 */

bool Container_game_object::drop(
		Game_object* obj    // May be deleted if combined.
) {
	if (!get_owner()) {    // Only accept if inside another.
		return false;
	}
	return add(obj, false, true);    // We'll take it, and try to combine.
}

/*
 *  Recursively count all objects of a given shape.
 */

int Container_game_object::count_objects(
		int shapenum,    // Shape#, or c_any_shapenum for any.
		int qual,        // Quality, or c_any_qual for any.
		int framenum     // Frame#, or c_any_framenum for any.
) {
	if (!Can_be_added(this, shapenum, true)) {
		return 0;
	}
	int             total = 0;
	Game_object*    obj;
	Object_iterator next(objects);
	while ((obj = next.get_next()) != nullptr) {
		if ((shapenum == c_any_shapenum || obj->get_shapenum() == shapenum) &&
			// Watch for reflection.
			(framenum == c_any_framenum
			 || (obj->get_framenum() & 31) == framenum)
			&& (qual == c_any_qual || obj->get_quality() == qual)) {
			// Check quantity.
			const int quant = obj->get_quantity();
			total += quant;
		}
		// Count recursively.
		total += obj->count_objects(shapenum, qual, framenum);
	}
	return total;
}

/*
 *  Recursively get all objects of a given shape.
 */

int Container_game_object::get_objects(
		Game_object_vector& vec,         // Objects returned here.
		int                 shapenum,    // Shape#, or c_any_shapenum for any.
		int                 qual,        // Quality, or c_any_qual for any.
		int                 framenum     // Frame#, or c_any_framenum for any.
) {
	const int       vecsize = vec.size();
	Game_object*    obj;
	Object_iterator next(objects);
	while ((obj = next.get_next()) != nullptr) {
		if ((shapenum == c_any_shapenum || obj->get_shapenum() == shapenum)
			&& (qual == c_any_qual || obj->get_quality() == qual) &&
			// Watch for reflection.
			(framenum == c_any_framenum
			 || (obj->get_framenum() & 31) == framenum)) {
			vec.push_back(obj);
		}
		// Search recursively.
		obj->get_objects(vec, shapenum, qual, framenum);
	}
	return vec.size() - vecsize;
}

/*
 *  Set a flag on this and all contents.
 */

void Container_game_object::set_flag_recursively(int flag) {
	set_flag(flag);
	Game_object*    obj;
	Object_iterator next(objects);
	while ((obj = next.get_next()) != nullptr) {
		obj->set_flag_recursively(flag);
	}
}

/*
 *  Write out container and its members.
 */

void Container_game_object::write_ireg(ODataSource* out) {
	unsigned char        buf[20];    // 12-byte entry.
	uint8*               ptr   = write_common_ireg(12, buf);
	Game_object*         first = objects.get_first();    // Guessing: +++++
	const unsigned short tword = first ? first->get_prev()->get_shapenum() : 0;
	little_endian::Write2(ptr, tword);
	Write1(ptr, 0);    // Unknown.
	Write1(ptr, get_quality());
	Write1(ptr, 0);                                         // "Quantity".
	Write1(ptr, nibble_swap(get_lift()));                   // Lift
	Write1(ptr, static_cast<unsigned char>(resistance));    // Resistance.
	// Flags:  B0=invis. B3=okay_to_take.
	Write1(ptr, (get_flag(Obj_flags::invisible) ? 1 : 0)
						+ (get_flag(Obj_flags::okay_to_take) ? (1 << 3) : 0));
	out->write(reinterpret_cast<char*>(buf), ptr - buf);
	write_contents(out);    // Write what's contained within.
	// Write scheduled usecode.
	Game_map::write_scheduled(out, this);
}

// Get size of IREG. Returns -1 if can't write to buffer
int Container_game_object::get_ireg_size() {
	// These shouldn't ever happen, but you never know
	if (gumpman->find_gump(this) || Usecode_script::find(this)) {
		return -1;
	}

	int total_size = 8 + get_common_ireg_size();

	// Now what's inside.
	if (!objects.is_empty()) {
		Game_object*    obj;
		Object_iterator next(objects);
		while ((obj = next.get_next()) != nullptr) {
			const int size = obj->get_ireg_size();

			if (size < 0) {
				return -1;
			}

			total_size += size;
		}
		total_size += 1;
	}

	return total_size;
}

/*
 *  Write contents (if there is any).
 */

void Container_game_object::write_contents(ODataSource* out) {
	if (!objects.is_empty()) {    // Now write out what's inside.
		Game_object*    obj;
		Object_iterator next(objects);
		while ((obj = next.get_next()) != nullptr) {
			obj->write_ireg(out);
		}
		out->write1(0x01);    // A 01 terminates the list.
	}
}

bool Container_game_object::extract_contents(Container_game_object* targ) {
	if (objects.is_empty()) {
		return true;
	}

	bool status = true;

	Game_object* obj;

	while ((obj = objects.get_first())) {
		const Game_object_shared keep = obj->shared_from_this();
		remove(obj);

		if (targ) {
			targ->add(obj, true);    // add without checking volume
		} else {
			obj->set_invalid();    // set to invalid chunk so move() doesn't
								   // fail
			if ((get_cx() == 255) && (get_cy() == 255)) {
				obj->remove_this(nullptr);
				status = false;
			} else {
				obj->move(get_tile());
			}
		}
	}

	return status;
}

void Container_game_object::delete_contents() {
	if (objects.is_empty()) {
		return;
	}

	Game_object* obj;
	while ((obj = objects.get_first())) {
		const Game_object_shared keep = obj->shared_from_this();
		remove(obj);
		obj->delete_contents();    // recurse into contained containers
		obj->remove_this(nullptr);
	}
}

void Container_game_object::remove_this(
		Game_object_shared* keep    // Non-null to not delete.
) {
	// Needs to be saved, as it is invalidated below but needed
	// shortly after.
	Game_object_shared     tmp_keep;
	Container_game_object* safe_owner = Container_game_object::get_owner();
	// Special case to avoid recursion.
	if (safe_owner) {
		// First remove from owner.
		Ireg_game_object::remove_this(&tmp_keep);
		if (keep) {    // Not deleting?  Then done.
			*keep = std::move(tmp_keep);
			return;
		}
	}
	if (!keep) {
		extract_contents(safe_owner);
	}

	Ireg_game_object::remove_this(keep);
}

/*
 *  Find ammo used by weapon.
 *
 *  Output: ->object if found. Additionally, is_readied is set to
 *  true if the ammo is readied.
 */

Game_object* Container_game_object::find_weapon_ammo(
		int weapon,    // Weapon shape.
		int needed, bool recursive) {
	ignore_unused_variable_warning(recursive);
	if (weapon < 0 || !Can_be_added(this, weapon)) {
		return nullptr;
	}
	const Weapon_info* winf = ShapeID::get_info(weapon).get_weapon_info();
	if (!winf) {
		return nullptr;
	}
	const int family = winf->get_ammo_consumed();
	if (family >= 0) {
		return nullptr;
	}

	Game_object_vector vec;    // Get list of all possessions.
	vec.reserve(50);
	get_objects(vec, c_any_shapenum, c_any_qual, c_any_framenum);
	for (auto* obj : vec) {
		if (obj->get_shapenum() != weapon) {
			continue;
		}
		const Shape_info& inf = obj->get_info();
		if (family == -2) {
			if (!inf.has_quality() || obj->get_quality() >= needed) {
				return obj;
			}
		}
		// Family -1 and family -3.
		else if (obj->get_quantity() >= needed) {
			return obj;
		}
	}
	return nullptr;
}
