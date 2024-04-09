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

#include "cheat_screen.h"

#include "Configuration.h"
#include "Gump_manager.h"
#include "actors.h"
#include "cheat.h"
#include "chunks.h"
#include "eventman.h"
#include "exult.h"
#include "files/U7file.h"
#include "font.h"
#include "game.h"
#include "gameclk.h"
#include "gamemap.h"
#include "gamewin.h"
#include "gump_utils.h"
#include "ignore_unused_variable_warning.h"
#include "miscinf.h"
#include "party.h"
#include "schedule.h"
#include "touchui.h"
#include "ucmachine.h"
#include "vgafile.h"

const char* CheatScreen::schedules[33] = {
		"Combat",    "Hor. Pace",  "Ver. Pace",  "Talk",  "Dance",     "Eat",
		"Farm",      "Tend Shop",  "Miner",      "Hound", "Stand",     "Loiter",
		"Wander",    "Blacksmith", "Sleep",      "Wait",  "Major Sit", "Graze",
		"Bake",      "Sew",        "Shy",        "Lab",   "Thief",     "Waiter",
		"Special",   "Kid Games",  "Eat at Inn", "Duel",  "Preach",    "Patrol",
		"Desk Work", "Follow Avt", "Move2Sched"};

const char* CheatScreen::flag_names[64] = {
		"invisible",    // 0x00
		"asleep",       // 0x01
		"charmed",      // 0x02
		"cursed",       // 0x03
		"dead",         // 0x04
		nullptr,        // 0x05
		"in_party",     // 0x06
		"paralyzed",    // 0x07

		"poisoned",           // 0x08
		"protection",         // 0x09
		"on_moving_barge",    // 0x0A
		"okay_to_take",       // 0x0B
		"might",              // 0x0C
		"immunities",         // 0x0D
		"cant_die",           // 0x0E
		"in_action",          // 0x0F

		"dont_move/bg_dont_render",    // 0x10
		"si_on_moving_barge",          // 0x11
		"is_temporary",                // 0x12
		nullptr,                       // 0x13
		"sailor",                      // 0x14
		"okay_to_land",                // 0x15
		"dont_render/bg_dont_move",    // 0x16
		"in_dungeon",                  // 0x17

		nullptr,               // 0x18
		"confused",            // 0x19
		"in_motion",           // 0x1A
		nullptr,               // 0x1B
		"met",                 // 0x1C
		"tournament",          // 0x1D
		"si_zombie",           // 0x1E
		"no_spell_casting",    // 0x1F

		"polymorph",         // 0x20
		"tattooed",          // 0x21
		"read",              // 0x22
		"petra",             // 0x23
		"si_lizard_king",    // 0x24
		"freeze",            // 0x25
		"naked",             // 0x26
		nullptr,             // 0x27

		nullptr,    // 0x28
		nullptr,    // 0x29
		nullptr,    // 0x2A
		nullptr,    // 0x2B
		nullptr,    // 0x2C
		nullptr,    // 0x2D
		nullptr,    // 0x2E
		nullptr,    // 0x2F

		nullptr,    // 0x30
		nullptr,    // 0x31
		nullptr,    // 0x32
		nullptr,    // 0x33
		nullptr,    // 0x34
		nullptr,    // 0x35
		nullptr,    // 0x36
		nullptr,    // 0x37

		nullptr,    // 0x38
		nullptr,    // 0x39
		nullptr,    // 0x3A
		nullptr,    // 0x3B
		nullptr,    // 0x3C
		nullptr,    // 0x3D
		nullptr,    // 0x3E
		nullptr,    // 0x3F
};

const char* CheatScreen::alignments[4] = {"Neutral", "Good", "Evil", "Chaotic"};

static int Find_highest_map() {
	int n = 0;
	int next;
	while ((next = Find_next_map(n + 1, 10)) != -1) {
		n = next;
	}
	return n;
}

void CheatScreen::show_screen() {
	gwin  = Game_window::get_instance();
	ibuf  = gwin->get_win()->get_ib8();
	font  = fontManager.get_font("MENU_FONT");
	clock = gwin->get_clock();
	maxx  = gwin->get_width();
#if defined(__IPHONEOS__) || defined(ANDROID)
	maxy = 200;
#else
	maxy = gwin->get_height();
#endif
	centerx = maxx / 2;
	centery = maxy / 2;
	if (touchui != nullptr) {
		touchui->hideGameControls();
		EventManager::getInstance()->start_text_input();
	}

	// Pause the game
	gwin->get_tqueue()->pause(SDL_GetTicks());

	const str_int_pair& pal_tuple_static = game->get_resource("palettes/0");
	const str_int_pair& pal_tuple_patch
			= game->get_resource("palettes/patch/0");
	pal.load(pal_tuple_static.str, pal_tuple_patch.str, pal_tuple_static.num);
	pal.apply();

	// Start the loop
	NormalLoop();

	// Resume the game clock
	gwin->get_tqueue()->resume(SDL_GetTicks());

	// Reset the palette
	clock->reset_palette();
	if (touchui != nullptr) {
		Gump_manager* gumpman = gwin->get_gump_man();
		if (!gumpman->gump_mode()) {
			touchui->showGameControls();
		}
		EventManager::getInstance()->stop_text_input();
	}
}

//
// DISPLAYS
//

//
// Shared
//

void CheatScreen::SharedPrompt(
		const std::string& input, const Cheat_Prompt& mode) {
	char buf[512];

#if defined(__IPHONEOS__) || defined(ANDROID)
	const int prompt    = 81;
	const int promptmes = 90;
	const int offsetx   = 15;
#else
	const int prompt    = maxy - 18;
	const int promptmes = maxy - 9;
	const int offsetx   = 0;
#endif
	font->paint_text_fixedwidth(ibuf, "Select->", offsetx, prompt, 8);

	if (!input.empty()) {
		int cursor_offset = static_cast<int>(input.size()) * 8;
		font->paint_text_fixedwidth(
				ibuf, input.data(), 64 + offsetx, prompt, 8);
		font->paint_text_fixedwidth(
				ibuf, "_", 64 + offsetx + cursor_offset, prompt, 8);
	} else {
		font->paint_text_fixedwidth(ibuf, "_", 64 + offsetx, prompt, 8);
	}

	// ...and Prompt Message
	switch (mode) {
	case CP_Command:
		font->paint_text_fixedwidth(
				ibuf, "Enter Command.", offsetx, promptmes, 8);
		break;

	case CP_HitKey:
		font->paint_text_fixedwidth(ibuf, "Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_NotAvail:
		font->paint_text_fixedwidth(
				ibuf, "Not yet available. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_InvalidNPC:
		font->paint_text_fixedwidth(
				ibuf, "Invalid NPC. Hit a key", offsetx, promptmes, 8);
		break;

	case CP_InvalidCom:
		font->paint_text_fixedwidth(
				ibuf, "Invalid Command. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_Canceled:
		font->paint_text_fixedwidth(
				ibuf, "Canceled. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_ClockSet:
		font->paint_text_fixedwidth(
				ibuf, "Clock Set. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_InvalidTime:
		font->paint_text_fixedwidth(
				ibuf, "Invalid Time. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_InvalidShape:
		font->paint_text_fixedwidth(
				ibuf, "Invalid Shape. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_InvalidValue:
		font->paint_text_fixedwidth(
				ibuf, "Invalid Value. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_Created:
		font->paint_text_fixedwidth(
				ibuf, "Item Created. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_ShapeSet:
		font->paint_text_fixedwidth(
				ibuf, "Shape Set. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_ValueSet:
		font->paint_text_fixedwidth(
				ibuf, "Clock Set. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_NameSet:
		font->paint_text_fixedwidth(
				ibuf, "Name Changed. Hit a key.", offsetx, promptmes, 8);
		break;

	case CP_WrongShapeFile:
		font->paint_text_fixedwidth(
				ibuf, "Wrong shape file. Must be SHAPES.VGA.", offsetx,
				promptmes, 8);
		break;

	case CP_ChooseNPC:
		font->paint_text_fixedwidth(
				ibuf, "Which NPC? (-1 to cancel.)", offsetx, promptmes, 8);
		break;

	case CP_EnterValue:
		font->paint_text_fixedwidth(
				ibuf, "Enter Value. (-1 to cancel.)", offsetx, promptmes, 8);
		break;

	case CP_EnterValueNoCancel:
		font->paint_text_fixedwidth(
				ibuf, "Enter Value.", offsetx, promptmes, 8);
		break;

	case CP_Minute:
		font->paint_text_fixedwidth(
				ibuf, "Enter Minute. (-1 to cancel.)", offsetx, promptmes, 8);
		break;

	case CP_Hour:
		font->paint_text_fixedwidth(
				ibuf, "Enter Hour. (-1 to cancel.)", offsetx, promptmes, 8);
		break;

	case CP_Day:
		font->paint_text_fixedwidth(
				ibuf, "Enter Day. (-1 to cancel.)", offsetx, promptmes, 8);
		break;

	case CP_Shape:
		font->paint_text_fixedwidth(
				ibuf, "Enter Shape (B=Browse or -1=Cancel)", offsetx, promptmes,
				8);
		break;

	case CP_Activity:
		font->paint_text_fixedwidth(
				ibuf, "Enter Activity 0-31. (-1 to cancel.)", offsetx,
				promptmes, 8);
		break;

	case CP_XCoord:
		snprintf(
				buf, sizeof(buf), "Enter X Coord. Max %i (-1 to cancel)",
				c_num_tiles);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8);
		break;

	case CP_YCoord:
		snprintf(
				buf, sizeof(buf), "Enter Y Coord. Max %i (-1 to cancel)",
				c_num_tiles);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8);
		break;

	case CP_Lift:
		font->paint_text_fixedwidth(
				ibuf, "Enter Lift. (-1 to cancel)", offsetx, promptmes, 8);
		break;

	case CP_GFlagNum:
		snprintf(
				buf, sizeof(buf), "Enter Global Flag 0-%d. (-1 to cancel)",
				c_last_gflag);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8);
		break;

	case CP_NFlagNum:
		font->paint_text_fixedwidth(
				ibuf, "Enter NPC Flag 0-63. (-1 to cancel)", offsetx, promptmes,
				8);
		break;

	case CP_TempNum:
		font->paint_text_fixedwidth(
				ibuf, "Enter Temperature 0-63. (-1 to cancel)", offsetx,
				promptmes, 8);
		break;

	case CP_NLatitude:
		font->paint_text_fixedwidth(
				ibuf, "Enter Latitude. Max 113 (-1 to cancel)", offsetx,
				promptmes, 8);
		break;

	case CP_SLatitude:
		font->paint_text_fixedwidth(
				ibuf, "Enter Latitude. Max 193 (-1 to cancel)", offsetx,
				promptmes, 8);
		break;

	case CP_WLongitude:
		font->paint_text_fixedwidth(
				ibuf, "Enter Longitude. Max 93 (-1 to cancel)", offsetx,
				promptmes, 8);
		break;

	case CP_ELongitude:
		font->paint_text_fixedwidth(
				ibuf, "Enter Longitude. Max 213 (-1 to cancel)", offsetx,
				promptmes, 8);
		break;

	case CP_Name:
		font->paint_text_fixedwidth(
				ibuf, "Enter a new Name...", offsetx, promptmes, 8);
		break;

	case CP_NorthSouth:
		font->paint_text_fixedwidth(
				ibuf, "Latitude [N]orth or [S]outh?", offsetx, promptmes, 8);
		break;

	case CP_WestEast:
		font->paint_text_fixedwidth(
				ibuf, "Longitude [W]est or [E]ast?", offsetx, promptmes, 8);
		break;

	case CP_HexXCoord:
		snprintf(
				buf, sizeof(buf), "Enter X Coord. Max %04x (-1 to cancel)",
				c_num_tiles);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8);
		break;

	case CP_HexYCoord:
		snprintf(
				buf, sizeof(buf), "Enter Y Coord. Max %04x (-1 to cancel)",
				c_num_tiles);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8);
		break;
	}
}

bool CheatScreen::SharedInput(
		std::string& input, KeyCodes& command, Cheat_Prompt& mode,
		bool& activate) {
	EventManager* eman = EventManager::getInstance();

	bool result  = false;
	bool looping = true;

	auto guard = eman->register_callbacks(
			[&](MouseEvent type, MouseButton button, int numClicks,
				const MousePosition& pos) {
				ignore_unused_variable_warning(button, numClicks, pos);
				if (type == MouseEvent::Pressed) {
					eman->toggle_text_input();
				}
			},
			[&](KeyboardEvent type, const KeyCodes sym, const KeyMod mod) {
				if (type != KeyboardEvent::Pressed) {
					return;
				}
				looping = false;
				result  = false;
				if ((sym == KeyCodes::Key_s)
					&& (mod & KeyMod::Alt) != KeyMod::NoMods
					&& (mod & KeyMod::Ctrl) != KeyMod::NoMods) {
					make_screenshot(true);
				} else if (mode == CP_NorthSouth) {
					if (input.empty()
						&& (sym == KeyCodes::Key_n || sym == KeyCodes::Key_s)) {
						input.push_back(static_cast<char>(sym));
						activate = true;
					}
				} else if (mode == CP_WestEast) {
					if (input.empty()
						&& (sym == KeyCodes::Key_w || sym == KeyCodes::Key_e)) {
						input.push_back(static_cast<char>(sym));
						activate = true;
					}
				} else if (mode >= CP_HexXCoord) {    // Want hex input
					// Activate (if possible)
					if (is_return(sym, mod)) {
						activate = true;
					} else if (is_minus(sym, mod) && input.empty()) {
						input.push_back('-');
					} else if (is_xdigit(sym, mod)) {
						input.push_back(get_xdigit(sym, mod));
					} else if (sym == KeyCodes::Key_Backspace) {
						if (!input.empty()) {
							input.pop_back();
						}
					}
				} else if (mode >= CP_Name) {    // Want Text input (len chars)
					if (is_return(sym, mod)) {
						activate = true;
					} else if (is_alnum(sym, mod)) {
						char chr = get_alnum(sym, mod);
						input.push_back(chr);
					} else if (sym == KeyCodes::Key_Space) {
						input.push_back(' ');
					} else if (sym == KeyCodes::Key_Backspace) {
						if (!input.empty()) {
							input.pop_back();
						}
					}
				} else if (mode >= CP_ChooseNPC) {    // Need to grab numerical
													  // input
					// Browse shape
					if (mode == CP_Shape && input.empty()
						&& sym == KeyCodes::Key_b
						&& (mod & (KeyMod::Alt | KeyMod::Ctrl | KeyMod::GUI))
								   == KeyMod::NoMods) {
						cheat.shape_browser();
						input.push_back('b');
						activate = true;
					}

					// Activate (if possible)
					if (is_return(sym, mod)) {
						activate = true;
					} else if (is_minus(sym, mod) && input.empty()) {
						input.push_back('-');
					} else if (is_digit(sym, mod)) {
						input.push_back(get_digit(sym, mod));
					} else if (sym == KeyCodes::Key_Backspace) {
						if (!input.empty()) {
							input.pop_back();
						}
					}
				} else if (mode != CP_Command) {    // Just want a key pressed
					mode = CP_Command;
					input.clear();
					command = KeyCodes::Key_Unknown;
				} else {    // Need the key pressed
					command = sym;
					result  = true;
				}
			});

	while (looping) {
		Delay();
		looping = true;
		eman->handle_events();
		gwin->paint_dirty();
	}
	return result;
}

//
// Normal
//

void CheatScreen::NormalLoop() {
	bool looping = true;

	// This is for the prompt message
	Cheat_Prompt mode = CP_Command;

	// This is the command
	std::string input;
	KeyCodes    command;
	bool        activate = false;

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

		if (SharedInput(input, command, mode, activate)) {
			looping = NormalCheck(input, command, mode, activate);
		}
	}
}

void CheatScreen::NormalDisplay() {
	char buf[512];
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx  = 15;
	const int offsety1 = 108;
	const int offsety2 = 54;
	const int offsety3 = 0;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsety2 = 0;
	const int offsety3 = 45;
#endif
	const int        curmap = gwin->get_map()->get_num();
	const Tile_coord t      = gwin->get_main_actor()->get_tile();

	font->paint_text_fixedwidth(
			ibuf, "Colourless' Advanced Option Cheat Screen", 0, offsety1, 8);

	if (Game::get_game_type() == BLACK_GATE) {
		snprintf(buf, sizeof(buf), "Running \"Ultima 7: The Black Gate\"");
	} else if (Game::get_game_type() == SERPENT_ISLE) {
		snprintf(
				buf, sizeof(buf), "Running \"Ultima 7: Part 2: Serpent Isle\"");
	} else {
		snprintf(
				buf, sizeof(buf), "Running Unknown Game Type %i",
				Game::get_game_type());
	}

	font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 18, 8);

	snprintf(buf, sizeof(buf), "Exult Version %s", VERSION);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 27, 8);

	snprintf(
			buf, sizeof(buf), "Current time: %i:%02i %s  Day: %i",
			((clock->get_hour() + 11) % 12) + 1, clock->get_minute(),
			clock->get_hour() < 12 ? "AM" : "PM", clock->get_day());
	font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety3, 8);

	const int longi = ((t.tx - 0x3A5) / 10);
	const int lati  = ((t.ty - 0x46E) / 10);
	snprintf(
			buf, sizeof(buf), "Coordinates %d %s %d %s, Map #%d", abs(lati),
			(lati < 0 ? "North" : "South"), abs(longi),
			(longi < 0 ? "West" : "East"), curmap);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 63 - offsety2, 8);

	snprintf(
			buf, sizeof(buf), "Coords in hex (%04x, %04x, %02x)", t.tx, t.ty,
			t.tz);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 72 - offsety2, 8);

	snprintf(
			buf, sizeof(buf), "Coords in dec (%04i, %04i, %02i)", t.tx, t.ty,
			t.tz);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 81 - offsety2, 8);
}

