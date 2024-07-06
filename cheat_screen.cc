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

// Disable the gcc warning because we cannot fix it in SDL's headers
#if defined(__GNUC__)
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#include "cheat_screen.h"

#include "Configuration.h"
#include "Gump_manager.h"
#include "actors.h"
#include "cheat.h"
#include "chunks.h"
#include "exult.h"
#include "files/U7file.h"
#include "font.h"
#include "game.h"
#include "gameclk.h"
#include "gamemap.h"
#include "gamewin.h"
#include "gump_utils.h"
#include "ignore_unused_variable_warning.h"
#include "imagewin.h"
#include "miscinf.h"
#include "party.h"
#include "schedule.h"
#include "touchui.h"
#include "ucmachine.h"
#include "version.h"
#include "vgafile.h"

#include <cstring>

// #define TEST_MOBILE 1

static const SDL_MouseID EXSDL_TOUCH_MOUSEID = SDL_TOUCH_MOUSEID;

// renable the warning that was disabled above
#if defined(__GNUC__)
#	pragma GCC diagnostic pop
#endif

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

int CheatScreen::Get_highest_map() {
	if (highest_map != INT_MIN) {
		return highest_map;
	}
	int n = 0;
	int next;
	while ((next = Find_next_map(n + 1, 10)) != -1) {
		n = next;
	}
	highest_map = n;
	return n;
}

void CheatScreen::show_screen() {
	gwin = Game_window::get_instance();
	ibuf = gwin->get_win()->get_ib8();

	if (!font) {
		// Try to get the Font form Blackgate first because it looks better than
		// the SI one
		font = std::make_shared<Font>();
		if (font->load(U7MAINSHP_FLX, 9, 1) != 0) {
			font.reset();
		}
	}

	// Get the font for this game if don't already have it
	if (!font) {
		font = fontManager.get_font("MENU_FONT");
	}
	clock = gwin->get_clock();
	maxx  = gwin->get_width();
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
	maxy = 200;
#else
	maxy = gwin->get_height();
#endif
	centerx            = maxx / 2;
	centery            = maxy / 2;
	SDL_Window* window = gwin->get_win()->get_screen_window();
	if (touchui != nullptr) {
		touchui->hideGameControls();
		SDL_StartTextInput(window);
	}

	// Pause the game
	gwin->get_tqueue()->pause(SDL_GetTicks());

	const str_int_pair& pal_tuple_static = game->get_resource("palettes/0");
	const str_int_pair& pal_tuple_patch
			= game->get_resource("palettes/patch/0");
	pal.load(pal_tuple_static.str, pal_tuple_patch.str, pal_tuple_static.num);
	pal.apply();

	// std::memset(highlighttable.colors, 1, sizeof(highlighttable.colors));
	// highlighttable.colors[0]   = 0;
	// highlighttable.colors[255] = 255;
	const int remaps[] = {
			5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 - 1,
	};

	pal.Generate_remap_xformtable(highlighttable.colors, remaps);

	const int hoverremaps[] = {
			1, 1, 1, 1, 1, 6, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 - 1,
	};

	pal.Generate_remap_xformtable(hovertable.colors, hoverremaps);
	ClearState clear(state);

	Mouse::mouse->hide();
	Mouse::Mouse_shapes saveshape = Mouse::mouse->get_shape();
	Mouse::mouse->set_shape(Mouse::hand);

	buttons_down.clear();
	// Start the loop
	NormalLoop();

	Mouse::mouse->set_shape(saveshape);
	Mouse::mouse->hide();
	gwin->paint();
	Mouse::mouse->show();

	// Resume the game clock
	gwin->get_tqueue()->resume(SDL_GetTicks());

	// Reset the palette
	clock->reset_palette();
	if (touchui != nullptr) {
		Gump_manager* gumpman = gwin->get_gump_man();
		if (!gumpman->gump_mode()) {
			touchui->showGameControls();
		}
		if (SDL_TextInputActive(window)) {
			SDL_StopTextInput(window);
		}
	}
}

//
// DISPLAYS
//

//
// Shared
//

void CheatScreen::SharedPrompt() {
	char buf[64];

#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
	const int prompt    = 81;
	const int promptmes = 90;
	const int offsetx   = 15;
#else
	const int prompt    = maxy - 18;
	const int promptmes = maxy - 9;
	const int offsetx   = 0;
#endif
	font->paint_text_fixedwidth(ibuf, "Select->", offsetx, prompt, 8);

	// Special handling for arrows when not doing text input
	const char* input = state.input;
	if (state.GetMode() < CP_ChooseNPC && !input[1]
		&& (*input == '<' || *input == '>' || *input == '^' || *input == 'V')) {
		PaintArrow(64 + offsetx, prompt, *input);
		input = " ";
	}
	if (input && std::strlen(input)) {
		font->paint_text_fixedwidth(ibuf, input, 64 + offsetx, prompt, 8);
		font->paint_text_fixedwidth(
				ibuf, "_", 64 + offsetx + int(std::strlen(input)) * 8, prompt,
				8);
	} else {
		font->paint_text_fixedwidth(ibuf, "_", 64 + offsetx, prompt, 8);
	}
	// Clear the input
	if (state.GetMode() < CP_ChooseNPC) {
		state.input[0] = 0;
	}
	// ...and Prompt Message
	switch (state.GetMode()) {
	default:

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
				ibuf, "Which NPC? (ESC to cancel.)", offsetx, promptmes, 8);
		break;

	case CP_EnterValue:
		font->paint_text_fixedwidth(
				ibuf, "Enter Value. (ESC to cancel.)", offsetx, promptmes, 8);
		break;

	case CP_CustomValue:
		if (state.custom_prompt) {
			font->paint_text_fixedwidth(
					ibuf, state.custom_prompt, offsetx, promptmes, 8);
		}
		break;

	case CP_EnterValueNoCancel:
		font->paint_text_fixedwidth(
				ibuf, "Enter Value.", offsetx, promptmes, 8);
		break;

	case CP_Minute:
		font->paint_text_fixedwidth(
				ibuf, "Enter Minute. (ESC to cancel.)", offsetx, promptmes, 8);
		break;

	case CP_Hour:
		font->paint_text_fixedwidth(
				ibuf, "Enter Hour. (ESC to cancel.)", offsetx, promptmes, 8);
		break;

	case CP_Day:
		font->paint_text_fixedwidth(
				ibuf, "Enter Day. (ESC to cancel.)", offsetx, promptmes, 8);
		break;

	case CP_Shape:
		snprintf(
				buf, std::size(buf),
				"Enter Shape Max %i (B=Browse or ESC=Cancel)",
				Shape_manager::get_instance()->get_shapes().get_num_shapes()
						- 1);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8);
		break;

	case CP_Activity:
		font->paint_text_fixedwidth(
				ibuf, "Enter Activity 0-31. (ESC to cancel.)", offsetx,
				promptmes, 8);
		break;

	case CP_XCoord:
		snprintf(
				buf, sizeof(buf), "Enter X Coord. Max %i (ESC to cancel)",
				c_num_tiles);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8);
		break;

	case CP_YCoord:
		snprintf(
				buf, sizeof(buf), "Enter Y Coord. Max %i (ESC to cancel)",
				c_num_tiles);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8);
		break;

	case CP_Lift:
		font->paint_text_fixedwidth(
				ibuf, "Enter Lift. (ESC to cancel)", offsetx, promptmes, 8);
		break;

	case CP_GFlagNum: {
		snprintf(
				buf, sizeof(buf), "Enter Global Flag 0-%d. (ESC to cancel)",
				c_last_gflag);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8);
		break;
	}

	case CP_NFlagNum:
		font->paint_text_fixedwidth(
				ibuf, "Enter NPC Flag 0-63. (ESC to cancel)", offsetx,
				promptmes, 8);
		break;

	case CP_TempNum:
		font->paint_text_fixedwidth(
				ibuf, "Enter Temperature 0-63. (ESC to cancel)", offsetx,
				promptmes, 8);
		break;

	case CP_NLatitude:
		font->paint_text_fixedwidth(
				ibuf, "Enter Latitude. Max 113 (ESC to cancel)", offsetx,
				promptmes, 8);
		break;

	case CP_SLatitude:
		font->paint_text_fixedwidth(
				ibuf, "Enter Latitude. Max 193 (ESC to cancel)", offsetx,
				promptmes, 8);
		break;

	case CP_WLongitude:
		font->paint_text_fixedwidth(
				ibuf, "Enter Longitude. Max 93 (ESC to cancel)", offsetx,
				promptmes, 8);
		break;

	case CP_ELongitude:
		font->paint_text_fixedwidth(
				ibuf, "Enter Longitude. Max 213 (ESC to cancel)", offsetx,
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
				buf, sizeof(buf), "Enter X Coord. Max %04x (ESC to cancel)",
				c_num_tiles);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8);
		break;

	case CP_HexYCoord:
		snprintf(
				buf, sizeof(buf), "Enter Y Coord. Max %04x (ESC to cancel)",
				c_num_tiles);
		font->paint_text_fixedwidth(ibuf, buf, offsetx, promptmes, 8);
		break;
	}
}

static void resizeline(float& axis1, float delta1, float& axis2) {
	float slope = axis2 / axis1;
	axis1 += delta1;
	axis2 = axis1 * slope;
}

