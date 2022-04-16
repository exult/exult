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
#  include <config.h>
#endif

#include <cstring>

#include "SDL_events.h"
#include "files/U7file.h"
#include "chunks.h"
#include "gamemap.h"
#include "gamewin.h"
#include "game.h"
#include "cheat_screen.h"
#include "font.h"
#include "actors.h"
#include "cheat.h"
#include "exult.h"
#include "imagewin.h"
#include "vgafile.h"
#include "gameclk.h"
#include "schedule.h"
#include "ucmachine.h"
#include "Configuration.h"
#include "SDL.h"
#include "party.h"
#include "miscinf.h"
#include "gump_utils.h"
#include "ignore_unused_variable_warning.h"
#include "touchui.h"
#include "Gump_manager.h"

const char *CheatScreen::schedules[33] = {
	"Combat",
	"Hor. Pace",
	"Ver. Pace",
	"Talk",
	"Dance",
	"Eat",
	"Farm",
	"Tend Shop",
	"Miner",
	"Hound",
	"Stand",
	"Loiter",
	"Wander",
	"Blacksmith",
	"Sleep",
	"Wait",
	"Major Sit",
	"Graze",
	"Bake",
	"Sew",
	"Shy",
	"Lab",
	"Thief",
	"Waiter",
	"Special",
	"Kid Games",
	"Eat at Inn",
	"Duel",
	"Preach",
	"Patrol",
	"Desk Work",
	"Follow Avt",
	"Move2Sched"
};

const char *CheatScreen::flag_names[64] = {
	"invisible",        // 0x00
	"asleep",       // 0x01
	"charmed",      // 0x02
	"cursed",       // 0x03
	"dead",         // 0x04
	nullptr,          // 0x05
	"in_party",     // 0x06
	"paralyzed",        // 0x07

	"poisoned",     // 0x08
	"protection",       // 0x09
	"on_moving_barge",  // 0x0A
	"okay_to_take",     // 0x0B
	"might",        // 0x0C
	"immunities",   //0x0D
	"cant_die",     // 0x0E
	"dancing",      // 0x0F

	"dont_move/bg_dont_render",     // 0x10
	"si_on_moving_barge",   // 0x11
	"is_temporary",     // 0x12
	nullptr,          // 0x13
	"sailor",          // 0x14
	"okay_to_land",     // 0x15
	"dont_render/bg_dont_move", // 0x16
	"in_dungeon",   // 0x17

	nullptr,          // 0x18
	"confused",     // 0x19
	"in_motion",        // 0x1A
	nullptr,         // 0x1B
	"met",          // 0x1C
	"tournament",   // 0x1D
	"si_zombie",    // 0x1E
	"no_spell_casting",  // 0x1F

	"polymorph",    // 0x20
	"tattooed",     // 0x21
	"read",         // 0x22
	"petra",        // 0x23
	"si_lizard_king",  // 0x24
	"freeze",       // 0x25
	"naked",    // 0x26
	nullptr,          // 0x27

	nullptr,          // 0x28
	nullptr,          // 0x29
	nullptr,          // 0x2A
	nullptr,          // 0x2B
	nullptr,          // 0x2C
	nullptr,          // 0x2D
	nullptr,          // 0x2E
	nullptr,          // 0x2F

	nullptr,          // 0x30
	nullptr,          // 0x31
	nullptr,          // 0x32
	nullptr,          // 0x33
	nullptr,          // 0x34
	nullptr,          // 0x35
	nullptr,          // 0x36
	nullptr,          // 0x37

	nullptr,          // 0x38
	nullptr,          // 0x39
	nullptr,          // 0x3A
	nullptr,          // 0x3B
	nullptr,          // 0x3C
	nullptr,          // 0x3D
	nullptr,          // 0x3E
	nullptr,          // 0x3F
};

const char *CheatScreen::alignments[4] = {
	"Neutral",
	"Good",
	"Evil",
	"Chaotic"
};

static int Find_highest_map(
) {
	int n = 0;
	int next;
	while ((next = Find_next_map(n + 1, 10)) != -1)
		n = next;
	return n;
}

void CheatScreen::show_screen() {
	gwin = Game_window::get_instance();
	ibuf = gwin->get_win()->get_ib8();
	font = fontManager.get_font("MENU_FONT");
	clock = gwin->get_clock();
	maxx = gwin->get_width();
#if defined(__IPHONEOS__) || defined(ANDROID)
	maxy = 200;
#else
	maxy = gwin->get_height();
#endif
	centerx = maxx / 2;
	centery = maxy / 2;
	if (touchui != nullptr) {
		touchui->hideGameControls();
		SDL_StartTextInput();
	}

	// Pause the game
	gwin->get_tqueue()->pause(SDL_GetTicks());

	const str_int_pair &pal_tuple_static = game->get_resource("palettes/0");
	const str_int_pair &pal_tuple_patch = game->get_resource("palettes/patch/0");
	pal.load(pal_tuple_static.str, pal_tuple_patch.str, pal_tuple_static.num);
	pal.apply();

	// Start the loop
	NormalLoop();

	// Resume the game clock
	gwin->get_tqueue()->resume(SDL_GetTicks());

	// Reset the palette
	clock->reset_palette();
	if (touchui != nullptr) {
		Gump_manager *gumpman = gwin->get_gump_man();
		if (!gumpman->gump_mode()) {
			touchui->showGameControls();
		}
		if (SDL_IsTextInputActive()) {
			SDL_StopTextInput();
		}
	}
}


//
// DISPLAYS
//

//
// Shared
//