void CheatScreen::NormalMenu() {
	char buf[512];
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx  = 15;
	const int offsety1 = 73;
	const int offsety2 = 55;
	const int offsetx1 = 160;
	const int offsety4 = 36;
	const int offsety5 = 72;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsety2 = 0;
	const int offsetx1 = 0;
	const int offsety4 = maxy - 45;
	const int offsety5 = maxy - 36;
#endif

	// Left Column

	// Use
#if !defined(__IPHONEOS__) && !defined(ANDROID)
	// Paperdolls can be toggled in the gumps, no need here for small screens
	Shape_manager* sman = Shape_manager::get_instance();
	if (sman->can_use_paperdolls() && sman->are_paperdolls_enabled()) {
		snprintf(buf, sizeof(buf), "[P]aperdolls..: Yes");
	} else {
		snprintf(buf, sizeof(buf), "[P]aperdolls..:  No");
	}
	font->paint_text_fixedwidth(ibuf, buf, 0, maxy - 99, 8);
#endif

	// GodMode
	snprintf(
			buf, sizeof(buf), "[G]od Mode....: %3s",
			cheat.in_god_mode() ? "On" : "Off");
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 90, 8);

	// Archwizzard Mode
	snprintf(
			buf, sizeof(buf), "[W]izard Mode.: %3s",
			cheat.in_wizard_mode() ? "On" : "Off");
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 81, 8);

	// Infravision
	snprintf(
			buf, sizeof(buf), "[I]nfravision.: %3s",
			cheat.in_infravision() ? "On" : "Off");
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 72, 8);

	// Hackmover
	snprintf(
			buf, sizeof(buf), "[H]ack Mover..: %3s",
			cheat.in_hack_mover() ? "Yes" : "No");
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 63, 8);

	// Eggs
	snprintf(
			buf, sizeof(buf), "[E]ggs Visible: %3s",
			gwin->paint_eggs ? "Yes" : "No");
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 54, 8);

	// Set Time
	font->paint_text_fixedwidth(
			ibuf, "[S]et Time", offsetx + offsetx1, offsety4, 8);

#if !defined(__IPHONEOS__) && !defined(ANDROID)
	// for small screens taking the liberty of leaving that out
	// Time Rate
	snprintf(buf, sizeof(buf), "[+-] Time Rate: %3i", clock->get_time_rate());
	font->paint_text_fixedwidth(ibuf, buf, 0, maxy - 36, 8);
#endif

	// Right Column

	// NPC Tool
	font->paint_text_fixedwidth(
			ibuf, "[N]PC Tool", offsetx + 160, maxy - offsety2 - 99, 8);

	// Global Flag Modify
	font->paint_text_fixedwidth(
			ibuf, "[F]lag Modifier", offsetx + 160, maxy - offsety2 - 90, 8);

	// Teleport
	font->paint_text_fixedwidth(
			ibuf, "[T]eleport", offsetx + 160, maxy - offsety2 - 81, 8);

	// eXit
	font->paint_text_fixedwidth(ibuf, "[X]it", offsetx + 160, offsety5, 8);
}

void CheatScreen::NormalActivate(
		std::string& input, KeyCodes& command, Cheat_Prompt& mode) {
	const int      npc  = std::atoi(input.data());
	Shape_manager* sman = Shape_manager::get_instance();

	mode = CP_Command;

	switch (command) {
		// God Mode
	case KeyCodes::Key_g:
		cheat.toggle_god();
		break;

		// Wizard Mode
	case KeyCodes::Key_w:
		cheat.toggle_wizard();
		break;

		// Infravision
	case KeyCodes::Key_i:
		cheat.toggle_infravision();
		pal.apply();
		break;

		// Eggs
	case KeyCodes::Key_e:
		cheat.toggle_eggs();
		break;

		// Hack mover
	case KeyCodes::Key_h:
		cheat.toggle_hack_mover();
		break;

		// Set Time
	case KeyCodes::Key_s:
		mode = TimeSetLoop();
		break;

		// - Time Rate
	case KeyCodes::Key_Minus:
		if (clock->get_time_rate() > 0) {
			clock->set_time_rate(clock->get_time_rate() - 1);
		}
		break;

		// + Time Rate
	case KeyCodes::Key_Plus:
		if (clock->get_time_rate() < 20) {
			clock->set_time_rate(clock->get_time_rate() + 1);
		}
		break;

		// Teleport
	case KeyCodes::Key_t:
		TeleportLoop();
		break;

		// NPC Tool
	case KeyCodes::Key_n:
		if (npc < -1 || (npc >= 356 && npc <= 359)) {
			mode = CP_InvalidNPC;
		} else if (npc == -1) {
			mode = CP_Canceled;
		} else if (input.empty()) {
			NPCLoop(-1);
		} else {
			mode = NPCLoop(npc);
		}
		break;

		// Global Flag Editor
	case KeyCodes::Key_f:
		if (npc < -1) {
			mode = CP_InvalidValue;
		} else if (npc > c_last_gflag) {
			mode = CP_InvalidValue;
		} else if (npc == -1 || input.empty()) {
			mode = CP_Canceled;
		} else {
			mode = GlobalFlagLoop(npc);
		}
		break;

		// Paperdolls
	case KeyCodes::Key_p:
		if ((Game::get_game_type() == BLACK_GATE
			 || Game::get_game_type() == EXULT_DEVEL_GAME)
			&& sman->can_use_paperdolls()) {
			sman->set_paperdoll_status(!sman->are_paperdolls_enabled());
			config->set(
					"config/gameplay/bg_paperdolls",
					sman->are_paperdolls_enabled() ? "yes" : "no", true);
		}
		break;

	default:
		break;
	}

	input.clear();
	command = KeyCodes::Key_Unknown;
}