bool CheatScreen::SharedInput() {
	SDL_Event event;
	SDL_Window* window = gwin->get_win()->get_screen_window();
	// Do repaint after 100 ms to allow for time dependant effects. 10 FPS seems
	// more that adequate for Cheat Screen If anyone needs to do smooth animaion
	// the can change this
	auto repainttime = SDL_GetTicks() + 100;
	Mouse::mouse->hide();    // Turn off mouse.
	std::memset(&event, 0, sizeof(event));

	while (SDL_GetTicks() < repainttime) {
		Delay();
		if (state.highlighttime && state.highlighttime < SDL_GetTicks()) {
			state.highlight     = 0;
			state.highlighttime = 0;
		}

		// How far finger needs to move for swipe to be interpreted as an a
		// cursor key input)
		bool do_swipe = std::abs(state.swipe_dx) > swipe_threshold
						|| std::abs(state.swipe_dy) > swipe_threshold;

		if (!do_swipe && state.last_swipe
			&& ((state.last_swipe + 200) < SDL_GetTicks())) {
			// zero out swipe state if it's been longer that 200ms since last
			// event recieved and it's shorter than the threshold
			state.last_swipe = 0;
			state.swipe_dx   = 0;
			state.swipe_dy   = 0;
		}
		Mouse::mouse_update = false;
		while (do_swipe || SDL_PollEvent(&event)) {
			int         gx, gy;
			SDL_Keycode simulate_key = SDLK_UNKNOWN;

			//
			if (do_swipe) {
				float ax = 0, ay = 0;
				ay = std::abs(state.swipe_dy);
				ax = std::abs(state.swipe_dx);
				CERR("Doing swipe ay: " << ay << " ax:" << ax);
				// More vertical motion or More Horizontal
				// perfectly diagonal motion will be treated as horizontal
				if (ay > ax) {
					// Swipe up the screen
					if (state.swipe_dy < -swipe_threshold) {
						simulate_key = SDLK_UP;
						resizeline(
								state.swipe_dy, +swipe_threshold,
								state.swipe_dx);
						// Swipe Down the screen
					} else if (state.swipe_dy > swipe_threshold) {
						simulate_key = SDLK_DOWN;
						resizeline(
								state.swipe_dy, -swipe_threshold,
								state.swipe_dx);
					}
				} else {
					// swipe right to left
					if (state.swipe_dx < -swipe_threshold) {
						simulate_key = SDLK_LEFT;
						resizeline(
								state.swipe_dx, +swipe_threshold,
								state.swipe_dy);
						// Swipe left to right
					} else if (state.swipe_dx > swipe_threshold) {
						simulate_key = SDLK_RIGHT;
						resizeline(
								state.swipe_dx, -swipe_threshold,
								state.swipe_dy);
					}
				}
			} else {
				CERR("event.type= " << event.type);
				switch (event.type) {
				case SDL_EVENT_QUIT: {
					CERR("SDL_EVENT_QUIT");
					ImageBufferPaintable screenshot;
					if (gwin->get_gump_man()->okay_to_quit(&screenshot)) {
						throw quit_exception();
					}
				} break;
				case SDL_EVENT_FINGER_DOWN: {
					CERR("SDL_EVENT_FINGER_DOWN");
					buttons_down.insert(button_down_finger);
					if ((!Mouse::use_touch_input)
						&& (event.tfinger.fingerID != 0)) {
						Mouse::use_touch_input = true;
					}
					break;
				}
				case SDL_EVENT_FINGER_UP: {
					CERR("SDL_EVENT_FINGER_UP");
					// Get iterator for first instance of button_down_finger and
					// erase it from the collection
					auto it = buttons_down.find(button_down_finger);
					if (it != buttons_down.end()) {
						buttons_down.erase(it);
					}

				} break;
					// Finger swiping converts to cursor keys
				case SDL_EVENT_FINGER_MOTION: {
					gwin->get_win()->screen_to_game(
							event.button.x, event.button.y,
							gwin->get_fastmouse(), gx, gy);

					static int  numFingers = 0;
					SDL_GetTouchFingers(event.tfinger.touchID, &numFingers);
					CERR("numFingers " << numFingers);

					// Will allow single finger swipes
					if (numFingers > 0) {
						// Hide on screen keyboard if we are swiping
						if (SDL_TextInputActive(window)) {
							SDL_StopTextInput(window);
						}
						// Accuulate the deltas onto
						// thevector
						state.swipe_dx += event.tfinger.dx;
						state.swipe_dy += event.tfinger.dy;
						// set last swipe value to now
						state.last_swipe = SDL_GetTicks();
					}
				} break;

				// Mousewheel scrolling with SDL2.
				case SDL_EVENT_MOUSE_WHEEL: {
					CERR("SDL_EVENT_MOUSE_WHEEL");
					if (event.wheel.y > 0) {
						simulate_key = SDLK_LEFT;
					} else if (event.wheel.y < 0) {
						simulate_key = SDLK_RIGHT;
					}
				} break;
				case SDL_EVENT_MOUSE_MOTION: {
					CERR("SDL_EVENT_MOUSE_MOTION");
					if (Mouse::use_touch_input
						&& event.motion.which != EXSDL_TOUCH_MOUSEID) {
						Mouse::use_touch_input = false;
					}
					gwin->get_win()->screen_to_game(
							event.motion.x, event.motion.y,
							gwin->get_fastmouse(), gx, gy);

					Mouse::mouse->move(gx, gy);
					Mouse::mouse_update = true;

				} break;
					// return;

				case SDL_EVENT_MOUSE_BUTTON_DOWN: {
					gwin->get_win()->screen_to_game(
							event.button.x, event.button.y,
							gwin->get_fastmouse(), gx, gy);
					CERR("SDL_EVENT_MOUSE_BUTTON_DOWN( " << gx << " , " << gy
														 << " )");
					buttons_down.insert(event.button.button);
					if (event.button.button == 1) {
						simulate_key = CheckHotspots(gx, gy);

						if (!simulate_key) {
							// Double click detection
							if (event.button.clicks >= 2) {
								simulate_key = SDLK_RETURN;
							}

							CERR("window size( " << gwin->get_width() << " , "
												 << gwin->get_height() << " )");
							// Touch on the cheat screen will bring up the
							// keyboard but not if the tap was within a 20 pixel
							// border on the edge of the game screen)
							if (SDL_TextInputActive(window)) {
								SDL_StopTextInput(window);
							} else if (
									gx > 20 && gy > 20
									&& gx < (gwin->get_width() - 20)
									&& gy < (gwin->get_height() - 20)) {
								SDL_StartTextInput(window);
							}
						}
					}
				} break;
				case SDL_EVENT_MOUSE_BUTTON_UP: {
					buttons_down.erase(event.button.button);
				} break;

				case SDL_EVENT_KEY_DOWN: {
					buttons_down.insert(int(event.key.key));
				} break;

				case SDL_EVENT_KEY_UP: {
					buttons_down.erase(int(event.key.key));
				} break;

				default:
					break;
				}
			}

			if (simulate_key) {
				std::memset(&event, 0, sizeof(event));
				event.type    = SDL_EVENT_KEY_DOWN;
				event.key.key = simulate_key;
				CERR("simmulate key " << event.key.key);
				// Simulated keys automatically execute the command if possible
				if (state.GetMode() >= CP_HitKey
					&& state.GetMode() <= CP_WrongShapeFile) {
					state.SetMode(CP_Command, true);
				}
			}
			if (event.type != SDL_EVENT_KEY_DOWN) {
				continue;
			}
			SDL_Keycode key_sym = event.key.key;
			SDL_Keymod  key_mod = event.key.mod;
			SDL_Keycode unicode = 0;
			if (!Translate_keyboard(event, key_sym, unicode, true)) {
				continue;
			}

			if (key_sym == SDLK_ESCAPE) {
				std::memset(state.input, 0, sizeof(state.input));
				// If current mode is needing to press a key return to command
				if (state.GetMode() >= CP_HitKey
					&& state.GetMode() <= CP_WrongShapeFile) {
					state.command = 0;
					state.SetMode(CP_Command, true);
					return false;
				}
				// Escape will cancel current mode
				else if (state.GetMode() != CP_Command) {
					state.command = key_sym;
					state.SetMode(CP_Canceled, true);
					return false;
				}
			}

			if ((key_sym == SDLK_S) && (key_mod & SDL_KMOD_ALT)
				&& (key_mod & SDL_KMOD_CTRL)) {
				make_screenshot(true);
				return false;
			}

			if (state.GetMode() == CP_NorthSouth) {
				if (!state.input[0] && (key_sym == SDLK_N || key_sym == SDLK_S)) {
					state.input[0] = char(key_sym);
					state.activate = true;
				}
			} else if (state.GetMode() == CP_WestEast) {
				if (!state.input[0] && (key_sym == SDLK_W || key_sym == SDLK_E)) {
					state.input[0] = char(key_sym);
					state.activate = true;
				}
			} else if (
					state.GetMode() >= CP_HexXCoord
					&& state.GetMode() <= CP_HexYCoord) {    // Want hex input
				// Activate (if possible)
				if (key_sym == SDLK_RETURN) {
					state.activate = true;
// Begin New
					// increment/decrement
				} else if (key_sym == SDLK_LEFT || key_sym == SDLK_RIGHT) {
					char* end   = nullptr;
					long  value = std::strtol(state.input, &end, 16);
					if (state.val_max < state.val_min) {
						std::swap(state.val_max, state.val_min);
					}
					if (end == state.input + strlen(state.input)) {
						if (key_sym == SDLK_LEFT && value != state.val_min) {
							value = std::max(value - 1, state.val_min);
						} else if (
								key_sym == SDLK_RIGHT
								&& value != state.val_max) {
							value = std::min(value + 1, state.val_max);
						}
						if (value < 0) {
							snprintf(
									state.input, std::size(state.input), "-%lx",
									-value);
						} else {
							snprintf(
									state.input, std::size(state.input), "%lx",
									value);
						}
					}
// End New
				} else if (
						(key_sym == SDLK_MINUS)
						&& !state.input[0]) {
					state.input[0] = '-';
				} else if (
						key_sym < 256 && std::isxdigit(key_sym)) {
					const size_t curlen = std::strlen(state.input);
					if (curlen < (std::size(state.input) - 1)) {
						state.input[curlen]     = char(std::tolower(key_sym));
						state.input[curlen + 1] = 0;
					}
				} else if (key_sym == SDLK_BACKSPACE) {
					const size_t curlen = std::strlen(state.input);
					if (curlen) {
						state.input[curlen - 1] = 0;
					}
				}
			} else if (state.GetMode() == CP_Name) {    // Want Text input
				if (key_sym == SDLK_RETURN) {
					state.activate = true;
				} else if (
						(unicode < 256 && std::isalnum(unicode))
						|| unicode == ' ') {
					const size_t curlen = std::strlen(state.input);
					if (curlen < (std::size(state.input) - 1)) {
						state.input[curlen]     = unicode;
						state.input[curlen + 1] = 0;
					}
				} else if (key_sym == SDLK_BACKSPACE) {
					const size_t curlen = std::strlen(state.input);
					if (curlen) {
						state.input[curlen - 1] = 0;
					}
				}
			} else if (state.GetMode() >= CP_ChooseNPC) {    // Need to grab
															 // numerical input
				// Browse shape
				if (state.GetMode() == CP_Shape && !state.input[0]
					&& key_sym == SDLK_B) {
					cheat.shape_browser();
					state.input[0] = 'b';
					state.activate = true;
				}

				if (key_sym == SDLK_LEFT || key_sym == SDLK_RIGHT) {
					char* end   = nullptr;
					long  value = std::strtol(state.input, &end, 10);

					if (state.val_max < state.val_min) {
						std::swap(state.val_max, state.val_min);
					}
					if (end == state.input + strlen(state.input)) {
						if (key_sym == SDLK_LEFT && value != state.val_min) {
							value = std::max(value - 1, state.val_min);
						} else if (
								key_sym == SDLK_RIGHT
								&& value != state.val_max) {
							value = std::min(value + 1, state.val_max);
						}
						snprintf(
								state.input, std::size(state.input) - 1, "%ld",
								value);
					}
				}
				// Activate (if possible)
				else if (key_sym == SDLK_RETURN) {
					state.activate = true;
				} else if (
						(key_sym == SDLK_MINUS)
						&& !state.input[0]) {
					state.input[0] = '-';
				} else if (
						key_sym < 256 && std::isdigit(key_sym)) {
					const size_t curlen = std::strlen(state.input);
					if (curlen < (std::size(state.input) - 1)) {
						state.input[curlen]     = key_sym;
						state.input[curlen + 1] = 0;
					}
				} else if (key_sym == SDLK_BACKSPACE) {
					const auto curlen = std::strlen(state.input);
					if (curlen) {
						state.input[curlen - 1] = 0;
					}
				}
			} else {
				char c = key_sym;

				// Translate arrow key into the characters we use for arrows
				switch (key_sym) {
				case SDLK_UP: {
					c = '^';
				} break;

				case SDLK_DOWN: {
					c = 'V';
				} break;

				case SDLK_RIGHT: {
					c = '>';
				} break;

				case SDLK_LEFT: {
					c = '<';
				} break;

				default: {
				} break;
				}
				// Set input to the typed character so it is shown with the
				// prompt
				std::memset(state.input, 0, sizeof(state.input));
				state.input[0] = c;
				state.input[1] = 0;

				if (state.GetMode()) {    // Just want a key pressed
					state.SetMode(CP_Command, true);
					state.command = 0;
				} else {    // Need the key pressed
					state.command       = key_sym;
					state.highlighttime = SDL_GetTicks() + 1000;
					state.highlight     = state.command;
					return true;
				}
			}
			return false;
		}
		gwin->rotatecolours();
		Mouse::mouse->show();    // Re-display mouse.
		if (gwin->show() || Mouse::mouse_update) {
			Mouse::mouse->blit_dirty();
		}
		Mouse::mouse->hide();    // Need to immediately turn off here to prevent
								 // flickering after repaint of whole screen
	}
	return false;
}

void CheatScreen::SharedMenu() {
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
	const int offsetx = 15;
	// const int offsety1 = 73;
	// const int offsety2 = 55;
	// const int offsetx1 = 160;
	// const int offsety4 = 36;
	const int offsety5 = 72;
#else
	const int offsetx = 0;
	// const int offsety1 = 0;
	// const int offsety2 = 0;
	// const int offsetx1 = 160;
	// const int offsety4 = maxy - 45;
	const int offsety5 = maxy - 36;
#endif    // eXit
	AddMenuItem(offsetx + 160, offsety5 + 9, SDLK_ESCAPE, " Exit");
}

SDL_Keycode CheatScreen::CheckHotspots(int mx, int my, int radius) {
	// Find the nearest hotspot
	SDL_Keycode nearest     = SDLK_UNKNOWN;
	int         nearestdist = INT_MAX;

	for (auto& hs : hotspots) {
		int dist = hs.distance(mx, my);
		if (dist < nearestdist) {
			nearest     = hs.keycode;
			nearestdist = dist;
		}
	}
	// Only return it if it is within the radius
	if (nearestdist <= radius) {
		return nearest;
	}
	return SDLK_UNKNOWN;
}

void CheatScreen::PaintHotspots() {
	int mx = Mouse::mouse->get_mousex();
	int my = Mouse::mouse->get_mousey();
	for (const auto& hs : hotspots) {
		if (hs) {
			if (hs.has_point(mx, my)) {
				// Draw mouse hover
				ibuf->fill_translucent8(0, hs.w, hs.h, hs.x, hs.y, hovertable);
			}
			// Draw the box in bright yellow
			// ibuf->draw_box(
			//		hs.x - 2, hs.y - 2, hs.w + 4, hs.h + 4, 2, 255, 5);
		}
	}
}

//
// Normal
//

void CheatScreen::NormalLoop() {
	bool looping = true;

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		gwin->clear_screen();

		// First the display
		NormalDisplay();

		// Now the Menu Column
		NormalMenu();

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			NormalActivate();
			state.activate = false;
			continue;
		}

		if (SharedInput()) {
			looping = NormalCheck();
		}
	}
	WaitButtonsUp();
}

void CheatScreen::NormalDisplay() {
	char buf[512];
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
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
			ibuf, "Advanced Option Cheat Screen", offsetx, offsety1, 8);

	if (Game::get_game_type() == BLACK_GATE) {
		snprintf(buf, sizeof(buf), "Running \"Ultima VII: The Black Gate\"");
	} else if (Game::get_game_type() == SERPENT_ISLE) {
		snprintf(
				buf, sizeof(buf), "Running \"Ultima VII Part 2: Serpent Isle\"");
	} else {
		snprintf(
				buf, sizeof(buf), "Running Unknown Game Type %i",
				Game::get_game_type());
	}

	font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 18, 8);

	strcpy(buf, "Exult Version " VERSION " Rev: ");
	auto rev    = VersionGetGitRevision(true);
	int  curlen = strlen(buf);
	rev.copy(buf + strlen(buf), rev.size());
	// Need to null terminate after copy
	buf[curlen + rev.size()] = 0;
	font->paint_text_fixedwidth(ibuf, buf, offsetx, offsety1 + 27, 8);

	font->paint_text_fixedwidth(
			ibuf, "Compiled " __DATE__ " " __TIME__, offsetx, offsety1 + 36, 8);

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
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 73;
	const int offsety2 = 55;
	const int offsetx1 = 160;
	const int offsety4 = 36;
	// const int offsety5 = 72;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsety2 = 0;
	const int offsetx1 = 0;
	const int offsety4 = maxy - 45;
	// const int offsety5 = maxy - 36;
