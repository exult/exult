/*
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "cheat.h"

#include "Configuration.h"
#include "Gump.h"
#include "Gump_manager.h"
#include "actors.h"
#include "browser.h"
#include "cheat_screen.h"
#include "chunks.h"
#include "drag.h"
#include "effects.h"
#include "egg.h"
#include "exult.h"
#include "game.h"
#include "gameclk.h"
#include "gamemap.h"
#include "gamewin.h"
#include "istring.h"
#include "miscinf.h"
#include "mouse.h"
#include "objiter.h"
#include "party.h"
#include "soundtest.h"
#include "touchui.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#ifdef USE_EXULTSTUDIO /* Only needed for exult studio. */
#	include "ignore_unused_variable_warning.h"
#	include "servemsg.h"
#	include "server.h"

#	ifdef _WIN32
#		ifndef WIN32_LEAN_AND_MEAN
#			define WIN32_LEAN_AND_MEAN
#		endif
#		include <windows.h>
#	endif    // _WIN32

#endif    // USE_EXULTSTUDIO

using std::setfill;
using std::setw;

using std::cout;
using std::endl;

Cheat cheat;

Cheat::Cheat() {
	enabled = false;

	god_mode    = false;
	wizard_mode = false;
	map_editor  = false;
	tile_grid   = false;
	edit_mode   = move;
	edit_lift   = 0;
	edit_shape = edit_frame = edit_chunknum = 0;
	infravision                             = false;
	pickpocket                              = false;
	grab_actor                              = true;
	npc_numbers                             = false;
	hack_mover                              = false;

	browser       = nullptr;
	tester        = nullptr;
	cscreen       = nullptr;
	chunksel_left = chunksel_top = c_num_chunks;
	chunksel_right = chunksel_bottom = -1;
}

Cheat::~Cheat() {
	delete browser;
	delete tester;
	delete cscreen;
}

void Cheat::init() {
	std::string cheating;
	config->value("config/gameplay/cheat", cheating, "no");
	enabled = cheating == "yes";

	std::string feeding;
	config->value("config/gameplay/feeding", feeding, "manual");
	if (!Pentagram::strcasecmp(feeding.c_str(), "disabled")) {
		food_use = FoodUse::Disabled;
	} else if (!Pentagram::strcasecmp(feeding.c_str(), "automatic")) {
		food_use = FoodUse ::Automatic;
	} else {
		food_use = FoodUse::Manual;
	}
}

void Cheat::finish_init() {
	browser = new ShapeBrowser();
	tester  = new SoundTester();
	cscreen = new CheatScreen();

	if (enabled) {
		cout << "Cheats enabled." << endl;
	}
}

void Cheat::set_enabled(bool en) {
	enabled = en;
	std::string cheating;
	if (enabled) {
		cheating = "yes";
	} else {
		cheating = "no";
	}
	config->set("config/gameplay/cheat", cheating, true);
}

void Cheat::toggle_god() {
	if (!enabled) {
		return;
	}

	god_mode = !god_mode;
	if (god_mode) {
		eman->center_text("God Mode Enabled");
		Actor*    party[9];    // Set attack mode to 'nearest'.
		const int cnt = gwin->get_party(party, 1);
		for (int i = 0; i < cnt; i++) {
			party[i]->set_attack_mode(Actor::nearest);
		}
	} else {
		eman->center_text("God Mode Disabled");
	}
}

void Cheat::toggle_wizard() {
	if (!enabled) {
		return;
	}

	wizard_mode = !wizard_mode;
	if (wizard_mode) {
		eman->center_text("Archwizard Mode Enabled");
	} else {
		eman->center_text("Archwizard Mode Disabled");
	}
}