// Checks the input
bool CheatScreen::NormalCheck(
		std::string& input, KeyCodes& command, Cheat_Prompt& mode,
		bool& activate) {
	switch (command) {
		// Simple commands
	case KeyCodes::Key_t:    // Teleport
	case KeyCodes::Key_g:    // God Mode
	case KeyCodes::Key_w:    // Wizard
	case KeyCodes::Key_i:    // iNfravision
	case KeyCodes::Key_s:    // Set Time
	case KeyCodes::Key_e:    // Eggs
	case KeyCodes::Key_h:    // Hack Mover
	case KeyCodes::Key_c:    // Create Item
	case KeyCodes::Key_p:    // Paperdolls
		input    = static_cast<char>(command);
		activate = true;
		break;

		// - Time
	case KeyCodes::Key_KP_Minus:
	case KeyCodes::Key_Minus:
		command  = KeyCodes::Key_Minus;
		input    = static_cast<char>(command);
		activate = true;
		break;

		// + Time
	case KeyCodes::Key_KP_Plus:
	case KeyCodes::Key_Equals:
	case KeyCodes::Key_Plus:
		command  = KeyCodes::Key_Plus;
		input    = static_cast<char>(command);
		activate = true;
		break;

		// NPC Tool
	case KeyCodes::Key_n:
		mode = CP_ChooseNPC;
		break;

		// Global Flag Editor
	case KeyCodes::Key_f:
		mode = CP_GFlagNum;
		break;

		// X and Escape leave
	case KeyCodes::Key_Escape:
	case KeyCodes::Key_x:
		input = static_cast<char>(command);
		return false;

	default:
		mode    = CP_InvalidCom;
		input   = static_cast<char>(command);
		command = KeyCodes::Key_Unknown;
		break;
	}

	return true;
}

//
// Activity Display
//

void CheatScreen::ActivityDisplay() {
	char buf[512];
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsety1 = 99;
#else
	const int offsety1 = 0;
#endif

	for (int i = 0; i < 11; i++) {
		snprintf(buf, sizeof(buf), "%2i %s", i, schedules[i]);
		font->paint_text_fixedwidth(ibuf, buf, 0, i * 9 + offsety1, 8);

		snprintf(buf, sizeof(buf), "%2i %s", i + 11, schedules[i + 11]);
		font->paint_text_fixedwidth(ibuf, buf, 112, i * 9 + offsety1, 8);

		if (i != 10) {
			snprintf(buf, sizeof(buf), "%2i %s", i + 22, schedules[i + 22]);
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
	std::string input;
	KeyCodes    command;
	bool        activate = false;

	int day  = 0;
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
			const int val = std::atoi(input.data());

			if (val == -1) {
				return CP_Canceled;
			} else if (val < -1) {
				return CP_InvalidTime;
			} else if (mode == CP_Day) {
				day  = val;
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
			input.clear();
			command = KeyCodes::Key_Unknown;
			continue;
		}

		SharedInput(input, command, mode, activate);
	}

	return CP_ClockSet;
}

//
// Global Flags
//

CheatScreen::Cheat_Prompt CheatScreen::GlobalFlagLoop(int num) {
	bool looping = true;
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx  = 15;
	const int offsety1 = 83;
	const int offsety2 = 72;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsety2 = maxy - 36;
#endif

	// This is for the prompt message
	Cheat_Prompt mode = CP_Command;

	// This is the command
	std::string input;
	int         i;
	KeyCodes    command  = KeyCodes::Key_Unknown;
	bool        activate = false;
	char        buf[512];

	Usecode_machine* usecode = Game_window::get_instance()->get_usecode();

	while (looping) {
		gwin->clear_screen();

#if defined(__IPHONEOS__) || defined(ANDROID)
		// on small screens we want lean and mean, so begone NormalDisplay
		font->paint_text_fixedwidth(ibuf, "Global Flags", 15, 0, 8);
#else
		NormalDisplay();
#endif

		// First the info
		snprintf(buf, sizeof(buf), "Global Flag %i", num);
		font->paint_text_fixedwidth(
				ibuf, buf, offsetx, maxy - offsety1 - 99, 8);

		snprintf(
				buf, sizeof(buf), "Flag is %s",
				usecode->get_global_flag(num) ? "SET" : "UNSET");
		font->paint_text_fixedwidth(
				ibuf, buf, offsetx, maxy - offsety1 - 90, 8);

		// Now the Menu Column
		if (!usecode->get_global_flag(num)) {
			font->paint_text_fixedwidth(
					ibuf, "[S]et Flag", offsetx + 160, maxy - offsety1 - 99, 8);
		} else {
			font->paint_text_fixedwidth(
					ibuf, "[U]nset Flag", offsetx + 160, maxy - offsety1 - 99,
					8);
		}

		// Change Flag

		font->paint_text_fixedwidth(
				ibuf, "[*] Change Flag", offsetx, maxy - offsety1 - 72, 8);
		if (num > 0 && num < c_last_gflag) {
			font->paint_text_fixedwidth(
					ibuf, "[+-] Scroll Flags", offsetx, maxy - offsety1 - 63,
					8);
		} else if (num == 0) {
			font->paint_text_fixedwidth(
					ibuf, "[+] Scroll Flags", offsetx, maxy - offsety1 - 63, 8);
		} else {
			font->paint_text_fixedwidth(
					ibuf, "[-] Scroll Flags", offsetx, maxy - offsety1 - 63, 8);
		}
		font->paint_text_fixedwidth(ibuf, "[X]it", offsetx, offsety2, 8);

		// Finally the Prompt...
		SharedPrompt(input, mode);

		// Draw it!
		gwin->get_win()->show();

		// Check to see if we need to change menus
		if (activate) {
			mode = CP_Command;
			if (command == KeyCodes::Key_Minus) {    // Decrement
				num--;
				if (num < 0) {
					num = 0;
				}
			} else if (command == KeyCodes::Key_Plus) {    // Increment
				num++;
				if (num > c_last_gflag) {
					num = c_last_gflag;
				}
			} else if (command == KeyCodes::Key_Asterisk) {    // Change Flag
				i = std::atoi(input.data());
				if (i < -1 || i > c_last_gflag) {
					mode = CP_InvalidValue;
				} else if (i == -1) {
					mode = CP_Canceled;
				} else if (!input.empty()) {
					num = i;
				}
			} else if (command == KeyCodes::Key_s) {    // Set
				usecode->set_global_flag(num, 1);
			} else if (command == KeyCodes::Key_u) {    // Unset
				usecode->set_global_flag(num, 0);
			}

			input.clear();
			command  = KeyCodes::Key_Unknown;
			activate = false;
			continue;
		}

		if (SharedInput(input, command, mode, activate)) {
			switch (command) {
				// Simple commands
			case KeyCodes::Key_s:    // Set Flag
			case KeyCodes::Key_u:    // Unset flag
				input    = static_cast<char>(command);
				activate = true;
				break;

				// Decrement
			case KeyCodes::Key_KP_Minus:
			case KeyCodes::Key_Minus:
				command  = KeyCodes::Key_Minus;
				input    = static_cast<char>(command);
				activate = true;
				break;

				// Increment
			case KeyCodes::Key_KP_Plus:
			case KeyCodes::Key_Equals:
			case KeyCodes::Key_Plus:
				command  = KeyCodes::Key_Plus;
				input    = static_cast<char>(command);
				activate = true;
				break;

				// * Change Flag
			case KeyCodes::Key_KP_Multiply:
			case KeyCodes::Key_8:
			case KeyCodes::Key_Asterisk:
				command = KeyCodes::Key_Asterisk;
				input.clear();
				mode = CP_GFlagNum;
				break;

				// X and Escape leave
			case KeyCodes::Key_Escape:
			case KeyCodes::Key_x:
				input   = static_cast<char>(command);
				looping = false;
				break;

			default:
				mode    = CP_InvalidCom;
				input   = static_cast<char>(command);
				command = KeyCodes::Key_Unknown;
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
	Actor* actor;

	bool looping = true;

	// This is for the prompt message
	Cheat_Prompt mode = CP_Command;

	// This is the command
	std::string input;
	KeyCodes    command;
	bool        activate = false;

	while (looping) {
		if (num == -1) {
			actor = grabbed;
		} else {
			actor = gwin->get_npc(num);
		}
		grabbed = actor;
		if (actor != nullptr) {
			num = actor->get_npc_num();
		}

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

		if (SharedInput(input, command, mode, activate)) {
			looping = NPCCheck(input, command, mode, activate, actor, num);
		}
	}
	return CP_Command;
}

void CheatScreen::NPCDisplay(Actor* actor, int& num) {
	char buf[512];
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx  = 15;
	const int offsety1 = 73;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
#endif
	if (actor != nullptr) {
		const Tile_coord t = actor->get_tile();

		// Paint the actors shape
		Shape_frame* shape = actor->get_shape();
		if (shape != nullptr) {
			actor->paint_shape(shape->get_xright() + 240, shape->get_yabove());
		}

		// Now the info
		const std::string namestr = actor->get_npc_name();
		snprintf(buf, sizeof(buf), "NPC %i - %s", num, namestr.c_str());
		font->paint_text_fixedwidth(ibuf, buf, offsetx, 0, 8);

		snprintf(buf, sizeof(buf), "Loc (%04i, %04i, %02i)", t.tx, t.ty, t.tz);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, 9, 8);

		snprintf(
				buf, sizeof(buf), "Shape %04i:%02i  %s", actor->get_shapenum(),
				actor->get_framenum(),
				actor->get_flag(Obj_flags::met) ? "Met" : "Not Met");
		font->paint_text_fixedwidth(ibuf, buf, offsetx, 18, 8);

		snprintf(
				buf, sizeof(buf), "Current Activity: %2i - %s",
				actor->get_schedule_type(),
				schedules[actor->get_schedule_type()]);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 36, 8);

		snprintf(
				buf, sizeof(buf), "Experience: %i",
				actor->get_property(Actor::exp));
		font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 45, 8);

		snprintf(buf, sizeof(buf), "Level: %i", actor->get_level());
		font->paint_text_fixedwidth(ibuf, buf, offsetx + 144, offsety1 + 45, 8);

		snprintf(
				buf, sizeof(buf), "Training: %2i  Health: %2i",
				actor->get_property(Actor::training),
				actor->get_property(Actor::health));
		font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 54, 8);

		if (num != -1) {
			int ucitemnum = 0x10000 - num;
			if (num == 0) {
				ucitemnum = 0xfe9c;
			}
			snprintf(
					buf, sizeof(buf), "Usecode item %4x function %x", ucitemnum,
					actor->get_usecode());
			font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 63, 8);
		} else {
			snprintf(
					buf, sizeof(buf), "Usecode function %x",
					actor->get_usecode());
			font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 63, 8);
		}

		if (actor->get_flag(Obj_flags::charmed)) {
			snprintf(
					buf, sizeof(buf), "Alignment: %s (orig: %s)",
					alignments[actor->get_effective_alignment()],
					alignments[actor->get_alignment()]);
		} else {
			snprintf(
					buf, sizeof(buf), "Alignment: %s",
					alignments[actor->get_alignment()]);
		}
		font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 72, 8);

		if (actor->get_polymorph() != -1) {
			snprintf(
					buf, sizeof(buf), "Polymorphed from %04i",
					actor->get_polymorph());
			font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 81, 8);
		}
	} else {
		snprintf(buf, sizeof(buf), "NPC %i - Invalid NPC!", num);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, 0, 8);
	}
}