#endif

	// Left Column

	// Use
#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(TEST_MOBILE)
	// Paperdolls can be toggled in the gumps, no need here for small screens
	Shape_manager* sman = Shape_manager::get_instance();
	if (sman->can_use_paperdolls() && sman->are_paperdolls_enabled()) {
		snprintf(buf, sizeof(buf), "aperdolls..: Yes");
	} else {
		snprintf(buf, sizeof(buf), "aperdolls..:  No");
	}
	AddMenuItem(offsetx, maxy - 99, SDLK_P, buf);

#endif

	// GodMode
	snprintf(
			buf, sizeof(buf), "od Mode....: %3s",
			cheat.in_god_mode() ? "On" : "Off");
	AddMenuItem(offsetx, maxy - offsety1 - 90, SDLK_G, buf);

	// Archwizzard Mode
	snprintf(
			buf, sizeof(buf), "izard Mode.: %3s",
			cheat.in_wizard_mode() ? "On" : "Off");
	AddMenuItem(offsetx, maxy - offsety1 - 81, SDLK_W, buf);

	// Infravision
	snprintf(
			buf, sizeof(buf), "nfravision.: %3s",
			cheat.in_infravision() ? "On" : "Off");
	AddMenuItem(offsetx, maxy - offsety1 - 72, SDLK_I, buf);

	// Hackmover
	snprintf(
			buf, sizeof(buf), "ack Mover..: %3s",
			cheat.in_hack_mover() ? "Yes" : "No");
	AddMenuItem(offsetx, maxy - offsety1 - 63, SDLK_H, buf);

	// Eggs
	snprintf(
			buf, sizeof(buf), "ggs Visible: %3s",
			gwin->paint_eggs ? "Yes" : "No");
	AddMenuItem(offsetx, maxy - offsety1 - 54, SDLK_E, buf);

	// Set Time
	AddMenuItem(offsetx + offsetx1, offsety4, SDLK_S, "et Time");

	// Right Column

	// NPC Tool
	AddMenuItem(offsetx + 160, maxy - offsety2 - 99, SDLK_N, "PC Tool");

	// Global Flag Modify
	AddMenuItem(offsetx + 160, maxy - offsety2 - 90, SDLK_F, "lag Modifier");

	// Teleport
	AddMenuItem(offsetx + 160, maxy - offsety2 - 81, SDLK_T, "eleport");

#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(TEST_MOBILE)
	// for small screens taking the liberty of leaving that out
	// Time Rate
	snprintf(buf, sizeof(buf), " Time Rate:%4i", clock->get_time_rate());
	AddLeftRightMenuItem(
			offsetx + 160, offsety4, buf, clock->get_time_rate() > 1,
			clock->get_time_rate() < 20, true, false);
#endif

	SharedMenu();
}

void CheatScreen::NormalActivate() {
	const int      npc  = std::atoi(state.input);
	Shape_manager* sman = Shape_manager::get_instance();

	state.SetMode(CP_Command, false);

	switch (state.command) {
		// God Mode
	case SDLK_G:
		cheat.toggle_god();
		break;

		// Wizard Mode
	case SDLK_W:
		cheat.toggle_wizard();
		break;

		// Infravision
	case SDLK_I:
		cheat.toggle_infravision();
		pal.apply();
		break;

		// Eggs
	case SDLK_E:
		cheat.toggle_eggs();
		break;

		// Hack mover
	case SDLK_H:
		cheat.toggle_hack_mover();
		break;

		// Set Time
	case SDLK_S:
		state.SetMode(TimeSetLoop());
		break;

		// - Time Rate
	case '<':
		if (clock->get_time_rate() > 0) {
			clock->set_time_rate(clock->get_time_rate() - 1);
		}
		break;

		// + Time Rate
	case '>':
		if (clock->get_time_rate() < 20) {
			clock->set_time_rate(clock->get_time_rate() + 1);
		}
		break;

		// Teleport
	case SDLK_T:
		TeleportLoop();
		break;

		// NPC Tool
	case SDLK_N:
		if (npc < 0 || (npc >= 356 && npc <= 359)) {
			state.SetMode(CP_InvalidNPC, false);
		} else if (!state.input[0]) {
			NPCLoop(-1);
		} else {
			state.SetMode(NPCLoop(npc));
		}
		break;

		// Global Flag Editor
	case SDLK_F:
		if (npc < 0) {
			state.SetMode(CP_InvalidValue, false);
		} else if (npc > c_last_gflag) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_Canceled, false);
		} else {
			state.SetMode(GlobalFlagLoop(npc));
		}
		break;

		// Paperdolls
	case SDLK_P:
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

	std::memset(state.input, 0, sizeof(state.input));

	state.command = 0;
}

// Checks the state.input
bool CheatScreen::NormalCheck() {
	switch (state.command) {
		// Simple commands
	case SDLK_T:    // Teleport
	case SDLK_G:    // God Mode
	case SDLK_W:    // Wizard
	case SDLK_I:    // iNfravision
	case SDLK_S:    // Set Time
	case SDLK_E:    // Eggs
	case SDLK_H:    // Hack Mover
	case SDLK_C:    // Create Item
	case SDLK_P:    // Paperdolls
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		state.activate = true;
		break;

		// - Time
	case SDLK_LEFT:
		state.command = '<';
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		state.activate = true;
		break;

		// + Time
	case SDLK_RIGHT:
		state.command = '>';
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		state.activate = true;
		break;

		// NPC Tool
	case SDLK_N:
		state.SetMode(CP_ChooseNPC);
		state.val_min = 0;
		state.val_max = gwin->get_num_npcs() - 1;
		break;

		// Global Flag Editor
	case SDLK_F:
		state.SetMode(CP_GFlagNum);
		state.val_min = 0;
		state.val_max = c_last_gflag;
		break;

		// X and Escape leave
	case SDLK_ESCAPE:
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		return false;

	default:
		state.SetMode(CP_InvalidCom, false);
		if (!state.input[0]) {
			state.input[0] = (state.command < 128 ? state.command : 0);
		}
		state.command = 0;
		break;
	}

	return true;
}

//
// Activity Display
//

void CheatScreen::ActivityDisplay() {
	char buf[512];
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
	const int offsety1 = 7;
#else
	const int offsety1 = 9;
#endif

	for (int i = 0; i < 11; i++) {
		snprintf(buf, sizeof(buf), "%2i %s", i, schedules[i]);
		font->paint_text_fixedwidth(ibuf, buf, 0, i * offsety1, 8);

		snprintf(buf, sizeof(buf), "%2i %s", i + 11, schedules[i + 11]);
		font->paint_text_fixedwidth(ibuf, buf, 112, i * offsety1, 8);

		if (i != 10) {
			snprintf(buf, sizeof(buf), "%2i %s", i + 22, schedules[i + 22]);
			font->paint_text_fixedwidth(ibuf, buf, 224, i * offsety1, 8);
		}
	}
}

void CheatScreen::PaintArrow(int offsetx, int offsety, int type) {
	// Need to draw arrows with overlapping characters
	// up arrow
	if (type == '^') {
		// Use an i as the stem of the arrow
		font->paint_text_fixedwidth(ibuf, "i", offsetx, offsety, 8);
		// Draw a black line to narrow the stem
		ibuf->draw_line8(0, offsetx + 4, offsety, offsetx + 4, offsety + 8);
		// Draw point of arrow
		font->paint_text_fixedwidth(ibuf, "^", offsetx, offsety, 8);
	}    // down arrow
	else if (type == 'V') {
		// Use an l as the stem of the arrow
		font->paint_text_fixedwidth(ibuf, "l", offsetx, offsety, 8);
		// Draw black lines to narrow the stem
		ibuf->draw_line8(0, offsetx + 2, offsety, offsetx + 2, offsety + 2);
		ibuf->draw_line8(0, offsetx + 4, offsety, offsetx + 4, offsety + 6);

		// Draw point of arrow
		font->paint_text_fixedwidth(ibuf, "m", offsetx, offsety + 2, 8);

		// Paint black lines to make it pointy
		ibuf->draw_line8(0, offsetx + 0, offsety + 5, offsetx + 0, offsety + 8);
		ibuf->draw_line8(0, offsetx + 6, offsety + 5, offsetx + 6, offsety + 8);
		ibuf->draw_line8(0, offsetx + 1, offsety + 6, offsetx + 1, offsety + 8);
		ibuf->draw_line8(0, offsetx + 5, offsety + 6, offsetx + 5, offsety + 8);
	}    // left arrow
	else if (type == '<') {
		// Stem of arrow
		font->paint_text_fixedwidth(ibuf, "-", offsetx + 1, offsety, 8);
		// Paint black line  to narrow the stem
		ibuf->draw_line8(0, offsetx + 0, offsety + 4, offsetx + 7, offsety + 4);
		// Point of  arrow
		font->paint_text_fixedwidth(ibuf, "<", offsetx, offsety, 8);
		// Paint black line to make it pointier
		ibuf->draw_line8(0, offsetx + 1, offsety + 4, offsetx + 4, offsety + 7);
		ibuf->put_pixel8(0, offsetx + 5, offsety + 7);
	}    // Right Arrow
	else if (type == '>') {
		// Stem of arrow
		font->paint_text_fixedwidth(ibuf, "-", offsetx, offsety, 8);
		// Paint black line to narrow the stem
		ibuf->draw_line8(0, offsetx + 0, offsety + 4, offsetx + 7, offsety + 4);
		// Point of arrow
		font->paint_text_fixedwidth(ibuf, ">", offsetx + 2, offsety, 8);
		// Paint black line to make it pointier
		ibuf->draw_line8(0, offsetx + 7, offsety + 4, offsetx + 3, offsety + 7);
	}
}

//
// TimeSet
//

CheatScreen::Cheat_Prompt CheatScreen::TimeSetLoop() {
	int        day  = 0;
	int        hour = 0;
	ClearState clear(state);
	state.SetMode(CP_Day);
	state.val_min = 0;
	state.val_max = INT_MAX;    // This seems unbounded
	while (true) {
		hotspots.clear();
		gwin->clear_screen();

		// First the display
		NormalDisplay();

		// Now the Menu Column
		NormalMenu();

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			const int val = std::atoi(state.input);

			if (val < 0) {
				return CP_InvalidTime;
			} else if (state.GetMode() == CP_Day) {
				day = val;
				state.SetMode(CP_Hour);
				state.val_min = 0;
				state.val_max = 23;
			} else if (val > 59) {
				return CP_InvalidTime;
			} else if (state.GetMode() == CP_Minute) {
				clock->reset();
				clock->set_day(day);
				clock->set_hour(hour);
				clock->set_minute(val);
				break;
			} else if (val > 23) {
				return CP_InvalidTime;
			} else if (state.GetMode() == CP_Hour) {
				hour = val;
				state.SetMode(CP_Minute);
				state.val_min = 0;
				state.val_max = 59;
			}

			state.activate = false;
			state.input[0] = 0;
			state.input[1] = 0;
			state.input[2] = 0;
			state.input[3] = 0;
			state.command  = 0;
			continue;
		}

		SharedInput();
		if (state.GetMode() == CP_Canceled) {
			return CP_Canceled;
		}
	}

	return CP_ClockSet;
}

//
// Global Flags
//

CheatScreen::Cheat_Prompt CheatScreen::GlobalFlagLoop(int num) {
	bool looping = true;
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 83;
	// const int offsety2 = 72;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	// const int offsety2 = maxy - 36;
#endif

	int  i;
	char buf[64];

	Usecode_machine* usecode = Game_window::get_instance()->get_usecode();

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		gwin->clear_screen();

#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
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
			AddMenuItem(offsetx + 160, maxy - offsety1 - 99, SDLK_S, "et Flag");
		} else {
			AddMenuItem(
					offsetx + 160, maxy - offsety1 - 99, SDLK_U, "nset Flag");
		}

		// Change Flag
		AddMenuItem(offsetx, maxy - offsety1 - 72, SDLK_UP, " Change Flag");
		AddLeftRightMenuItem(
				offsetx, maxy - offsety1 - 63, "Scroll Flags", num > 0,
				num < c_last_gflag, false, true);

		SharedMenu();

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			state.SetMode(CP_Command, false);
			if (state.command == '<') {    // Decrement
				num--;
				if (num < 0) {
					num = 0;
				}
			} else if (state.command == '>') {    // Increment
				num++;
				if (num > c_last_gflag) {
					num = c_last_gflag;
				}
			} else if (state.command == '^') {    // Change Flag
				i = std::atoi(state.input);
				if (i < 0 || i > c_last_gflag) {
					state.SetMode(CP_InvalidValue, false);
				} else if (state.input[0]) {
					num = i;
				}
			} else if (state.command == 's') {    // Set
				usecode->set_global_flag(num, 1);
			} else if (state.command == 'u') {    // Unset
				usecode->set_global_flag(num, 0);
			}
			std::memset(state.input, 0, sizeof(state.input));

			state.command  = 0;
			state.activate = false;
			continue;
		}

		if (SharedInput()) {
			switch (state.command) {
				// Simple commands
			case SDLK_S:    // Set Flag
			case SDLK_U:    // Unset flag
				if (!state.input[0]) {
					state.input[0] = state.command;
				}
				state.activate = true;
				break;

				// Decrement
			case SDLK_LEFT:
				state.command = '<';
				if (!state.input[0]) {
					state.input[0] = state.command;
				}
				state.activate = true;
				break;

				// Increment
			case SDLK_RIGHT:
				state.command = '>';
				if (!state.input[0]) {
					state.input[0] = state.command;
				}
				state.activate = true;
				break;

				// * Change Flag
			case SDLK_UP:
				state.command  = '^';
				state.input[0] = 0;
				state.SetMode(CP_GFlagNum);
				state.val_min = 0;
				state.val_max = c_last_gflag;
				break;

				// X and Escape leave
			case SDLK_ESCAPE:
				if (!state.input[0]) {
					state.input[0] = state.command;
				}
				looping = false;
				break;

			default:
				state.SetMode(CP_InvalidCom, false);
				if (!state.input[0]) {
					state.input[0] = (state.command < 128 ? state.command : 0);
				}
				state.command = 0;
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

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		if (num == -1) {
			actor = grabbed;
		} else {
			actor = gwin->get_npc(num);
		}
		grabbed = actor;
		if (actor) {
			num = actor->get_npc_num();
		}

		gwin->clear_screen();

		// First the display
		NPCDisplay(actor, num);

		// Now the Menu Column
		NPCMenu(actor, num);

		// Finally the Prompt...
		SharedPrompt();
		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			NPCActivate(actor, num);
			state.activate = false;
			continue;
		}

		if (SharedInput()) {
			looping = NPCCheck(actor, num);
		}
	}
	return CP_Command;
}