void CheatScreen::SharedPrompt(char *input, const Cheat_Prompt &mode) {
	char buf[512];

#if defined(__IPHONEOS__) || defined(ANDROID)
	const int prompt = 81;
	const int promptmes = 90;
	const int offsetx = 15;
#else
	const int prompt = maxy - 18;
	const int promptmes = maxy - 9;
	const int offsetx = 0;
#endif
	font->paint_text_fixedwidth(ibuf, "Select->", offsetx, prompt, 8);

	if (input && std::strlen(input)) {
		font->paint_text_fixedwidth(ibuf, input, 64 + offsetx , prompt, 8);
		font->paint_text_fixedwidth(ibuf, "_", 64 + offsetx + std::strlen(input) * 8, prompt, 8);
	} else
		font->paint_text_fixedwidth(ibuf, "_", 64 + offsetx, prompt, 8);

	// ...and Prompt Message
	switch (mode) {
	default:
	case CP_Command:
		font->paint_text_fixedwidth(ibuf, "Enter Command.", offsetx, promptmes, 8);
		break;

	case CP_HitKey:
		font->paint_text_fixedwidth(ibuf, "Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_NotAvail:
		font->paint_text_fixedwidth(ibuf, "Not yet available. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_InvalidNPC:
		font->paint_text_fixedwidth(ibuf, "Invalid NPC. Hit a key", offsetx, promptmes, 8);
		break;

	case CP_InvalidCom:
		font->paint_text_fixedwidth(ibuf, "Invalid Command. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_Canceled:
		font->paint_text_fixedwidth(ibuf, "Canceled. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_ClockSet:
		font->paint_text_fixedwidth(ibuf, "Clock Set. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_InvalidTime:
		font->paint_text_fixedwidth(ibuf, "Invalid Time. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_InvalidShape:
		font->paint_text_fixedwidth(ibuf, "Invalid Shape. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_InvalidValue:
		font->paint_text_fixedwidth(ibuf, "Invalid Value. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_Created:
		font->paint_text_fixedwidth(ibuf, "Item Created. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_ShapeSet:
		font->paint_text_fixedwidth(ibuf, "Shape Set. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_ValueSet:
		font->paint_text_fixedwidth(ibuf, "Clock Set. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_NameSet:
		font->paint_text_fixedwidth(ibuf, "Name Changed. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_WrongShapeFile:
		font->paint_text_fixedwidth(ibuf, "Wrong shape file. Must be SHAPES.VGA.", offsetx, promptmes, 8);
		break;


	case CP_ChooseNPC:
		font->paint_text_fixedwidth(ibuf, "Which NPC? (-1 to cancel.)", offsetx, promptmes, 8);
		break;

	case CP_EnterValue:
		font->paint_text_fixedwidth(ibuf, "Enter Value. (-1 to cancel.)", offsetx, promptmes, 8);
		break;

	case CP_EnterValueNoCancel:
		font->paint_text_fixedwidth(ibuf, "Enter Value.", offsetx, promptmes, 8);
		break;

	case CP_Minute:
		font->paint_text_fixedwidth(ibuf, "Enter Minute. (-1 to cancel.)", offsetx, promptmes, 8);
		break;

	case CP_Hour:
		font->paint_text_fixedwidth(ibuf, "Enter Hour. (-1 to cancel.)", offsetx, promptmes, 8);
		break;

	case CP_Day:
		font->paint_text_fixedwidth(ibuf, "Enter Day. (-1 to cancel.)", offsetx, promptmes, 8);
		break;

	case CP_Shape:
		font->paint_text_fixedwidth(ibuf, "Enter Shape (B=Browse or -1=Cancel)", offsetx, promptmes, 8);
		break;

	case CP_Activity:
		font->paint_text_fixedwidth(ibuf, "Enter Activity 0-31. (-1 to cancel.)", offsetx, promptmes, 8);
		break;

	case CP_XCoord:
		snprintf(buf, 512, "Enter X Coord. Max %i (-1 to cancel)", c_num_tiles);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8);
		break;

	case CP_YCoord:
		snprintf(buf, 512, "Enter Y Coord. Max %i (-1 to cancel)", c_num_tiles);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8);
		break;

	case CP_Lift:
		font->paint_text_fixedwidth(ibuf, "Enter Lift. (-1 to cancel)", offsetx, promptmes, 8);
		break;

	case CP_GFlagNum: {
		char buf[50];
		snprintf(buf, 50, "Enter Global Flag 0-%d. (-1 to cancel)", c_last_gflag);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8);
		break;
	}

	case CP_NFlagNum:
		font->paint_text_fixedwidth(ibuf, "Enter NPC Flag 0-63. (-1 to cancel)", offsetx, promptmes, 8);
		break;

	case CP_TempNum:
		font->paint_text_fixedwidth(ibuf, "Enter Temperature 0-63. (-1 to cancel)", offsetx, promptmes, 8);
		break;

	case CP_NLatitude:
		font->paint_text_fixedwidth(ibuf, "Enter Latitude. Max 113 (-1 to cancel)", offsetx, promptmes, 8);
		break;

	case CP_SLatitude:
		font->paint_text_fixedwidth(ibuf, "Enter Latitude. Max 193 (-1 to cancel)", offsetx, promptmes, 8);
		break;

	case CP_WLongitude:
		font->paint_text_fixedwidth(ibuf, "Enter Longitude. Max 93 (-1 to cancel)", offsetx, promptmes, 8);
		break;

	case CP_ELongitude:
		font->paint_text_fixedwidth(ibuf, "Enter Longitude. Max 213 (-1 to cancel)", offsetx, promptmes, 8);
		break;

	case CP_Name:
		font->paint_text_fixedwidth(ibuf, "Enter a new Name...", offsetx, promptmes, 8);
		break;

	case CP_NorthSouth:
		font->paint_text_fixedwidth(ibuf, "Latitude [N]orth or [S]outh?", offsetx, promptmes, 8);
		break;

	case CP_WestEast:
		font->paint_text_fixedwidth(ibuf, "Longitude [W]est or [E]ast?", offsetx, promptmes, 8);
		break;

	case CP_HexXCoord:
		snprintf(buf, 512, "Enter X Coord. Max %04x (-1 to cancel)", c_num_tiles);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8);
		break;

	case CP_HexYCoord:
		snprintf(buf, 512, "Enter Y Coord. Max %04x (-1 to cancel)", c_num_tiles);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8);
		break;

	}
}

static int SDLScanCodeToInt(SDL_Keycode sym) {
	switch (sym) {
	case SDLK_KP_0:
		return '0';
	case SDLK_KP_1:
		return '1';
	case SDLK_KP_2:
		return '2';
	case SDLK_KP_3:
		return '3';
	case SDLK_KP_4:
		return '4';
	case SDLK_KP_5:
		return '5';
	case SDLK_KP_6:
		return '6';
	case SDLK_KP_7:
		return '7';
	case SDLK_KP_8:
		return '8';
	case SDLK_KP_9:
		return '9';
	default:
		return -1;
	}
}

bool CheatScreen::SharedInput(char *input, int len, int &command, Cheat_Prompt &mode, bool &activate) {
	SDL_Event event;

	while (true) {
		Delay();
		while (SDL_PollEvent(&event)) {
			// Touch on the cheat screen will bring up the keyboard
			if (event.type == SDL_MOUSEBUTTONDOWN) {
				if (SDL_IsTextInputActive())
					SDL_StopTextInput();
				else
					SDL_StartTextInput();
			}

			if (event.type != SDL_KEYDOWN)
				continue;
			SDL_Keysym &key = event.key.keysym;

			if ((key.sym == SDLK_s) && (key.mod & KMOD_ALT) && (key.mod & KMOD_CTRL)) {
				make_screenshot(true);
				return false;
			}

			if (mode == CP_NorthSouth) {
				if (!input[0] && (key.sym == 'n' || key.sym == 's')) {
					input[0] = key.sym;
					activate = true;
				}
			} else if (mode == CP_WestEast) {
				if (!input[0] && (key.sym == 'w' || key.sym == 'e')) {
					input[0] = key.sym;
					activate = true;
				}
			} else if (mode >= CP_HexXCoord) { // Want hex input
				// Activate (if possible)
				if (key.sym == SDLK_RETURN || key.sym == SDLK_KP_ENTER) {
					activate = true;
				} else if ((key.sym == '-' || key.sym == SDLK_KP_MINUS) && !input[0]) {
					input[0] = '-';
				} else if (std::isxdigit(key.sym)) {
					int curlen = std::strlen(input);
					if (curlen < (len - 1)) {
						input[curlen] = std::tolower(key.sym);
						input[curlen + 1] = 0;
					}
				} else if ((key.sym >= SDLK_KP_1 && key.sym <= SDLK_KP_9) || key.sym == SDLK_KP_0) {
					int curlen = std::strlen(input);
					if (curlen < (len - 1)) {
						int sym = SDLScanCodeToInt(key.sym);
						input[curlen] = sym;
						input[curlen + 1] = 0;
					}
				} else if (key.sym == SDLK_BACKSPACE) {
					int curlen = std::strlen(input);
					if (curlen) input[curlen - 1] = 0;
				}
			} else if (mode >= CP_Name) {      // Want Text input (len chars)
				if (key.sym == SDLK_RETURN || key.sym == SDLK_KP_ENTER) {
					activate = true;
				} else if (std::isalnum(key.sym) || key.sym == ' ') {
					int curlen = std::strlen(input);
					char chr = key.sym;
					if (key.mod & KMOD_SHIFT) {
						chr = static_cast<char>(std::toupper(static_cast<unsigned char>(chr)));
					}
					if (curlen < (len - 1)) {
						input[curlen] = chr;
						input[curlen + 1] = 0;
					}
				} else if (key.sym == SDLK_BACKSPACE) {
					int curlen = std::strlen(input);
					if (curlen) input[curlen - 1] = 0;
				}
			} else if (mode >= CP_ChooseNPC) { // Need to grab numerical input
				// Browse shape
				if (mode == CP_Shape && !input[0] && key.sym == 'b') {
					cheat.shape_browser();
					input[0] = 'b';
					activate = true;
				}

				// Activate (if possible)
				if (key.sym == SDLK_RETURN || key.sym == SDLK_KP_ENTER) {
					activate = true;
				} else if ((key.sym == '-' || key.sym == SDLK_KP_MINUS) && !input[0]) {
					input[0] = '-';
				} else if (std::isdigit(key.sym)) {
					int curlen = std::strlen(input);
					if (curlen < (len - 1)) {
						input[curlen] = key.sym;
						input[curlen + 1] = 0;
					}
				} else if ((key.sym >= SDLK_KP_1 && key.sym <= SDLK_KP_9) || key.sym == SDLK_KP_0) {
					int curlen = std::strlen(input);
					if (curlen < (len - 1)) {
						int sym = SDLScanCodeToInt(key.sym);
						input[curlen] = sym;
						input[curlen + 1] = 0;
					}
				} else if (key.sym == SDLK_BACKSPACE) {
					int curlen = std::strlen(input);
					if (curlen) input[curlen - 1] = 0;
				}
			} else if (mode) {      // Just want a key pressed
				mode = CP_Command;
				for (int i = 0; i < len; i++) input[i] = 0;
				command = 0;
			} else {            // Need the key pressed
				command = key.sym;
				return true;
			}
			return false;
		}
		gwin->paint_dirty();
	}
	return false;
}


//
// Normal
//

void CheatScreen::NormalLoop() {
	bool looping = true;

	// This is for the prompt message
	Cheat_Prompt mode = CP_Command;

	// This is the command
	char input[5] = { 0, 0, 0, 0, 0 };
	int command;
	bool activate = false;

	while (looping) {
		gwin->clear_screen();

		// First the display
		NormalDisplay();

		// Now the Menu Column
		NormalMenu();

		// Finally the Prompt...
		SharedPrompt(input, mode);

		// Draw it!
		gwin->get_win()->show();

		// Check to see if we need to change menus
		if (activate) {
			NormalActivate(input, command, mode);
			activate = false;
			continue;
		}

		if (SharedInput(input, 5, command, mode, activate))
			looping = NormalCheck(input, command, mode, activate);
	}
}

void CheatScreen::NormalDisplay() {
	char    buf[512];
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx = 15;
	const int offsety1 = 108;
	const int offsety2 = 54;
	const int offsety3 = 0;
#else
	const int offsetx = 0;
	const int offsety1 = 0;
	const int offsety2 = 0;
	const int offsety3 = 45;
#endif
	int curmap = gwin->get_map()->get_num(); 
	Tile_coord t = gwin->get_main_actor()->get_tile();

	font->paint_text_fixedwidth(ibuf, "Colourless' Advanced Option Cheat Screen", 0, offsety1, 8);

	if (Game::get_game_type() == BLACK_GATE)
		snprintf(buf, 512, "Running \"Ultima 7: The Black Gate\"");
	else if (Game::get_game_type() == SERPENT_ISLE)
		snprintf(buf, 512, "Running \"Ultima 7: Part 2: Serpent Isle\"");
	else
		snprintf(buf, 512, "Running Unknown Game Type %i", Game::get_game_type());

	font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 18, 8);

	snprintf(buf, 512, "Exult Version %s", VERSION);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 27, 8);


	snprintf(buf, 512, "Current time: %i:%02i %s  Day: %i",
	         ((clock->get_hour() + 11) % 12) + 1,
	         clock->get_minute(),
	         clock->get_hour() < 12 ? "AM" : "PM",
	         clock->get_day());
	font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety3, 8);

	int longi = ((t.tx - 0x3A5) / 10);
	int lati = ((t.ty - 0x46E) / 10);
	snprintf(buf, 512, "Coordinates %d %s %d %s, Map #%d",
		abs(lati), (lati < 0 ? "North" : "South"),
		abs(longi), (longi < 0 ? "West" : "East"), curmap);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 63 - offsety2, 8);

	snprintf(buf, 512, "Coords in hex (%04x, %04x, %02x)",
	         t.tx, t.ty, t.tz);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 72 - offsety2, 8);

	snprintf(buf, 512, "Coords in dec (%04i, %04i, %02i)",
	         t.tx, t.ty, t.tz);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 81 - offsety2, 8);
}

void CheatScreen::NormalMenu() {
	char    buf[512];
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx = 15;
	const int offsety1 = 73;
	const int offsety2 = 55;
	const int offsetx1 = 160;
	const int offsety4 = 36;
	const int offsety5 = 72;
#else
	const int offsetx = 0;
	const int offsety1 = 0;
	const int offsety2 = 0;
	const int offsetx1 = 0;
	const int offsety4 = maxy - 45;
	const int offsety5 = maxy - 36;
#endif

	// Left Column

	// Use
#if !defined(__IPHONEOS__) && !defined(ANDROID)
	// Paperdolls can be toggled in the gumps, no need here for iOS
	Shape_manager *sman = Shape_manager::get_instance();
	if (sman->can_use_paperdolls() && sman->are_paperdolls_enabled())
		snprintf(buf, 512, "[P]aperdolls..: Yes");
	else
		snprintf(buf, 512, "[P]aperdolls..:  No");
	font->paint_text_fixedwidth(ibuf, buf, 0, maxy - 99, 8);
#endif

	// GodMode
	snprintf(buf, 512, "[G]od Mode....: %3s", cheat.in_god_mode() ? "On" : "Off");
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 90, 8);

	// Archwizzard Mode
	snprintf(buf, 512, "[W]izard Mode.: %3s", cheat.in_wizard_mode() ? "On" : "Off");
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 81, 8);

	// Infravision
	snprintf(buf, 512, "[I]nfravision.: %3s", cheat.in_infravision() ? "On" : "Off");
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 72, 8);

	// Hackmover
	snprintf(buf, 512, "[H]ack Mover..: %3s", cheat.in_hack_mover() ? "Yes" : "No");
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 63, 8);

	// Eggs
	snprintf(buf, 512, "[E]ggs Visible: %3s", gwin->paint_eggs ? "Yes" : "No");
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 54, 8);

	// Set Time
	font->paint_text_fixedwidth(ibuf, "[S]et Time", offsetx + offsetx1, offsety4, 8);