void CheatScreen::NPCMenu(Actor* actor, int& num) {
	ignore_unused_variable_warning(num);
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx  = 15;
	const int offsety1 = 74;
	const int offsetx2 = 15;
	const int offsety2 = 72;
	const int offsetx3 = 175;
	const int offsety3 = 63;
	const int offsety4 = 72;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsetx2 = 0;
	const int offsety2 = maxy - 36;
	const int offsetx3 = offsetx + 160;
	const int offsety3 = maxy - 45;
	const int offsety4 = maxy - 36;
#endif
	// Left Column

	if (actor != nullptr) {
		// Business Activity
		font->paint_text_fixedwidth(
				ibuf, "[B]usiness Activity", offsetx, maxy - offsety1 - 99, 8);

		// Change Shape
		font->paint_text_fixedwidth(
				ibuf, "[C]hange Shape", offsetx, maxy - offsety1 - 90, 8);

		// XP
		font->paint_text_fixedwidth(
				ibuf, "[E]xperience", offsetx, maxy - offsety1 - 81, 8);

		// NPC Flags
		font->paint_text_fixedwidth(
				ibuf, "[N]pc Flags", offsetx, maxy - offsety1 - 72, 8);

		// Name
		font->paint_text_fixedwidth(
				ibuf, "[1] Name", offsetx, maxy - offsety1 - 63, 8);
	}

	// eXit
	font->paint_text_fixedwidth(ibuf, "[X]it", offsetx2, offsety2, 8);

	// Right Column

	if (actor != nullptr) {
		// Stats
		font->paint_text_fixedwidth(
				ibuf, "[S]tats", offsetx + 160, maxy - offsety1 - 99, 8);

		// Training Points
		font->paint_text_fixedwidth(
				ibuf, "[2] Training Points", offsetx + 160,
				maxy - offsety1 - 90, 8);

		// Teleport
		font->paint_text_fixedwidth(
				ibuf, "[T]eleport", offsetx + 160, maxy - offsety1 - 81, 8);

		// Change NPC
		font->paint_text_fixedwidth(
				ibuf, "[*] Change NPC", offsetx3, offsety3, 8);
	}

	// Change NPC
	font->paint_text_fixedwidth(
			ibuf, "[+-] Scroll NPCs", offsetx3, offsety4, 8);
}

void CheatScreen::NPCActivate(
		std::string& input, KeyCodes& command, Cheat_Prompt& mode, Actor* actor,
		int& num) {
	int       i = std::atoi(input.data());
	const int nshapes
			= Shape_manager::get_instance()->get_shapes().get_num_shapes();

	mode = CP_Command;

	if (command == KeyCodes::Key_Minus) {
		num--;
		if (num < 0) {
			num = 0;
		} else if (num >= 356 && num <= 359) {
			num = 355;
		}
	} else if (command == KeyCodes::Key_Plus) {
		num++;
		if (num >= 356 && num <= 359) {
			num = 360;
		}
	} else if (command == KeyCodes::Key_Asterisk) {    // Change NPC
		if (i < -1 || (i >= 356 && i <= 359)) {
			mode = CP_InvalidNPC;
		} else if (i == -1) {
			mode = CP_Canceled;
		} else if (!input.empty()) {
			num = i;
		}
	} else if (actor != nullptr) {
		switch (command) {
		case KeyCodes::Key_b:    // Business
			BusinessLoop(actor);
			break;

		case KeyCodes::Key_n:    // Npc flags
			FlagLoop(actor);
			break;

		case KeyCodes::Key_s:    // stats
			StatLoop(actor);
			break;

		case KeyCodes::Key_t:    // Teleport
			Game_window::get_instance()->teleport_party(
					actor->get_tile(), false, actor->get_map_num());
			break;

		case KeyCodes::Key_e:    // Experience
			if (i < 0) {
				mode = CP_Canceled;
			} else {
				actor->set_property(Actor::exp, i);
			}
			break;

		case KeyCodes::Key_2:    // Training Points
			if (i < 0) {
				mode = CP_Canceled;
			} else {
				actor->set_property(Actor::training, i);
			}
			break;

		case KeyCodes::Key_c:         // Change shape
			if (input[0] == 'b') {    // Browser
				int n;
				if (!cheat.get_browser_shape(i, n)) {
					mode = CP_WrongShapeFile;
					break;
				}
			}

			if (i == -1) {
				mode = CP_Canceled;
			} else if (i < 0) {
				mode = CP_InvalidShape;
			} else if (i >= nshapes) {
				mode = CP_InvalidShape;
			} else if (
					!input.empty() && (input[0] != '-' || input.size() > 1)) {
				if (actor->get_npc_num() != 0) {
					actor->set_shape(i);
				} else {
					actor->set_polymorph(i);
				}
				mode = CP_ShapeSet;
			}
			break;

		case KeyCodes::Key_1:    // Name
			if (input.empty()) {
				mode = CP_Canceled;
			} else {
				actor->set_npc_name(input.data());
				mode = CP_NameSet;
			}
			break;

		default:
			break;
		}
	}
	input.clear();
	command = KeyCodes::Key_Unknown;
}

// Checks the input
bool CheatScreen::NPCCheck(
		std::string& input, KeyCodes& command, Cheat_Prompt& mode,
		bool& activate, Actor* actor, int& num) {
	ignore_unused_variable_warning(num);
	switch (command) {
		// Simple commands
	case KeyCodes::Key_a:    // Attack mode
	case KeyCodes::Key_b:    // BUsiness
	case KeyCodes::Key_n:    // Npc flags
	case KeyCodes::Key_d:    // pop weapon
	case KeyCodes::Key_s:    // stats
	case KeyCodes::Key_z:    // Target
	case KeyCodes::Key_t:    // Teleport
		input = static_cast<char>(command);
		if (actor == nullptr) {
			mode = CP_InvalidCom;
		} else {
			activate = true;
		}
		break;

		// Value entries
	case KeyCodes::Key_e:    // Experience
	case KeyCodes::Key_2:    // Training Points
		if (actor == nullptr) {
			mode = CP_InvalidCom;
		} else {
			mode = CP_EnterValue;
		}
		break;

		// Change shape
	case KeyCodes::Key_c:
		if (actor == nullptr) {
			mode = CP_InvalidCom;
		} else {
			mode = CP_Shape;
		}
		break;

		// Name
	case KeyCodes::Key_1:
		if (actor == nullptr) {
			mode = CP_InvalidCom;
		} else {
			mode = CP_Name;
		}
		break;

		// - NPC
	case KeyCodes::Key_KP_Minus:
	case KeyCodes::Key_Minus:
		command  = KeyCodes::Key_Minus;
		input    = static_cast<char>(command);
		activate = true;
		break;

		// + NPC
	case KeyCodes::Key_KP_Plus:
	case KeyCodes::Key_Equals:
	case KeyCodes::Key_Plus:
		command  = KeyCodes::Key_Plus;
		input    = static_cast<char>(command);
		activate = true;
		break;

		// * Change NPC
	case KeyCodes::Key_KP_Multiply:
	case KeyCodes::Key_8:
	case KeyCodes::Key_Asterisk:
		command = KeyCodes::Key_Asterisk;
		input.clear();
		mode = CP_ChooseNPC;
		break;

		// X and Escape leave
	case KeyCodes::Key_Escape:
	case KeyCodes::Key_x:
		input = static_cast<char>(command);
		return false;

	default:
		mode    = CP_InvalidCom;
		input   = static_cast<char>(command);
		command = KeyCodes::Key_Unknown;
		break;
	}

	return true;
}

//
// NPC Flags
//

void CheatScreen::FlagLoop(Actor* actor) {
#if !defined(__IPHONEOS__) && !defined(ANDROID)
	int num = actor->get_npc_num();
#endif
	bool looping = true;

	// This is for the prompt message
	Cheat_Prompt mode = CP_Command;

	// This is the command
	std::string input;
	KeyCodes    command;
	bool        activate = false;

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

		if (SharedInput(input, command, mode, activate)) {
			looping = FlagCheck(input, command, mode, activate, actor);
		}
	}
}