void CheatScreen::NPCDisplay(Actor* actor, int& num) {
	char buf[512];
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 73;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
#endif
	if (actor) {
		const Tile_coord t = actor->get_tile();

		// Paint the actors shape
		Shape_frame* shape = actor->get_shape();
		if (shape) {
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
			if (!num) {
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
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 74;
	// const int offsetx2 = 15;
	// const int offsety2 = 72;
	const int offsetx3 = 175;
	const int offsety3 = 63;
	const int offsety4 = 72;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	// const int offsetx2 = 0;
	// const int offsety2 = maxy - 36;
	const int offsetx3 = offsetx + 160;
	const int offsety3 = maxy - 45;
	const int offsety4 = maxy - 36;
#endif
	// Left Column

	if (actor) {
		// Business Activity
		AddMenuItem(offsetx, maxy - offsety1 - 99, SDLK_B, "usiness Activity");

		// Change Shape
		AddMenuItem(offsetx, maxy - offsety1 - 90, SDLK_C, "hange Shape");

		// XP
		AddMenuItem(offsetx, maxy - offsety1 - 81, SDLK_E, "xperience");

		// NPC Flags
		AddMenuItem(offsetx, maxy - offsety1 - 72, SDLK_N, "pc Flags");

		// Name
		AddMenuItem(offsetx, maxy - offsety1 - 63, SDLK_1, " Name");
	}

	SharedMenu();

	// Right Column

	if (actor) {
		// Stats
		AddMenuItem(offsetx + 160, maxy - offsety1 - 99, SDLK_S, "tats");

		// Training Points
		AddMenuItem(offsetx + 160, maxy - offsety1 - 90, SDLK_2, " Training Points");

				
		// Teleport
		AddMenuItem(
				offsetx + 160, maxy - offsety1 - 81, SDLK_T, "eleport to NPC");

		// Palette Effect
		AddMenuItem(
				offsetx + 160, maxy - offsety1 - 72, SDLK_P, "alette Effect");
	}

	// Change NPC

	AddMenuItem(offsetx3, offsety3, SDLK_UP, " Change NPC");

	// Scroll NPCs

	AddLeftRightMenuItem(
			offsetx3, offsety4, "Scroll NPCs", num > 0,
			num < gwin->get_num_npcs(), false, true);
}

void CheatScreen::NPCActivate(Actor* actor, int& num) {
	int       i = std::atoi(state.input);
	const int nshapes
			= Shape_manager::get_instance()->get_shapes().get_num_shapes();

	state.SetMode(CP_Command, false);

	if (state.command == '<') {
		num--;
		if (num < 0) {
			num = 0;
		} else if (num >= 356 && num <= 359) {
			num = 355;
		}
	} else if (state.command == '>') {
		num++;
		if (num >= 356 && num <= 359) {
			num = 360;
		}
	} else if (state.command == '^') {    // Change NPC
		if (i < 0 || (i >= 356 && i <= 359)) {
			state.SetMode(CP_InvalidNPC, false);
		} else if (state.input[0]) {
			num = i;
		}
	} else if (actor) {
		switch (state.command) {
		case SDLK_B:    // Business
			BusinessLoop(actor);
			break;

		case SDLK_N:    // Npc flags
			FlagLoop(actor);
			break;

		case SDLK_S:    // stats
			StatLoop(actor);
			break;

		case SDLK_P:
			PalEffectLoop(actor);
			break;

		case SDLK_T:    // Teleport
			Game_window::get_instance()->teleport_party(
					actor->get_tile(), false, actor->get_map_num());
			break;

		case SDLK_E:    // Experience
			if (i < 0) {
				state.SetMode(CP_Canceled, false);
			} else {
				actor->set_property(Actor::exp, i);
			}
			break;

		case SDLK_2:    // Training Points
			if (i < 0) {
				state.SetMode(CP_Canceled, false);
			} else {
				actor->set_property(Actor::training, i);
			}
			break;

		case SDLK_C:                        // Change shape
			if (state.input[0] == 'b') {    // Browser
				int n;
				if (!cheat.get_browser_shape(i, n)) {
					state.SetMode(CP_WrongShapeFile);
					break;
				}
			}

			if (i < 0) {
				state.SetMode(CP_InvalidShape, false);
			} else if (i >= nshapes) {
				state.SetMode(CP_InvalidShape, false);
			} else if (state.input[0]) {
				if (actor->get_npc_num() != 0) {
					actor->set_shape(i);
				} else {
					actor->set_polymorph(i);
				}
				state.SetMode(CP_ShapeSet, false);
			}
			break;

		case SDLK_1:    // Name
			if (!std::strlen(state.input)) {
				state.SetMode(CP_Canceled, false);
			} else {
				actor->set_npc_name(state.input);
				state.SetMode(CP_NameSet, false);
			}
			break;

		default:
			break;
		}
	}
	std::memset(state.input, 0, sizeof(state.input));

	state.command = 0;
}

// Checks the state.input
bool CheatScreen::NPCCheck(Actor* actor, int& num) {
	ignore_unused_variable_warning(num);
	switch (state.command) {
		// Simple commands
	case SDLK_A:    // Attack mode
	case SDLK_B:    // BUsiness
	case SDLK_N:    // Npc flags
	case SDLK_D:    // pop weapon
	case SDLK_S:    // stats
	case SDLK_Z:    // Target
	case SDLK_T:    // Teleport
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		if (!actor) {
			state.SetMode(CP_InvalidCom, false);
		} else {
			state.activate = true;
		}
		break;

		// Value entries
	case SDLK_E:    // Experience
	case SDLK_2:    // Training Points
		if (!actor) {
			state.SetMode(CP_InvalidCom, false);
		} else {
			state.SetMode(CP_EnterValue);
			state.val_min = 255;
		}
		break;

		// Palette Effect
	case 'p':
		if (!actor) {
			state.SetMode(CP_InvalidCom, false);
		} else {
			state.activate = true;
		}
		break;

		// Change shape
	case SDLK_C:
		if (!actor) {
			state.SetMode(CP_InvalidCom, false);
		} else {
			state.SetMode(CP_Shape);
			state.val_min = 0;
			state.val_max = Shape_manager::get_instance()
									->get_shapes()
									.get_num_shapes()
							- 1;
		}
		break;

		// Name
	case SDLK_1:
		if (!actor) {
			state.SetMode(CP_InvalidCom, false);
		} else {
			state.SetMode(CP_Name);
		}
		break;

		// - NPC
	case SDLK_LEFT:
		state.command = '<';
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		state.activate = true;
		break;

		// + NPC
	case SDLK_RIGHT:
		state.command = '>';
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		state.activate = true;
		break;

		// * Change NPC
	case SDLK_UP:
		state.command  = '^';
		state.input[0] = 0;
		state.SetMode(CP_ChooseNPC);
		state.val_min = 0;
		state.val_max = gwin->get_num_npcs() - 1;
		break;

		// X and Escape leave
	case SDLK_ESCAPE:
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		return false;

	default:
		state.SetMode(CP_InvalidCom, false);
		if (!state.input[0]) {
			state.input[0] = (state.command < 128 ? state.command : 0);
		}
		state.command = 0;
		break;
	}

	return true;
}

//
// NPC Flags
//

void CheatScreen::FlagLoop(Actor* actor) {
#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(TEST_MOBILE)
	int num = actor->get_npc_num();
#endif
	bool looping = true;

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		gwin->clear_screen();

#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(TEST_MOBILE)
		// First the display
		NPCDisplay(actor, num);
#endif

		// Now the Menu Column
		FlagMenu(actor);

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			FlagActivate(actor);
			state.activate = false;
			continue;
		}

		if (SharedInput()) {
			looping = FlagCheck(actor);
		}
	}
}

void CheatScreen::FlagMenu(Actor* actor) {
	char buf[512];
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
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
			buf, sizeof(buf), " Asleep.%c",
			actor->get_flag(Obj_flags::asleep) ? 'Y' : 'N');
	AddMenuItem(offsetx, maxy - offsety1 - 108, SDLK_A, buf);

	// Charmed
	snprintf(
			buf, sizeof(buf), " Charmd.%c",
			actor->get_flag(Obj_flags::charmed) ? 'Y' : 'N');

	AddMenuItem(offsetx, maxy - offsety1 - 99, SDLK_B, buf);

	// Cursed
	snprintf(
			buf, sizeof(buf), " Cursed.%c",
			actor->get_flag(Obj_flags::cursed) ? 'Y' : 'N');
	AddMenuItem(offsetx, maxy - offsety1 - 90, SDLK_C, buf);

	// Paralyzed
	snprintf(
			buf, sizeof(buf), " Prlyzd.%c",
			actor->get_flag(Obj_flags::paralyzed) ? 'Y' : 'N');
	AddMenuItem(offsetx, maxy - offsety1 - 81, SDLK_D, buf);

	// Poisoned
	snprintf(
			buf, sizeof(buf), " Poisnd.%c",
			actor->get_flag(Obj_flags::poisoned) ? 'Y' : 'N');
	AddMenuItem(offsetx, maxy - offsety1 - 72, SDLK_E, buf);

	// Protected
	snprintf(
			buf, sizeof(buf), " Prtctd.%c",
			actor->get_flag(Obj_flags::protection) ? 'Y' : 'N');
	AddMenuItem(offsetx, maxy - offsety1 - 63, SDLK_F, buf);

	// Tournament (Original is SI only -- allowing for BG in Exult)
	snprintf(
			buf, sizeof(buf), " Tourna.%c",
			actor->get_flag(Obj_flags::tournament) ? 'Y' : 'N');
	AddMenuItem(offsetx, maxy - offsety1 - 54, SDLK_G, buf);

	// Polymorph
	snprintf(
			buf, sizeof(buf), " Polymo.%c",
			actor->get_flag(Obj_flags::polymorph) ? 'Y' : 'N');
	AddMenuItem(offsetx, maxy - offsety1 - 45, SDLK_H, buf);
	// Advanced Editor

	AddMenuItem(offsetx, maxy - offsety1 - 36, SDLK_UP, "Advanced");

	SharedMenu();

	// Center Column

	// Party
	snprintf(
			buf, sizeof(buf), " Party..%c",
			actor->get_flag(Obj_flags::in_party) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 104, maxy - offsety1 - 108, SDLK_I, buf);

	// Invisible
	snprintf(
			buf, sizeof(buf), " Invsbl.%c",
			actor->get_flag(Obj_flags::invisible) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 104, maxy - offsety1 - 99, SDLK_J, buf);

	// Fly
	snprintf(
			buf, sizeof(buf), " Fly....%c",
			actor->get_type_flag(Actor::tf_fly) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 104, maxy - offsety1 - 90, SDLK_K, buf);

	// Walk
	snprintf(
			buf, sizeof(buf), " Walk...%c",
			actor->get_type_flag(Actor::tf_walk) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 104, maxy - offsety1 - 81, SDLK_L, buf);

	// Swim
	snprintf(
			buf, sizeof(buf), " Swim...%c",
			actor->get_type_flag(Actor::tf_swim) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 104, maxy - offsety1 - 72, SDLK_M, buf);

	// Ethereal
	snprintf(
			buf, sizeof(buf), " Ethrel.%c",
			actor->get_type_flag(Actor::tf_ethereal) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 104, maxy - offsety1 - 63, SDLK_N, buf);

	// Protectee
	snprintf(buf, sizeof(buf), " Prtcee.%c", '?');
	AddMenuItem(offsetx1 + 104, maxy - offsety1 - 54, SDLK_O, buf);

	// Conjured
	snprintf(
			buf, sizeof(buf), " Conjrd.%c",
			actor->get_type_flag(Actor::tf_conjured) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 104, maxy - offsety1 - 45, SDLK_P, buf);

	// Naked (AV ONLY)
	if (!actor->get_npc_num()) {
		snprintf(
				buf, sizeof(buf), " Naked..%c",
				actor->get_flag(Obj_flags::naked) ? 'Y' : 'N');
		AddMenuItem(offsetx1 + 104, maxy - offsety1 - 36, SDLK_7, buf);
	}

	// Right Column

	// Summoned
	snprintf(
			buf, sizeof(buf), " Summnd.%c",
			actor->get_type_flag(Actor::tf_summonned) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 208, maxy - offsety1 - 108, SDLK_Q, buf);

	// Bleeding
	snprintf(
			buf, sizeof(buf), " Bleedn.%c",
			actor->get_type_flag(Actor::tf_bleeding) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 208, maxy - offsety1 - 99, SDLK_R, buf);

	if (!actor->get_npc_num()) {    // Avatar
		// Sex
		snprintf(
				buf, sizeof(buf), " Sex....%c",
				actor->get_type_flag(Actor::tf_sex) ? 'F' : 'M');
		AddMenuItem(offsetx1 + 208, maxy - offsety1 - 90, SDLK_S, buf);

		// Skin
		snprintf(buf, sizeof(buf), " Skin...%d", actor->get_skin_color());
		AddMenuItem(offsetx1 + 208, maxy - offsety1 - 81, SDLK_1, buf);

		// Read
		snprintf(
				buf, sizeof(buf), " Read...%c",
				actor->get_flag(Obj_flags::read) ? 'Y' : 'N');
		AddMenuItem(offsetx1 + 208, maxy - offsety1 - 72, SDLK_4, buf);
	} else {    // Not Avatar
		// Met
		snprintf(
				buf, sizeof(buf), " Met....%c",
				actor->get_flag(Obj_flags::met) ? 'Y' : 'N');
		AddMenuItem(offsetx1 + 208, maxy - offsety1 - 90, SDLK_T, buf);

		// NoCast
		snprintf(
				buf, sizeof(buf), " NoCast.%c",
				actor->get_flag(Obj_flags::no_spell_casting) ? 'Y' : 'N');
		AddMenuItem(offsetx1 + 208, maxy - offsety1 - 81, SDLK_U, buf);

		// ID
		snprintf(buf, sizeof(buf), " ID#:%02i", actor->get_ident());
		AddMenuItem(offsetx1 + 208, maxy - offsety1 - 72, SDLK_V, buf);
	}

	// Freeze
	snprintf(
			buf, sizeof(buf), " Freeze.%c",
			actor->get_flag(Obj_flags::freeze) ? 'Y' : 'N');
	AddMenuItem(offsetx1 + 208, maxy - offsety1 - 63, SDLK_W, buf);

	// Party
	if (actor->is_in_party()) {
		// Temp
		snprintf(buf, sizeof(buf), " Temp: %02i", actor->get_temperature());
		AddMenuItem(offsetx1 + 208, maxy - offsety1 - 54, SDLK_Y, buf);

		// Warmth
		snprintf(buf, sizeof(buf), "Warmth: %04i", actor->figure_warmth());
		font->paint_text_fixedwidth(
				ibuf, buf, offsetx1 + 208, maxy - offsety1 - 45, 8);
	}

	// Petra (AV SI ONLY)
	if (!actor->get_npc_num()) {
		snprintf(
				buf, sizeof(buf), " Petra..%c",
				actor->get_flag(Obj_flags::petra) ? 'Y' : 'N');
		AddMenuItem(offsetx1 + 208, maxy - offsety1 - 36, SDLK_5, buf);
	}
}