#if !defined(__IPHONEOS__) && !defined(ANDROID)
	// for iOS taking the liberty of leaving that out
	// Time Rate
	snprintf(buf, 512, "[+-] Time Rate: %3i", clock->get_time_rate());
	font->paint_text_fixedwidth(ibuf, buf, 0, maxy - 36, 8);
#endif


	// Right Column

	// NPC Tool
	font->paint_text_fixedwidth(ibuf, "[N]PC Tool", offsetx + 160, maxy - offsety2 - 99, 8);

	// Global Flag Modify
	font->paint_text_fixedwidth(ibuf, "[F]lag Modifier", offsetx + 160, maxy - offsety2 - 90, 8);

	// Teleport
	font->paint_text_fixedwidth(ibuf, "[T]eleport", offsetx + 160, maxy - offsety2 - 81, 8);

	// eXit
	font->paint_text_fixedwidth(ibuf, "[X]it", offsetx + 160, offsety5, 8);
}

void CheatScreen::NormalActivate(char *input, int &command, Cheat_Prompt &mode) {
	int npc = std::atoi(input);
	Shape_manager *sman = Shape_manager::get_instance();

	mode = CP_Command;

	switch (command) {
		// God Mode
	case 'g':
		cheat.toggle_god();
		break;

		// Wizard Mode
	case 'w':
		cheat.toggle_wizard();
		break;

		// Infravision
	case 'i':
		cheat.toggle_infravision();
		pal.apply();
		break;

		// Eggs
	case 'e':
		cheat.toggle_eggs();
		break;

		// Hack mover
	case 'h':
		cheat.toggle_hack_mover();
		break;

		// Set Time
	case 's':
		mode = TimeSetLoop();
		break;

		// - Time Rate
	case '-':
		if (clock->get_time_rate() > 0)
			clock->set_time_rate(clock->get_time_rate() - 1);
		break;

		// + Time Rate
	case '+':
		if (clock->get_time_rate() < 20)
			clock->set_time_rate(clock->get_time_rate() + 1);
		break;

		// Teleport
	case 't':
		TeleportLoop();
		break;

		// NPC Tool
	case 'n':
		if (npc < -1 || (npc >= 356 && npc <= 359)) mode = CP_InvalidNPC;
		else if (npc == -1) mode = CP_Canceled;
		else if (!input[0]) NPCLoop(-1);
		else mode = NPCLoop(npc);
		break;

		// Global Flag Editor
	case 'f':
		if (npc < -1) mode = CP_InvalidValue;
		else if (npc > c_last_gflag) mode = CP_InvalidValue;
		else if (npc == -1 || !input[0]) mode = CP_Canceled;
		else mode = GlobalFlagLoop(npc);
		break;

		// Paperdolls
	case 'p':
		if ((Game::get_game_type() == BLACK_GATE
		        || Game::get_game_type() == EXULT_DEVEL_GAME)
		        && sman->can_use_paperdolls()) {
			sman->set_paperdoll_status(!sman->are_paperdolls_enabled());
			config->set("config/gameplay/bg_paperdolls",
			            sman->are_paperdolls_enabled() ? "yes" : "no", true);
		}
		break;

	default:
		break;
	}

	input[0] = 0;
	input[1] = 0;
	input[2] = 0;
	input[3] = 0;
	command = 0;
}

// Checks the input
bool CheatScreen::NormalCheck(char *input, int &command, Cheat_Prompt &mode, bool &activate) {
	switch (command) {
		// Simple commands
	case 't':   // Teleport
	case 'g':   // God Mode
	case 'w':   // Wizard
	case 'i':   // iNfravision
	case 's':   // Set Time
	case 'e':   // Eggs
	case 'h':   // Hack Mover
	case 'c':   // Create Item
	case 'p':   // Paperdolls
		input[0] = command;
		activate = true;
		break;

		// - Time
	case SDLK_KP_MINUS:
	case '-':
		command = '-';
		input[0] = command;
		activate = true;
		break;

		// + Time
	case SDLK_KP_PLUS:
	case '=':
		command = '+';
		input[0] = command;
		activate = true;
		break;

		// NPC Tool
	case 'n':
		mode = CP_ChooseNPC;
		break;

		// Global Flag Editor
	case 'f':
		mode = CP_GFlagNum;
		break;

		// X and Escape leave
	case SDLK_ESCAPE:
	case 'x':
		input[0] = command;
		return false;

	default:
		mode = CP_InvalidCom;
		input[0] = command;
		command = 0;
		break;
	}

	return true;
}

//
// Activity Display
//

void CheatScreen::ActivityDisplay() {
	char    buf[512];
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsety1 = 99;
#else
	const int offsety1 = 0;
#endif

	for (int i = 0; i < 11; i++) {
		snprintf(buf, 512, "%2i %s", i, schedules[i]);
		font->paint_text_fixedwidth(ibuf, buf, 0, i * 9 + offsety1, 8);

		snprintf(buf, 512, "%2i %s", i + 11, schedules[i + 11]);
		font->paint_text_fixedwidth(ibuf, buf, 112, i * 9 + offsety1, 8);

		if (i != 10) {
			snprintf(buf, 512, "%2i %s", i + 22, schedules[i + 22]);
			font->paint_text_fixedwidth(ibuf, buf, 224, i * 9 + offsety1, 8);
		}
	}

}


//
// TimeSet
//

CheatScreen::Cheat_Prompt CheatScreen::TimeSetLoop() {
	// This is for the prompt message
	Cheat_Prompt mode = CP_Day;

	// This is the command
	char input[5] = { 0, 0, 0, 0, 0 };
	int command;
	bool activate = false;

	int day = 0;
	int hour = 0;

	while (true) {
		gwin->clear_screen();

		// First the display
		NormalDisplay();

		// Now the Menu Column
		NormalMenu();

		// Finally the Prompt...
		SharedPrompt(input, mode);

		// Draw it!
		gwin->get_win()->show();

		// Check to see if we need to change menus
		if (activate) {
			int val = std::atoi(input);

			if (val == -1) {
				return CP_Canceled;
			} else if (val < -1) {
				return CP_InvalidTime;
			} else if (mode == CP_Day) {
				day = val;
				mode = CP_Hour;
			} else if (val > 59) {
				return CP_InvalidTime;
			} else if (mode == CP_Minute) {
				clock->reset();
				clock->set_day(day);
				clock->set_hour(hour);
				clock->set_minute(val);
				break;
			} else if (val > 23) {
				return CP_InvalidTime;
			} else if (mode == CP_Hour) {
				hour = val;
				mode = CP_Minute;
			}

			activate = false;
			input[0] = 0;
			input[1] = 0;
			input[2] = 0;
			input[3] = 0;
			command = 0;
			continue;
		}

		SharedInput(input, 5, command, mode, activate);
	}

	return CP_ClockSet;
}


//
// Global Flags
//

CheatScreen::Cheat_Prompt CheatScreen::GlobalFlagLoop(int num) {
	bool looping = true;
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx = 15;
	const int offsety1 = 83;
	const int offsety2 = 72;
#else
	const int offsetx = 0;
	const int offsety1 = 0;
	const int offsety2 = maxy - 36;
#endif

	// This is for the prompt message
	Cheat_Prompt mode = CP_Command;

	// This is the command
	char input[17];
	int i;
	int command;
	bool activate = false;
	char    buf[512];

	for (i = 0; i < 17; i++) input[i] = 0;

	Usecode_machine *usecode = Game_window::get_instance()->get_usecode();

	while (looping) {
		gwin->clear_screen();

#if defined(__IPHONEOS__) || defined(ANDROID)
		// on iOS we want lean and mean, so begone NormalDisplay
		font->paint_text_fixedwidth(ibuf, "Global Flags", 15, 0, 8);
#else
		NormalDisplay();
#endif

		// First the info
		snprintf(buf, 512, "Global Flag %i", num);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 99, 8);

		snprintf(buf, 512, "Flag is %s", usecode->get_global_flag(num) ? "SET" : "UNSET");
		font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 90, 8);

		// Now the Menu Column
		if (!usecode->get_global_flag(num)) font->paint_text_fixedwidth(ibuf, "[S]et Flag", offsetx + 160, maxy - offsety1 - 99, 8);
		else font->paint_text_fixedwidth(ibuf, "[U]nset Flag", offsetx + 160, maxy - offsety1 - 99, 8);

		// Change Flag

		font->paint_text_fixedwidth(ibuf, "[*] Change Flag", offsetx, maxy - offsety1 - 72, 8);
		if (num > 0 && num < c_last_gflag) font->paint_text_fixedwidth(ibuf, "[+-] Scroll Flags", offsetx, maxy - offsety1 - 63, 8);
		else if (num == 0) font->paint_text_fixedwidth(ibuf, "[+] Scroll Flags", offsetx, maxy - offsety1 - 63, 8);
		else font->paint_text_fixedwidth(ibuf, "[-] Scroll Flags", offsetx, maxy - offsety1 - 63, 8);
		font->paint_text_fixedwidth(ibuf, "[X]it", offsetx, offsety2, 8);

		// Finally the Prompt...
		SharedPrompt(input, mode);

		// Draw it!
		gwin->get_win()->show();

		// Check to see if we need to change menus
		if (activate) {
			mode = CP_Command;
			if (command == '-') {   // Decrement
				num--;
				if (num < 0) {
					num = 0;
				}
			} else if (command == '+') { // Increment
				num++;
				if (num > c_last_gflag) {
					num = c_last_gflag;
				}
			} else if (command == '*') { // Change Flag
				i = std::atoi(input);
				if (i < -1 || i > c_last_gflag) {
					mode = CP_InvalidValue;
				} else if (i == -1) {
					mode = CP_Canceled;
				} else if (input[0]) {
					num = i;
				}
			} else if (command == 's') { // Set
				usecode->set_global_flag(num, 1);
			} else if (command == 'u') { // Unset
				usecode->set_global_flag(num, 0);
			}

			for (i = 0; i < 17; i++) {
				input[i] = 0;
			}
			command = 0;
			activate = false;
			continue;
		}

		if (SharedInput(input, 17, command, mode, activate)) {
			switch (command) {
				// Simple commands
			case 's':   // Set Flag
			case 'u':   // Unset flag
				input[0] = command;
				activate = true;
				break;

				// Decrement
			case SDLK_KP_MINUS:
			case '-':
				command = '-';
				input[0] = command;
				activate = true;
				break;

				// Increment
			case SDLK_KP_PLUS:
			case '=':
				command = '+';
				input[0] = command;
				activate = true;
				break;

				// * Change Flag
			case SDLK_KP_MULTIPLY:
			case '8':
				command = '*';
				input[0] = 0;
				mode = CP_GFlagNum;
				break;

				// X and Escape leave
			case SDLK_ESCAPE:
			case 'x':
				input[0] = command;
				looping = false;
				break;

			default:
				mode = CP_InvalidCom;
				input[0] = command;
				command = 0;
				break;
			}
		}
	}
	return CP_Command;
}

//
// NPCs
//

