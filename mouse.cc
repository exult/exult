/*
 *  mouse.cc - Mouse pointers.
 *
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

#include "Gump.h"
#include "Gump_manager.h"
#include "actors.h"
#include "barge.h"
#include "cheat.h"
#include "combat.h"
#include "combat_opts.h"
#include "fnames.h"
#include "gamewin.h"
#include "mouse.h"
#include "schedule.h" /* To get Schedule::combat */
#include "ucsched.h"

#ifndef max
using std::max;
#endif

bool Mouse::use_touch_input = false;

static inline bool should_hide_frame(int frame) {
	// on touch input only we hide the cursor
	if (Mouse::use_touch_input) {
		return frame == 0 || (frame >= 8 && frame <= 47);
	} else {
		return false;
	}
}

short Mouse::short_arrows[8] = {8, 9, 10, 11, 12, 13, 14, 15};
short Mouse::med_arrows[8]   = {16, 17, 18, 19, 20, 21, 22, 23};
short Mouse::long_arrows[8]  = {24, 25, 26, 27, 28, 29, 30, 31};

short Mouse::short_combat_arrows[8] = {32, 33, 34, 35, 36, 37, 38, 39};
short Mouse::med_combat_arrows[8]   = {40, 41, 42, 43, 44, 45, 46, 47};

Mouse* Mouse::current_mouse = nullptr;
bool   Mouse::mouse_update  = false;

/*
 *  Create.
 */

void Mouse::MakeCurrent() {
	// we are already current so do nothing
	if (this == current_mouse) {
		return;
	}
	// remove this from the current list if it is in it
	for (Mouse** list = &current_mouse; *list; list = &(*list)->previous) {
		if (this == *list) {
			*list    = previous;
			previous = nullptr;
			break;
		}
	}

	previous      = current_mouse;
	current_mouse = this;
}

Mouse::Mouse(Game_window* gw    // Where to draw.
			 )
		: gwin(gw), iwin(gwin->get_win()), box(0, 0, 0, 0), dirty(0, 0, 0, 0),
		  cur_framenum(0), cur(nullptr),
		  avatar_speed(100 * gwin->get_std_delay() / slow_speed_factor) {
	float fx, fy;
	SDL_GetMouseState(&fx, &fy);
	mousex = int(fx);
	mousey = int(fy);
	iwin->screen_to_game(mousex, mousey, gwin->get_fastmouse(), mousex, mousey);
	if (is_system_path_defined("<PATCH>") && U7exists(PATCH_POINTERS)) {
		pointers.load(PATCH_POINTERS);
	} else {
		pointers.load(POINTERS);
	}
	Init();
	set_shape(get_short_arrow(east));    // +++++For now.
	MakeCurrent();
}

Mouse::Mouse(
		Game_window* gw,    // Where to draw.
		IDataSource& shapes)
		: gwin(gw), iwin(gwin->get_win()), box(0, 0, 0, 0), dirty(0, 0, 0, 0),
		  cur_framenum(0), cur(nullptr),
		  avatar_speed(100 * gwin->get_std_delay() / slow_speed_factor) {
	float fx, fy;
	SDL_GetMouseState(&fx, &fy);
	mousex = int(fx);
	mousey = int(fy);
	iwin->screen_to_game(mousex, mousey, gwin->get_fastmouse(), mousex, mousey);
	pointers.load(&shapes);
	Init();
	set_shape0(0);
	MakeCurrent();
}

Mouse::~Mouse() {
	// remove this from the current list if it is in it
	for (Mouse** list = &current_mouse; *list; list = &(*list)->previous) {
		if (this == *list) {
			*list    = previous;
			previous = nullptr;
			break;
		}
	}
}

void Mouse::Init() {
	int maxleft  = 0;
	int maxright = 0;
	int maxabove = 0;
	int maxbelow = 0;
	for (auto& frame : pointers) {
		const int xleft  = frame->get_xleft();
		const int xright = frame->get_xright();
		const int yabove = frame->get_yabove();
		const int ybelow = frame->get_ybelow();
		if (xleft > maxleft) {
			maxleft = xleft;
		}
		if (xright > maxright) {
			maxright = xright;
		}
		if (yabove > maxabove) {
			maxabove = yabove;
		}
		if (ybelow > maxbelow) {
			maxbelow = ybelow;
		}
	}
	const int maxw = maxleft + maxright;
	const int maxh = maxabove + maxbelow;
	// Create backup buffer.
	backup = iwin->create_buffer(maxw, maxh);
	box.w  = maxw;
	box.h  = maxh;

	onscreen = false;    // initially offscreen
}

