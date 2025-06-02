/*
 *  mouse.h - Mouse pointers.
 *
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

#ifndef MOUSE_H
#define MOUSE_H

#include "dir.h"
#include "iwin8.h"
#include "rect.h"
#include "vgafile.h"

class Game_window;

/*
 *  Handle custom mouse pointers.
 */
class Mouse {
protected:
	Shape_file                    pointers;    // Pointers from 'pointers.shp'.
	Game_window*                  gwin;        // Where to draw.
	Image_window8*                iwin;        // From gwin.
	std::unique_ptr<Image_buffer> backup;    // Stores image below mouse shape.
	TileRect                      box;       // Area backed up.
	TileRect                      dirty;     // Dirty area from mouse move.
	int          mousex, mousey;             // Last place where mouse was.
	int          cur_framenum;               // Frame # of current shape.
	Shape_frame* cur;                        // Current shape.
	bool         onscreen;           // true if mouse is drawn on screen.
	static short short_arrows[8];    // Frame #'s of short arrows, indexed
	//   by direction (0-7, 0=east).
	static short med_arrows[8];             // Medium arrows.
	static short long_arrows[8];            // Frame #'s of long arrows.
	static short short_combat_arrows[8];    // Short red arrows
	static short med_combat_arrows[8];      // Medium red arrows
	void set_shape0(int framenum);          // Set shape without checking first.

	// Offset when fast mouse is enabled. Static so the values persists when a
	// new mouse object is created when switching menus
	static int fast_offset_x;
	static int fast_offset_y;

	void Init();

public:
	enum Mouse_shapes {        // List of shapes' frame #'s.
		dontchange  = 1000,    // Flag to not change.
		hand        = 0,
		redx        = 1,
		greenselect = 2,    // For modal select.
		tooheavy    = 3,
		outofrange  = 4,
		outofammo   = 5,
		wontfit     = 6,
		hourglass   = 7,
		greensquare = 23,
		blocked     = 49
	};

	/* Avatar speed, relative to standard delay:
	 * avatar_speed = standard_delay * 100 / avatar_speed_factor
	 *
	 *
	 * Experimental results, Serpent Isle
	 *
	 * "short" arrow within central 0.4 of screen in each dimension
	 * "middle" arrow within central 0.8 of screen in each dimension
	 * "long" arrow (non-combat, non-threat only) outside
	 *
	 * relative speeds:
	 * (movement type           - time for a certain dist. - rel. speed)
	 *  non-combat short arrow  -          8               -   1
	 *  non-combat medium arrow -          4               -   2
	 *  non-combat long arrow   -          2               -   4
	 *  combat short arrow      -          8               -   1
	 *  combat medium arrow     -          6               -   4/3
	 */
	enum Avatar_Speed_Factors {
		slow_speed_factor          = 100,
		medium_combat_speed_factor = 150,
		medium_speed_factor        = 200,
		fast_speed_factor          = 400
	};

	int avatar_speed;

	static bool mouse_update;

private:
	// This is updated By MakeCurrent to point to the current mouse object
	// that is being used when the current mouse is destructed this is set to
	// previous
	static Mouse* current_mouse;

	// The previous current mouse when MakeCurrent was called
	// This forms a linked list with head current of the active
	// mouse objects
	// MakeCurrent() and  ~Mouse() will update the list as needed
	Mouse* previous = nullptr;

public:
	// Get the current Mouse
	static Mouse* mouse() {
		return current_mouse;
	}

	// Make this mouse object the current object
	// Automatically called by the constructors
	void MakeCurrent();

	Mouse(Game_window* gw);
	Mouse(Game_window* gw, IDataSource& shapes);
	~Mouse();

	void show(unsigned char* trans = nullptr);    // Paint it.

	void hide() {    // Restore area under mouse.
		if (onscreen) {
			onscreen = false;
			iwin->put(backup.get(), box.x, box.y);
			dirty = box;    // Init. dirty to box.
		}
	}

	void set_shape(int framenum) {    // Set to desired shape.
		if (framenum != cur_framenum) {
			set_shape0(framenum);
		}
	}

	void set_shape(Mouse_shapes shape) {
		set_shape(static_cast<int>(shape));
	}

	Mouse_shapes get_shape() {
		return static_cast<Mouse_shapes>(cur_framenum);
	}

	// Apply the fastmouse offset to a position
	// This is used by Image_window::screen_to_game
	void apply_fast_offset(int& gx, int& gy);

	// Unapply the fastmouse offset to a position
	// This is used by Image_window::game_to_screen
	void unapply_fast_offset(int& gx, int& gy);

	// Move mouse to a new location when handling mouse motion events.
	// Updates the input coords to reflects the actual position the mouse will
	// be displayed at, if it gets changed for some reason like fast mouse
	// bound clipping
	// You Must call Image_window::screen_to_game() first to translate screen
	// coords to the game coords expected by this function
	void move(int& gx, int& gy);

	void blit_dirty() {    // Blit dirty area.
		iwin->show(dirty.x - 1, dirty.y - 1, dirty.w + 2, dirty.h + 2);
	}

protected:
	void set_location(
			int x, int y);    // Set to given location. This generally shouldn't
							  // be used. move should be used instead
public:
	// Flash desired shape for 1/2 sec.
	void flash_shape(Mouse_shapes flash);

	// Set to short arrow.
	int get_short_arrow(Direction dir) {
		return short_arrows[static_cast<int>(dir)];
	}

	// Set to medium arrow.
	int get_medium_arrow(Direction dir) {
		return med_arrows[static_cast<int>(dir)];
	}

	// Set to long arrow.
	int get_long_arrow(Direction dir) {
		return long_arrows[static_cast<int>(dir)];
	}

	// Set to short combat mode arrow.
	int get_short_combat_arrow(Direction dir) {
		return short_combat_arrows[static_cast<int>(dir)];
	}

	// Set to medium combat mode arrow.
	int get_medium_combat_arrow(Direction dir) {
		return med_combat_arrows[static_cast<int>(dir)];
	}

	bool is_onscreen() {
		return onscreen;
	}

	inline int get_mousex() const {
		return mousex;
	}

	inline int get_mousey() const {
		return mousey;
	}

	// Sets hand or speed cursors
	void set_speed_cursor();

	// only use touch input?
	static bool use_touch_input;
};

#endif