void CheatScreen::FlagMenu(Actor* actor) {
	char buf[512];
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx  = 10;
	const int offsetx1 = 6;
	const int offsety1 = 92;
#else
	const int offsetx  = 0;
	const int offsetx1 = 0;
	const int offsety1 = 0;
#endif

	// Left Column

	// Asleep
	snprintf(
			buf, sizeof(buf), "[A] Asleep.%c",
			actor->get_flag(Obj_flags::asleep) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 108, 8);

	// Charmed
	snprintf(
			buf, sizeof(buf), "[B] Charmd.%c",
			actor->get_flag(Obj_flags::charmed) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 99, 8);

	// Cursed
	snprintf(
			buf, sizeof(buf), "[C] Cursed.%c",
			actor->get_flag(Obj_flags::cursed) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 90, 8);

	// Paralyzed
	snprintf(
			buf, sizeof(buf), "[D] Prlyzd.%c",
			actor->get_flag(Obj_flags::paralyzed) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 81, 8);

	// Poisoned
	snprintf(
			buf, sizeof(buf), "[E] Poisnd.%c",
			actor->get_flag(Obj_flags::poisoned) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 72, 8);

	// Protected
	snprintf(
			buf, sizeof(buf), "[F] Prtctd.%c",
			actor->get_flag(Obj_flags::protection) ? 'Y' : 'N');
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 63, 8);

	// Advanced Editor
	font->paint_text_fixedwidth(
			ibuf, "[*] Advanced", offsetx, maxy - offsety1 - 45, 8);

	// Exit
	font->paint_text_fixedwidth(
			ibuf, "[X]it", offsetx, maxy - offsety1 - 36, 8);

	// Center Column

	// Party
	snprintf(
			buf, sizeof(buf), "[I] Party..%c",
			actor->get_flag(Obj_flags::in_party) ? 'Y' : 'N');
	font->paint_text_fixedwidth(
			ibuf, buf, offsetx1 + 104, maxy - offsety1 - 108, 8);

	// Invisible
	snprintf(
			buf, sizeof(buf), "[J] Invsbl.%c",
			actor->get_flag(Obj_flags::invisible) ? 'Y' : 'N');
	font->paint_text_fixedwidth(
			ibuf, buf, offsetx1 + 104, maxy - offsety1 - 99, 8);

	// Fly
	snprintf(
			buf, sizeof(buf), "[K] Fly....%c",
			actor->get_type_flag(Actor::tf_fly) ? 'Y' : 'N');
	font->paint_text_fixedwidth(
			ibuf, buf, offsetx1 + 104, maxy - offsety1 - 90, 8);

	// Walk
	snprintf(
			buf, sizeof(buf), "[L] Walk...%c",
			actor->get_type_flag(Actor::tf_walk) ? 'Y' : 'N');
	font->paint_text_fixedwidth(
			ibuf, buf, offsetx1 + 104, maxy - offsety1 - 81, 8);

	// Swim
	snprintf(
			buf, sizeof(buf), "[M] Swim...%c",
			actor->get_type_flag(Actor::tf_swim) ? 'Y' : 'N');
	font->paint_text_fixedwidth(
			ibuf, buf, offsetx1 + 104, maxy - offsety1 - 72, 8);

	// Ethereal
	snprintf(
			buf, sizeof(buf), "[N] Ethrel.%c",
			actor->get_type_flag(Actor::tf_ethereal) ? 'Y' : 'N');
	font->paint_text_fixedwidth(
			ibuf, buf, offsetx1 + 104, maxy - offsety1 - 63, 8);

	// Protectee
	snprintf(buf, sizeof(buf), "[O] Prtcee.%c", '?');
	font->paint_text_fixedwidth(
			ibuf, buf, offsetx1 + 104, maxy - offsety1 - 54, 8);

	// Conjured
	snprintf(
			buf, sizeof(buf), "[P] Conjrd.%c",
			actor->get_type_flag(Actor::tf_conjured) ? 'Y' : 'N');
	font->paint_text_fixedwidth(
			ibuf, buf, offsetx1 + 104, maxy - offsety1 - 45, 8);

	// Tournament (Original is SI only -- allowing for BG in Exult)
	snprintf(
			buf, sizeof(buf), "[3] Tourna.%c",
			actor->get_flag(Obj_flags::tournament) ? 'Y' : 'N');
	font->paint_text_fixedwidth(
			ibuf, buf, offsetx1 + 104, maxy - offsety1 - 36, 8);

	// Naked (AV ONLY)
	if (actor->get_npc_num() == 0) {
		snprintf(
				buf, sizeof(buf), "[7] Naked..%c",
				actor->get_flag(Obj_flags::naked) ? 'Y' : 'N');
		font->paint_text_fixedwidth(
				ibuf, buf, offsetx1 + 104, maxy - offsety1 - 27, 8);
	}

	// Right Column

	// Summoned
	snprintf(
			buf, sizeof(buf), "[Q] Summnd.%c",
			actor->get_type_flag(Actor::tf_summonned) ? 'Y' : 'N');
	font->paint_text_fixedwidth(
			ibuf, buf, offsetx1 + 208, maxy - offsety1 - 108, 8);

	// Bleeding
	snprintf(
			buf, sizeof(buf), "[R] Bleedn.%c",
			actor->get_type_flag(Actor::tf_bleeding) ? 'Y' : 'N');
	font->paint_text_fixedwidth(
			ibuf, buf, offsetx1 + 208, maxy - offsety1 - 99, 8);

	if (actor->get_npc_num() == 0) {    // Avatar
		// Sex
		snprintf(
				buf, sizeof(buf), "[S] Sex....%c",
				actor->get_type_flag(Actor::tf_sex) ? 'F' : 'M');
		font->paint_text_fixedwidth(
				ibuf, buf, offsetx1 + 208, maxy - offsety1 - 90, 8);

		// Skin
		snprintf(buf, sizeof(buf), "[1] Skin...%d", actor->get_skin_color());
		font->paint_text_fixedwidth(
				ibuf, buf, offsetx1 + 208, maxy - offsety1 - 81, 8);

		// Read
		snprintf(
				buf, sizeof(buf), "[4] Read...%c",
				actor->get_flag(Obj_flags::read) ? 'Y' : 'N');
		font->paint_text_fixedwidth(
				ibuf, buf, offsetx1 + 208, maxy - offsety1 - 72, 8);
	} else {    // Not Avatar
		// Met
		snprintf(
				buf, sizeof(buf), "[T] Met....%c",
				actor->get_flag(Obj_flags::met) ? 'Y' : 'N');
		font->paint_text_fixedwidth(
				ibuf, buf, offsetx1 + 208, maxy - offsety1 - 90, 8);

		// NoCast
		snprintf(
				buf, sizeof(buf), "[U] NoCast.%c",
				actor->get_flag(Obj_flags::no_spell_casting) ? 'Y' : 'N');
		font->paint_text_fixedwidth(
				ibuf, buf, offsetx1 + 208, maxy - offsety1 - 81, 8);

		// ID
		snprintf(buf, sizeof(buf), "[V] ID#:%02i", actor->get_ident());
		font->paint_text_fixedwidth(
				ibuf, buf, offsetx1 + 208, maxy - offsety1 - 72, 8);
	}

	// Freeze
	snprintf(
			buf, sizeof(buf), "[W] Freeze.%c",
			actor->get_flag(Obj_flags::freeze) ? 'Y' : 'N');
	font->paint_text_fixedwidth(
			ibuf, buf, offsetx1 + 208, maxy - offsety1 - 63, 8);

	// Party
	if (actor->is_in_party()) {
		// Temp
		snprintf(buf, sizeof(buf), "[Y] Temp: %02i", actor->get_temperature());
		font->paint_text_fixedwidth(
				ibuf, buf, offsetx1 + 208, maxy - offsety1 - 54, 8);

		// Conjured
		snprintf(buf, sizeof(buf), "Warmth: %04i", actor->figure_warmth());
		font->paint_text_fixedwidth(
				ibuf, buf, offsetx1 + 208, maxy - offsety1 - 45, 8);
	}

	// Polymorph
	snprintf(
			buf, sizeof(buf), "[2] Polymo.%c",
			actor->get_flag(Obj_flags::polymorph) ? 'Y' : 'N');
	font->paint_text_fixedwidth(
			ibuf, buf, offsetx1 + 208, maxy - offsety1 - 36, 8);

	// Patra (AV SI ONLY)
	if (actor->get_npc_num() == 0) {
		snprintf(
				buf, sizeof(buf), "[5] Petra..%c",
				actor->get_flag(Obj_flags::petra) ? 'Y' : 'N');
		font->paint_text_fixedwidth(
				ibuf, buf, offsetx1 + 208, maxy - offsety1 - 27, 8);
	}
}

void CheatScreen::FlagActivate(
		std::string& input, KeyCodes& command, Cheat_Prompt& mode,
		Actor* actor) {
	int       i = std::atoi(input.data());
	const int nshapes
			= Shape_manager::get_instance()->get_shapes().get_num_shapes();

	mode = CP_Command;
	switch (command) {
		// Everyone

		// Toggles
	case KeyCodes::Key_a:    // Asleep
		if (actor->get_flag(Obj_flags::asleep)) {
			actor->clear_flag(Obj_flags::asleep);
		} else {
			actor->set_flag(Obj_flags::asleep);
		}
		break;

	case KeyCodes::Key_b:    // Charmed
		if (actor->get_flag(Obj_flags::charmed)) {
			actor->clear_flag(Obj_flags::charmed);
		} else {
			actor->set_flag(Obj_flags::charmed);
		}
		break;

	case KeyCodes::Key_c:    // Cursed
		if (actor->get_flag(Obj_flags::cursed)) {
			actor->clear_flag(Obj_flags::cursed);
		} else {
			actor->set_flag(Obj_flags::cursed);
		}
		break;

	case KeyCodes::Key_d:    // Paralyzed
		if (actor->get_flag(Obj_flags::paralyzed)) {
			actor->clear_flag(Obj_flags::paralyzed);
		} else {
			actor->set_flag(Obj_flags::paralyzed);
		}
		break;

	case KeyCodes::Key_e:    // Poisoned
		if (actor->get_flag(Obj_flags::poisoned)) {
			actor->clear_flag(Obj_flags::poisoned);
		} else {
			actor->set_flag(Obj_flags::poisoned);
		}
		break;

	case KeyCodes::Key_f:    // Protected
		if (actor->get_flag(Obj_flags::protection)) {
			actor->clear_flag(Obj_flags::protection);
		} else {
			actor->set_flag(Obj_flags::protection);
		}
		break;

	case KeyCodes::Key_j:    // Invisible
		if (actor->get_flag(Obj_flags::invisible)) {
			actor->clear_flag(Obj_flags::invisible);
		} else {
			actor->set_flag(Obj_flags::invisible);
		}
		pal.apply();
		break;

	case KeyCodes::Key_k:    // Fly
		if (actor->get_type_flag(Actor::tf_fly)) {
			actor->clear_type_flag(Actor::tf_fly);
		} else {
			actor->set_type_flag(Actor::tf_fly);
		}
		break;

	case KeyCodes::Key_l:    // Walk
		if (actor->get_type_flag(Actor::tf_walk)) {
			actor->clear_type_flag(Actor::tf_walk);
		} else {
			actor->set_type_flag(Actor::tf_walk);
		}
		break;

	case KeyCodes::Key_m:    // Swim
		if (actor->get_type_flag(Actor::tf_swim)) {
			actor->clear_type_flag(Actor::tf_swim);
		} else {
			actor->set_type_flag(Actor::tf_swim);
		}
		break;

	case KeyCodes::Key_n:    // Ethereal
		if (actor->get_type_flag(Actor::tf_ethereal)) {
			actor->clear_type_flag(Actor::tf_ethereal);
		} else {
			actor->set_type_flag(Actor::tf_ethereal);
		}
		break;

	case KeyCodes::Key_p:    // Conjured
		if (actor->get_type_flag(Actor::tf_conjured)) {
			actor->clear_type_flag(Actor::tf_conjured);
		} else {
			actor->set_type_flag(Actor::tf_conjured);
		}
		break;

	case KeyCodes::Key_q:    // Summoned
		if (actor->get_type_flag(Actor::tf_summonned)) {
			actor->clear_type_flag(Actor::tf_summonned);
		} else {
			actor->set_type_flag(Actor::tf_summonned);
		}
		break;

	case KeyCodes::Key_r:    // Bleeding
		if (actor->get_type_flag(Actor::tf_bleeding)) {
			actor->clear_type_flag(Actor::tf_bleeding);
		} else {
			actor->set_type_flag(Actor::tf_bleeding);
		}
		break;

	case KeyCodes::Key_s:    // Sex
		if (actor->get_type_flag(Actor::tf_sex)) {
			actor->clear_type_flag(Actor::tf_sex);
		} else {
			actor->set_type_flag(Actor::tf_sex);
		}
		break;

	case KeyCodes::Key_4:    // Read
		if (actor->get_flag(Obj_flags::read)) {
			actor->clear_flag(Obj_flags::read);
		} else {
			actor->set_flag(Obj_flags::read);
		}
		break;

	case KeyCodes::Key_5:    // Petra
		if (actor->get_flag(Obj_flags::petra)) {
			actor->clear_flag(Obj_flags::petra);
		} else {
			actor->set_flag(Obj_flags::petra);
		}
		break;

	case KeyCodes::Key_7:    // Naked
		if (actor->get_flag(Obj_flags::naked)) {
			actor->clear_flag(Obj_flags::naked);
		} else {
			actor->set_flag(Obj_flags::naked);
		}
		break;

	case KeyCodes::Key_t:    // Met
		if (actor->get_flag(Obj_flags::met)) {
			actor->clear_flag(Obj_flags::met);
		} else {
			actor->set_flag(Obj_flags::met);
		}
		break;

	case KeyCodes::Key_u:    // No Cast
		if (actor->get_flag(Obj_flags::no_spell_casting)) {
			actor->clear_flag(Obj_flags::no_spell_casting);
		} else {
			actor->set_flag(Obj_flags::no_spell_casting);
		}
		break;

	case KeyCodes::Key_z:    // Zombie
		if (actor->get_flag(Obj_flags::si_zombie)) {
			actor->clear_flag(Obj_flags::si_zombie);
		} else {
			actor->set_flag(Obj_flags::si_zombie);
		}
		break;

	case KeyCodes::Key_w:    // Freeze
		if (actor->get_flag(Obj_flags::freeze)) {
			actor->clear_flag(Obj_flags::freeze);
		} else {
			actor->set_flag(Obj_flags::freeze);
		}
		break;

	case KeyCodes::Key_i:    // Party
		if (actor->get_flag(Obj_flags::in_party)) {
			gwin->get_party_man()->remove_from_party(actor);
			gwin->revert_schedules(actor);
			// Just to be sure.
			actor->clear_flag(Obj_flags::in_party);
		} else if (gwin->get_party_man()->add_to_party(actor)) {
			actor->set_schedule_type(Schedule::follow_avatar);
		}
		break;

	case KeyCodes::Key_o:    // Protectee
		break;

		// Value
	case KeyCodes::Key_v:    // ID
		if (i < -1) {
			mode = CP_InvalidValue;
		} else if (i > 63) {
			mode = CP_InvalidValue;
		} else if (i == -1 || input.empty()) {
			mode = CP_Canceled;
		} else {
			actor->set_ident(i);
		}
		break;

	case KeyCodes::Key_1:    // Skin color
		actor->set_skin_color(Shapeinfo_lookup::GetNextSkin(
				actor->get_skin_color(), actor->get_type_flag(Actor::tf_sex),
				Shape_manager::get_instance()->have_si_shapes()));
		break;

	case KeyCodes::Key_3:    // Tournament
		if (actor->get_flag(Obj_flags::tournament)) {
			actor->clear_flag(Obj_flags::tournament);
		} else {
			actor->set_flag(Obj_flags::tournament);
		}
		break;

	case KeyCodes::Key_y:    // Warmth
		if (i < -1) {
			mode = CP_InvalidValue;
		} else if (i > 63) {
			mode = CP_InvalidValue;
		} else if (i == -1 || input.empty()) {
			mode = CP_Canceled;
		} else {
			actor->set_temperature(i);
		}
		break;

	case KeyCodes::Key_2:    // Polymorph
		// Clear polymorph
		if (actor->get_polymorph() != -1) {
			actor->set_polymorph(actor->get_polymorph());
			break;
		}

		if (input[0] == 'b') {    // Browser
			int n;
			if (!cheat.get_browser_shape(i, n)) {
				mode = CP_WrongShapeFile;
				break;
			}
		}

		if (i == -1) {
			mode = CP_Canceled;
		} else if (i < 0) {
			mode = CP_InvalidShape;
		} else if (i >= nshapes) {
			mode = CP_InvalidShape;
		} else if (!input.empty() && (input[0] != '-' || input.size() > 1)) {
			actor->set_polymorph(i);
			mode = CP_ShapeSet;
		}

		break;

		// Advanced Numeric Flag Editor
	case KeyCodes::Key_KP_Multiply:
	case KeyCodes::Key_Asterisk:
	case KeyCodes::Key_8:
		if (i < -1) {
			mode = CP_InvalidValue;
		} else if (i > 63) {
			mode = CP_InvalidValue;
		} else if (i == -1 || input.empty()) {
			mode = CP_Canceled;
		} else {
			mode = AdvancedFlagLoop(i, actor);
		}
		break;

	default:
		break;
	}
	input.clear();
	command = KeyCodes::Key_Unknown;
}

