/*
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

#ifndef CHEAT_SCREEN_H
#define CHEAT_SCREEN_H

#include "imagebuf.h"
#include "palette.h"
#include "rect.h"

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

#include <climits>
#include <memory>
#include <unordered_set>
#include <vector>

class Game_window;
class Image_buffer8;
class Font;
class Game_clock;
class Actor;

class CheatScreen {
	Actor*             grabbed = nullptr;
	static const char* schedules[33];
	static const char* flag_names[64];
	static const char* alignments[4];

public:
	CheatScreen() : highlighttable(), hovertable(), buttons_down() {}

	void show_screen();

	void SetGrabbedActor(Actor* g) {
		grabbed = g;
	}

	void ClearThisGrabbedActor(Actor* g) const {
		if (g == grabbed) {
			g = nullptr;
		}
	}

	void clear_buttons() {
		buttons_down.clear();
	}

private:
	enum Cheat_Prompt {
		CP_Command = 0,

		CP_HitKey         = 1,
		CP_NotAvail       = 2,
		CP_InvalidNPC     = 3,
		CP_InvalidCom     = 4,
		CP_Canceled       = 5,
		CP_ClockSet       = 6,
		CP_InvalidTime    = 7,
		CP_InvalidShape   = 8,
		CP_InvalidValue   = 9,
		CP_Created        = 10,
		CP_ShapeSet       = 11,
		CP_ValueSet       = 12,
		CP_NameSet        = 13,
		CP_WrongShapeFile = 14,

		CP_ChooseNPC  = 16,
		CP_EnterValue = 17,
		CP_Minute     = 18,
		CP_Hour       = 19,
		CP_Day        = 20,
		CP_Shape      = 21,
		CP_Activity   = 22,
		CP_XCoord     = 23,
		CP_YCoord     = 24,
		CP_Lift       = 25,
		CP_GFlagNum   = 26,
		CP_NFlagNum   = 27,
		CP_TempNum    = 28,
		CP_NLatitude  = 29,
		CP_SLatitude  = 30,
		CP_WLongitude = 31,
		CP_ELongitude = 32,

		CP_Name       = 33,
		CP_NorthSouth = 34,
		CP_WestEast   = 35,

		CP_HexXCoord          = 37,
		CP_HexYCoord          = 38,
		CP_EnterValueNoCancel = 39,
		CP_CustomValue
	};

	struct Hotspot : TileRect {
		SDL_Keycode keycode;

		Hotspot(SDL_Keycode keycode, int x, int y, int w, int h)
				: TileRect(x, y, w, h), keycode(keycode) {}
	};

	std::vector<Hotspot> hotspots;

	struct {
		Uint32      highlight     = 0;
		Uint32      highlighttime = 0;
		char        input[17]     = {0};
		int         command       = 0;
		bool        activate      = false;
		const char* custom_prompt = nullptr;
		int         saved_value   = 0;
		long        val_min       = LONG_MIN;
		long        val_max       = LONG_MAX;
		long        value;
		Uint32      last_swipe = 0;
		// Accumulated swipe deltas. We treat these as a vector
		float swipe_dx = 0;
		float swipe_dy = 0;

	private:
		Cheat_Prompt mode = CP_Command;

	public:
		Cheat_Prompt GetMode() {
			return mode;
		}

		void SetMode(Cheat_Prompt newmode, bool clearinput = true) {
			// Clear the input if changing to or from a text/value input mode
			if (clearinput) {
				input[0] = 0;
			}
			mode = newmode;
		}
	} state;

	template <class T>
	struct ClearState {
		T* ptr = nullptr;

		ClearState() = delete;

		ClearState(T& obj, bool now = true, bool on_destruct = true)
				: ptr(&obj) {
			if (now) {
				*ptr = T();
			}
			if (!on_destruct) {
				ptr = nullptr;
			}
		}

		~ClearState() {
			if (ptr) {
				*ptr = T();
			}
		}
	};

	Game_window*          gwin  = nullptr;
	Image_buffer8*        ibuf  = nullptr;
	std::shared_ptr<Font> font  = nullptr;
	Game_clock*           clock = nullptr;
	int                   maxx = 0, maxy = 0;
	int                   centerx = 0, centery = 0;
	Palette               pal;
	Xform_palette         highlighttable;
	Xform_palette         hovertable;
	Xform_palette         fontcolor;

	// Turn off clang-format so it doesn't wrap the long comments
	// clang-format off
 
 	// Constants used for touch input
	const float  swipe_threshold     = 0.075f;	// Threshold for Swipes to be converted into key inputs.

	// clang-format on

	void        SharedPrompt();
	bool        SharedInput();
	void        SharedMenu();
	SDL_Keycode CheckHotspots(int mx, int my, int radius = 4);
	void        PaintHotspots();
	void        NormalLoop();
	void        NormalDisplay();
	void        NormalMenu();
	void        NormalActivate();
	bool        NormalCheck();

	void ActivityDisplay();

	// Paint an arrow using the font, type is one of '^' 'v' '<' '>'
	void PaintArrow(int offsetx, int offsety, int type);

	Cheat_Prompt GlobalFlagLoop(int num);

	Cheat_Prompt TimeSetLoop();

	Cheat_Prompt NPCLoop(int num);
	void         NPCDisplay(Actor* actor, int& num);
	void         NPCMenu(Actor* actor, int& num);
	void         NPCActivate(Actor* actor, int& num);
	bool         NPCCheck(Actor* actor, int& num);

	void         FlagLoop(Actor* actor);
	void         FlagMenu(Actor* actor);
	void         FlagActivate(Actor* actor);
	bool         FlagCheck(Actor* actor);
	Cheat_Prompt AdvancedFlagLoop(int flagnum, Actor* actor);

	void BusinessLoop(Actor* actor);
	void BusinessDisplay(Actor* actor);
	void BusinessMenu(Actor* actor);
	void BusinessActivate(Actor* actor, int& time, int& prev);
	bool BusinessCheck(Actor* actor, int& time);

	void StatLoop(Actor* actor);
	void StatMenu(Actor* actor);
	void StatActivate(Actor* actor);
	bool StatCheck(Actor* actor);
	void PalEffectLoop(Actor*);
	void PalEffectMenu(Actor* actor);
	void PalEffectActivate(Actor* actor);
	bool PalEffectCheck(Actor* actor);
	void TeleportLoop();
	void TeleportDisplay();
	void TeleportMenu();
	void TeleportActivate(int& prev);
	bool TeleportCheck();

	//! @brief Add a menu item with a hotspot for the Specified key
	//! @param offsetx X coord for the menu item
	//! @param offsety Y coord for the menu item
	//! @param keycode Keycode used to activate the menuitem
	//! @param label Label of the Menu item
	//! @return Width in pixels of the menu item
	int AddMenuItem(
			int offsetx, int offsety, SDL_Keycode keycode, const char* label);

	//! @brief Add a menuitem for left and right cursor keys
	//! @param offsetx X coord for the menu item
	//! @param offsety  Y coord for the menu item
	//! @param label Label of the Menu item
	//! @param left Flag for if the left arrow should be shown
	//! @param right Flag for if the right arrow should be shown
	//! @param leaveempty Flag to indicate that blank space should be used
	//! inplace of missing arrows
	//! @param fixedlabel Flag to indicate the label should be drawn at a fixed
	//! position if arrows are mising and !leaveempty
	//! @return Width in pixels of the menu item
	int AddLeftRightMenuItem(
			int offsetx, int offsety, const char* label, bool left, bool right,
			bool leaveempty, bool fixedlabel = false);

	int  highest_map = INT_MIN;
	int  Get_highest_map();
	void EndFrame();

	//
	static const int             button_down_finger = -1;
	std::unordered_multiset<int> buttons_down;
	void                         WaitButtonsUp();
};

#endif