CheatScreen::Cheat_Prompt CheatScreen::NPCLoop(int num) {
	Actor *actor;

	bool looping = true;

	// This is for the prompt message
	Cheat_Prompt mode = CP_Command;

	// This is the command
	char input[17];
	int i;
	int command;
	bool activate = false;

	for (i = 0; i < 17; i++) input[i] = 0;

	while (looping) {
		if (num == -1) actor = grabbed;
		else actor = gwin->get_npc(num);
		grabbed = actor;
		if (actor) num = actor->get_npc_num();

		gwin->clear_screen();

		// First the display
		NPCDisplay(actor, num);

		// Now the Menu Column
		NPCMenu(actor, num);

		// Finally the Prompt...
		SharedPrompt(input, mode);

		// Draw it!
		gwin->get_win()->show();

		// Check to see if we need to change menus
		if (activate) {
			NPCActivate(input, command, mode, actor, num);
			activate = false;
			continue;
		}

		if (SharedInput(input, 17, command, mode, activate))
			looping = NPCCheck(input, command, mode, activate, actor, num);
	}
	return CP_Command;
}

void CheatScreen::NPCDisplay(Actor *actor, int &num) {
	char    buf[512];
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx = 15;
	const int offsety1 = 73;
#else
	const int offsetx = 0;
	const int offsety1 = 0;
#endif
	if (actor) {
		Tile_coord t = actor->get_tile();

		// Paint the actors shape
		Shape_frame *shape = actor->get_shape();
		if (shape) actor->paint_shape(shape->get_xright() + 240, shape->get_yabove());

		// Now the info
		std::string namestr = actor->get_npc_name();
		snprintf(buf, 512, "NPC %i - %s", num, namestr.c_str());
		font->paint_text_fixedwidth(ibuf, buf, offsetx, 0, 8);

		snprintf(buf, 512, "Loc (%04i, %04i, %02i)",
		         t.tx, t.ty, t.tz);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, 9, 8);

		snprintf(buf, 512, "Shape %04i:%02i  %s", actor->get_shapenum(), actor->get_framenum(), actor->get_flag(Obj_flags::met) ? "Met" : "Not Met");
		font->paint_text_fixedwidth(ibuf, buf, offsetx, 18, 8);

		snprintf(buf, 512, "Current Activity: %2i - %s", actor->get_schedule_type(), schedules[actor->get_schedule_type()]);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 36, 8);

		snprintf(buf, 512, "Experience: %i", actor->get_property(Actor::exp));
		font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 45, 8);

		snprintf(buf, 512, "Level: %i", actor->get_level());
		font->paint_text_fixedwidth(ibuf, buf, offsetx + 144, offsety1 + 45, 8);

		snprintf(buf, 512, "Training: %2i  Health: %2i", actor->get_property(Actor::training), actor->get_property(Actor::health));
		font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 54, 8);

		if (num != -1) {
			int ucitemnum = 0x10000 - num;
			if (!num) ucitemnum = 0xfe9c;
			snprintf(buf, 512, "Usecode item %4x function %x", ucitemnum, actor->get_usecode());
			font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 63, 8);
		} else {
			snprintf(buf, 512, "Usecode function %x", actor->get_usecode());
			font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 63, 8);
		}

		if (actor->get_flag(Obj_flags::charmed))
			snprintf(buf, 512, "Alignment: %s (orig: %s)", alignments[actor->get_effective_alignment()], alignments[actor->get_alignment()]);
		else
			snprintf(buf, 512, "Alignment: %s", alignments[actor->get_alignment()]);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 72, 8);

		if (actor->get_polymorph() != -1) {
			snprintf(buf, 512, "Polymorphed from %04i", actor->get_polymorph());
			font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 81, 8);
		}
	} else {
		snprintf(buf, 512, "NPC %i - Invalid NPC!", num);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, 0, 8);
	}
}

void CheatScreen::NPCMenu(Actor *actor, int &num) {
	ignore_unused_variable_warning(num);
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx = 15;
	const int offsety1 = 74;
	const int offsetx2 = 15;
	const int offsety2 = 72;
	const int offsetx3 = 175;
	const int offsety3 = 63;
	const int offsety4 = 72;
#else
	const int offsetx = 0;
	const int offsety1 = 0;
	const int offsetx2 = 0;
	const int offsety2 = maxy - 36;
	const int offsetx3 = offsetx + 160;
	const int offsety3 = maxy - 45;
	const int offsety4 = maxy - 36;
#endif
	// Left Column

	if (actor) {
		// Business Activity
		font->paint_text_fixedwidth(ibuf, "[B]usiness Activity", offsetx, maxy - offsety1 - 99, 8);

		// Change Shape
		font->paint_text_fixedwidth(ibuf, "[C]hange Shape", offsetx, maxy - offsety1 - 90, 8);

		// XP
		font->paint_text_fixedwidth(ibuf, "[E]xperience", offsetx, maxy - offsety1 - 81, 8);

		// NPC Flags
		font->paint_text_fixedwidth(ibuf, "[N]pc Flags", offsetx, maxy - offsety1 - 72, 8);

		// Name
		font->paint_text_fixedwidth(ibuf, "[1] Name", offsetx, maxy - offsety1 - 63, 8);
	}

	// eXit
	font->paint_text_fixedwidth(ibuf, "[X]it", offsetx2, offsety2, 8);

	// Right Column

	if (actor) {
		// Stats
		font->paint_text_fixedwidth(ibuf, "[S]tats", offsetx + 160, maxy - offsety1 - 99, 8);

		// Training Points
		font->paint_text_fixedwidth(ibuf, "[2] Training Points", offsetx + 160, maxy - offsety1 - 90, 8);

		// Teleport
		font->paint_text_fixedwidth(ibuf, "[T]eleport", offsetx + 160, maxy - offsety1 - 81, 8);

		// Change NPC
		font->paint_text_fixedwidth(ibuf, "[*] Change NPC", offsetx3, offsety3, 8);
	}

	// Change NPC
	font->paint_text_fixedwidth(ibuf, "[+-] Scroll NPCs", offsetx3, offsety4, 8);
}

void CheatScreen::NPCActivate(char *input, int &command, Cheat_Prompt &mode, Actor *actor, int &num) {
	int i = std::atoi(input);
	int nshapes =
	    Shape_manager::get_instance()->get_shapes().get_num_shapes();

	mode = CP_Command;

	if (command == '-') {
		num--;
		if (num < 0)
			num = 0;
		else if (num >= 356 && num <= 359)
			num = 355;
	} else if (command == '+') {
		num++;
		if (num >= 356 && num <= 359)
			num = 360;
	} else if (command == '*') { // Change NPC
		if (i < -1 || (i >= 356 && i <= 359)) mode = CP_InvalidNPC;
		else if (i == -1) mode = CP_Canceled;
		else if (input[0]) num = i;
	} else if (actor) switch (command) {
		case 'b':   // Business
			BusinessLoop(actor);
			break;

		case 'n':   // Npc flags
			FlagLoop(actor);
			break;

		case 's':   // stats
			StatLoop(actor);
			break;

		case 't':   // Teleport
			Game_window::get_instance()->teleport_party(actor->get_tile(),
			        false, actor->get_map_num());
			break;

		case 'e':   // Experience
			if (i < 0) mode = CP_Canceled;
			else actor->set_property(Actor::exp, i);
			break;

		case '2':   // Training Points
			if (i < 0) mode = CP_Canceled;
			else actor->set_property(Actor::training, i);
			break;

		case 'c':   // Change shape
			if (input[0] == 'b') {  // Browser
				int n;
				if (!cheat.get_browser_shape(i, n)) {
					mode = CP_WrongShapeFile;
					break;
				}
			}

			if (i == -1) mode = CP_Canceled;
			else if (i < 0) mode = CP_InvalidShape;
			else if (i >= nshapes) mode = CP_InvalidShape;
			else if (input[0] && (input[0] != '-' || input[1])) {
				if (actor->get_npc_num() != 0)
					actor->set_shape(i);
				else
					actor->set_polymorph(i);
				mode = CP_ShapeSet;
			}
			break;

		case '1':   // Name
			if (!std::strlen(input)) mode = CP_Canceled;
			else {
				actor->set_npc_name(input);
				mode = CP_NameSet;
			}
			break;

		default:
			break;
		}
	for (i = 0; i < 17; i++) input[i] = 0;
	command = 0;
}

// Checks the input
bool CheatScreen::NPCCheck(char *input, int &command, Cheat_Prompt &mode, bool &activate, Actor *actor, int &num) {
	ignore_unused_variable_warning(num);
	switch (command) {
		// Simple commands
	case 'a':   // Attack mode
	case 'b':   // BUsiness
	case 'n':   // Npc flags
	case 'd':   // pop weapon
	case 's':   // stats
	case 'z':   // Target
	case 't':   // Teleport
		input[0] = command;
		if (!actor) mode = CP_InvalidCom;
		else activate = true;
		break;

		// Value entries
	case 'e':   // Experience
	case '2':   // Training Points
		if (!actor) mode = CP_InvalidCom;
		else mode = CP_EnterValue;
		break;

		// Change shape
	case 'c':
		if (!actor) mode = CP_InvalidCom;
		else mode = CP_Shape;
		break;

		// Name
	case '1':
		if (!actor) mode = CP_InvalidCom;
		else mode = CP_Name;
		break;

		// - NPC
	case SDLK_KP_MINUS:
	case '-':
		command = '-';
		input[0] = command;
		activate = true;
		break;

		// + NPC
	case SDLK_KP_PLUS:
	case '=':
		command = '+';
		input[0] = command;
		activate = true;
		break;

		// * Change NPC
	case SDLK_KP_MULTIPLY:
	case '8':
		command = '*';
		input[0] = 0;
		mode = CP_ChooseNPC;
		break;

		// X and Escape leave
	case SDLK_ESCAPE:
	case 'x':
		input[0] = command;
		return false;

	default:
		mode = CP_InvalidCom;
		input[0] = command;
		command = 0;
		break;
	}

	return true;
}

//
// NPC Flags
//

void CheatScreen::FlagLoop(Actor *actor) {
#if !defined(__IPHONEOS__) && !defined(ANDROID)
	int num = actor->get_npc_num();
#endif
	bool looping = true;

	// This is for the prompt message
	Cheat_Prompt mode = CP_Command;

	// This is the command
	char input[17];
	int i;
	int command;
	bool activate = false;

	for (i = 0; i < 17; i++) input[i] = 0;

	while (looping) {
		gwin->clear_screen();

#if !defined(__IPHONEOS__) && !defined(ANDROID)
		// First the display
		NPCDisplay(actor, num);
#endif

		// Now the Menu Column
		FlagMenu(actor);

		// Finally the Prompt...
		SharedPrompt(input, mode);

		// Draw it!
		gwin->get_win()->show();

		// Check to see if we need to change menus
		if (activate) {
			FlagActivate(input, command, mode, actor);
			activate = false;
			continue;
		}

		if (SharedInput(input, 17, command, mode, activate))
			looping = FlagCheck(input, command, mode, activate, actor);
	}
}