void Cheat::toggle_map_editor() {
	if (!enabled) {
		return;
	}

	map_editor = !map_editor;
	if (map_editor) {
		eman->center_text("Map Editor Mode Enabled");
		// Stop time.
		gwin->set_time_stopped(-1);
#ifdef USE_EXULTSTUDIO              /* Launch ExultStudio! */
		if (!gwin->paint_eggs) {    // Show eggs too.
			gwin->paint_eggs = true;
			gwin->paint();
		}
		if (client_socket < 0 && !gwin->get_win()->is_fullscreen()) {
			std::string cmnd("exult_studio -x ");    // Set up command.
#	ifdef _WIN32
			if (get_system_path("<HOME>") == ".") {    // portable
				cmnd += " -p ";
			}
#	endif
			std::string data_path;
#	ifdef MACOSX
			// First try current folder, then /Applications for Exult_Studio app
			// bundle and use its data folder
			std::string app_path("./Exult_Studio.app");
			if (!U7exists(app_path)) {
				app_path = "/Applications/Exult_Studio.app";
			}
			if (U7exists(app_path)) {
				// Convert relative path to absolute path for --args -x
				if (app_path.length() < PATH_MAX) {
					char* resolved_path = realpath(app_path.c_str(), nullptr);
					if (resolved_path != nullptr) {
						app_path = resolved_path;
						free(resolved_path);
					}
				}
				data_path = app_path;
				cmnd      = "open ";
				cmnd += app_path;
				cmnd += " --args -x ";
				data_path += "/Contents/Resources/data/";
				std::cout << "cmnd Path   : " << cmnd << std::endl;
			} else if (is_system_path_defined("<BUNDLE>")) {
				data_path = get_system_path("<BUNDLE>");
			} else
#	endif
				data_path = get_system_path("<DATA>");
			if (data_path.find(' ') != std::string::npos) {
				data_path = "\"" + data_path + "\"";
			}
			cmnd += data_path;    // Path to where .glade file should be.
			cmnd += " -g ";       // Now want game name.
			cmnd += Game::get_gametitle();
			// Now want mod name.
			const std::string modnamestr = Game::get_modtitle();
			if (!modnamestr.empty()) {
				cmnd += " -m ";
				cmnd += modnamestr;
			}
			const std::string alt_cfg = get_system_path("<alt_cfg>");
			if (alt_cfg != "<alt_cfg>") {
				cmnd += " -c ";
				cmnd += alt_cfg;
			}
			cmnd += " &";
			cout << "Executing: " << cmnd << endl;
#	ifndef _WIN32
			const int ret = system(cmnd.c_str());
			if (ret == 127 || ret == -1) {
				cout << "Couldn't run Exult Studio" << endl;
			}
#	else
			PROCESS_INFORMATION pi;
			STARTUPINFO         si;

			std::memset(&si, 0, sizeof(si));
			si.cb = sizeof(si);

			const int ret = CreateProcess(
					nullptr, &cmnd[0], nullptr, nullptr, FALSE, 0, nullptr,
					nullptr, &si, &pi);
			if (!ret) {
				cout << "Couldn't run Exult Studio" << endl;
			}
#	endif
		}
#endif
	} else {
		clear_selected();    // Selection goes away.
		eman->center_text("Map Editor Mode Disabled");
		// Stop time-stop.
		gwin->set_time_stopped(0);
	}
}

void Cheat::toggle_tile_grid() {
	if (!enabled) {
		return;
	}
	tile_grid = !tile_grid;
	gwin->set_all_dirty();
}

void Cheat::set_edit_mode(Map_editor_mode md) {
	edit_mode = md;
	if (edit_mode != select_chunks) {
		clear_chunksel();
		gwin->set_all_dirty();
	}
}

void Cheat::clear_chunksel() {
	if (chunksel_right >= 0 && chunksel_bottom >= 0) {
		const int startx = chunksel_left;
		const int stopx  = chunksel_right + 1;
		const int starty = chunksel_top;
		const int stopy  = chunksel_bottom + 1;
		for (int cy = starty; cy != stopy; cy = INCR_CHUNK(cy)) {
			for (int cx = startx; cx != stopx; cx = INCR_CHUNK(cx)) {
				Map_chunk* chunk = gmap->get_chunk(cx, cy);
				chunk->set_selected(false);
			}
		}
	}
	chunksel_left = chunksel_top = c_num_chunks;
	chunksel_right = chunksel_bottom = -1;
}

void Cheat::add_chunksel(Map_chunk* chunk, bool extend) {
	ignore_unused_variable_warning(extend);
	chunk->set_selected(true);
	const int cx = chunk->get_cx();
	const int cy = chunk->get_cy();
	if (cx < chunksel_left) {
		chunksel_left = cx;
	}
	if (cx > chunksel_right) {
		chunksel_right = cx;
	}
	if (cy < chunksel_top) {
		chunksel_top = cy;
	}
	if (cy > chunksel_bottom) {
		chunksel_bottom = cy;
	}
	// ++++++++LATER:  Handle extend.
}