void CheatScreen::FlagActivate(Actor* actor) {
	int       i = std::atoi(state.input);
	const int nshapes
			= Shape_manager::get_instance()->get_shapes().get_num_shapes();

	state.SetMode(CP_Command, false);
	switch (state.command) {
		// Everyone

		// Toggles
	case SDLK_A:    // Asleep
		if (actor->get_flag(Obj_flags::asleep)) {
			actor->clear_flag(Obj_flags::asleep);
		} else {
			actor->set_flag(Obj_flags::asleep);
		}
		break;

	case SDLK_B:    // Charmed
		if (actor->get_flag(Obj_flags::charmed)) {
			actor->clear_flag(Obj_flags::charmed);
		} else {
			actor->set_flag(Obj_flags::charmed);
		}
		break;

	case SDLK_C:    // Cursed
		if (actor->get_flag(Obj_flags::cursed)) {
			actor->clear_flag(Obj_flags::cursed);
		} else {
			actor->set_flag(Obj_flags::cursed);
		}
		break;

	case SDLK_D:    // Paralyzed
		if (actor->get_flag(Obj_flags::paralyzed)) {
			actor->clear_flag(Obj_flags::paralyzed);
		} else {
			actor->set_flag(Obj_flags::paralyzed);
		}
		break;

	case SDLK_E:    // Poisoned
		if (actor->get_flag(Obj_flags::poisoned)) {
			actor->clear_flag(Obj_flags::poisoned);
		} else {
			actor->set_flag(Obj_flags::poisoned);
		}
		break;

	case SDLK_F:    // Protected
		if (actor->get_flag(Obj_flags::protection)) {
			actor->clear_flag(Obj_flags::protection);
		} else {
			actor->set_flag(Obj_flags::protection);
		}
		break;

	case SDLK_J:    // Invisible
		if (actor->get_flag(Obj_flags::invisible)) {
			actor->clear_flag(Obj_flags::invisible);
		} else {
			actor->set_flag(Obj_flags::invisible);
		}
		pal.apply();
		break;

	case SDLK_K:    // Fly
		if (actor->get_type_flag(Actor::tf_fly)) {
			actor->clear_type_flag(Actor::tf_fly);
		} else {
			actor->set_type_flag(Actor::tf_fly);
		}
		break;

	case SDLK_L:    // Walk
		if (actor->get_type_flag(Actor::tf_walk)) {
			actor->clear_type_flag(Actor::tf_walk);
		} else {
			actor->set_type_flag(Actor::tf_walk);
		}
		break;

	case SDLK_M:    // Swim
		if (actor->get_type_flag(Actor::tf_swim)) {
			actor->clear_type_flag(Actor::tf_swim);
		} else {
			actor->set_type_flag(Actor::tf_swim);
		}
		break;

	case SDLK_N:    // Ethrel
		if (actor->get_type_flag(Actor::tf_ethereal)) {
			actor->clear_type_flag(Actor::tf_ethereal);
		} else {
			actor->set_type_flag(Actor::tf_ethereal);
		}
		break;

	case SDLK_P:    // Conjured
		if (actor->get_type_flag(Actor::tf_conjured)) {
			actor->clear_type_flag(Actor::tf_conjured);
		} else {
			actor->set_type_flag(Actor::tf_conjured);
		}
		break;

	case SDLK_Q:    // Summoned
		if (actor->get_type_flag(Actor::tf_summonned)) {
			actor->clear_type_flag(Actor::tf_summonned);
		} else {
			actor->set_type_flag(Actor::tf_summonned);
		}
		break;

	case SDLK_R:    // Bleeding
		if (actor->get_type_flag(Actor::tf_bleeding)) {
			actor->clear_type_flag(Actor::tf_bleeding);
		} else {
			actor->set_type_flag(Actor::tf_bleeding);
		}
		break;

	case SDLK_S:    // Sex
		if (actor->get_type_flag(Actor::tf_sex)) {
			actor->clear_type_flag(Actor::tf_sex);
		} else {
			actor->set_type_flag(Actor::tf_sex);
		}
		break;

	case SDLK_4:    // Read
		if (actor->get_flag(Obj_flags::read)) {
			actor->clear_flag(Obj_flags::read);
		} else {
			actor->set_flag(Obj_flags::read);
		}
		break;

	case SDLK_5:    // Petra
		if (actor->get_flag(Obj_flags::petra)) {
			actor->clear_flag(Obj_flags::petra);
		} else {
			actor->set_flag(Obj_flags::petra);
		}
		break;

	case SDLK_7:    // Naked
		if (actor->get_flag(Obj_flags::naked)) {
			actor->clear_flag(Obj_flags::naked);
		} else {
			actor->set_flag(Obj_flags::naked);
		}
		break;

	case SDLK_T:    // Met
		if (actor->get_flag(Obj_flags::met)) {
			actor->clear_flag(Obj_flags::met);
		} else {
			actor->set_flag(Obj_flags::met);
		}
		break;

	case SDLK_U:    // No Cast
		if (actor->get_flag(Obj_flags::no_spell_casting)) {
			actor->clear_flag(Obj_flags::no_spell_casting);
		} else {
			actor->set_flag(Obj_flags::no_spell_casting);
		}
		break;

	case SDLK_Z:    // Zombie
		if (actor->get_flag(Obj_flags::si_zombie)) {
			actor->clear_flag(Obj_flags::si_zombie);
		} else {
			actor->set_flag(Obj_flags::si_zombie);
		}
		break;

	case SDLK_W:    // Freeze
		if (actor->get_flag(Obj_flags::freeze)) {
			actor->clear_flag(Obj_flags::freeze);
		} else {
			actor->set_flag(Obj_flags::freeze);
		}
		break;

	case SDLK_I:    // Party
		if (actor->get_flag(Obj_flags::in_party)) {
			gwin->get_party_man()->remove_from_party(actor);
			gwin->revert_schedules(actor);
			// Just to be sure.
			actor->clear_flag(Obj_flags::in_party);
		} else if (gwin->get_party_man()->add_to_party(actor)) {
			actor->set_schedule_type(Schedule::follow_avatar);
		}
		break;

	case SDLK_O:    // Protectee
		break;

		// Value
	case SDLK_V:    // ID
		if (i < 0) {
			state.SetMode(CP_InvalidValue, false);
		} else if (i > 63) {
			state.SetMode(CP_InvalidValue, false);
		} else if (i == -1 || !state.input[0]) {
			state.SetMode(CP_Canceled);
		} else {
			actor->set_ident(unsigned(i));
		}
		break;

	case SDLK_1:    // Skin color
		actor->set_skin_color(Shapeinfo_lookup::GetNextSkin(
				actor->get_skin_color(), actor->get_type_flag(Actor::tf_sex),
				Shape_manager::get_instance()->have_si_shapes()));
		break;

	case SDLK_G:    // Tournament
		if (actor->get_flag(Obj_flags::tournament)) {
			actor->clear_flag(Obj_flags::tournament);
		} else {
			actor->set_flag(Obj_flags::tournament);
		}
		break;

	case SDLK_Y:    // Warmth
		if (i < -1) {
			state.SetMode(CP_InvalidValue, false);
		} else if (i > 63) {
			state.SetMode(CP_InvalidValue, false);
		} else if (i == -1 || !state.input[0]) {
			state.SetMode(CP_Canceled);
		} else {
			actor->set_temperature(i);
		}
		break;

	case SDLK_H:    // Polymorph

		// Clear polymorph
		if (actor->get_polymorph() != -1) {
			actor->set_polymorph(actor->get_polymorph());
			break;
		}

		if (state.input[0] == 'b') {    // Browser
			int n;
			if (!cheat.get_browser_shape(i, n)) {
				state.SetMode(CP_WrongShapeFile);
				break;
			}
		}

		if (i == -1) {
			state.SetMode(CP_Canceled);
		} else if (i < 0) {
			state.SetMode(CP_InvalidShape, false);
		} else if (i >= nshapes) {
			state.SetMode(CP_InvalidShape, false);
		} else if (
				state.input[0] && (state.input[0] != '-' || state.input[1])) {
			actor->set_polymorph(i);
			state.SetMode(CP_ShapeSet);
		}

		break;

		// Advanced Numeric Flag Editor
	case '^':
		if (i < 0 || i > 63) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_Canceled);
		} else {
			state.SetMode(AdvancedFlagLoop(i, actor));
		}
		break;

	default:
		break;
	}
	std::memset(state.input, 0, sizeof(state.input));

	state.command = 0;
}