/*
 *  Show the mouse.
 */

void Mouse::show(unsigned char* trans) {
	if (should_hide_frame(cur_framenum)) {
		hide();
		return;
	}
	if (!onscreen) {
		onscreen = true;
		// Save background.
		iwin->get(backup.get(), box.x, box.y);
		// Paint new location.
		//		cur->paint_rle(iwin->get_ib8(), mousex, mousey);
		if (trans) {
			cur->paint_rle_remapped(mousex, mousey, trans);
		} else {
			cur->paint_rle(mousex, mousey);
		}
	}
}

/*
 *  Move cursor
 */

int Mouse::fast_offset_x = 0;
int Mouse::fast_offset_y = 0;

// Apply the fastmouse offset to a position
// This is used by Image_window::screen_to_game

void Mouse::apply_fast_offset(int& gx, int& gy) {
	if (gwin->get_fastmouse()) {
		gx += fast_offset_x;
		gy += fast_offset_y;
	}
}

// Unapply the fastmouse offset to a position
// This is used by Image_window::game_to_screen

void Mouse::unapply_fast_offset(int& gx, int& gy) {
	if (gwin->get_fastmouse()) {
		gx -= fast_offset_x;
		gy -= fast_offset_y;
	}
}

void Mouse::move(int& x, int& y) {
	// COUT("Start mouse offset " << fast_offset_x << "," << fast_offset_y);
	// COUT("Start mouse coord " << x << "," << y);

	// Special handling for fast mouse to keep the pointer within the game
	// screen
	if (gwin->get_fastmouse()) {
		// Clip the mouse to be within the bounds of the actual game screen
		int wx = std::max(0, std::min(x, gwin->get_win()->get_end_x() - 1));
		int wy = std::max(0, std::min(y, gwin->get_win()->get_end_y() - 1));

		// Adjust offset if mouse position changes
		fast_offset_x += wx - x;
		fast_offset_y += wy - y;

		// Update the mouse position for the rest ofthe function
		x = wx;
		y = wy;
	}
#ifdef DEBUG
	if (onscreen) {
		std::cerr << "Trying to move mouse while onscreen!" << std::endl;
	}
#endif
	// Shift to new position.
	box.shift(x - mousex, y - mousey);
	dirty  = dirty.add(box);    // Enlarge dirty area.
	mousex = x;
	mousey = y;
}

/*
 *  Set to new shape.  Should be called after checking that frame #
 *  actually changed.
 */

void Mouse::set_shape0(int framenum) {
	cur_framenum = framenum;
	cur          = pointers.get_frame(framenum);
	while (!cur) {    // For newly-created games.
		cur = pointers.get_frame(--framenum);
	}
	// Set backup box to cover mouse.
	box.x = mousex - cur->get_xleft();
	box.y = mousey - cur->get_yabove();
	dirty = dirty.add(box);    // Update dirty area.
	if (should_hide_frame(cur_framenum)) {
		hide();
	}
}

/*
 *  Set to an arbitrary location.
 */

void Mouse::set_location(
		int x, int y    // Mouse position.
) {
	mousex = x;
	mousey = y;
	box.x  = mousex - cur->get_xleft();
	box.y  = mousey - cur->get_yabove();
}

/*
 *  Flash a desired shape for about 1/2 second.
 */

void Mouse::flash_shape(Mouse_shapes flash) {
	const Mouse_shapes saveshape = get_shape();
	hide();
	set_shape(flash);
	show();
	gwin->show(true);
	SDL_Delay(600);
	hide();
	gwin->paint();
	set_shape(saveshape);
	gwin->set_painted();
}

/*
 *  Set default cursor
 */