/*  Move a given chunk. */
void Cheat::move_chunk(Map_chunk* chunk, int dx, int dy) {
	// Figure dest. with wrapping.
	const int  tox     = (chunk->get_cx() + dx + c_num_chunks) % c_num_chunks;
	const int  toy     = (chunk->get_cy() + dy + c_num_chunks) % c_num_chunks;
	Map_chunk* tochunk = gmap->get_chunk(tox, toy);
	Game_object_shared_vector tmplist;    // Delete objs. in 'tochunk'.

	{
		// Iterator needs its own scope.
		Game_object*    obj = nullptr;
		Object_iterator toiter(tochunk->get_objects());
		while ((obj = toiter.get_next()) != nullptr) {
			if (!obj->as_npc()) {
				tmplist.push_back(obj->shared_from_this());
			}
		}
	}
	for (auto& it : tmplist) {
		it->remove_this();
	}
	tmplist.clear();
	// Copy terrain into new chunk.
	tochunk->set_terrain(chunk->get_terrain());
	{
		Object_iterator fromiter(chunk->get_objects());
		Game_object*    obj = nullptr;
		while ((obj = fromiter.get_next()) != nullptr) {
			if (!obj->as_terrain()) {
				tmplist.push_back(obj->shared_from_this());
			}
		}
	}
	dx *= c_tiles_per_chunk;
	dy *= c_tiles_per_chunk;
	for (auto& obj : tmplist) {
		const Tile_coord t = obj->get_tile();
		// Got to move objects legally.
		obj->move(
				(t.tx + dx + c_num_tiles) % c_num_tiles,
				(t.ty + dy + c_num_tiles) % c_num_tiles, t.tz);
	}
	// For now, set terrain to #0.
	chunk->set_terrain(Game_map::get_terrain(0));
	chunk->set_selected(false);
	tochunk->set_selected(true);
}

/*  Move all the selected chunks. */
void Cheat::move_selected_chunks(int dx, int dy) {
	int startx;
	int stopx;
	int dirx;
	int starty;
	int stopy;
	int diry;

	if (dx <= 0) {
		startx = chunksel_left;
		stopx  = chunksel_right + 1;
		dirx   = 1;
	} else {
		startx = chunksel_right;
		stopx  = chunksel_left - 1;
		dirx   = -1;
	}
	if (dy <= 0) {
		starty = chunksel_top;
		stopy  = chunksel_bottom + 1;
		diry   = 1;
	} else {
		starty = chunksel_bottom;
		stopy  = chunksel_top - 1;
		diry   = -1;
	}
	for (int cy = starty; cy != stopy; cy += diry) {
		for (int cx = startx; cx != stopx; cx += dirx) {
			Map_chunk* chunk = gmap->get_chunk(cx, cy);
			if (chunk->is_selected()) {
				move_chunk(chunk, dx, dy);
			}
		}
	}
	gwin->set_all_dirty();
	chunksel_left   = (chunksel_left + dx + c_num_chunks) % c_num_chunks;
	chunksel_right  = (chunksel_right + dx + c_num_chunks) % c_num_chunks;
	chunksel_top    = (chunksel_top + dy + c_num_chunks) % c_num_chunks;
	chunksel_bottom = (chunksel_bottom + dy + c_num_chunks) % c_num_chunks;
}

void Cheat::set_edit_lift(int lift) {
	if (!enabled) {
		return;
	}
	edit_lift = lift;
	gwin->set_all_dirty();
}

void Cheat::set_edit_shape(int sh, int fr) {
	edit_shape = sh;
	edit_frame = fr;
}

void Cheat::toggle_infravision() {
	if (!enabled) {
		return;
	}

	infravision = !infravision;
	if (infravision) {
		eman->center_text("Infravision Enabled");
	} else {
		eman->center_text("Infravision Disabled");
	}
	gclock->set_palette();
}

void Cheat::toggle_pickpocket() {
	if (!enabled) {
		return;
	}

	pickpocket = !pickpocket;
	if (pickpocket) {
		eman->center_text("Pick Pocket Enabled");
		gwin->get_pal()->set(0);
	} else {
		eman->center_text("Pick Pocket Disabled");
	}
}

void Cheat::toggle_hack_mover() {
	if (!enabled) {
		return;
	}

	hack_mover = !hack_mover;
	if (hack_mover) {
		eman->center_text("Hack mover Enabled");
	} else {
		eman->center_text("Hack mover Disabled");
	}
}

void Cheat::change_gender() const {
	if (!enabled) {
		return;
	}

	if (gwin->get_main_actor()->get_type_flag(Actor::tf_sex)) {
		gwin->get_main_actor()->clear_type_flag(Actor::tf_sex);
		eman->center_text("Avatar is now male");
	} else {
		gwin->get_main_actor()->set_type_flag(Actor::tf_sex);
		eman->center_text("Avatar is now female");
	}
	gwin->set_all_dirty();
}