// Checks the state.input
bool CheatScreen::FlagCheck(Actor* actor) {
	switch (state.command) {
		// Everyone

		// Toggles
	case SDLK_A:    // Asleep
	case SDLK_B:    // Charmed
	case SDLK_C:    // Cursed
	case SDLK_D:    // Paralyzed
	case SDLK_E:    // Poisoned
	case SDLK_F:    // Protected
	case SDLK_I:    // Party
	case SDLK_J:    // Invisible
	case SDLK_K:    // Fly
	case SDLK_L:    // Walk
	case SDLK_M:    // Swim
	case SDLK_N:    // Ethrel
	case SDLK_O:    // Protectee
	case SDLK_P:    // Conjured
	case SDLK_Q:    // Summoned
	case SDLK_R:    // Bleedin
	case SDLK_W:    // Freeze
	case SDLK_G:    // Tournament
		state.activate = true;
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		break;

		// Value
	case SDLK_H:    // Polymorph
		if (actor->get_polymorph() == -1) {
			state.SetMode(CP_Shape);
			state.val_min = 0;
			state.val_max = Shape_manager::get_instance()
									->get_shapes()
									.get_num_shapes()
							- 1;
			state.input[0] = 0;
		} else {
			state.activate = true;
			if (!state.input[0]) {
				state.input[0] = state.command;
			}
		}
		break;

		// Party Only

		// Value
	case SDLK_Y:    // Temp
		if (!actor->is_in_party()) {
			state.command = 0;
		} else {
			state.SetMode(CP_TempNum);
			state.val_max = 0;
			state.val_min = 63;
		}
		state.input[0] = 0;
		break;

		// Avatar Only

		// Toggles
	case SDLK_S:    // Sex
	case SDLK_4:    // Read
		if (actor->get_npc_num()) {
			state.command = 0;
		} else {
			state.activate = true;
		}
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		break;

		// Toggles SI
	case SDLK_5:    // Petra
	case SDLK_7:    // Naked
		if (actor->get_npc_num()) {
			state.command = 0;
		} else {
			state.activate = true;
		}
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		break;

		// Value SI
	case SDLK_1:    // Skin
		if (actor->get_npc_num()) {
			state.command = 0;
		} else {
			state.activate = true;
		}
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		break;

		// Everyone but avatar

		// Toggles
	case SDLK_T:    // Met
	case SDLK_U:    // No Cast
	case SDLK_Z:    // Zombie
		if (!actor->get_npc_num()) {
			state.command = 0;
		} else {
			state.activate = true;
		}
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		break;

		// Value
	case SDLK_V:    // ID
		if (!actor->get_npc_num()) {
			state.command = 0;
		} else {
			state.SetMode(CP_EnterValue);
			state.val_min = 0;
			state.val_max = 63;
		}
		state.input[0] = 0;
		break;

		// NPC Flag Editor

	case SDLK_UP:
		state.command  = '^';
		state.input[0] = 0;
		state.SetMode(CP_NFlagNum);
		state.val_max = 0;
		state.val_min = 63;
		break;

		// X and Escape leave
	case SDLK_ESCAPE:
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		return false;

		// Unknown
	default:
		state.command = 0;
		break;
	}

	return true;
}

//
// Business Schedules
//

void CheatScreen::BusinessLoop(Actor* actor) {
	bool looping = true;

	int time = 0;
	int prev = 0;

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		gwin->clear_screen();

		// First the display
		if (state.GetMode() == CP_Activity) {
			ActivityDisplay();
		} else {
			BusinessDisplay(actor);
		}

		// Now the Menu Column
		BusinessMenu(actor);

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			BusinessActivate(actor, time, prev);
			state.activate = false;
			continue;
		}

		if (SharedInput()) {
			looping = BusinessCheck(actor, time);
		}
	}
}

void CheatScreen::BusinessDisplay(Actor* actor) {
	char             buf[512];
	const Tile_coord t = actor->get_tile();
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
	const int offsetx  = 10;
	const int offsety1 = 20;
	const int offsetx2 = 171;
	const int offsety2 = 8;
	const int offsety3 = 0;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsetx2 = offsetx;
	const int offsety2 = 28;
	const int offsety3 = 16;
#endif

	// Now the info
	const std::string namestr = actor->get_npc_name();
	snprintf(
			buf, sizeof(buf), "NPC %i - %s", actor->get_npc_num(),
			namestr.c_str());
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 0, 8);

	snprintf(buf, sizeof(buf), "Loc (%04i, %04i, %02i)", t.tx, t.ty, t.tz);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 8, 8);

#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
	const char activity_msg[] = "%2i %s";
#else
	const char activity_msg[] = "Current Activity:  %2i - %s";
#endif
	snprintf(
			buf, sizeof(buf), activity_msg, actor->get_schedule_type(),
			schedules[actor->get_schedule_type()]);
	font->paint_text_fixedwidth(ibuf, buf, offsetx2, offsety3, 8);

	Schedule_change* scheds;
	int              num;
	actor->get_schedules(scheds, num);

	if (num) {
		font->paint_text_fixedwidth(ibuf, "Schedules:", offsetx2, offsety2, 8);

		int types[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
		int x[8]     = {0};
		int y[8]     = {0};

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
	char buf[64];
	// Left Column
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
	const int offsetx = 10;
	const int offsety = 0;
#else
	const int offsetx = 0;
	const int offsety = 4;
#endif
	int nextx;
	// Might break on monster npcs?
	if (actor->get_npc_num() > 0 && !actor->is_monster()) {
		for (int h = 0; h < 24; h += 3) {
			int row = h / 3;
			int h12 = h % 12;
			h12     = h12 ? h12 : 12;
			int y   = 96 - row * 8;
			snprintf(buf, sizeof(buf), "%2i %cM:", h12, h / 12 ? 'P' : 'A');
			nextx = offsetx;
			nextx += 8
					 + font->paint_text_fixedwidth(
							 ibuf, buf, nextx, maxy - offsety - y, 8);
			nextx += 8
					 + AddMenuItem(
							 nextx, maxy - offsety - y, SDLK_A + row, " Set");
			nextx += 8
					 + AddMenuItem(
							 nextx, maxy - offsety - y, SDLK_I + row,
							 " Location");
			AddMenuItem(nextx, maxy - offsety - y, SDLK_1 + row, " Clear");
		}
		nextx = offsetx;
		nextx += 8
				 + AddMenuItem(
						 nextx, maxy - offsety - 32, SDLK_S,
						 "et Current Activity");

		AddMenuItem(nextx, maxy - offsety - 32, SDLK_R, "evert");

	} else {
		AddMenuItem(
				offsetx, maxy - offsety - 96, SDLK_S, "et Current Activity");
	}
	SharedMenu();
}

void CheatScreen::BusinessActivate(Actor* actor, int& time, int& prev) {
	int i = std::atoi(state.input);

	state.SetMode(CP_Command, false);
	const int old = state.command;
	state.command = 0;
	switch (old) {
	case SDLK_A:    // Set Activity
		if (i < 0 || i > 31) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_Activity);
			state.val_min = 0;
			state.val_max = 31;
			state.command = 'a';
		} else {
			actor->set_schedule_time_type(time, i);
		}
		break;

	case SDLK_I:    // X Coord
		if (i < 0 || i > c_num_tiles) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_XCoord);
			state.val_min = 0;
			state.val_max = c_num_tiles;
			state.command = 'i';
		} else {
			prev = i;
			state.SetMode(CP_YCoord);
			state.val_min = 0;
			state.val_max = c_num_tiles;
			state.command = 'j';
		}
		break;

	case SDLK_J:    // Y Coord
		if (i < 0 || i > c_num_tiles) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_YCoord);
			state.val_min = 0;
			state.val_max = c_num_tiles;
			state.command = 'j';
		} else {
			actor->set_schedule_time_location(time, prev, i);
		}
		break;

	case SDLK_1:    // Clear
		actor->remove_schedule(time);
		break;

	case SDLK_S:    // Set Current
		if (i < 0 || i > 31) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_Activity);
			state.val_min = 0;
			state.val_max = 31;
			state.command = 's';
		} else {
			actor->set_schedule_type(i);
		}
		break;

	case SDLK_R:    // Revert
		Game_window::get_instance()->revert_schedules(actor);
		break;

	default:
		break;
	}
	std::memset(state.input, 0, sizeof(state.input));
}

// Checks the state.input
bool CheatScreen::BusinessCheck(Actor* actor, int& time) {
	// Might break on monster npcs?
	if (actor->get_npc_num() > 0) {
		switch (state.command) {
		case SDLK_A:
		case SDLK_B:
		case SDLK_C:
		case SDLK_D:
		case SDLK_E:
		case SDLK_F:
		case SDLK_G:
		case SDLK_H:
			time          = state.command - 'a';
			state.command = 'a';
			state.SetMode(CP_Activity);
			state.val_min = 0;
			state.val_max = 31;
			return true;

		case SDLK_I:
		case SDLK_J:
		case SDLK_K:
		case SDLK_L:
		case SDLK_M:
		case SDLK_N:
		case SDLK_O:
		case SDLK_P:
			time          = state.command - 'i';
			state.command = 'i';
			state.SetMode(CP_XCoord);
			state.val_min = 0;
			state.val_max = c_num_tiles;
			return true;

		case SDLK_1:
		case SDLK_2:
		case SDLK_3:
		case SDLK_4:
		case SDLK_5:
		case SDLK_6:
		case SDLK_7:
		case SDLK_8:
			time           = state.command - '1';
			state.command  = '1';
			state.activate = true;
			return true;

		case SDLK_R:
			state.command  = 'r';
			state.activate = true;
			return true;

		default:
			break;
		}
	}

	switch (state.command) {
		// Set Current
	case SDLK_S:
		state.command  = 's';
		state.input[0] = 0;
		state.SetMode(CP_Activity);
		state.val_min = 0;
		state.val_max = 31;
		break;

		// X and Escape leave
	case SDLK_ESCAPE:
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		return false;

		// Unknown
	default:
		state.command = 0;
		state.SetMode(CP_InvalidCom, false);
		break;
	}

	return true;
}

//
// NPC Stats
//

void CheatScreen::StatLoop(Actor* actor) {
#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(TEST_MOBILE)
	int num = actor->get_npc_num();
#endif
	bool looping = true;

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		gwin->clear_screen();

#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(TEST_MOBILE)
		// First the display
		NPCDisplay(actor, num);
#endif

		// Now the Menu Column
		StatMenu(actor);

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			StatActivate(actor);
			state.activate = false;
			continue;
		}

		if (SharedInput()) {
			looping = StatCheck(actor);
		}
	}
}

void CheatScreen::StatMenu(Actor* actor) {
	char buf[512];
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 92;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
#endif

	// Left Column

	// Dexterity
	snprintf(
			buf, sizeof(buf), "exterity....%3i",
			actor->get_property(Actor::dexterity));
	AddMenuItem(offsetx, maxy - offsety1 - 108, SDLK_D, buf);

	// Food Level
	snprintf(
			buf, sizeof(buf), "ood Level...%3i",
			actor->get_property(Actor::food_level));
	AddMenuItem(offsetx, maxy - offsety1 - 99, SDLK_F, buf);

	// Intelligence
	snprintf(
			buf, sizeof(buf), "ntellicence.%3i",
			actor->get_property(Actor::intelligence));
	AddMenuItem(offsetx, maxy - offsety1 - 90, SDLK_I, buf);

	// Strength
	snprintf(
			buf, sizeof(buf), "trength.....%3i",
			actor->get_property(Actor::strength));
	AddMenuItem(offsetx, maxy - offsety1 - 81, SDLK_S, buf);

	// Combat Skill
	snprintf(
			buf, sizeof(buf), "ombat Skill.%3i",
			actor->get_property(Actor::combat));
	AddMenuItem(offsetx, maxy - offsety1 - 72, SDLK_C, buf);

	// Hit Points
	snprintf(
			buf, sizeof(buf), "it Points...%3i",
			actor->get_property(Actor::health));
	AddMenuItem(offsetx, maxy - offsety1 - 63, SDLK_H, buf);

	// Magic
	// Magic Points
	snprintf(
			buf, sizeof(buf), "agic Points.%3i",
			actor->get_property(Actor::magic));
	AddMenuItem(offsetx, maxy - offsety1 - 54, SDLK_M, buf);

	// Mana
	snprintf(
			buf, sizeof(buf), "ana Level...%3i",
			actor->get_property(Actor::mana));
	AddMenuItem(offsetx, maxy - offsety1 - 45, SDLK_V, buf);

	SharedMenu();
}

void CheatScreen::StatActivate(Actor* actor) {
	int i = std::atoi(state.input);
	state.SetMode(CP_Command, false);
	// Enforce sane bounds.
	if (i > 60) {
		i = 60;
	} else if (i < 0 && state.command != 'h') {
		if (i == -1) {    // canceled
			std::memset(state.input, 0, sizeof(state.input));

			state.command = 0;
			return;
		}
		i = 0;
	} else if (i < -50) {
		i = -50;
	}

	switch (state.command) {
	case SDLK_D:    // Dexterity
		actor->set_property(Actor::dexterity, i);
		break;

	case SDLK_F:    // Food Level
		actor->set_property(Actor::food_level, i);
		break;

	case SDLK_I:    // Intelligence
		actor->set_property(Actor::intelligence, i);
		break;

	case SDLK_S:    // Strength
		actor->set_property(Actor::strength, i);
		break;

	case SDLK_C:    // Combat Points
		actor->set_property(Actor::combat, i);
		break;

	case SDLK_H:    // Hit Points
		actor->set_property(Actor::health, i);
		break;

	case SDLK_M:    // Magic
		actor->set_property(Actor::magic, i);
		break;

	case SDLK_V:    // [V]ana
		actor->set_property(Actor::mana, i);
		break;

	default:
		break;
	}
	std::memset(state.input, 0, sizeof(state.input));

	state.command = 0;
}

// Checks the state.input
bool CheatScreen::StatCheck(Actor* actor) {
	ignore_unused_variable_warning(state.activate, actor);

	switch (state.command) {
		// Everyone
	case SDLK_H:    // Hit Points
		state.input[0] = 0;
		state.SetMode(CP_EnterValueNoCancel);
		state.val_min = 0;
		state.val_max = actor->get_property(Actor::strength);
		;
		break;
	case SDLK_D:    // Dexterity
	case SDLK_F:    // Food Level
	case SDLK_I:    // Intelligence
	case SDLK_S:    // Strength
	case SDLK_C:    // Combat Points
	case SDLK_M:    // Magic
	case SDLK_V:    // [V]ana
		state.input[0] = 0;
		state.SetMode(CP_EnterValue);
		state.val_min = 0;
		state.val_max = 255;
		break;

		// X and Escape leave
	case SDLK_ESCAPE:
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		return false;

		// Unknown
	default:
		state.command = 0;
		break;
	}

	return true;
}