void CheatScreen::FlagMenu(Actor *actor) {
	char    buf[512];
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx = 10;
	const int offsetx1 = 6;
	const int offsety1 = 92;
#else
	const int offsetx = 0;
	const int offsetx1 = 0;
	const int offsety1 = 0;
#endif

	// Left Column

	// Asleep
	snprintf(buf, 512, "[A] Asleep.%c", actor->get_flag(Obj_flags::asleep) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 108, 8);

	// Charmed
	snprintf(buf, 512, "[B] Charmd.%c", actor->get_flag(Obj_flags::charmed) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 99, 8);

	// Cursed
	snprintf(buf, 512, "[C] Cursed.%c", actor->get_flag(Obj_flags::cursed) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 90, 8);

	// Paralyzed
	snprintf(buf, 512, "[D] Prlyzd.%c", actor->get_flag(Obj_flags::paralyzed) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 81, 8);

	// Poisoned
	snprintf(buf, 512, "[E] Poisnd.%c", actor->get_flag(Obj_flags::poisoned) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 72, 8);

	// Protected
	snprintf(buf, 512, "[F] Prtctd.%c", actor->get_flag(Obj_flags::protection) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 63, 8);

	// Advanced Editor
	font->paint_text_fixedwidth(ibuf, "[*] Advanced", offsetx, maxy - offsety1 - 45, 8);

	// Exit
	font->paint_text_fixedwidth(ibuf, "[X]it", offsetx, maxy - offsety1 - 36, 8);


	// Center Column

	// Party
	snprintf(buf, 512, "[I] Party..%c", actor->get_flag(Obj_flags::in_party) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 104, maxy - offsety1 - 108, 8);

	// Invisible
	snprintf(buf, 512, "[J] Invsbl.%c", actor->get_flag(Obj_flags::invisible) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 104, maxy - offsety1 - 99, 8);

	// Fly
	snprintf(buf, 512, "[K] Fly....%c", actor->get_type_flag(Actor::tf_fly) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 104, maxy - offsety1 - 90, 8);

	// Walk
	snprintf(buf, 512, "[L] Walk...%c", actor->get_type_flag(Actor::tf_walk) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 104, maxy - offsety1 - 81, 8);

	// Swim
	snprintf(buf, 512, "[M] Swim...%c", actor->get_type_flag(Actor::tf_swim) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 104, maxy - offsety1 - 72, 8);

	// Ethereal
	snprintf(buf, 512, "[N] Ethrel.%c", actor->get_type_flag(Actor::tf_ethereal) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 104, maxy - offsety1 - 63, 8);

	// Protectee
	snprintf(buf, 512, "[O] Prtcee.%c", '?');
	font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 104, maxy - offsety1 - 54, 8);

	// Conjured
	snprintf(buf, 512, "[P] Conjrd.%c", actor->get_type_flag(Actor::tf_conjured) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 104, maxy - offsety1 - 45, 8);

	// Tournament (Original is SI only -- allowing for BG in Exult)
	snprintf(buf, 512, "[3] Tourna.%c", actor->get_flag(
	             Obj_flags::tournament) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 104, maxy - offsety1 - 36, 8);

	// Naked (AV ONLY)
	if (!actor->get_npc_num()) {
		snprintf(buf, 512, "[7] Naked..%c", actor->get_flag(Obj_flags::naked) ? 'Y' : 'N');
		font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 104, maxy - offsety1 - 27, 8);
	}


	// Right Column

	// Summoned
	snprintf(buf, 512, "[Q] Summnd.%c", actor->get_type_flag(Actor::tf_summonned) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 208, maxy - offsety1 - 108, 8);

	// Bleeding
	snprintf(buf, 512, "[R] Bleedn.%c", actor->get_type_flag(Actor::tf_bleeding) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 208, maxy - offsety1 - 99, 8);

	if (!actor->get_npc_num()) { // Avatar
		// Sex
		snprintf(buf, 512, "[S] Sex....%c", actor->get_type_flag(Actor::tf_sex) ? 'F' : 'M');
		font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 208, maxy - offsety1 - 90, 8);

		// Skin
		snprintf(buf, 512, "[1] Skin...%d", actor->get_skin_color());
		font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 208, maxy - offsety1 - 81, 8);

		// Read
		snprintf(buf, 512, "[4] Read...%c", actor->get_flag(Obj_flags::read) ? 'Y' : 'N');
		font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 208, maxy - offsety1 - 72, 8);
	} else { // Not Avatar
		// Met
		snprintf(buf, 512, "[T] Met....%c", actor->get_flag(Obj_flags::met) ? 'Y' : 'N');
		font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 208, maxy - offsety1 - 90, 8);

		// NoCast
		snprintf(buf, 512, "[U] NoCast.%c", actor->get_flag(
		             Obj_flags::no_spell_casting) ? 'Y' : 'N');
		font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 208, maxy - offsety1 - 81, 8);

		// ID
		snprintf(buf, 512, "[V] ID#:%02i", actor->get_ident());
		font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 208, maxy - offsety1 - 72, 8);
	}

	// Freeze
	snprintf(buf, 512, "[W] Freeze.%c", actor->get_flag(
	             Obj_flags::freeze) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 208, maxy - offsety1 - 63, 8);

	// Party
	if (actor->is_in_party()) {
		// Temp
		snprintf(buf, 512, "[Y] Temp: %02i",
		         actor->get_temperature());
		font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 208, maxy - offsety1 - 54, 8);

		// Conjured
		snprintf(buf, 512, "Warmth: %04i",
		         actor->figure_warmth());
		font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 208, maxy - offsety1 - 45, 8);
	}

	// Polymorph
	snprintf(buf, 512, "[2] Polymo.%c", actor->get_flag(Obj_flags::polymorph) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 208, maxy - offsety1 - 36, 8);

	// Patra (AV SI ONLY)
	if (!actor->get_npc_num()) {
		snprintf(buf, 512, "[5] Petra..%c", actor->get_flag(Obj_flags::petra) ? 'Y' : 'N');
		font->paint_text_fixedwidth(ibuf, buf, offsetx1 + 208, maxy - offsety1 - 27, 8);
	}

}

void CheatScreen::FlagActivate(char *input, int &command, Cheat_Prompt &mode, Actor *actor) {
	int i = std::atoi(input);
	int nshapes =
	    Shape_manager::get_instance()->get_shapes().get_num_shapes();

	mode = CP_Command;
	switch (command) {
		// Everyone

		// Toggles
	case 'a':   // Asleep
		if (actor->get_flag(Obj_flags::asleep))
			actor->clear_flag(Obj_flags::asleep);
		else
			actor->set_flag(Obj_flags::asleep);
		break;

	case 'b':   // Charmed
		if (actor->get_flag(Obj_flags::charmed))
			actor->clear_flag(Obj_flags::charmed);
		else
			actor->set_flag(Obj_flags::charmed);
		break;

	case 'c':   // Cursed
		if (actor->get_flag(Obj_flags::cursed))
			actor->clear_flag(Obj_flags::cursed);
		else
			actor->set_flag(Obj_flags::cursed);
		break;

	case 'd':   // Paralyzed
		if (actor->get_flag(Obj_flags::paralyzed))
			actor->clear_flag(Obj_flags::paralyzed);
		else
			actor->set_flag(Obj_flags::paralyzed);
		break;

	case 'e':   // Poisoned
		if (actor->get_flag(Obj_flags::poisoned))
			actor->clear_flag(Obj_flags::poisoned);
		else
			actor->set_flag(Obj_flags::poisoned);
		break;

	case 'f':   // Protected
		if (actor->get_flag(Obj_flags::protection))
			actor->clear_flag(Obj_flags::protection);
		else
			actor->set_flag(Obj_flags::protection);
		break;

	case 'j':   // Invisible
		if (actor->get_flag(Obj_flags::invisible))
			actor->clear_flag(Obj_flags::invisible);
		else
			actor->set_flag(Obj_flags::invisible);
		pal.apply();
		break;

	case 'k':   // Fly
		if (actor->get_type_flag(Actor::tf_fly))
			actor->clear_type_flag(Actor::tf_fly);
		else
			actor->set_type_flag(Actor::tf_fly);
		break;

	case 'l':   // Walk
		if (actor->get_type_flag(Actor::tf_walk))
			actor->clear_type_flag(Actor::tf_walk);
		else
			actor->set_type_flag(Actor::tf_walk);
		break;

	case 'm':   // Swim
		if (actor->get_type_flag(Actor::tf_swim))
			actor->clear_type_flag(Actor::tf_swim);
		else
			actor->set_type_flag(Actor::tf_swim);
		break;

	case 'n':   // Ethrel
		if (actor->get_type_flag(Actor::tf_ethereal))
			actor->clear_type_flag(Actor::tf_ethereal);
		else
			actor->set_type_flag(Actor::tf_ethereal);
		break;

	case 'p':   // Conjured
		if (actor->get_type_flag(Actor::tf_conjured))
			actor->clear_type_flag(Actor::tf_conjured);
		else
			actor->set_type_flag(Actor::tf_conjured);
		break;

	case 'q':   // Summoned
		if (actor->get_type_flag(Actor::tf_summonned))
			actor->clear_type_flag(Actor::tf_summonned);
		else
			actor->set_type_flag(Actor::tf_summonned);
		break;

	case 'r':   // Bleeding
		if (actor->get_type_flag(Actor::tf_bleeding))
			actor->clear_type_flag(Actor::tf_bleeding);
		else
			actor->set_type_flag(Actor::tf_bleeding);
		break;

	case 's':   // Sex
		if (actor->get_type_flag(Actor::tf_sex))
			actor->clear_type_flag(Actor::tf_sex);
		else
			actor->set_type_flag(Actor::tf_sex);
		break;

	case '4':   // Read
		if (actor->get_flag(Obj_flags::read))
			actor->clear_flag(Obj_flags::read);
		else
			actor->set_flag(Obj_flags::read);
		break;

	case '5':   // Petra
		if (actor->get_flag(Obj_flags::petra))
			actor->clear_flag(Obj_flags::petra);
		else
			actor->set_flag(Obj_flags::petra);
		break;

	case '7':   // Naked
		if (actor->get_flag(Obj_flags::naked))
			actor->clear_flag(Obj_flags::naked);
		else
			actor->set_flag(Obj_flags::naked);
		break;

	case 't':   // Met
		if (actor->get_flag(Obj_flags::met))
			actor->clear_flag(Obj_flags::met);
		else
			actor->set_flag(Obj_flags::met);
		break;

	case 'u':   // No Cast
		if (actor->get_flag(Obj_flags::no_spell_casting))
			actor->clear_flag(Obj_flags::no_spell_casting);
		else
			actor->set_flag(Obj_flags::no_spell_casting);
		break;

	case 'z':   // Zombie
		if (actor->get_flag(Obj_flags::si_zombie))
			actor->clear_flag(Obj_flags::si_zombie);
		else
			actor->set_flag(Obj_flags::si_zombie);
		break;

	case 'w':   // Freeze
		if (actor->get_flag(Obj_flags::freeze))
			actor->clear_flag(Obj_flags::freeze);
		else
			actor->set_flag(Obj_flags::freeze);
		break;

	case 'i':   // Party
		if (actor->get_flag(Obj_flags::in_party)) {
			gwin->get_party_man()->remove_from_party(actor);
			gwin->revert_schedules(actor);
			// Just to be sure.
			actor->clear_flag(Obj_flags::in_party);
		} else if (gwin->get_party_man()->add_to_party(actor))
			actor->set_schedule_type(Schedule::follow_avatar);
		break;

	case 'o':   // Protectee
		break;

		// Value
	case 'v':   // ID
		if (i < -1) mode = CP_InvalidValue;
		else if (i > 63) mode = CP_InvalidValue;
		else if (i == -1 || !input[0]) mode = CP_Canceled;
		else actor->set_ident(i);
		break;

	case '1':   // Skin color
		actor->set_skin_color(
		    Shapeinfo_lookup::GetNextSkin(
		        actor->get_skin_color(), actor->get_type_flag(Actor::tf_sex),
		        Shape_manager::get_instance()->have_si_shapes()));
		break;

	case '3':   // Tournament
		if (actor->get_flag(Obj_flags::tournament))
			actor->clear_flag(Obj_flags::tournament);
		else
			actor->set_flag(Obj_flags::tournament);
		break;

	case 'y':   // Warmth
		if (i < -1) mode = CP_InvalidValue;
		else if (i > 63) mode = CP_InvalidValue;
		else if (i == -1 || !input[0]) mode = CP_Canceled;
		else actor->set_temperature(i);
		break;

	case '2':   // Polymorph

		// Clear polymorph
		if (actor->get_polymorph() != -1) {
			actor->set_polymorph(actor->get_polymorph());
			break;
		}

		if (input[0] == 'b') {  // Browser
			int n;
			if (!cheat.get_browser_shape(i, n)) {
				mode = CP_WrongShapeFile;
				break;
			}
		}

		if (i == -1) mode = CP_Canceled;
		else if (i < 0) mode = CP_InvalidShape;
		else if (i >= nshapes) mode = CP_InvalidShape;
		else if (input[0] && (input[0] != '-' || input[1])) {
			actor->set_polymorph(i);
			mode = CP_ShapeSet;
		}

		break;

		// Advanced Numeric Flag Editor
	case '*':
		if (i < -1) mode = CP_InvalidValue;
		else if (i > 63) mode = CP_InvalidValue;
		else if (i == -1 || !input[0]) mode = CP_Canceled;
		else mode = AdvancedFlagLoop(i, actor);
		break;

	default:
		break;
	}
	for (i = 0; i < 17; i++) input[i] = 0;
	command = 0;
}