// Checks the input
bool CheatScreen::FlagCheck(
		std::string& input, KeyCodes& command, Cheat_Prompt& mode,
		bool& activate, Actor* actor) {
	switch (command) {
		// Everyone

		// Toggles
	case KeyCodes::Key_a:    // Asleep
	case KeyCodes::Key_b:    // Charmed
	case KeyCodes::Key_c:    // Cursed
	case KeyCodes::Key_d:    // Paralyzed
	case KeyCodes::Key_e:    // Poisoned
	case KeyCodes::Key_f:    // Protected
	case KeyCodes::Key_i:    // Party
	case KeyCodes::Key_j:    // Invisible
	case KeyCodes::Key_k:    // Fly
	case KeyCodes::Key_l:    // Walk
	case KeyCodes::Key_m:    // Swim
	case KeyCodes::Key_n:    // Ethrel
	case KeyCodes::Key_o:    // Protectee
	case KeyCodes::Key_p:    // Conjured
	case KeyCodes::Key_q:    // Summoned
	case KeyCodes::Key_r:    // Bleedin
	case KeyCodes::Key_w:    // Freeze
	case KeyCodes::Key_3:    // Tournament
		activate = true;
		input    = static_cast<char>(command);
		break;

		// Value
	case KeyCodes::Key_2:    // Polymorph
		if (actor->get_polymorph() == -1) {
			mode = CP_Shape;
			input.clear();
		} else {
			activate = true;
			input    = static_cast<char>(command);
		}
		break;

		// Party Only

		// Value
	case KeyCodes::Key_y:    // Temp
		if (!actor->is_in_party()) {
			command = KeyCodes::Key_Unknown;
		} else {
			mode = CP_TempNum;
		}
		input.clear();
		break;

		// Avatar Only

		// Toggles
	case KeyCodes::Key_s:    // Sex
	case KeyCodes::Key_4:    // Read
		if (actor->get_npc_num() != 0) {
			command = KeyCodes::Key_Unknown;
		} else {
			activate = true;
		}
		input = static_cast<char>(command);
		break;

		// Toggles SI
	case KeyCodes::Key_5:    // Petra
	case KeyCodes::Key_7:    // Naked
		if (actor->get_npc_num() != 0) {
			command = KeyCodes::Key_Unknown;
		} else {
			activate = true;
		}
		input = static_cast<char>(command);
		break;

		// Value SI
	case KeyCodes::Key_1:    // Skin
		if (actor->get_npc_num() != 0) {
			command = KeyCodes::Key_Unknown;
		} else {
			activate = true;
		}
		input = static_cast<char>(command);
		break;

		// Everyone but avatar

		// Toggles
	case KeyCodes::Key_t:    // Met
	case KeyCodes::Key_u:    // No Cast
	case KeyCodes::Key_z:    // Zombie
		if (actor->get_npc_num() == 0) {
			command = KeyCodes::Key_Unknown;
		} else {
			activate = true;
		}
		input = static_cast<char>(command);
		break;

		// Value
	case KeyCodes::Key_v:    // ID
		if (actor->get_npc_num() == 0) {
			command = KeyCodes::Key_Unknown;
		} else {
			mode = CP_EnterValue;
		}
		input.clear();
		break;

		// NPC Flag Editor

	case KeyCodes::Key_KP_Multiply:
	case KeyCodes::Key_8:
	case KeyCodes::Key_Asterisk:
		command = KeyCodes::Key_Asterisk;
		input.clear();
		mode = CP_NFlagNum;
		break;

		// X and Escape leave
	case KeyCodes::Key_Escape:
	case KeyCodes::Key_x:
		input = static_cast<char>(command);
		return false;

		// Unknown
	default:
		command = KeyCodes::Key_Unknown;
		break;
	}

	return true;
}

//
// Business Schedules
//

void CheatScreen::BusinessLoop(Actor* actor) {
	bool looping = true;

	// This is for the prompt message
	Cheat_Prompt mode = CP_Command;

	// This is the command
	std::string input;
	KeyCodes    command;
	bool        activate = false;
	int         time     = 0;
	int         prev     = 0;

	while (looping) {
		gwin->clear_screen();

		// First the display
		if (mode == CP_Activity) {
			ActivityDisplay();
		} else {
			BusinessDisplay(actor);
		}

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

		if (SharedInput(input, command, mode, activate)) {
			looping = BusinessCheck(
					input, command, mode, activate, actor, time);
		}
	}
}

void CheatScreen::BusinessDisplay(Actor* actor) {
	char             buf[512];
	const Tile_coord t = actor->get_tile();
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx  = 10;
	const int offsety1 = 20;
	const int offsetx2 = 161;
	const int offsety2 = 8;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsetx2 = offsetx;
	const int offsety2 = 16;
#endif

	// Now the info
	const std::string namestr = actor->get_npc_name();
	snprintf(
			buf, sizeof(buf), "NPC %i - %s - Schedules:", actor->get_npc_num(),
			namestr.c_str());
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 0, 8);

	snprintf(buf, sizeof(buf), "Loc (%04i, %04i, %02i)", t.tx, t.ty, t.tz);
	font->paint_text_fixedwidth(ibuf, buf, 0, 8, 8);

#if defined(__IPHONEOS__) || defined(ANDROID)
	const char activity_msg[] = "-Act: %2i %s";
#else
	const char activity_msg[] = "Current Activity:  %2i - %s";
#endif
	snprintf(
			buf, sizeof(buf), activity_msg, actor->get_schedule_type(),
			schedules[actor->get_schedule_type()]);
	font->paint_text_fixedwidth(ibuf, buf, offsetx2, offsety2, 8);

	// Avatar can't have schedules
	if (actor->get_npc_num() > 0) {
#if !defined(__IPHONEOS__) && !defined(ANDROID)
		font->paint_text_fixedwidth(ibuf, "Schedules:", offsetx, 16, 8);
#endif

		Schedule_change* scheds;
		int              num;
		int              types[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
		int              x[8];
		int              y[8];

		actor->get_schedules(scheds, num);

		for (int i = 0; i < num; i++) {
			const int time        = scheds[i].get_time();
			types[time]           = scheds[i].get_type();
			const Tile_coord tile = scheds[i].get_pos();
			x[time]               = tile.tx;
			y[time]               = tile.ty;
		}

		font->paint_text_fixedwidth(ibuf, "12 AM:", offsetx, 36 - offsety1, 8);
		font->paint_text_fixedwidth(ibuf, " 3 AM:", offsetx, 44 - offsety1, 8);
		font->paint_text_fixedwidth(ibuf, " 6 AM:", offsetx, 52 - offsety1, 8);
		font->paint_text_fixedwidth(ibuf, " 9 AM:", offsetx, 60 - offsety1, 8);
		font->paint_text_fixedwidth(ibuf, "12 PM:", offsetx, 68 - offsety1, 8);
		font->paint_text_fixedwidth(ibuf, " 3 PM:", offsetx, 76 - offsety1, 8);
		font->paint_text_fixedwidth(ibuf, " 6 PM:", offsetx, 84 - offsety1, 8);
		font->paint_text_fixedwidth(ibuf, " 9 PM:", offsetx, 92 - offsety1, 8);

		for (int i = 0; i < 8; i++) {
			if (types[i] != -1) {
				snprintf(
						buf, sizeof(buf), "%2i (%4i,%4i) - %s", types[i], x[i],
						y[i], schedules[types[i]]);
				font->paint_text_fixedwidth(
						ibuf, buf, offsetx + 56, (36 - offsety1) + i * 8, 8);
			}
		}
	}
}

void CheatScreen::BusinessMenu(Actor* actor) {
	// Left Column
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx = 10;
#else
	const int offsetx = 0;
#endif
	// Might break on monster npcs?
	if (actor->get_npc_num() > 0) {
		font->paint_text_fixedwidth(
				ibuf, "12 AM: [A] Set [I] Location [1] Clear", offsetx,
				maxy - 96, 8);
		font->paint_text_fixedwidth(
				ibuf, " 3 AM: [B] Set [J] Location [2] Clear", offsetx,
				maxy - 88, 8);
		font->paint_text_fixedwidth(
				ibuf, " 6 AM: [C] Set [K] Location [3] Clear", offsetx,
				maxy - 80, 8);
		font->paint_text_fixedwidth(
				ibuf, " 9 AM: [D] Set [L] Location [4] Clear", offsetx,
				maxy - 72, 8);
		font->paint_text_fixedwidth(
				ibuf, "12 PM: [E] Set [M] Location [5] Clear", offsetx,
				maxy - 64, 8);
		font->paint_text_fixedwidth(
				ibuf, " 3 PM: [F] Set [N] Location [6] Clear", offsetx,
				maxy - 56, 8);
		font->paint_text_fixedwidth(
				ibuf, " 6 PM: [G] Set [O] Location [7] Clear", offsetx,
				maxy - 48, 8);
		font->paint_text_fixedwidth(
				ibuf, " 9 PM: [H] Set [P] Location [8] Clear", offsetx,
				maxy - 40, 8);

		font->paint_text_fixedwidth(
				ibuf, "[S]et Current Activity [X]it [R]evert", offsetx,
				maxy - 30, 8);
	} else {
		font->paint_text_fixedwidth(
				ibuf, "[S]et Current Activity [X]it", offsetx, maxy - 30, 8);
	}
}