//
// Pallete Effect
//

void CheatScreen::PalEffectLoop(Actor* actor) {
#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(TEST_MOBILE)
	int num = actor->get_npc_num();
#endif
	bool looping = true;

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		gwin->clear_screen();

#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(TEST_MOBILE)
		// First the display
		NPCDisplay(actor, num);
#endif

		// Now the Menu Column
		PalEffectMenu(actor);

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			PalEffectActivate(actor);
			state.activate = false;
			continue;
		}

		if (SharedInput()) {
			looping = PalEffectCheck(actor);
		}
	}
}

void CheatScreen::PalEffectMenu(Actor* actor) {
	char buf[512];
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 81;
	// const int offsety2 = 72;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	// const int offsety2 = maxy - 36;

#endif
	int pt = actor->get_palette_transform();
	if (pt == 0) {
		snprintf(buf, sizeof(buf), "Palette effect: None");
	} else if (
			(pt & ShapeID::PT_RampRemapAllFrom)
			== ShapeID::PT_RampRemapAllFrom) {
		snprintf(
				buf, sizeof(buf), "Palette effect: Ramp Remap All To %i",
				pt & 31);
	} else if ((pt & ShapeID::PT_RampRemap) == ShapeID::PT_RampRemap) {
		snprintf(
				buf, sizeof(buf), "Palette effect: Ramp Remap %i To %i",
				(pt >> 5) & 31, pt & 31);
	} else if ((pt & ShapeID::PT_xForm) == ShapeID::PT_xForm) {
		snprintf(buf, sizeof(buf), "Palette effect: XForm %i", pt & 31);
	} else if (pt < 256) {
		snprintf(buf, sizeof(buf), "Palette effect: Shift by %i", pt & 0xff);
	}
	font->paint_text_fixedwidth(ibuf, buf, offsetx, maxy - offsety1 - 119, 8);

	if (state.command == 't') {
		if (state.saved_value == 255) {
			snprintf(buf, sizeof(buf), "From Ramp: All");
		} else {
			snprintf(buf, sizeof(buf), "From Ramp: %i", state.saved_value & 31);
		}

		font->paint_text_fixedwidth(
				ibuf, buf, offsetx, maxy - offsety1 - 110, 8);
	}

	// Left Column

	// ramp remap
	AddMenuItem(offsetx, maxy - offsety1 - 99, SDLK_R, "amp Remap");

	// xform
	AddMenuItem(offsetx, maxy - offsety1 - 90, SDLK_X, "form");

	// Shift
	AddMenuItem(offsetx, maxy - offsety1 - 81, SDLK_S, "hift");

	// clear
	AddMenuItem(offsetx, maxy - offsety1 - 72, SDLK_C, "lear");

	SharedMenu();
}

void CheatScreen::PalEffectActivate(Actor* actor) {
	int   i = std::atoi(state.input);
	char* end;
	auto  u = std::strtoul(state.input, &end, 10);

	switch (state.command) {
	case 'x':    // XForm
		actor->set_palette_transform(
				ShapeID::PT_xForm
				| i % int(Shape_manager::get_instance()->get_xforms_cnt()));
		break;

	case 'f':    // from Ramp
	{
		const char   prompttext[] = "enter To Ramp number Index (0-%i)";
		static char  staticPrompttext[sizeof(prompttext) + 16];
		unsigned int numramps = 0;
		gwin->get_pal()->get_ramps(numramps);
		if (numramps) {
			numramps--;
		}
		if (u >= numramps && u != 255) {
			state.SetMode(CP_InvalidValue, false);
			break;
		}
		std::snprintf(
				staticPrompttext, sizeof(staticPrompttext), prompttext,
				numramps);
		state.input[0]      = 0;
		state.custom_prompt = staticPrompttext;
		state.SetMode(CP_CustomValue);
		state.command     = 't';
		state.saved_value = i;
		state.val_min     = 0;
		state.val_max     = int(numramps);
		return;
	}

	case 't':    // to ramp
	{
		unsigned int numramps = 0;
		gwin->get_pal()->get_ramps(numramps);
		if (u >= numramps) {
			state.SetMode(CP_InvalidValue, false);
			break;
		}
		if (state.saved_value == 255) {
			actor->set_palette_transform(
					ShapeID::PT_RampRemapAllFrom | (i & 31));
		} else {
			actor->set_palette_transform(
					ShapeID::PT_RampRemap | (i & 31)
					| ((state.saved_value & 0xff) << 5));
		}
	} break;

	case 's':    // Shift
		actor->set_palette_transform(ShapeID::PT_Shift | (i & 0xff));
		break;

	case 'c':    // clear
		actor->set_palette_transform(0);
		break;

	default:
		break;
	}
	ClearState clear(state, false, true);
	state.command = 0;
}

// Checks the state.input
bool CheatScreen::PalEffectCheck(Actor* actor) {
	ignore_unused_variable_warning(state.activate, actor);
	switch (state.command) {
	case 'r':    // [R]amp Remap
	{
		const char   prompttext[] = "enter From Ramp (0-%u) or 255 for all";
		static char  staticPrompttext[sizeof(prompttext) + 16];
		unsigned int numramps = 0;
		gwin->get_pal()->get_ramps(numramps);
		if (numramps) {
			numramps--;
		}
		std::snprintf(
				staticPrompttext, sizeof(staticPrompttext), prompttext,
				numramps);
		state.input[0]      = 0;
		state.custom_prompt = staticPrompttext;
		state.SetMode(CP_CustomValue);
		state.command = 'f';
		state.val_min = 0;
		state.val_max = 255;
	} break;

	case 'x':    // [X]Form
	{
		const char  prompttext[] = "enter XFORM Index (0-%zu)";
		static char staticPrompttext[sizeof(prompttext) + 16];
		size_t      numxforms = Shape_manager::get_instance()->get_xforms_cnt();
		if (numxforms) {
			numxforms--;
		}
		std::snprintf(
				staticPrompttext, sizeof(staticPrompttext), prompttext,
				numxforms);
		state.input[0]      = 0;
		state.custom_prompt = staticPrompttext;
		state.SetMode(CP_CustomValue);
		state.val_min = 0;
		state.val_max = int(numxforms);
	} break;

	case 's':    // [S]hift
		state.input[0]      = 0;
		state.custom_prompt = "enter shift amount (0-255)";
		state.SetMode(CP_EnterValue);
		state.val_min = 0;
		state.val_max = 255;
		break;

		// Escape leaves
		// X and Escape leave
	case SDLK_ESCAPE:
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		return false;

		// clear
	case 'c':
		state.activate = true;
		break;

		// Unknown
	default:
		state.command = 0;
		break;
	}

	return true;
}

//
// Advanced Flag Edition
//

CheatScreen::Cheat_Prompt CheatScreen::AdvancedFlagLoop(int num, Actor* actor) {
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 83;
	// const int offsety2 = 72;
#else
	int       npc_num  = actor->get_npc_num();
	const int offsetx  = 0;
	const int offsety1 = 0;
	// const int offsety2 = maxy - 36;
#endif
	bool looping = true;
	char buf[64];

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		gwin->clear_screen();

#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(TEST_MOBILE)
		NPCDisplay(actor, npc_num);
#endif

		if (num < 0) {
			num = 0;
		} else if (num > 63) {
			num = 63;
		}

		// First the info
		if (flag_names[num]) {
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
			AddMenuItem(offsetx + 160, maxy - offsety1 - 90, SDLK_S, "et Flag");
		} else {
			AddMenuItem(
					offsetx + 160, maxy - offsety1 - 90, SDLK_U, "nset Flag");
		}

		// Change Flag
		AddMenuItem(offsetx, maxy - offsety1 - 72, SDLK_UP, " Change Flag");

		AddLeftRightMenuItem(
				offsetx, maxy - offsety1 - 63, "Scroll Flags", num > 0,
				num < 63, false, true);

		SharedMenu();

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			state.SetMode(CP_Command);
			if (state.command == '<') {    // Decrement
				num--;
				if (num < 0) {
					num = 0;
				}
			} else if (state.command == '>') {    // Increment
				num++;
				if (num > 63) {
					num = 63;
				}
			} else if (state.command == '^') {    // Change Flag
				int i = std::atoi(state.input);
				if (i < 0 || i > 63) {
					state.SetMode(CP_InvalidValue, false);
				} else if (state.input[0]) {
					num = i;
				}
			} else if (state.command == 's') {    // Set
				actor->set_flag(num);
				if (num == Obj_flags::in_party) {
					gwin->get_party_man()->add_to_party(actor);
					actor->set_schedule_type(Schedule::follow_avatar);
				}
			} else if (state.command == 'u') {    // Unset
				if (num == Obj_flags::in_party) {
					gwin->get_party_man()->remove_from_party(actor);
					gwin->revert_schedules(actor);
				}
				actor->clear_flag(num);
			}

			ClearState clearer(state);
			continue;
		}

		if (SharedInput()) {
			switch (state.command) {
				// Simple commands
			case SDLK_S:    // Set Flag
			case SDLK_U:    // Unset flag
				if (!state.input[0]) {
					state.input[0] = state.command;
				}
				state.activate = true;
				break;

				// Decrement
			case SDLK_LEFT:
				state.command = '<';
				if (!state.input[0]) {
					state.input[0] = state.command;
				}
				state.activate = true;
				break;

				// Increment
			case SDLK_RIGHT:
				state.command = '>';
				if (!state.input[0]) {
					state.input[0] = state.command;
				}
				state.activate = true;
				break;

				// * Change Flag
			case SDLK_UP:
				state.command  = '^';
				state.input[0] = 0;
				state.SetMode(CP_NFlagNum);
				state.val_max = 0;
				state.val_min = 63;
				break;

				// X and Escape leave
			case SDLK_ESCAPE:
				if (!state.input[0]) {
					state.input[0] = state.command;
				}
				looping = false;
				break;

			default:
				state.SetMode(CP_InvalidCom, false);
				if (!state.input[0]) {
					state.input[0] = (state.command < 128 ? state.command : 0);
				}
				state.command = 0;
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

	int prev = 0;

	ClearState clear(state);
	while (looping) {
		hotspots.clear();
		gwin->clear_screen();

		// First the display
		TeleportDisplay();

		// Now the Menu Column
		TeleportMenu();

		// Finally the Prompt...
		SharedPrompt();

		// Draw it!
		EndFrame();

		// Check to see if we need to change menus
		if (state.activate) {
			TeleportActivate(prev);
			state.activate = false;
			continue;
		}

		if (SharedInput()) {
			looping = TeleportCheck();
		}
	}
}

void CheatScreen::TeleportDisplay() {
	char             buf[512];
	const Tile_coord t       = gwin->get_main_actor()->get_tile();
	const int        curmap  = gwin->get_map()->get_num();
	const int        highest = Get_highest_map();
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 54;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
#endif

#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
	font->paint_text_fixedwidth(
			ibuf, "Teleport Menu - Dangerous!", offsetx, 0, 8);
#else
	font->paint_text_fixedwidth(ibuf, "Teleport Menu", offsetx, 0, 8);
	font->paint_text_fixedwidth(
			ibuf, "Dangerous - use with care!", offsetx, 18, 8);
#endif

	const int longi = ((t.tx - 0x3A5) / 10);
	const int lati  = ((t.ty - 0x46E) / 10);
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
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

#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID) && !defined(TEST_MOBILE)
	snprintf(buf, sizeof(buf), "On Map #%d of %d", curmap, highest);
	font->paint_text_fixedwidth(ibuf, buf, offsetx, 90, 8);
#endif
}

void CheatScreen::TeleportMenu() {
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
	const int offsetx  = 15;
	const int offsety1 = 64;
	const int offsetx2 = 175;
	const int offsety2 = 63;
	// const int offsety3 = 72;
#else
	const int offsetx  = 0;
	const int offsety1 = 0;
	const int offsetx2 = offsetx;
	const int offsety2 = maxy - 63;
	// const int offsety3 = maxy - 36;
#endif

	// Left Column
	// Geo
	AddMenuItem(offsetx, maxy - offsety1 - 99, SDLK_G, "eographic Coordinates");
	// Hex
	AddMenuItem(
			offsetx, maxy - offsety1 - 90, SDLK_H, "exadecimal Coordinates");

	// Dec
	AddMenuItem(offsetx, maxy - offsety1 - 81, SDLK_D, "ecimal Coordinates");

	// NPC
	AddMenuItem(offsetx, maxy - offsety1 - 72, SDLK_N, "PC Number");

	AddMenuItem(offsetx2, offsety2, SDLK_M, "ap Number");
	// Map

	SharedMenu();
}

void CheatScreen::TeleportActivate(int& prev) {
	int        i = std::atoi(state.input);
	static int lat;
	Tile_coord t       = gwin->get_main_actor()->get_tile();
	const int  highest = Get_highest_map();

	state.SetMode(CP_Command, false);
	switch (state.command) {
	case SDLK_G:    // North or South
		if (!state.input[0]) {
			state.SetMode(CP_NorthSouth);
			state.command = 'g';
		} else if (state.input[0] == 'n' || state.input[0] == 's') {
			prev = state.input[0];
			if (prev == 'n') {
				state.SetMode(CP_NLatitude);
				state.val_max = 0;
				state.val_min = 113;
			} else {
				state.SetMode(CP_SLatitude);
				state.val_max = 0;
				state.val_min = 193;
			}
			state.command = 'a';
		} else {
			state.SetMode(CP_InvalidValue, false);
		}
		break;

	case SDLK_A:    // latitude
		if (i < 0 || (prev == 'n' && i > 113) || (prev == 's' && i > 193)) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			if (prev == 'n') {
				state.SetMode(CP_NLatitude);
				state.val_max = 0;
				state.val_min = 113;
			} else {
				state.SetMode(CP_SLatitude);
				state.val_max = 0;
				state.val_min = 193;
			}
			state.command = 'a';
		} else {
			if (prev == 'n') {
				lat = ((i * -10) + 0x46E);
			} else {
				lat = ((i * 10) + 0x46E);
			}
			state.SetMode(CP_WestEast);
			state.command = 'b';
		}
		break;

	case SDLK_B:    // West or East
		if (!state.input[0]) {
			state.SetMode(CP_WestEast);
			state.command = 'b';
		} else if (state.input[0] == 'w' || state.input[0] == 'e') {
			prev = state.input[0];
			if (prev == 'w') {
				state.SetMode(CP_WLongitude);
				state.val_max = 0;
				state.val_min = 93;
			} else {
				state.SetMode(CP_ELongitude);
				state.val_max = 0;
				state.val_min = 213;
			}
			state.command = 'c';
		} else {
			state.SetMode(CP_InvalidValue, false);
		}
		break;

	case SDLK_C:    // longitude
		if (i < 0 || (prev == 'w' && i > 93) || (prev == 'e' && i > 213)) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			if (prev == 'w') {
				state.SetMode(CP_WLongitude);
				state.val_max = 0;
				state.val_min = 93;
			} else {
				state.SetMode(CP_ELongitude);
				state.val_max = 0;
				state.val_min = 213;
			}
			state.command = 'c';
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

	case SDLK_H:    // hex X coord
		i = strtol(state.input, nullptr, 16);
		if (i < 0 || i > c_num_tiles) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_HexXCoord);
			state.val_min = 0;
			state.val_max = c_num_tiles;
			state.command = 'h';
		} else {
			prev = i;
			state.SetMode(CP_HexYCoord);
			state.val_min = 0;
			state.val_max = c_num_tiles;
			state.val_min = 0;
			state.val_max = c_num_tiles;
			state.command = 'i';
		}
		state.val_max = 0;
		state.val_min = c_num_tiles;
		break;

	case SDLK_I:    // hex Y coord
		i = strtol(state.input, nullptr, 16);
		if (i < 0 || i > c_num_tiles) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_HexYCoord);
			state.val_min = 0;
			state.val_max = c_num_tiles;
			state.command = 'i';
		} else {
			t.tx = prev;
			t.ty = i;
			t.tz = 0;
			gwin->teleport_party(t);
		}
		break;

	case SDLK_D:    // dec X coord
		if (i < 0 || i > c_num_tiles) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_XCoord);
			state.command = 'd';
			state.val_min = 0;
			state.val_max = c_num_tiles;
		} else {
			prev = i;
			state.SetMode(CP_YCoord);
			state.command = 'e';
			state.val_min = 0;
			state.val_max = c_num_tiles;
		}
		break;

	case SDLK_E:    // dec Y coord
		if (i < 0 || i > c_num_tiles) {
			state.SetMode(CP_InvalidValue, false);
		} else if (!state.input[0]) {
			state.SetMode(CP_YCoord);
			state.val_min = 0;
			state.val_max = c_num_tiles;
			state.command = 'e';
		} else {
			t.tx = prev;
			t.ty = i;
			t.tz = 0;
			gwin->teleport_party(t);
		}
		break;

	case SDLK_N:    // NPC
		if (i < 0 || (i >= 356 && i <= 359)) {
			state.SetMode(CP_InvalidNPC);
		} else {
			Actor* actor = gwin->get_npc(i);
			Game_window::get_instance()->teleport_party(
					actor->get_tile(), false, actor->get_map_num());
		}
		break;

	case SDLK_M:    // map
		if ((i < 0 || i > 255) || i > highest) {
			state.SetMode(CP_InvalidValue, false);
		} else {
			gwin->teleport_party(gwin->get_main_actor()->get_tile(), true, i);
		}
		break;

	default:
		break;
	}
	std::memset(state.input, 0, sizeof(state.input));
}