void Cheat::toggle_eggs() const {
	if (!enabled) {
		return;
	}

	gwin->paint_eggs = !gwin->paint_eggs;
	if (gwin->paint_eggs) {
		eman->center_text("Eggs display enabled");
	} else {
		eman->center_text("Eggs display disabled");
	}
	gwin->paint();
}

void Cheat::toggle_Petra() const {
	if (!enabled || (Game::get_game_type() != SERPENT_ISLE)) {
		return;
	}

	if (gwin->get_main_actor()->get_flag(Obj_flags::petra)) {
		gwin->get_main_actor()->clear_flag(Obj_flags::petra);
	} else {
		gwin->get_main_actor()->set_flag(Obj_flags::petra);
	}
	gwin->set_all_dirty();
}

void Cheat::toggle_naked() const {
	if (!enabled) {
		return;
	}

	if (gwin->get_main_actor()->get_flag(Obj_flags::naked)) {
		gwin->get_main_actor()->clear_flag(Obj_flags::naked);
	} else {
		gwin->get_main_actor()->set_flag(Obj_flags::naked);
	}
	gwin->set_all_dirty();
}

void Cheat::change_skin() const {
	if (!enabled) {
		return;
	}

	int        color = gwin->get_main_actor()->get_skin_color();
	const bool sex   = gwin->get_main_actor()->get_type_flag(Actor::tf_sex);
	color = Shapeinfo_lookup::GetNextSkin(color, sex, sman->have_si_shapes());

	gwin->get_main_actor()->set_skin_color(color);
	gwin->set_all_dirty();
}

void Cheat::levelup_party() const {
	if (!enabled) {
		return;
	}

	Actor* party[9];
	bool   leveledup = false;

	// get party, including Avatar
	const int cnt = gwin->get_party(party, 1);

	for (int i = 0; i < cnt; i++) {
		const int level = party[i]->get_level();
		if (level < 10) {
			leveledup        = true;
			const int newexp = 25 * (2 << level);    // one level higher
			party[i]->set_property(Actor::exp, newexp);
		}
	}

	if (leveledup) {
		eman->center_text("Level up!");
	} else {
		eman->center_text("Maximum level reached");
	}
}

void Cheat::fake_time_period() const {
	if (!enabled) {
		return;
	}

	if (!map_editor) {
		std::ostringstream s;
		gwin->get_clock()->fake_next_period();
		s << "Game clock incremented to " << gclock->get_hour() << ":"
		  << setfill('0') << setw(2) << gclock->get_minute();
		eman->center_text(s.str().c_str());
	}
}

void Cheat::dec_skip_lift() const {
	if (!enabled) {
		return;
	}

	if (gwin->skip_lift == 16) {
		gwin->skip_lift = 11;
	} else {
		gwin->skip_lift--;
	}
	if (gwin->skip_lift < 0) {    // 0 means 'terrain-editing'.
		gwin->skip_lift = 16;
	}
#ifdef DEBUG
	cout << "Skip_lift = " << gwin->skip_lift << endl;
#endif
	gwin->paint();
}

void Cheat::set_skip_lift(int skip) const {
	if (!enabled) {
		return;
	}

	if ((skip >= 1 && skip <= 11) || skip == 16) {
		gwin->skip_lift = skip;
	}
#ifdef DEBUG
	cout << "Skip_lift = " << gwin->skip_lift << endl;
#endif
	gwin->paint();
}

/*
 *  Tell EStudio whether there's a selection and clipboard.
 */
void Cheat::send_select_status() {
#ifdef USE_EXULTSTUDIO
	if (client_socket >= 0) {
		unsigned char msg[2];
		msg[0] = selected.empty() ? 0 : 1;
		msg[1] = clipboard.empty() ? 0 : 1;
		Exult_server::Send_data(
				client_socket, Exult_server::select_status, &msg[0], 2);
	}
#endif
}

/*
 *  Add an object to the selected list without checking.
 */
void Cheat::append_selected(Game_object* obj) {
	selected.push_back(obj->shared_from_this());
	if (selected.size() == 1) {    // First one?
		send_select_status();
	}
}

/*
 *  Toggle the selection of an object.
 */
void Cheat::toggle_selected(Game_object* obj) {
	if (!obj->get_owner()) {
		gwin->add_dirty(obj);
	} else {
		gwin->set_all_dirty();
	}
	// In list?
	for (auto it = selected.begin(); it != selected.end(); ++it) {
		if (it->get() == obj) {
			// Yes, so remove it.
			selected.erase(it);
			if (selected.empty()) {    // Last one?
				send_select_status();
			}
			return;
		}
	}
	selected.push_back(obj->shared_from_this());    // No, so add it.
	if (selected.size() == 1) {                     // 1st one?
		send_select_status();
	}
}