void CheatScreen::BusinessActivate(
		std::string& input, KeyCodes& command, Cheat_Prompt& mode, Actor* actor,
		int& time, int& prev) {
	int i = std::atoi(input.data());

	mode               = CP_Command;
	const KeyCodes old = std::exchange(command, KeyCodes::Key_Unknown);
	switch (old) {
	case KeyCodes::Key_a:    // Set Activity
		if (i < -1 || i > 31) {
			mode = CP_InvalidValue;
		} else if (i == -1) {
			mode = CP_Canceled;
		} else if (input.empty()) {
			mode    = CP_Activity;
			command = KeyCodes::Key_a;
		} else {
			actor->set_schedule_time_type(time, i);
		}
		break;

	case KeyCodes::Key_i:    // X Coord
		if (i < -1 || i > c_num_tiles) {
			mode = CP_InvalidValue;
		} else if (i == -1) {
			mode = CP_Canceled;
		} else if (input.empty()) {
			mode    = CP_XCoord;
			command = KeyCodes::Key_i;
		} else {
			prev    = i;
			mode    = CP_YCoord;
			command = KeyCodes::Key_j;
		}
		break;

	case KeyCodes::Key_j:    // Y Coord
		if (i < -1 || i > c_num_tiles) {
			mode = CP_InvalidValue;
		} else if (i == -1) {
			mode = CP_Canceled;
		} else if (input.empty()) {
			mode    = CP_YCoord;
			command = KeyCodes::Key_j;
		} else {
			actor->set_schedule_time_location(time, prev, i);
		}
		break;

	case KeyCodes::Key_1:    // Clear
		actor->remove_schedule(time);
		break;

	case KeyCodes::Key_s:    // Set Current
		if (i < -1 || i > 31) {
			mode = CP_InvalidValue;
		} else if (i == -1) {
			mode = CP_Canceled;
		} else if (input.empty()) {
			mode    = CP_Activity;
			command = KeyCodes::Key_s;
		} else {
			actor->set_schedule_type(i);
		}
		break;

	case KeyCodes::Key_r:    // Revert
		Game_window::get_instance()->revert_schedules(actor);
		break;

	default:
		break;
	}
	input.clear();
}

// Checks the input
bool CheatScreen::BusinessCheck(
		std::string& input, KeyCodes& command, Cheat_Prompt& mode,
		bool& activate, Actor* actor, int& time) {
	// Might break on monster npcs?
	if (actor->get_npc_num() > 0) {
		switch (command) {
		case KeyCodes::Key_a:
		case KeyCodes::Key_b:
		case KeyCodes::Key_c:
		case KeyCodes::Key_d:
		case KeyCodes::Key_e:
		case KeyCodes::Key_f:
		case KeyCodes::Key_g:
		case KeyCodes::Key_h:
			time    = command - KeyCodes::Key_a;
			command = KeyCodes::Key_a;
			mode    = CP_Activity;
			return true;

		case KeyCodes::Key_i:
		case KeyCodes::Key_j:
		case KeyCodes::Key_k:
		case KeyCodes::Key_l:
		case KeyCodes::Key_m:
		case KeyCodes::Key_n:
		case KeyCodes::Key_o:
		case KeyCodes::Key_p:
			time    = command - KeyCodes::Key_i;
			command = KeyCodes::Key_i;
			mode    = CP_XCoord;
			return true;

		case KeyCodes::Key_1:
		case KeyCodes::Key_2:
		case KeyCodes::Key_3:
		case KeyCodes::Key_4:
		case KeyCodes::Key_5:
		case KeyCodes::Key_6:
		case KeyCodes::Key_7:
		case KeyCodes::Key_8:
			time     = command - KeyCodes::Key_1;
			command  = KeyCodes::Key_1;
			activate = true;
			return true;

		case KeyCodes::Key_r:
			command  = KeyCodes::Key_r;
			activate = true;
			return true;

		default:
			break;
		}
	}

	switch (command) {
		// Set Current
	case KeyCodes::Key_s:
		command = KeyCodes::Key_s;
		input.clear();
		mode = CP_Activity;
		break;

		// X and Escape leave
	case KeyCodes::Key_Escape:
	case KeyCodes::Key_x:
		input = static_cast<char>(command);
		return false;

		// Unknown
	default:
		command = KeyCodes::Key_Unknown;
		mode    = CP_InvalidCom;
		break;
	}

	return true;
}

//
// NPC Stats
//

void CheatScreen::StatLoop(Actor* actor) {
#if !defined(__IPHONEOS__) && !defined(ANDROID)
	int num = actor->get_npc_num();
#endif
	bool looping = true;

	// This is for the prompt message
	Cheat_Prompt mode = CP_Command;

	// This is the command
	std::string input;
	KeyCodes    command;
	bool        activate = false;

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

		if (SharedInput(input, command, mode, activate)) {
			looping = StatCheck(input, command, mode, activate, actor);
		}
	}
}

void CheatScreen::StatMenu(Actor* actor) {
	char buf[512];
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx  = 15;
	const int offsety1 = 92;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
#endif

	// Left Column

	// Dexterity
	snprintf(
			buf, sizeof(buf), "[D]exterity....%3i",
			actor->get_property(Actor::dexterity));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 108, 8);

	// Food Level
	snprintf(
			buf, sizeof(buf), "[F]ood Level...%3i",
			actor->get_property(Actor::food_level));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 99, 8);

	// Intelligence
	snprintf(
			buf, sizeof(buf), "[I]ntellicence.%3i",
			actor->get_property(Actor::intelligence));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 90, 8);

	// Strength
	snprintf(
			buf, sizeof(buf), "[S]trength.....%3i",
			actor->get_property(Actor::strength));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 81, 8);

	// Combat Skill
	snprintf(
			buf, sizeof(buf), "[C]ombat Skill.%3i",
			actor->get_property(Actor::combat));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 72, 8);

	// Hit Points
	snprintf(
			buf, sizeof(buf), "[H]it Points...%3i",
			actor->get_property(Actor::health));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 63, 8);

	// Magic
	// Magic Points
	snprintf(
			buf, sizeof(buf), "[M]agic Points.%3i",
			actor->get_property(Actor::magic));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 54, 8);

	// Mana
	snprintf(
			buf, sizeof(buf), "[V]ana Level...%3i",
			actor->get_property(Actor::mana));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 45, 8);

	// Exit
	font->paint_text_fixedwidth(
			ibuf, "[X]it", offsetx, maxy - offsety1 - 36, 8);
}

void CheatScreen::StatActivate(
		std::string& input, KeyCodes& command, Cheat_Prompt& mode,
		Actor* actor) {
	int i = std::atoi(input.data());
	mode  = CP_Command;
	// Enforce sane bounds.
	if (i > 60) {
		i = 60;
	} else if (i < 0 && command != KeyCodes::Key_h) {
		if (i == -1) {    // canceled
			input.clear();
			command = KeyCodes::Key_Unknown;
			return;
		}
		i = 0;
	} else if (i < -50) {
		i = -50;
	}

	switch (command) {
	case KeyCodes::Key_d:    // Dexterity
		actor->set_property(Actor::dexterity, i);
		break;

	case KeyCodes::Key_f:    // Food Level
		actor->set_property(Actor::food_level, i);
		break;

	case KeyCodes::Key_i:    // Intelligence
		actor->set_property(Actor::intelligence, i);
		break;

	case KeyCodes::Key_s:    // Strength
		actor->set_property(Actor::strength, i);
		break;

	case KeyCodes::Key_c:    // Combat Points
		actor->set_property(Actor::combat, i);
		break;

	case KeyCodes::Key_h:    // Hit Points
		actor->set_property(Actor::health, i);
		break;

	case KeyCodes::Key_m:    // Magic
		actor->set_property(Actor::magic, i);
		break;

	case KeyCodes::Key_v:    // [V]ana
		actor->set_property(Actor::mana, i);
		break;

	default:
		break;
	}
	input.clear();
	command = KeyCodes::Key_Unknown;
}

// Checks the input
bool CheatScreen::StatCheck(
		std::string& input, KeyCodes& command, Cheat_Prompt& mode,
		bool& activate, Actor* actor) {
	ignore_unused_variable_warning(activate, actor);
	switch (command) {
		// Everyone
	case KeyCodes::Key_h:    // Hit Points
		input.clear();
		mode = CP_EnterValueNoCancel;
		break;
	case KeyCodes::Key_d:    // Dexterity
	case KeyCodes::Key_f:    // Food Level
	case KeyCodes::Key_i:    // Intelligence
	case KeyCodes::Key_s:    // Strength
	case KeyCodes::Key_c:    // Combat Points
	case KeyCodes::Key_m:    // Magic
	case KeyCodes::Key_v:    // [V]ana
		input.clear();
		mode = CP_EnterValue;
		break;

		// X and Escape leave
	case KeyCodes::Key_Escape:
	case KeyCodes::Key_x:
		input = static_cast<char>(command);
		return false;

		// Unknown
	default:
		command = KeyCodes::Key_Unknown;
		break;
	}

	return true;
}

//
// Advanced Flag Edition
//