// Checks the input
bool CheatScreen::FlagCheck(char *input, int &command, Cheat_Prompt &mode, bool &activate, Actor *actor) {
	switch (command) {
		// Everyone

		// Toggles
	case 'a':   // Asleep
	case 'b':   // Charmed
	case 'c':   // Cursed
	case 'd':   // Paralyzed
	case 'e':   // Poisoned
	case 'f':   // Protected
	case 'i':   // Party
	case 'j':   // Invisible
	case 'k':   // Fly
	case 'l':   // Walk
	case 'm':   // Swim
	case 'n':   // Ethrel
	case 'o':   // Protectee
	case 'p':   // Conjured
	case 'q':   // Summoned
	case 'r':   // Bleedin
	case 'w':   // Freeze
	case '3':   // Tournament
		activate = true;
		input[0] = command;
		break;

		// Value
	case '2':   // Polymorph
		if (actor->get_polymorph() == -1) {
			mode = CP_Shape;
			input[0] = 0;
		} else {
			activate = true;
			input[0] = command;
		}
		break;


		// Party Only

		// Value
	case 'y':   // Temp
		if (!actor->is_in_party()) command = 0;
		else  mode = CP_TempNum;
		input[0] = 0;
		break;


		// Avatar Only

		// Toggles
	case 's':   // Sex
	case '4':   // Read
		if (actor->get_npc_num()) command = 0;
		else activate = true;
		input[0] = command;
		break;

		// Toggles SI
	case '5':   // Petra
	case '7':   // Naked
		if (actor->get_npc_num()) command = 0;
		else activate = true;
		input[0] = command;
		break;

		// Value SI
	case '1':   // Skin
		if (actor->get_npc_num()) command = 0;
		else activate = true;
		input[0] = command;
		break;


		// Everyone but avatar

		// Toggles
	case 't':   // Met
	case 'u':   // No Cast
	case 'z':   // Zombie
		if (!actor->get_npc_num()) command = 0;
		else activate = true;
		input[0] = command;
		break;

		// Value
	case 'v':   // ID
		if (!actor->get_npc_num()) command = 0;
		else mode = CP_EnterValue;
		input[0] = 0;
		break;

		// NPC Flag Editor

	case SDLK_KP_MULTIPLY:
	case '8':
		command = '*';
		input[0] = 0;
		mode = CP_NFlagNum;
		break;


		// X and Escape leave
	case SDLK_ESCAPE:
	case 'x':
		input[0] = command;
		return false;

		// Unknown
	default:
		command = 0;
		break;
	}

	return true;
}

//
// Business Schedules
//

void CheatScreen::BusinessLoop(Actor *actor) {
	bool looping = true;

	// This is for the prompt message
	Cheat_Prompt mode = CP_Command;

	// This is the command
	char input[17];
	int i;
	int command;
	bool activate = false;
	int time = 0;
	int prev = 0;

	for (i = 0; i < 17; i++) input[i] = 0;

	while (looping) {
		gwin->clear_screen();

		// First the display
		if (mode == CP_Activity)
			ActivityDisplay();
		else
			BusinessDisplay(actor);

		// Now the Menu Column
		BusinessMenu(actor);

		// Finally the Prompt...
		SharedPrompt(input, mode);

		// Draw it!
		gwin->get_win()->show();

		// Check to see if we need to change menus
		if (activate) {
			BusinessActivate(input, command, mode, actor, time, prev);
			activate = false;
			continue;
		}

		if (SharedInput(input, 17, command, mode, activate))
			looping = BusinessCheck(input, command, mode, activate, actor, time);
	}
}

void CheatScreen::BusinessDisplay(Actor *actor) {
	char    buf[512];
	Tile_coord t = actor->get_tile();
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx = 10;
	const int offsety1 = 20;
	const int offsetx2 = 161;
	const int offsety2 = 8;
#else
	const int offsetx = 0;
	const int offsety1 = 0;
	const int offsetx2 = offsetx;
	const int offsety2 = 16;
#endif

	// Now the info
	std::string namestr = actor->get_npc_name();
	snprintf(buf, 512, "NPC %i - %s - Schedules:", actor->get_npc_num(), namestr.c_str());
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 0, 8);

	snprintf(buf, 512, "Loc (%04i, %04i, %02i)", t.tx, t.ty, t.tz);
	font->paint_text_fixedwidth(ibuf, buf, 0, 8, 8);

#if defined(__IPHONEOS__) || defined(ANDROID)
	const char activity_msg[] = "-Act: %2i %s";
#else
	const char activity_msg[] = "Current Activity:  %2i - %s";
#endif
	snprintf(buf, 512, activity_msg, actor->get_schedule_type(), schedules[actor->get_schedule_type()]);
	font->paint_text_fixedwidth(ibuf, buf, offsetx2, offsety2, 8);


	// Avatar can't have schedules
	if (actor->get_npc_num() > 0) {
#if !defined(__IPHONEOS__) && !defined(ANDROID)
		font->paint_text_fixedwidth(ibuf, "Schedules:", offsetx, 16, 8);
#endif

		Schedule_change *scheds;
		int num;
		int types[8] = { -1, -1, -1, -1, -1, -1, -1, -1};
		int x[8];
		int y[8];

		actor->get_schedules(scheds, num);

		for (int i = 0; i < num; i++) {
			int time = scheds[i].get_time();
			types[time] = scheds[i].get_type();
			Tile_coord tile = scheds[i].get_pos();
			x[time] = tile.tx;
			y[time] = tile.ty;
		}

		font->paint_text_fixedwidth(ibuf, "12 AM:", offsetx, 36 - offsety1, 8);
		font->paint_text_fixedwidth(ibuf, " 3 AM:", offsetx, 44 - offsety1, 8);
		font->paint_text_fixedwidth(ibuf, " 6 AM:", offsetx, 52 - offsety1, 8);
		font->paint_text_fixedwidth(ibuf, " 9 AM:", offsetx, 60 - offsety1, 8);
		font->paint_text_fixedwidth(ibuf, "12 PM:", offsetx, 68 - offsety1, 8);
		font->paint_text_fixedwidth(ibuf, " 3 PM:", offsetx, 76 - offsety1, 8);
		font->paint_text_fixedwidth(ibuf, " 6 PM:", offsetx, 84 - offsety1, 8);
		font->paint_text_fixedwidth(ibuf, " 9 PM:", offsetx, 92 - offsety1, 8);

		for (int i = 0; i < 8; i++) if (types[i] != -1) {
				snprintf(buf, 512, "%2i (%4i,%4i) - %s", types[i], x[i], y[i], schedules[types[i]]);
				font->paint_text_fixedwidth(ibuf, buf, offsetx + 56, (36  - offsety1) + i * 8, 8);
			}
	}
}

void CheatScreen::BusinessMenu(Actor *actor) {
	// Left Column
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx = 10;
#else
	const int offsetx = 0;
#endif
	// Might break on monster npcs?
	if (actor->get_npc_num() > 0) {
		font->paint_text_fixedwidth(ibuf, "12 AM: [A] Set [I] Location [1] Clear", offsetx, maxy - 96, 8);
		font->paint_text_fixedwidth(ibuf, " 3 AM: [B] Set [J] Location [2] Clear", offsetx, maxy - 88, 8);
		font->paint_text_fixedwidth(ibuf, " 6 AM: [C] Set [K] Location [3] Clear", offsetx, maxy - 80, 8);
		font->paint_text_fixedwidth(ibuf, " 9 AM: [D] Set [L] Location [4] Clear", offsetx, maxy - 72, 8);
		font->paint_text_fixedwidth(ibuf, "12 PM: [E] Set [M] Location [5] Clear", offsetx, maxy - 64, 8);
		font->paint_text_fixedwidth(ibuf, " 3 PM: [F] Set [N] Location [6] Clear", offsetx, maxy - 56, 8);
		font->paint_text_fixedwidth(ibuf, " 6 PM: [G] Set [O] Location [7] Clear", offsetx, maxy - 48, 8);
		font->paint_text_fixedwidth(ibuf, " 9 PM: [H] Set [P] Location [8] Clear", offsetx, maxy - 40, 8);

		font->paint_text_fixedwidth(ibuf, "[S]et Current Activity [X]it [R]evert", offsetx, maxy - 30, 8);
	} else
		font->paint_text_fixedwidth(ibuf, "[S]et Current Activity [X]it", offsetx, maxy - 30, 8);
}