/*
 *  Clear out selection.
 */
void Cheat::clear_selected() {
	if (selected.empty()) {
		return;
	}
	for (auto& obj : selected) {
		if (!obj->get_owner()) {
			gwin->add_dirty(obj.get());
		} else {
			gwin->set_all_dirty();
		}
	}
	selected.clear();
	send_select_status();
}

/*
 *  Delete all selected objects.
 */
void Cheat::delete_selected() {
	if (selected.empty()) {
		return;
	}
	while (!selected.empty()) {
		const Game_object_shared obj = selected.back();
		selected.pop_back();
		if (obj->get_owner()) {
			gwin->add_dirty(obj.get());
		} else {    // In a gump?
			gwin->set_all_dirty();
		}
		obj->remove_this();
	}
	send_select_status();
}

/*
 *  Move the selected objects by given #tiles.  Objects inside another are
 *  treated as being at the location of their owner.
 */
void Cheat::move_selected_objs(int dx, int dy, int dz) {
	if (selected.empty()) {
		return;    // Nothing to do.
	}
	std::map<Game_object*, Tile_coord> tiles;    // Store locations here.
	int                                lowz  = 1000;
	int                                highz = -1000;    // Get min/max lift.
	// Remove & store old locations.
	for (auto& it : selected) {
		Game_object* obj   = it.get();
		Game_object* owner = obj->get_outermost();
		// Get location.  Use owner if inside.
		const Tile_coord tile = owner->get_tile();
		tiles[obj]            = tile;
		if (obj == owner) {    // Not inside?
			gwin->add_dirty(obj);
		} else {    // In a gump.  Repaint all for now.
			gwin->set_all_dirty();
		}
		Game_object_shared keep;
		obj->remove_this(&keep);
		if (tile.tz < lowz) {
			lowz = tile.tz;
		}
		if (tile.tz > highz) {
			highz = tile.tz;
		}
	}
	if (lowz + dz < 0) {    // Too low?
		dz = -lowz;
	}
	if (highz + dz > 255) {    // Too high?
		dz = 255 - highz;
	}
	// Add back in new locations.
	for (auto& it : selected) {
		const Tile_coord tile  = tiles[it.get()];
		const int        newtx = (tile.tx + dx + c_num_tiles) % c_num_tiles;
		const int        newty = (tile.ty + dy + c_num_tiles) % c_num_tiles;
		const int        newtz = (tile.tz + dz + 256) % 256;
		it->set_invalid();
		it->move(newtx, newty, newtz);
	}
}

/*  Move selected objects/chunks. */
void Cheat::move_selected(int dx, int dy, int dz) {
	if (edit_mode == select_chunks) {
		move_selected_chunks(dx, dy);
	} else {
		move_selected_objs(dx, dy, dz);
	}
}

/*
 *  Cycle the frame of the selected objects.
 */
void Cheat::cycle_selected_frame(int direction) {
	if (selected.empty()) {
		return;
	}
	for (auto& it : selected) {
		Game_object* obj          = it.get();
		int          maxFrames    = obj->get_num_frames();
		int          currentFrame = obj->get_framenum();
		int          newFrame     = currentFrame + direction;

		if (newFrame >= maxFrames) {
			newFrame = 0;
		} else if (newFrame < 0) {
			newFrame = maxFrames - 1;
		}

		obj->change_frame(newFrame);
		gwin->add_dirty(obj);
	}
	gwin->set_all_dirty();
}

/*
 *  Rotate the frame of the selected objects.
 */
void Cheat::rotate_selected_frame() {
	if (selected.empty()) {
		return;
	}
	for (auto& it : selected) {
		Game_object* obj = it.get();
		obj->change_frame(obj->get_rotated_frame(1));
		gwin->add_dirty(obj);
	}
	gwin->set_all_dirty();
}

bool Cheat::is_selected(Game_object* o) {
	for (auto& it : selected) {
		if (o == it.get()) {
			return true;
		}
	}
	return false;
}

/*
 *  Want lowest, southmost, then eastmost first.
 */
class Clip_compare {
public:
	bool operator()(
			const Game_object_shared& o1, const Game_object_shared& o2) {
		const Tile_coord t1 = o1->get_tile();
		const Tile_coord t2 = o2->get_tile();
		if (t1.tz != t2.tz) {
			return t1.tz < t2.tz;
		} else if (t1.ty != t2.ty) {
			return t1.ty > t2.ty;
		} else {
			return t1.tx >= t2.tx;
		}
	}
};