// Checks the state.input
bool CheatScreen::TeleportCheck() {
	ignore_unused_variable_warning(state.activate);
	switch (state.command) {
		// Simple commands
	case SDLK_G:    // geographic
		state.SetMode(CP_NorthSouth);
		return true;

	case SDLK_H:    // hex
		state.SetMode(CP_HexXCoord);
		state.val_min = 0;
		state.val_max = c_num_tiles;
		return true;

	case SDLK_D:    // dec teleport
		state.SetMode(CP_XCoord);
		state.val_min = 0;
		state.val_max = c_num_tiles;
		return true;

	case SDLK_N:    // NPC teleport
		state.SetMode(CP_ChooseNPC);
		break;

	case SDLK_M:    // NPC teleport
		state.SetMode(CP_EnterValue);
		state.val_min = 0;
		state.val_max = gwin->get_num_npcs() - 1;
		break;

		// X and Escape leave
	case SDLK_ESCAPE:
		if (!state.input[0]) {
			state.input[0] = state.command;
		}
		return false;

	default:
		state.command = 0;
		state.SetMode(CP_InvalidCom, false);
		break;
	}

	return true;
}

int CheatScreen::AddMenuItem(
		int offsetx, int offsety, SDL_Keycode keycode, const char* label) {
	int keywidth = 8;
	switch (keycode) {
	case SDLK_UP:
		PaintArrow(offsetx + 8, offsety, '^');
		break;
	case SDLK_DOWN:
		PaintArrow(offsetx + 8, offsety, 'V');
		break;
	case SDLK_LEFT:
		PaintArrow(offsetx + 8, offsety, '<');
		break;
	case SDLK_RIGHT:
		PaintArrow(offsetx + 8, offsety, '>');
		break;
	case SDLK_ESCAPE:
		keywidth = 24;
		font->paint_text_fixedwidth(ibuf, "ESC", offsetx + 8, offsety, 8);
		break;
	default:
		if (std::isalnum(keycode)) {
			char buf[] = {char(std::toupper(keycode)), 0};

			font->paint_text_fixedwidth(ibuf, buf, offsetx + 8, offsety, 8);
		}
		break;
	}
	font->paint_text_fixedwidth(ibuf, "[", offsetx, offsety, 8);
	font->paint_text_fixedwidth(ibuf, "]", offsetx + keywidth + 8, offsety, 8);
	int labelstart = 16 + keywidth;
	int labelwidth = font->paint_text_fixedwidth(
			ibuf, label, offsetx + labelstart, offsety, 8);
	hotspots.push_back(Hotspot(keycode, offsetx, offsety, 16 + keywidth, 8));
	if (state.highlight == keycode) {
		ibuf->fill_translucent8(
				0, 8 * (int(std::strlen(label)) + 2) + keywidth, 8, offsetx,
				offsety, highlighttable);
	}
	return labelstart + labelwidth;
}

int CheatScreen::AddLeftRightMenuItem(
		int offsetx, int offsety, const char* label, bool left, bool right,
		bool leaveempty, bool fixedlabel) {
	// Change NPC

	int right_offset = (left || leaveempty) ? 8 : 0;
	int totalspace   = right_offset + ((right || leaveempty) ? 8 : 0);
	int xwidth       = right ? 8 : leaveempty ? 24 : 16;

	if (left) {
		PaintArrow(offsetx + 8, offsety, '<');
	}
	if (right) {
		PaintArrow(offsetx + 9 + right_offset, offsety, '>');
		hotspots.push_back(Hotspot(
				SDLK_RIGHT, offsetx + 9 + right_offset, offsety, 16, 8));
	}
	if (left) {
		hotspots.push_back(
				Hotspot(SDLK_LEFT, offsetx - 8, offsety, 16 + xwidth, 8));
	}

	font->paint_text_fixedwidth(ibuf, "[", offsetx, offsety, 8);
	font->paint_text_fixedwidth(
			ibuf, "]", offsetx + totalspace + 8, offsety, 8);
	int labelstart = (fixedlabel ? 32 : (totalspace + 16));
	int labelwidth = font->paint_text_fixedwidth(
			ibuf, label, offsetx + labelstart, offsety, 8);

	if (state.highlight == SDLK_LEFT && left) {
		ibuf->fill_translucent8(
				0, 8 + xwidth, 8, offsetx, offsety, highlighttable);
	} else if (state.highlight == SDLK_RIGHT && right) {
		int extend = !left ? 16 : 0;
		ibuf->fill_translucent8(
				0, 16 + extend, 8, offsetx + 8 + right_offset - extend, offsety,
				highlighttable);
	}

	return labelstart + labelwidth;
}

void CheatScreen::EndFrame() {
	PaintHotspots();
	Mouse::mouse->show();
	gwin->get_win()->show();
	Mouse::mouse->hide();    // Must immediately hide to prevent flickering
}

const int CheatScreen::button_down_finger;

void CheatScreen::WaitButtonsUp() {
	Uint32     show_message = SDL_GetTicks() + 1000;
	ClearState clear(state);
	hotspots.clear();
	while (buttons_down.size()) {
		SharedInput();
		hotspots.clear();
		gwin->clear_screen();

		// exit if escape is pressed
		if (state.command == SDLK_ESCAPE) {
			// But first eat up events if there are any
			SharedInput();
			break;
		}

		if (show_message < SDL_GetTicks()) {
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID) || defined(TEST_MOBILE)
			const int offsetx_start = 15;

			// const int offsety1 = 73;
			// const int offsety2 = 55;
			const int offsetx1 = 160;
			// const int offsety4 = 36;
			const int offsety5 = 72;
#else
			const int offsetx_start = 0;
			// const int offsety1 = 0;
			// const int offsety2 = 0;
			const int offsetx1 = 160;
			// const int offsety4 = maxy - 45;
			const int offsety5 = maxy - 36;
#endif    // eXit

			int        offsetx       = offsetx_start;
			const char msg_waiting[] = "Waiting for up events: ";
			int        offsety       = 36;
			offsetx                  = offsetx1 - 4 * std::size(msg_waiting);
			offsetx += font->paint_text_fixedwidth(
					ibuf, msg_waiting, offsetx, offsety, 8);

			bool first       = true;
			int  last_button = 0;
			for (int button : buttons_down) {
				char        buf[80]     = {0};
				const char* button_name = nullptr;

				// only each button once.. Standard guarantees that all keys
				// that compare equivalent are grouped together
				if (button == last_button) {
					continue;
				}
				last_button = button;

				switch (button) {
				case button_down_finger: {
					size_t num_fingers = buttons_down.count(button_down_finger);
					button_name        = buf;

					if (num_fingers > 1) {
						snprintf(buf, sizeof(buf), "%zu Fingers", num_fingers);
					} else {
						button_name = "Finger";
					}
				} break;

				case 1:
				case 2:
				case 3: {
					const char* button_names[]
							= {"Left Mouse Button", "Middle Mouse Button",
							   "Right Mouse Button"};

					// Don't show mouse buttons if also waiting for finger
					// up
					if (buttons_down.find(button_down_finger)
						!= buttons_down.end()) {
						continue;
					}

					button_name = button_names[button - 1];
				} break;

				default: {
					// It should be an SDL_Keycode
					const char* keyname = SDL_GetKeyName(SDL_Keycode(button));
					button_name         = buf;

					// Only display keyname if there is one and it is in ASCII
					if (keyname[0]
						&& std::none_of(
								keyname, keyname + strlen(keyname), [](char c) {
									return static_cast<unsigned char>(c) >= 128;
								})) {
						snprintf(buf, sizeof(buf), "Key %s", keyname);
					} else {
						snprintf(buf, sizeof(buf), "Unknown Key #%i", button);
					}
				} break;
				}

				if (!first) {
					// Paint a comma at the end of the previous name
					offsetx += font->paint_text_fixedwidth(
							ibuf, ", ", offsetx, offsety, 8);
				}
				first = false;

				// check if we have enough space on this line for the keyname
				// if not start a new line
				if (offsetx + strlen(button_name) * 8 > 312) {
					offsetx = offsetx_start;
					offsety += 9;
					// If we advance so many lines just break out now
					if (offsety >= (offsety5 + 9)) {
						break;
					}
				} else {
				}

				offsetx += font->paint_text_fixedwidth(
						ibuf, button_name, offsetx, offsety, 8);
			}

			offsetx                = 0;
			const char msg_press[] = "Press";
			font->paint_text_fixedwidth(
					ibuf, msg_press, offsetx + 160 - 8 * std::size(msg_press),
					offsety5 + 9, 8);

			AddMenuItem(
					offsetx + 160, offsety5 + 9, SDLK_ESCAPE, " to exit now");
		}

		EndFrame();
	}
}