void CheatScreen::BusinessActivate(char *input, int &command, Cheat_Prompt &mode, Actor *actor, int &time, int &prev) {
	int i = std::atoi(input);

	mode = CP_Command;
	int old = command;
	command = 0;
	switch (old) {
	case 'a':   // Set Activity
		if (i < -1 || i > 31) mode = CP_InvalidValue;
		else if (i == -1) mode = CP_Canceled;
		else if (!input[0]) {
			mode = CP_Activity;
			command = 'a';
		} else {
			actor->set_schedule_time_type(time, i);
		}
		break;

	case 'i':   // X Coord
		if (i < -1 || i > c_num_tiles) mode = CP_InvalidValue;
		else if (i == -1) mode = CP_Canceled;
		else if (!input[0]) {
			mode = CP_XCoord;
			command = 'i';
		} else {
			prev = i;
			mode = CP_YCoord;
			command = 'j';
		}
		break;

	case 'j':   // Y Coord
		if (i < -1 || i > c_num_tiles) mode = CP_InvalidValue;
		else if (i == -1) mode = CP_Canceled;
		else if (!input[0]) {
			mode = CP_YCoord;
			command = 'j';
		} else {
			actor->set_schedule_time_location(time, prev, i);
		}
		break;


	case '1':   // Clear
		actor->remove_schedule(time);
		break;


	case 's':   // Set Current
		if (i < -1 || i > 31) mode = CP_InvalidValue;
		else if (i == -1) mode = CP_Canceled;
		else if (!input[0]) {
			mode = CP_Activity;
			command = 's';
		} else {
			actor->set_schedule_type(i);
		}
		break;


	case 'r':   // Revert
		Game_window::get_instance()->revert_schedules(actor);
		break;


	default:
		break;
	}
	for (i = 0; i < 17; i++) input[i] = 0;
}

// Checks the input
bool CheatScreen::BusinessCheck(char *input, int &command, Cheat_Prompt &mode, bool &activate, Actor *actor, int &time) {
	// Might break on monster npcs?
	if (actor->get_npc_num() > 0) switch (command) {
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'g':
		case 'h':
			time = command - 'a';
			command = 'a';
			mode = CP_Activity;
			return true;

		case 'i':
		case 'j':
		case 'k':
		case 'l':
		case 'm':
		case 'n':
		case 'o':
		case 'p':
			time = command - 'i';
			command = 'i';
			mode = CP_XCoord;
			return true;

		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
			time = command - '1';
			command = '1';
			activate = true;
			return true;

		case 'r':
			command = 'r';
			activate = true;
			return true;

		default:
			break;
		}

	switch (command) {
		// Set Current
	case 's':
		command = 's';
		input[0] = 0;
		mode = CP_Activity;
		break;

		// X and Escape leave
	case SDLK_ESCAPE:
	case 'x':
		input[0] = command;
		return false;

		// Unknown
	default:
		command = 0;
		mode = CP_InvalidCom;
		break;
	}

	return true;
}

//
// NPC Stats
//

void CheatScreen::StatLoop(Actor *actor) {
#if !defined(__IPHONEOS__) && !defined(ANDROID)
    int num = actor->get_npc_num();
#endif
	bool looping = true;

	// This is for the prompt message
	Cheat_Prompt mode = CP_Command;

	// This is the command
	char input[17];
	int i;
	int command;
	bool activate = false;

	for (i = 0; i < 17; i++) input[i] = 0;

	while (looping) {
		gwin->clear_screen();

#if !defined(__IPHONEOS__) && !defined(ANDROID)
		// First the display
		NPCDisplay(actor, num);
#endif

		// Now the Menu Column
		StatMenu(actor);

		// Finally the Prompt...
		SharedPrompt(input, mode);

		// Draw it!
		gwin->get_win()->show();

		// Check to see if we need to change menus
		if (activate) {
			StatActivate(input, command, mode, actor);
			activate = false;
			continue;
		}

		if (SharedInput(input, 17, command, mode, activate))
			looping = StatCheck(input, command, mode, activate, actor);
	}
}

void CheatScreen::StatMenu(Actor *actor) {
	char    buf[512];
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx = 15;
	const int offsety1 = 92;
#else
	const int offsetx = 0;
	const int offsety1 = 0;
#endif

	// Left Column

	// Dexterity
	snprintf(buf, 512, "[D]exterity....%3i", actor->get_property(Actor::dexterity));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 108 , 8);

	// Food Level
	snprintf(buf, 512, "[F]ood Level...%3i", actor->get_property(Actor::food_level));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 99, 8);

	// Intelligence
	snprintf(buf, 512, "[I]ntellicence.%3i", actor->get_property(Actor::intelligence));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 90, 8);

	// Strength
	snprintf(buf, 512, "[S]trength.....%3i", actor->get_property(Actor::strength));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 81, 8);

	// Combat Skill
	snprintf(buf, 512, "[C]ombat Skill.%3i", actor->get_property(Actor::combat));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 72, 8);

	// Hit Points
	snprintf(buf, 512, "[H]it Points...%3i", actor->get_property(Actor::health));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 63, 8);

	// Magic
	// Magic Points
	snprintf(buf, 512, "[M]agic Points.%3i", actor->get_property(Actor::magic));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 54, 8);

	// Mana
	snprintf(buf, 512, "[V]ana Level...%3i", actor->get_property(Actor::mana));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 45, 8);

	// Exit
	font->paint_text_fixedwidth(ibuf, "[X]it", offsetx, maxy - offsety1 - 36, 8);

}

void CheatScreen::StatActivate(char *input, int &command, Cheat_Prompt &mode, Actor *actor) {
	int i = std::atoi(input);
	mode = CP_Command;
	// Enforce sane bounds.
	if (i > 60) {
		i = 60;
	} else if (i < 0 && command != 'h') {
		if (i == -1) {   // canceled
			for (i = 0; i < 17; i++)
				input[i] = 0;
			command = 0;
			return;
		}
		i = 0;
	} else if (i < -50) {
		i = -50;
	}

	switch (command) {
	case 'd':   // Dexterity
		actor->set_property(Actor::dexterity, i);
		break;

	case 'f':   // Food Level
		actor->set_property(Actor::food_level, i);
		break;

	case 'i':   // Intelligence
		actor->set_property(Actor::intelligence, i);
		break;

	case 's':   // Strength
		actor->set_property(Actor::strength, i);
		break;

	case 'c':   // Combat Points
		actor->set_property(Actor::combat, i);
		break;

	case 'h':   // Hit Points
		actor->set_property(Actor::health, i);
		break;

	case 'm':   // Magic
		actor->set_property(Actor::magic, i);
		break;

	case 'v':   // [V]ana
		actor->set_property(Actor::mana, i);
		break;


	default:
		break;
	}
	for (i = 0; i < 17; i++) input[i] = 0;
	command = 0;
}

// Checks the input
bool CheatScreen::StatCheck(char *input, int &command, Cheat_Prompt &mode, bool &activate, Actor *actor) {
	ignore_unused_variable_warning(activate, actor);
	switch (command) {
		// Everyone
	case 'h':   // Hit Points
		input[0] = 0;
		mode = CP_EnterValueNoCancel;
		break;
	case 'd':   // Dexterity
	case 'f':   // Food Level
	case 'i':   // Intelligence
	case 's':   // Strength
	case 'c':   // Combat Points
	case 'm':   // Magic
	case 'v':   // [V]ana
		input[0] = 0;
		mode = CP_EnterValue;
		break;

		// X and Escape leave
	case SDLK_ESCAPE:
	case 'x':
		input[0] = command;
		return false;

		// Unknown
	default:
		command = 0;
		break;
	}

	return true;
}


//
// Advanced Flag Edition
//

CheatScreen::Cheat_Prompt CheatScreen::AdvancedFlagLoop(int num, Actor *actor) {
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx = 15;
	const int offsety1 = 83;
	const int offsety2 = 72;
#else
	int npc_num = actor->get_npc_num();
	const int offsetx = 0;
	const int offsety1 = 0;
	const int offsety2 = maxy - 36;
#endif
	bool looping = true;
	// This is for the prompt message
	Cheat_Prompt mode = CP_Command;

	// This is the command
	char input[17];
	int i;
	int command;
	bool activate = false;
	char    buf[512];

	for (i = 0; i < 17; i++) input[i] = 0;

	while (looping) {
		gwin->clear_screen();

#if !defined(__IPHONEOS__) && !defined(ANDROID)
		NPCDisplay(actor, npc_num);
#endif

		if (num < 0) num = 0;
		else if (num > 63) num = 63;

		// First the info
		if (flag_names[num])
			snprintf(buf, 512, "NPC Flag %i: %s", num, flag_names[num]);
		else
			snprintf(buf, 512, "NPC Flag %i", num);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 108, 8);

		snprintf(buf, 512, "Flag is %s", actor->get_flag(num) ? "SET" : "UNSET");
		font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 90, 8);


		// Now the Menu Column
		if (!actor->get_flag(num)) font->paint_text_fixedwidth(ibuf, "[S]et Flag", offsetx + 160, maxy - offsety1 - 90, 8);
		else font->paint_text_fixedwidth(ibuf, "[U]nset Flag", offsetx + 160, maxy - offsety1 - 90, 8);

		// Change Flag
		font->paint_text_fixedwidth(ibuf, "[*] Change Flag", offsetx, maxy - offsety1 - 72, 8);
		if (num > 0 && num < 63) font->paint_text_fixedwidth(ibuf, "[+-] Scroll Flags", offsetx, maxy - offsety1 - 63, 8);
		else if (num == 0) font->paint_text_fixedwidth(ibuf, "[+] Scroll Flags", offsetx, maxy - offsety1 - 63, 8);
		else font->paint_text_fixedwidth(ibuf, "[-] Scroll Flags", offsetx, maxy - offsety1 - 63, 8);

		font->paint_text_fixedwidth(ibuf, "[X]it", offsetx, offsety2, 8);

		// Finally the Prompt...
		SharedPrompt(input, mode);

		// Draw it!
		gwin->get_win()->show();

		// Check to see if we need to change menus
		if (activate) {
			mode = CP_Command;
			if (command == '-') {   // Decrement
				num--;
				if (num < 0) {
					num = 0;
				}
			} else if (command == '+') { // Increment
				num++;
				if (num > 63) {
					num = 63;
				}
			} else if (command == '*') { // Change Flag
				i = std::atoi(input);
				if (i < -1 || i > 63) {
					mode = CP_InvalidValue;
				} else if (i == -1) {
					mode = CP_Canceled;
				} else if (input[0]) {
					num = i;
				}
			} else if (command == 's') { // Set
				actor->set_flag(num);
				if (num == Obj_flags::in_party) {
					gwin->get_party_man()->add_to_party(actor);
					actor->set_schedule_type(Schedule::follow_avatar);
				}
			} else if (command == 'u') { // Unset
				if (num == Obj_flags::in_party) {
					gwin->get_party_man()->remove_from_party(actor);
					gwin->revert_schedules(actor);
				}
				actor->clear_flag(num);
			}

			for (i = 0; i < 17; i++) {
				input[i] = 0;
			}
			command = 0;
			activate = false;
			continue;
		}

		if (SharedInput(input, 17, command, mode, activate)) {
			switch (command) {
				// Simple commands
			case 's':   // Set Flag
			case 'u':   // Unset flag
				input[0] = command;
				activate = true;
				break;

				// Decrement
			case SDLK_KP_MINUS:
			case '-':
				command = '-';
				input[0] = command;
				activate = true;
				break;

				// Increment
			case SDLK_KP_PLUS:
			case '=':
				command = '+';
				input[0] = command;
				activate = true;
				break;

				// * Change Flag
			case SDLK_KP_MULTIPLY:
			case '8':
				command = '*';
				input[0] = 0;
				mode = CP_NFlagNum;
				break;

				// X and Escape leave
			case SDLK_ESCAPE:
			case 'x':
				input[0] = command;
				looping = false;
				break;

			default:
				mode = CP_InvalidCom;
				input[0] = command;
				command = 0;
				break;
			}
		}
	}
	return CP_Command;
}