/*
 *  Cut/copy.
 */
void Cheat::cut(bool copy) {
	if (selected.empty()) {
		return;    // Nothing selected.
	}
	const bool clip_was_empty = clipboard.empty();
	// Clear out old clipboard.
	clipboard.resize(0);
	clipboard.reserve(selected.size());
	if (!copy) {    // Removing?  Force repaint.
		gwin->set_all_dirty();
	}
	// Go through selected objects.
	for (auto& it : selected) {
		Game_object_shared newobj;
		Game_object_shared keep;
		Game_object*       obj = it.get();
		const Tile_coord   t   = obj->get_outermost()->get_tile();
		if (copy) {
			// Check if this is an egg and clone it properly
			Egg_object* egg = obj->as_egg();
			if (egg) {
				// Clone the egg with all its data
				newobj = egg->clone_egg(0, 0, 0);
			} else {
				// Regular object - create new ireg object
				newobj = gwin->get_map()->create_ireg_object(
						obj->get_shapenum(), obj->get_framenum());
			}
		} else {    // Cut:  Remove but don't delete.
			newobj = obj->shared_from_this();
			obj->remove_this(&keep);
		}
		// Set pos. & add to list.
		newobj->set_shape_pos(
				t.tx % c_tiles_per_chunk, t.ty % c_tiles_per_chunk);
		clipboard.push_back(newobj);
	}
	// Sort.
	std::sort(selected.begin(), selected.end(), Clip_compare());
	if (!copy) {                    // Cut?  Remove selection.
		clear_selected();           // (This will send status.)
	} else if (clip_was_empty) {    // First clipboard object?
		send_select_status();
	}
}

/*
 *  Create an object as moveable (IREG) or fixed.
 *  ++++++++Goes away when we have obj->clone()++++++++++
 */

static Game_object_shared Create_object(
		Game_window* gwin, int shape, int frame    // What to create.
) {
	const Shape_info& info   = ShapeID::get_info(shape);
	const int         sclass = info.get_shape_class();
	// Is it an ireg (changeable) obj?
	const bool ireg
			= (sclass != Shape_info::unusable
			   && sclass != Shape_info::building);
	Game_object_shared newobj;
	if (ireg) {
		newobj = gwin->get_map()->create_ireg_object(
				info, shape, frame, 0, 0, 0);
	} else {
		newobj = gwin->get_map()->create_ifix_object(shape, frame);
	}
	return newobj;
}

/*
 *  Paste selection.
 */
void Cheat::paste(
		int mx, int my    // Mouse position.
) {
	if (clipboard.empty()) {
		return;    // Nothing there.
	}
	// Use lowest/south/east for position.
	const Tile_coord hot = clipboard[0]->get_tile();
	clear_selected();    // Remove old selected.
	for (const auto& obj : clipboard) {
		const Tile_coord t = obj->get_tile();
		// Figure spot rel. to hot-spot.
		const int liftpix = ((t.tz - hot.tz) * c_tilesize) / 2;
		const int x       = mx + (t.tx - hot.tx) * c_tilesize - liftpix;
		const int y       = my + (t.ty - hot.ty) * c_tilesize - liftpix;

		Game_object_shared newobj;
		// Check if this is an egg - if so, clone it with all data
		Egg_object* egg = obj->as_egg();
		if (egg) {
			newobj = egg->clone_egg(0, 0, 0);
		} else {
			// Regular object - create using Create_object
			newobj = Create_object(
					gwin, obj->get_shapenum(), obj->get_framenum());
		}

		Dragging_info drag(newobj);
		if (drag.drop(x, y, true)) {    // (Dels if it fails.)
			append_selected(newobj.get());
		}
	}
	gwin->set_all_dirty();    // Just repaint all.
}

/*
 *  Prompt for spot to paste to.
 */

void Cheat::paste() {
	if (clipboard.empty()) {
		return;
	}
	int x;
	int y;    // Allow dragging while here:
	if (Get_click(x, y, Mouse::greenselect, nullptr, true)) {
		paste(x, y);
	}
}

const int border    = 2;    // For showing map.
const int worldsize = c_tiles_per_chunk * c_num_chunks;

/*
 *  Show 'cheat' map.
 */
class Cheat_map : public Game_singletons, public Paintable {
public:
	int          x, y;    // Where it's painted.
	int          w, h;
	Shape_frame* map  = nullptr;
	Vga_file*    mini = nullptr;    // If "minimaps.vga" is found.