CheatScreen::Cheat_Prompt CheatScreen::AdvancedFlagLoop(int num, Actor* actor) {
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx  = 15;
	const int offsety1 = 83;
	const int offsety2 = 72;
#else
	int       npc_num  = actor->get_npc_num();
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsety2 = maxy - 36;
#endif
	bool looping = true;
	// This is for the prompt message
	Cheat_Prompt mode = CP_Command;

	// This is the command
	std::string input;
	KeyCodes    command  = KeyCodes::Key_Unknown;
	bool        activate = false;
	char        buf[512];

	while (looping) {
		gwin->clear_screen();

#if !defined(__IPHONEOS__) && !defined(ANDROID)
		NPCDisplay(actor, npc_num);
#endif

		if (num < 0) {
			num = 0;
		} else if (num > 63) {
			num = 63;
		}

		// First the info
		if (flag_names[num] != nullptr) {
			snprintf(buf, sizeof(buf), "NPC Flag %i: %s", num, flag_names[num]);
		} else {
			snprintf(buf, sizeof(buf), "NPC Flag %i", num);
		}
		font->paint_text_fixedwidth(
				ibuf, buf, offsetx, maxy - offsety1 - 108, 8);

		snprintf(
				buf, sizeof(buf), "Flag is %s",
				actor->get_flag(num) ? "SET" : "UNSET");
		font->paint_text_fixedwidth(
				ibuf, buf, offsetx, maxy - offsety1 - 90, 8);

		// Now the Menu Column
		if (!actor->get_flag(num)) {
			font->paint_text_fixedwidth(
					ibuf, "[S]et Flag", offsetx + 160, maxy - offsety1 - 90, 8);
		} else {
			font->paint_text_fixedwidth(
					ibuf, "[U]nset Flag", offsetx + 160, maxy - offsety1 - 90,
					8);
		}

		// Change Flag
		font->paint_text_fixedwidth(
				ibuf, "[*] Change Flag", offsetx, maxy - offsety1 - 72, 8);
		if (num > 0 && num < 63) {
			font->paint_text_fixedwidth(
					ibuf, "[+-] Scroll Flags", offsetx, maxy - offsety1 - 63,
					8);
		} else if (num == 0) {
			font->paint_text_fixedwidth(
					ibuf, "[+] Scroll Flags", offsetx, maxy - offsety1 - 63, 8);
		} else {
			font->paint_text_fixedwidth(
					ibuf, "[-] Scroll Flags", offsetx, maxy - offsety1 - 63, 8);
		}

		font->paint_text_fixedwidth(ibuf, "[X]it", offsetx, offsety2, 8);

		// Finally the Prompt...
		SharedPrompt(input, mode);

		// Draw it!
		gwin->get_win()->show();

		// Check to see if we need to change menus
		if (activate) {
			mode = CP_Command;
			if (command == KeyCodes::Key_Minus) {    // Decrement
				num--;
				if (num < 0) {
					num = 0;
				}
			} else if (command == KeyCodes::Key_Plus) {    // Increment
				num++;
				if (num > 63) {
					num = 63;
				}
			} else if (command == KeyCodes::Key_Asterisk) {    // Change Flag
				int i = std::atoi(input.data());
				if (i < -1 || i > 63) {
					mode = CP_InvalidValue;
				} else if (i == -1) {
					mode = CP_Canceled;
				} else if (!input.empty()) {
					num = i;
				}
			} else if (command == KeyCodes::Key_s) {    // Set
				actor->set_flag(num);
				if (num == Obj_flags::in_party) {
					gwin->get_party_man()->add_to_party(actor);
					actor->set_schedule_type(Schedule::follow_avatar);
				}
			} else if (command == KeyCodes::Key_u) {    // Unset
				if (num == Obj_flags::in_party) {
					gwin->get_party_man()->remove_from_party(actor);
					gwin->revert_schedules(actor);
				}
				actor->clear_flag(num);
			}

			input.clear();
			command  = KeyCodes::Key_Unknown;
			activate = false;
			continue;
		}

		if (SharedInput(input, command, mode, activate)) {
			switch (command) {
				// Simple commands
			case KeyCodes::Key_s:    // Set Flag
			case KeyCodes::Key_u:    // Unset flag
				input    = static_cast<char>(command);
				activate = true;
				break;

				// Decrement
			case KeyCodes::Key_KP_Minus:
			case KeyCodes::Key_Minus:
				command  = KeyCodes::Key_Minus;
				input    = static_cast<char>(command);
				activate = true;
				break;

				// Increment
			case KeyCodes::Key_KP_Plus:
			case KeyCodes::Key_Equals:
			case KeyCodes::Key_Plus:
				command  = KeyCodes::Key_Plus;
				input    = static_cast<char>(command);
				activate = true;
				break;

				// * Change Flag
			case KeyCodes::Key_KP_Multiply:
			case KeyCodes::Key_8:
			case KeyCodes::Key_Asterisk:
				command = KeyCodes::Key_Asterisk;
				input.clear();
				mode = CP_NFlagNum;
				break;

				// X and Escape leave
			case KeyCodes::Key_Escape:
			case KeyCodes::Key_x:
				input   = static_cast<char>(command);
				looping = false;
				break;

			default:
				mode    = CP_InvalidCom;
				input   = static_cast<char>(command);
				command = KeyCodes::Key_Unknown;
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
	std::string input;
	KeyCodes    command;
	bool        activate = false;
	int         prev     = 0;

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

		if (SharedInput(input, command, mode, activate)) {
			looping = TeleportCheck(input, command, mode, activate);
		}
	}
}

void CheatScreen::TeleportDisplay() {
	char             buf[512];
	const Tile_coord t       = gwin->get_main_actor()->get_tile();
	const int        curmap  = gwin->get_map()->get_num();
	const int        highest = Find_highest_map();
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx  = 15;
	const int offsety1 = 54;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
#endif

#if defined(__IPHONEOS__) || defined(ANDROID)
	font->paint_text_fixedwidth(
			ibuf, "Teleport Menu - Dangerous!", offsetx, 0, 8);
#else
	font->paint_text_fixedwidth(ibuf, "Teleport Menu", offsetx, 0, 8);
	font->paint_text_fixedwidth(
			ibuf, "Dangerous - use with care!", offsetx, 18, 8);
#endif

	const int longi = ((t.tx - 0x3A5) / 10);
	const int lati  = ((t.ty - 0x46E) / 10);
#if defined(__IPHONEOS__) || defined(ANDROID)
	snprintf(
			buf, sizeof(buf), "Coords %d %s %d %s, Map #%d of %d", abs(lati),
			(lati < 0 ? "North" : "South"), abs(longi),
			(longi < 0 ? "West" : "East"), curmap, highest);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 9, 8);
#else
	snprintf(
			buf, sizeof(buf), "Coordinates   %d %s %d %s", abs(lati),
			(lati < 0 ? "North" : "South"), abs(longi),
			(longi < 0 ? "West" : "East"));
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 63, 8);
#endif

	snprintf(
			buf, sizeof(buf), "Coords in hex (%04x, %04x, %02x)", t.tx, t.ty,
			t.tz);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 72 - offsety1, 8);

	snprintf(
			buf, sizeof(buf), "Coords in dec (%04i, %04i, %02i)", t.tx, t.ty,
			t.tz);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 81 - offsety1, 8);

#if !defined(__IPHONEOS__) && !defined(ANDROID)
	snprintf(buf, sizeof(buf), "On Map #%d of %d", curmap, highest);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 90, 8);
#endif
}

void CheatScreen::TeleportMenu() {
#if defined(__IPHONEOS__) || defined(ANDROID)
	const int offsetx  = 15;
	const int offsety1 = 64;
	const int offsetx2 = 175;
	const int offsety2 = 63;
	const int offsety3 = 72;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsetx2 = offsetx;
	const int offsety2 = maxy - 63;
	const int offsety3 = maxy - 36;
#endif

	// Left Column
	// Geo
	font->paint_text_fixedwidth(
			ibuf, "[G]eographic Coordinates", offsetx, maxy - offsety1 - 99, 8);

	// Hex
	font->paint_text_fixedwidth(
			ibuf, "[H]exadecimal Coordinates", offsetx, maxy - offsety1 - 90,
			8);

	// Dec
	font->paint_text_fixedwidth(
			ibuf, "[D]ecimal Coordinates", offsetx, maxy - offsety1 - 81, 8);

	// NPC
	font->paint_text_fixedwidth(
			ibuf, "[N]PC Number", offsetx, maxy - offsety1 - 72, 8);

	// Map
	font->paint_text_fixedwidth(ibuf, "[M]ap Number", offsetx2, offsety2, 8);

	// eXit
	font->paint_text_fixedwidth(ibuf, "[X]it", offsetx, offsety3, 8);
}

void CheatScreen::TeleportActivate(
		std::string& input, KeyCodes& command, Cheat_Prompt& mode, int& prev) {
	int        i = std::atoi(input.data());
	static int lat;
	Tile_coord t       = gwin->get_main_actor()->get_tile();
	const int  highest = Find_highest_map();

	mode = CP_Command;
	switch (command) {
	case KeyCodes::Key_g:    // North or South
		if (input.empty()) {
			mode    = CP_NorthSouth;
			command = KeyCodes::Key_g;
		} else if (input[0] == 'n' || input[0] == 's') {
			prev = input[0];
			if (prev == 'n') {
				mode = CP_NLatitude;
			} else {
				mode = CP_SLatitude;
			}
			command = KeyCodes::Key_a;
		} else {
			mode = CP_InvalidValue;
		}
		break;

	case KeyCodes::Key_a:    // latitude
		if (i < -1 || (prev == 'n' && i > 113) || (prev == 's' && i > 193)) {
			mode = CP_InvalidValue;
		} else if (i == -1) {
			mode = CP_Canceled;
		} else if (input.empty()) {
			if (prev == 'n') {
				mode = CP_NLatitude;
			} else {
				mode = CP_SLatitude;
			}
			command = KeyCodes::Key_a;
		} else {
			if (prev == 'n') {
				lat = ((i * -10) + 0x46E);
			} else {
				lat = ((i * 10) + 0x46E);
			}
			mode    = CP_WestEast;
			command = KeyCodes::Key_b;
		}
		break;

	case KeyCodes::Key_b:    // West or East
		if (input.empty()) {
			mode    = CP_WestEast;
			command = KeyCodes::Key_b;
		} else if (input[0] == 'w' || input[0] == 'e') {
			prev = input[0];
			if (prev == 'w') {
				mode = CP_WLongitude;
			} else {
				mode = CP_ELongitude;
			}
			command = KeyCodes::Key_c;
		} else {
			mode = CP_InvalidValue;
		}
		break;

	case KeyCodes::Key_c:    // longitude
		if (i < -1 || (prev == 'w' && i > 93) || (prev == 'e' && i > 213)) {
			mode = CP_InvalidValue;
		} else if (i == -1) {
			mode = CP_Canceled;
		} else if (input.empty()) {
			if (prev == 'w') {
				mode = CP_WLongitude;
			} else {
				mode = CP_ELongitude;
			}
			command = KeyCodes::Key_c;
		} else {
			if (prev == 'w') {
				t.tx = ((i * -10) + 0x3A5);
			} else {
				t.tx = ((i * 10) + 0x3A5);
			}
			t.ty = lat;
			t.tz = 0;
			gwin->teleport_party(t);
		}
		break;

	case KeyCodes::Key_h:    // hex X coord
		i = strtol(input.data(), nullptr, 16);
		if (i < -1 || i > c_num_tiles) {
			mode = CP_InvalidValue;
		} else if (i == -1) {
			mode = CP_Canceled;
		} else if (input.empty()) {
			mode    = CP_HexXCoord;
			command = KeyCodes::Key_h;
		} else {
			prev    = i;
			mode    = CP_HexYCoord;
			command = KeyCodes::Key_i;
		}
		break;

	case KeyCodes::Key_i:    // hex Y coord
		i = strtol(input.data(), nullptr, 16);
		if (i < -1 || i > c_num_tiles) {
			mode = CP_InvalidValue;
		} else if (i == -1) {
			mode = CP_Canceled;
		} else if (input.empty()) {
			mode    = CP_HexYCoord;
			command = KeyCodes::Key_i;
		} else {
			t.tx = prev;
			t.ty = i;
			t.tz = 0;
			gwin->teleport_party(t);
		}
		break;

	case KeyCodes::Key_d:    // dec X coord
		if (i < -1 || i > c_num_tiles) {
			mode = CP_InvalidValue;
		} else if (i == -1) {
			mode = CP_Canceled;
		} else if (input.empty()) {
			mode    = CP_XCoord;
			command = KeyCodes::Key_d;
		} else {
			prev    = i;
			mode    = CP_YCoord;
			command = KeyCodes::Key_e;
		}
		break;

	case KeyCodes::Key_e:    // dec Y coord
		if (i < -1 || i > c_num_tiles) {
			mode = CP_InvalidValue;
		} else if (i == -1) {
			mode = CP_Canceled;
		} else if (input.empty()) {
			mode    = CP_YCoord;
			command = KeyCodes::Key_e;
		} else {
			t.tx = prev;
			t.ty = i;
			t.tz = 0;
			gwin->teleport_party(t);
		}
		break;

	case KeyCodes::Key_n:    // NPC
		if (i < -1 || (i >= 356 && i <= 359)) {
			mode = CP_InvalidNPC;
		} else if (i == -1) {
			mode = CP_Canceled;
		} else {
			Actor* actor = gwin->get_npc(i);
			Game_window::get_instance()->teleport_party(
					actor->get_tile(), false, actor->get_map_num());
		}
		break;

	case KeyCodes::Key_m:    // map
		if (i == -1) {
			mode = CP_Canceled;
		} else if ((i < 0 || i > 255) || i > highest) {
			mode = CP_InvalidValue;
		} else {
			gwin->teleport_party(gwin->get_main_actor()->get_tile(), true, i);
		}
		break;

	default:
		break;
	}
	input.clear();
}

// Checks the input
bool CheatScreen::TeleportCheck(
		std::string& input, KeyCodes& command, Cheat_Prompt& mode,
		bool& activate) {
	ignore_unused_variable_warning(activate);
	switch (command) {
		// Simple commands
	case KeyCodes::Key_g:    // geographic
		mode = CP_NorthSouth;
		return true;

	case KeyCodes::Key_h:    // hex
		mode = CP_HexXCoord;
		return true;

	case KeyCodes::Key_d:    // dec teleport
		mode = CP_XCoord;
		return true;

	case KeyCodes::Key_n:    // NPC teleport
		mode = CP_ChooseNPC;
		break;

	case KeyCodes::Key_m:    // NPC teleport
		mode = CP_EnterValue;
		break;

		// X and Escape leave
	case KeyCodes::Key_Escape:
	case KeyCodes::Key_x:
		input = static_cast<char>(command);
		return false;

	default:
		command = KeyCodes::Key_Unknown;
		mode    = CP_InvalidCom;
		break;
	}

	return true;
}