//
// Teleport screen
//

void CheatScreen::TeleportLoop() {
	bool looping = true;

	// This is for the prompt message
	Cheat_Prompt mode = CP_Command;

	// This is the command
	char input[5];
	int i;
	int command;
	bool activate = false;
	int prev = 0;

	for (i = 0; i < 5; i++) input[i] = 0;

	while (looping) {
		gwin->clear_screen();

		// First the display
		TeleportDisplay();

		// Now the Menu Column
		TeleportMenu();

		// Finally the Prompt...
		SharedPrompt(input, mode);

		// Draw it!
		gwin->get_win()->show();

		// Check to see if we need to change menus
		if (activate) {
			TeleportActivate(input, command, mode, prev);
			activate = false;
			continue;
		}

		if (SharedInput(input, 5, command, mode, activate))
			looping = TeleportCheck(input, command, mode, activate);
	}
}

void CheatScreen::TeleportDisplay() {
	char    buf[512];
	Tile_coord t = gwin->get_main_actor()->get_tile();
	int curmap = gwin->get_map()->get_num();
	int highest = Find_highest_map();
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx = 15;
	const int offsety1 = 54;
#else
	const int offsetx = 0;
	const int offsety1 = 0;
#endif

#if defined(__IPHONEOS__) || defined(ANDROID)
	font->paint_text_fixedwidth(ibuf, "Teleport Menu - Dangerous!", offsetx, 0, 8);
#else
	font->paint_text_fixedwidth(ibuf, "Teleport Menu", offsetx, 0, 8);
	font->paint_text_fixedwidth(ibuf, "Dangerous - use with care!", offsetx, 18, 8);
#endif

	int longi = ((t.tx - 0x3A5) / 10);
	int lati = ((t.ty - 0x46E) / 10);
#if defined(__IPHONEOS__) || defined(ANDROID)
	snprintf(buf, 512, "Coords %d %s %d %s, Map #%d of %d",
		abs(lati), (lati < 0 ? "North" : "South"),
		abs(longi), (longi < 0 ? "West" : "East"), curmap, highest);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 9, 8);
#else
	snprintf(buf, 512, "Coordinates   %d %s %d %s",
		abs(lati), (lati < 0 ? "North" : "South"),
		abs(longi), (longi < 0 ? "West" : "East"));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 63, 8);
#endif

	snprintf(buf, 512, "Coords in hex (%04x, %04x, %02x)",
	         t.tx, t.ty, t.tz);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 72 - offsety1, 8);

	snprintf(buf, 512, "Coords in dec (%04i, %04i, %02i)",
	         t.tx, t.ty, t.tz);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 81 - offsety1, 8);

#if !defined(__IPHONEOS__) && !defined(ANDROID)
	snprintf(buf, 512, "On Map #%d of %d",
	         curmap, highest);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 90, 8);
#endif
}


void CheatScreen::TeleportMenu() {

#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx = 15;
	const int offsety1 = 64;
	const int offsetx2 = 175;
	const int offsety2 = 63;
	const int offsety3 = 72;
#else
	const int offsetx = 0;
	const int offsety1 = 0;
	const int offsetx2 = offsetx;
	const int offsety2 = maxy - 63;
	const int offsety3 = maxy - 36;
#endif

	// Left Column
	// Geo
	font->paint_text_fixedwidth(ibuf, "[G]eographic Coordinates", offsetx, maxy - offsety1 - 99, 8);


	// Hex
	font->paint_text_fixedwidth(ibuf, "[H]exadecimal Coordinates", offsetx, maxy - offsety1 - 90, 8);

	// Dec
	font->paint_text_fixedwidth(ibuf, "[D]ecimal Coordinates", offsetx, maxy - offsety1 - 81, 8);

	// NPC
	font->paint_text_fixedwidth(ibuf, "[N]PC Number", offsetx, maxy - offsety1 - 72, 8);

	// Map
	font->paint_text_fixedwidth(ibuf, "[M]ap Number", offsetx2, offsety2, 8);

	// eXit
	font->paint_text_fixedwidth(ibuf, "[X]it", offsetx, offsety3, 8);
}

void CheatScreen::TeleportActivate(char *input, int &command, Cheat_Prompt &mode, int &prev) {
	int i = std::atoi(input);
	static int lat;
	Tile_coord t = gwin->get_main_actor()->get_tile();
	int highest = Find_highest_map();

	mode = CP_Command;
	switch (command) {

	case 'g':   // North or South
		if (!input[0]) {
			mode = CP_NorthSouth;
			command = 'g';
		} else if (input[0] == 'n' || input[0] == 's') {
			prev = input[0];
			if (prev == 'n') mode = CP_NLatitude;
			else mode = CP_SLatitude;
			command = 'a';
		}else
			mode = CP_InvalidValue;
		break;

	case 'a':   // latitude
		if (i < -1 || (prev == 'n' && i > 113) || (prev == 's' && i > 193)) mode = CP_InvalidValue;
		else if (i == -1) mode = CP_Canceled;
		else if (!input[0]) {
			if (prev == 'n') mode = CP_NLatitude;
			else mode = CP_SLatitude;
			command = 'a';
		} else {
			if (prev == 'n')
				lat = ((i * -10) + 0x46E);
			else
				lat = ((i * 10) + 0x46E);
			mode = CP_WestEast;
			command = 'b';
		}
		break;

	case 'b':   // West or East
		if (!input[0]) {
			mode = CP_WestEast;
			command = 'b';
		} else if (input[0] == 'w' || input[0] == 'e') {
			prev = input[0];
			if (prev == 'w') mode = CP_WLongitude;
			else mode = CP_ELongitude;
			command = 'c';
		}
		else
			mode = CP_InvalidValue;
		break;

	case 'c':   // longitude
		if (i < -1 || (prev == 'w' && i > 93) || (prev == 'e' && i > 213)) mode = CP_InvalidValue;
		else if (i == -1) mode = CP_Canceled;
		else if (!input[0]) {
			if (prev == 'w') mode = CP_WLongitude;
			else mode = CP_ELongitude;
			command = 'c';
		} else {
			if (prev == 'w')
				t.tx = ((i * -10) + 0x3A5);
			else
				t.tx = ((i * 10) + 0x3A5);
			t.ty = lat;
			t.tz = 0;
			gwin->teleport_party(t);
		}
		break;

	case 'h':   // hex X coord
		i = strtol(input, nullptr, 16);
		if (i < -1 || i > c_num_tiles) mode = CP_InvalidValue;
		else if (i == -1) mode = CP_Canceled;
		else if (!input[0]) {
			mode = CP_HexXCoord;
			command = 'h';
		} else {
			prev = i;
			mode = CP_HexYCoord;
			command = 'i';
		}
		break;

	case 'i':   // hex Y coord
		i = strtol(input, nullptr, 16);
		if (i < -1 || i > c_num_tiles) mode = CP_InvalidValue;
		else if (i == -1) mode = CP_Canceled;
		else if (!input[0]) {
			mode = CP_HexYCoord;
			command = 'i';
		} else {
			t.tx = prev;
			t.ty = i;
			t.tz = 0;
			gwin->teleport_party(t);
		}
		break;

	case 'd':   // dec X coord
		if (i < -1 || i > c_num_tiles) mode = CP_InvalidValue;
		else if (i == -1) mode = CP_Canceled;
		else if (!input[0]) {
			mode = CP_XCoord;
			command = 'd';
		} else {
			prev = i;
			mode = CP_YCoord;
			command = 'e';
		}
		break;

	case 'e':   // dec Y coord
		if (i < -1 || i > c_num_tiles) mode = CP_InvalidValue;
		else if (i == -1) mode = CP_Canceled;
		else if (!input[0]) {
			mode = CP_YCoord;
			command = 'e';
		} else {
			t.tx = prev;
			t.ty = i;
			t.tz = 0;
			gwin->teleport_party(t);
		}
		break;

	case 'n':   // NPC
		if (i < -1 || (i >= 356 && i <= 359)) mode = CP_InvalidNPC;
		else if (i == -1) mode = CP_Canceled;
		else {
			Actor *actor = gwin->get_npc(i);
			Game_window::get_instance()->teleport_party(actor->get_tile(),
			false, actor->get_map_num());
		}
		break;

	case 'm':   // map
		if (i == -1) mode = CP_Canceled;
		else if ((i < 0 || i > 255) || i > highest) mode = CP_InvalidValue;
		else
			gwin->teleport_party(gwin->get_main_actor()->get_tile(), true, i);
		break;

	default:
		break;
	}
	for (i = 0; i < 5; i++) input[i] = 0;
}

// Checks the input
bool CheatScreen::TeleportCheck(char *input, int &command, Cheat_Prompt &mode, bool &activate) {
	ignore_unused_variable_warning(activate);
	switch (command) {
		// Simple commands
	case 'g':   // geographic
		mode = CP_NorthSouth;
		return true;

	case 'h':   // hex
		mode = CP_HexXCoord;
		return true;

	case 'd':   // dec teleport
		mode = CP_XCoord;
		return true;

	case 'n':   // NPC teleport
		mode = CP_ChooseNPC;
		break;

	case 'm':   // NPC teleport
		mode = CP_EnterValue;
		break;

		// X and Escape leave
	case SDLK_ESCAPE:
	case 'x':
		input[0] = command;
		return false;

	default:
		command = 0;
		mode = CP_InvalidCom;
		break;
	}

	return true;
}