	Cheat_map(int mapnum = 0) {
		if (U7exists(PATCH_MINIMAPS)) {
			mini = new Vga_file(PATCH_MINIMAPS);
			if (!(map = mini->get_shape(0, mapnum))) {
				map = mini->get_shape(0, 0);
			}
		} else {
			const ShapeID mapid(
					game->get_shape("sprites/cheatmap"), 1, SF_GAME_FLX);
			map = mapid.get_shape();
		}
		// Get coords. for centered view.
		w = map->get_width();
		h = map->get_height();
		x = (gwin->get_width() - w) / 2 + map->get_xleft();
		y = (gwin->get_height() - h) / 2 + map->get_yabove();
	}

	~Cheat_map() override {
		delete mini;
	}

	void paint() override {
		sman->paint_shape(x, y, map, true);

		// mark current location
		int              xx;
		int              yy;
		const Tile_coord t = gwin->get_main_actor()->get_tile();

		xx = ((t.tx * (w - border * 2)) / worldsize);
		yy = ((t.ty * (h - border * 2)) / worldsize);

		xx += x - map->get_xleft() + border;
		yy += y - map->get_yabove() + border;
		gwin->get_win()->fill8(50, 1, 5, xx, yy - 2);
		gwin->get_win()->fill8(50, 5, 1, xx - 2, yy);
	}
};

void Cheat::map_teleport() const {
	if (!enabled) {
		return;
	}
	Gump_manager* gumpman = gwin->get_gump_man();
	if (touchui != nullptr) {
		touchui->hideGameControls();
	}
	Cheat_map map(gwin->get_map()->get_num());
	int       xx;
	int       yy;
	if (!Get_click(xx, yy, Mouse::greenselect, nullptr, false, &map)) {
		gwin->paint();
		if (touchui != nullptr && !gumpman->gump_mode()) {
			touchui->showGameControls();
		}
		return;
	}

	xx -= map.x - map.map->get_xleft() + border;
	yy -= map.y - map.map->get_yabove() + border;
	Tile_coord t;
	t.tx = static_cast<int>(((xx + 0.5) * worldsize) / (map.w - 2 * border));
	t.ty = static_cast<int>(((yy + 0.5) * worldsize) / (map.h - 2 * border));

	// World-wrapping.
	t.tx = (t.tx + c_num_tiles) % c_num_tiles;
	t.ty = (t.ty + c_num_tiles) % c_num_tiles;
	cout << "Teleporting to " << t.tx << "," << t.ty << "!" << endl;
	t.tz = 0;
	if (!gumpman->gump_mode() && touchui != nullptr) {
		touchui->showGameControls();
	}
	gwin->teleport_party(t);
	eman->center_text("Teleport!!!");
}

void Cheat::cursor_teleport() const {
	if (!enabled) {
		return;
	}

	int        x = Mouse::mouse()->get_mousex();
	int        y = Mouse::mouse()->get_mousey();
	Tile_coord t(
			gwin->get_scrolltx() + x / c_tilesize,
			gwin->get_scrollty() + y / c_tilesize, 0);
	t.fixme();
	gwin->teleport_party(t);
	eman->center_text("Teleport!!!");
}

void Cheat::next_map_teleport() const {
	const int curmap = gwin->get_map()->get_num();
	int       newmap = Find_next_map(curmap + 1, 4);    // Look forwards by 4.
	if (newmap == -1) {                                 // Not found?
		// Look from 0.
		newmap = Find_next_map(0, curmap);
		if (newmap == -1) {
			eman->center_text("Map not found");
			return;
		}
	}
	gwin->teleport_party(gwin->get_main_actor()->get_tile(), true, newmap);
	char msg[80];
	snprintf(msg, sizeof(msg), "To map #%02x", newmap);
	eman->center_text(msg);
}

void Cheat::create_coins() const {
	if (!enabled) {
		return;
	}

	gwin->get_main_actor()->add_quantity(100, 644);
	eman->center_text("Added 100 gold coins");
}

void Cheat::create_last_shape() const {
	if (!enabled) {
		return;
	}

	int current_shape = 0;
	int current_frame = 0;
	if (browser->get_shape(current_shape, current_frame)) {
		const Game_object_shared obj = gwin->get_map()->create_ireg_object(
				current_shape, current_frame);
		obj->set_flag(Obj_flags::okay_to_take);
		const Tile_coord t = Map_chunk::find_spot(
				gwin->get_main_actor()->get_tile(), 4, obj.get(), 2);
		if (t.tx != -1) {
			obj->move(t);
			eman->center_text("Object created");
		} else {
			eman->center_text("No room");
		}
	} else {
		eman->center_text("Can only create from 'shapes.vga'");
	}
}

