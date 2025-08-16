/*
Copyright (C) 2011-2024 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef SHORTCUTBAR_GUMP_H
#define SHORTCUTBAR_GUMP_H

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

#include "Gamemenu_gump.h"
#include "Modal_gump.h"
#include "gamewin.h"
#include "misc_buttons.h"
#include "objs.h"

/* -------------------------------------------- */

enum ShortcutBarButtonItemType {
	SB_ITEM_DISK,
	SB_ITEM_TOGGLE_COMBAT,
	SB_ITEM_MAP,
	SB_ITEM_SPELLBOOK,
	SB_ITEM_BACKPACK,
	SB_ITEM_KEY,
	SB_ITEM_KEYRING,
	SB_ITEM_NOTEBOOK,
	SB_ITEM_TARGET,
	SB_ITEM_JAWBONE,
	SB_ITEM_FEED
};

Game_object* is_party_item(
		int shnum, int frnum = c_any_framenum, int qual = c_any_qual);

struct ShortcutBarButtonItem {
	const char*               name;
	ShortcutBarButtonItemType type;
	ShapeID*                  shapeId;
	TileRect                  rect;    // Shortcut bar button click area
	int  mx, my;                       // Coordinates where shape is to be drawn
	bool pushed;
	bool translucent;
};

#define MAX_SHORTCUT_BAR_ITEMS 10

class ShortcutBar_gump : public Gump {
public:
	ShortcutBar_gump(int placex = 0, int placey = 0);
	~ShortcutBar_gump() override;
	int  handle_event(SDL_Event* event);
	void paint() override;

	// Don't close on end_gump_mode
	bool is_persistent() const override {
		return true;
	}

	// Can't be dragged with mouse
	bool is_draggable() const override {
		return false;
	}

	// Show the hand cursor
	bool no_handcursor() const override {
		return true;
	}

	static uint32 eventType;

	enum {
		EVENT_CODE_INVALID    = 0,
		SHORTCUT_BAR_MOUSE_UP = 0x53425545
	};

	int  startx;
	int  resx;
	int  gamex;
	int  starty;
	int  resy;
	int  gamey;
	void handleMouseUp(SDL_Event& event);
	// add dirty region, if dirty
	void update_gump() override;

	void set_changed() {
		has_changed = true;
	}

	void        check_for_updates(int shnum);
	static void HideGump();
	static void ShowGump();
	static bool Visible();

private:
	ShortcutBarButtonItem buttonItems[MAX_SHORTCUT_BAR_ITEMS];
	int                   numButtons;
	SDL_TimerID           timerId;
	void                  createButtons();
	void                  deleteButtons();
	void                  onItemClicked(int index, bool doubleClicked);
	void                  sdl_mouse_down(SDL_Event* event, int mx, int my);
	void                  sdl_mouse_up(SDL_Event* event, int mx, int my);
	bool                  has_changed;

	int locx;
	int locy;
	int width;
	int height;
};

#endif