void Mouse::set_speed_cursor() {
	Game_window*  gwin     = Game_window::get_instance();
	Gump_manager* gump_man = gwin->get_gump_man();

	int cursor = dontchange;

	// Check if we are in dont_move mode, in this case display the hand cursor
	if (gwin->main_actor_dont_move()) {
		cursor = hand;
	}
	/* Can again, optionally move in gump mode. */
	else if (gump_man->gump_mode()) {
		if (gump_man->gumps_dont_pause_game()) {
			Gump* gump = gump_man->find_gump(mousex, mousey);

			if (gump && !gump->no_handcursor()) {
				cursor = hand;
			}
		} else {
			cursor = hand;
		}
	}

	else if (gwin->get_dragging_gump()) {
		cursor = hand;
	}

	else if (cheat.in_map_editor()) {
		switch (cheat.get_edit_mode()) {
		case Cheat::move:
			cursor = hand;
			break;
		case Cheat::paint:
			cursor = short_combat_arrows[4];
			break;    // Short S red arrow.
		case Cheat::paint_chunks:
			cursor = med_combat_arrows[0];
			break;    // Med. N red arrow.
		case Cheat::combo_pick:
		case Cheat::select_chunks:
		case Cheat::edit_pick:
			cursor = greenselect;
			break;    // Nice to have something else.
		}
	} else if (Combat::is_paused()) {
		cursor = short_combat_arrows[0];    // Short N red arrow.
	}
	if (cursor == dontchange) {
		int           ax;
		int           ay;    // Get Avatar/barge screen location.
		Barge_object* barge = gwin->get_moving_barge();
		if (barge) {
			// Use center of barge.
			gwin->get_shape_location(barge, ax, ay);
			ax -= barge->get_xtiles() * (c_tilesize / 2);
			ay -= barge->get_ytiles() * (c_tilesize / 2);
		} else {
			gwin->get_shape_location(gwin->get_main_actor(), ax, ay);
		}

		const int       dy  = ay - mousey;
		const int       dx  = mousex - ax;
		const Direction dir = Get_direction_NoWrap(dy, dx);

		// Create a speed rectangle that's half of the game window dimensions
		// but with a minimum size of 200x200
		const TileRect game_rect = gwin->get_game_rect();
		const int      rect_size = std::max(
                std::min(200, std::min(game_rect.w, game_rect.h)),
                std::min(game_rect.w, game_rect.h) / 2);
		const TileRect speed_rect(
				ax - rect_size / 2, ay - rect_size / 2, rect_size, rect_size);
		const bool in_speed_rect
				= (mousex >= speed_rect.x
				   && mousex < speed_rect.x + speed_rect.w
				   && mousey >= speed_rect.y
				   && mousey < speed_rect.y + speed_rect.h);

		float speed_section = 1.0f;
		if (in_speed_rect) {
			// Calculate speed_section based on the dynamic rectangle size
			const int half_size = rect_size / 2;
			speed_section
					= max(max(-static_cast<float>(dx) / half_size,
							  static_cast<float>(dx) / half_size),
						  max(static_cast<float>(dy) / half_size,
							  -static_cast<float>(dy) / half_size));
		}

		const bool      nearby_hostile        = gwin->is_hostile_nearby() && !cheat.in_god_mode();
		bool            has_active_nohalt_scr = false;
		Usecode_script* scr                   = nullptr;
		Actor*          act                   = gwin->get_main_actor();
		while ((scr = Usecode_script::find_active(act, scr)) != nullptr) {
			// We should only be here if scripts are nohalt, but just
			// in case...
			if (scr->is_no_halt()) {
				has_active_nohalt_scr = true;
				break;
			}
		}

		const int base_speed = 200 * gwin->get_std_delay();
		if (!in_speed_rect) {
			// Beyond the 200x200 rectangle - use long arrow with fast movement
			// But respect combat/hostile conditions just like inside the
			// rectangle
			if (gwin->in_combat()) {
				cursor = get_medium_combat_arrow(
						dir);    // No long combat arrows exist
				avatar_speed = base_speed / medium_combat_speed_factor;
			} else if (nearby_hostile || has_active_nohalt_scr) {
				cursor       = get_medium_arrow(dir);
				avatar_speed = base_speed / medium_speed_factor;
			} else {
				cursor       = get_long_arrow(dir);
				avatar_speed = base_speed / fast_speed_factor;
			}
		} else if (speed_section < 0.4f) {
			// Inside the rectangle with low speed_section - use short arrow
			if (gwin->in_combat()) {
				cursor = get_short_combat_arrow(dir);
			} else {
				cursor = get_short_arrow(dir);
			}
			avatar_speed = base_speed / slow_speed_factor;
		} else if (
				speed_section < 0.8f || gwin->in_combat() || nearby_hostile
				|| has_active_nohalt_scr) {
			if (gwin->in_combat()) {
				cursor = get_medium_combat_arrow(dir);
			} else {
				cursor = get_medium_arrow(dir);
			}
			if (gwin->in_combat() || nearby_hostile) {
				avatar_speed = base_speed / medium_combat_speed_factor;
			} else {
				avatar_speed = base_speed / medium_speed_factor;
			}
		} else /* Fast - NB, we can't get here in combat mode; there is no
				* long combat arrow, nor is there a fast combat speed. */
		{
			cursor       = get_long_arrow(dir);
			avatar_speed = base_speed / fast_speed_factor;
		}
	}

	if (cursor != dontchange) {
		set_shape(cursor);
	}
}