void Cheat::delete_object() {
	if (!enabled) {
		return;
	}

	int          x = Mouse::mouse()->get_mousex();
	int          y = Mouse::mouse()->get_mousey();
	Game_object* obj;
	Gump*        gump = gwin->get_gump_man()->find_gump(x, y);
	if (gump) {
		obj = gump->find_object(x, y);
	} else {    // Search rest of world.
		obj = gwin->find_object(x, y);
	}

	if (obj) {
		clear_selected();    // Unselect all.
		obj->remove_this();
		eman->center_text("Object deleted");
		gwin->paint();
	}
}

void Cheat::heal_party() const {
	if (!enabled) {
		return;
	}

	int            i;    // for MSVC
	Party_manager* partyman = gwin->get_party_man();
	// NOTE:  dead_party_count decrs. as we
	//   resurrect.
	int count = partyman->get_dead_count();
	int dead[16];    // Save in separate list.
	if (count > 16) {
		count = 16;
	}
	for (i = 0; i < count; i++) {
		dead[i] = partyman->get_dead_member(i);
	}
	for (i = 0; i < count; i++) {
		const int  npc_num  = dead[i];
		Dead_body* body     = gwin->get_body(npc_num);
		Actor*     live_npc = gwin->get_npc(npc_num);
		if (body && live_npc) {
			const Tile_coord avpos = gwin->get_main_actor()->get_tile();
			body->move(avpos);
			live_npc->resurrect(body);
		}
	}

	// heal everyone
	Actor* party[9];
	count = gwin->get_party(party, 1);
	for (i = 0; i < count; i++) {
		if (!party[i]->is_dead()) {
			// heal
			party[i]->set_property(
					Actor::health, party[i]->get_property(Actor::strength));
			// cure poison
			party[i]->clear_flag(Obj_flags::poisoned);
			party[i]->clear_flag(Obj_flags::charmed);      // cure charmed
			party[i]->clear_flag(Obj_flags::cursed);       // cure cursed
			party[i]->clear_flag(Obj_flags::paralyzed);    // cure paralysis
			party[i]->set_temperature(0);                  // reset freezing

			// remove hunger  +++++ what is "normal" food level??
			party[i]->set_property(Actor::food_level, 30);

			// restore mana
			if (party[i]->get_effective_prop(Actor::magic) > 0) {
				party[i]->set_property(
						Actor::mana, party[i]->get_property(Actor::magic));
			}
		}
	}

	eman->center_text("Party healed");
	gwin->paint();
}

void Cheat::shape_browser() const {
	if (!enabled) {
		return;
	}

	browser->browse_shapes();
	gwin->paint();
	gclock->reset_palette();
}

bool Cheat::get_browser_shape(int& shape, int& frame) const {
	if (!enabled) {
		return false;
	}

	return browser->get_shape(shape, frame);
}

void Cheat::sound_tester() const {
	if (!enabled) {
		return;
	}

	tester->test_sound();
	gwin->paint();
}

void Cheat::cheat_screen() const {
	if (!enabled) {
		return;
	}

	cscreen->show_screen();
	gwin->set_all_dirty();
	gwin->paint();
}

void Cheat::toggle_grab_actor() {
	if (!enabled) {
		return;
	}

	grab_actor = !grab_actor;
	if (grab_actor) {
		eman->center_text("NPC Tool Actor Grabbing Enabled");
	} else {
		eman->center_text("NPC Tool Actor Grabbing Disabled");
	}
}

void Cheat::set_grabbed_actor(Actor* actor) const {
	if (!enabled || !cscreen) {
		return;
	}

	cscreen->SetGrabbedActor(actor);
}

void Cheat::clear_this_grabbed_actor(Actor* actor) const {
	if (!enabled || !cscreen) {
		return;
	}

	cscreen->ClearThisGrabbedActor(actor);
}

void Cheat::toggle_number_npcs() {
	if (!enabled) {
		return;
	}

	npc_numbers = !npc_numbers;
	if (npc_numbers) {
		eman->center_text("NPC Numbers Enabled");
	} else {
		eman->center_text("NPC Numbers Disabled");
	}
}

void Cheat::SetFoodUse(FoodUse newuse, bool writeout) {
	food_use = newuse;

	if (newuse == FoodUse::Manual) {
		config->set("config/gameplay/feeding", "manual", writeout);
	}
	if (newuse == FoodUse::Automatic) {
		config->set("config/gameplay/feeding", "automatic", writeout);
	}
	if (newuse == FoodUse::Disabled) {
		config->set("config/gameplay/feeding", "disabled", writeout);
	}
}
