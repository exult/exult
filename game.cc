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

#include "game.h"

#include "Audio.h"
#include "Configuration.h"
#include "databuf.h"
#include "exult.h"
#include "exult_constants.h"
#include "exult_flx.h"
#include "files/U7file.h"
#include "files/U7fileman.h"
#include "files/utils.h"
#include "font.h"
#include "gameclk.h"
#include "gamemgr/bggame.h"
#include "gamemgr/devgame.h"
#include "gamemgr/modmgr.h"
#include "gamemgr/sigame.h"
#include "gamewin.h"
#include "items.h"
#include "keys.h"
#include "menulist.h"
#include "mouse.h"
#include "palette.h"
#include "shapeid.h"
#include "shapes/miscinf.h"

#include <unistd.h>

#include <array>
#include <cstdlib>
#include <cstring>
#include <sstream>

using std::cout;
using std::endl;
using std::ifstream;
using std::string;

bool          Game::new_game_flag = false;
bool          Game::editing_flag  = false;
Game*         game                = nullptr;
Exult_Game    Game::game_type     = NONE;
Game_Language Game::language      = ENGLISH;
bool          Game::expansion     = false;
bool          Game::sibeta        = false;

static char av_name[17] = "";
static int  av_sex      = -1;
static int  av_skin     = -1;

std::string Game::gametitle;
std::string Game::modtitle;

unsigned int Game::ticks = 0;

Game::Game() {
	try {    // Okay to fail if development game.
		menushapes.load(MAINSHP_FLX, PATCH_MAINSHP);
	} catch (const exult_exception&) {
		if (!is_editing()) {
			throw;
		}
	}
	jive    = false;
	gwin    = Game_window::get_instance();
	win     = gwin->get_win();
	ibuf    = win->get_ib8();
	topx    = (gwin->get_width() - 320) / 2;
	topy    = (gwin->get_height() - 200) / 2;
	centerx = gwin->get_width() / 2;
	centery = gwin->get_height() / 2;
}

Game::~Game() {
	game_type = NONE;
	language  = ENGLISH;
	delete xml;
	while (!xmlstrings.empty()) {
		char* str = xmlstrings.back();
		xmlstrings.pop_back();
		delete[] str;
	}
	U7FileManager::get_ptr()->reset();
	Shapeinfo_lookup::reset();
}

Game* Game::create_game(BaseGameInfo* mygame) {
	mygame->setup_game_paths();
	gametitle    = mygame->get_cfgname();
	modtitle     = mygame->get_mod_title();
	game_type    = mygame->get_game_type();
	language     = mygame->get_game_language();
	expansion    = mygame->have_expansion();
	sibeta       = mygame->is_si_beta();
	editing_flag = mygame->being_edited();

	// Need to do this here. Maybe force on for EXULT_DEVEL_GAME too?
	std::string str;
	config->value("config/gameplay/bg_paperdolls", str, "yes");
	sman->set_paperdoll_status(game_type == SERPENT_ISLE || str == "yes");
	config->set("config/gameplay/bg_paperdolls", str, true);
	char buf[256];
	if (!mygame->get_mod_title().empty()) {
		snprintf(
				buf, sizeof(buf), " with the '%s' modification.",
				mygame->get_mod_title().c_str());
	} else {
		buf[0] = 0;
	}
	switch (game_type) {
	case BLACK_GATE:
		cout << "Starting a BLACK GATE game" << buf << endl;
		game = new BG_Game();
		break;
	case SERPENT_ISLE:
		cout << "Starting a SERPENT ISLE game" << endl;
		game = new SI_Game();
		break;
	case EXULT_DEVEL_GAME:
		cout << "Starting '" << gametitle << "' game" << endl;
		game = new DEV_Game();
		break;
	case EXULT_MENU_GAME:
		cout << "Starting '" << gametitle << "' game" << endl;
		game = new DEV_Game();
		break;
	default:
		cout << "Unrecognized game type!" << endl;
		game = nullptr;
	}

	cout << "Game path settings:" << std::endl;
	cout << "Static  : " << get_system_path("<STATIC>") << endl;
	cout << "Gamedat : " << get_system_path("<GAMEDAT>") << endl;
	cout << "Savegame: " << get_system_path("<SAVEGAME>") << endl;
	if (is_system_path_defined("<PATCH>")) {
		cout << "Patch   : " << get_system_path("<PATCH>") << endl;
	} else {
		cout << "Patch   : none" << endl;
	}
	cout << endl;

	return game;
}

void Game::show_congratulations(Palette* pal0) {
	Game_clock* clock      = gwin->get_clock();
	int         total_time = clock->get_total_hours();

	if (wait_delay(100)) {
		throw UserSkipException();
	}

	// Paint background black
	gwin->clear_screen(true);
	win->fill8(0);

	std::shared_ptr<Font> end_font = fontManager.get_font("EXULT_END_FONT");
	const int             starty
			= (gwin->get_height() - end_font->get_text_height() * 8) / 2;

	// calculate the time it took to complete the game
	// in exultmsg.txt it is "%d year s ,  %d month s , &  %d day s"
	// only showing years or months if there were any
	auto messages = get_congratulations_messages();
	for (unsigned i = 0; i < messages.size(); i++) {
		const char* message = get_text_msg(messages[i]);
		// if we are on the line with the time played
		if (i == 2) {
			enum TokenTypes {
				TokenAnd,
				TokenOnly,
				TokenExactly,
				TokenYear,
				TokenYears,
				TokenMonth,
				TokenMonths,
				TokenDay,
				TokenDays,
				TokenHour,
				TokenHours,
				TokenNegativeTime,
				TokenCount
			};

			std::stringstream        reader(message);
			std::vector<std::string> tokens;
			tokens.reserve(TokenCount);
			std::string temp;
			while (std::getline(reader, temp, '|')) {
				tokens.emplace_back(std::move(temp));
			}
			tokens.resize(TokenCount);    // Just in case
			// this is how you would calculate years but since UltimaVII
			// congrats screen has 13 months per year and VI in game calendar
			// states 12 months per year it was decided to keep only months as
			// we don't know which year is correct 8064 hours = 336 days, 672
			// hours = 28 days const int year = total_time/8064; total_time %=
			// 8064; remove the initial 6 hours
			std::string displayMessage;
			displayMessage.reserve(50);
			total_time -= 6;
			if (total_time < 0) {
				displayMessage += tokens[TokenNegativeTime];
			} else {
				const int month = total_time / 672;
				total_time %= 672;
				const int day = total_time / 24;
				total_time %= 24;
				const int hour = total_time;
				auto      get_token =
						[&tokens](int value, TokenTypes one, TokenTypes many) {
							return std::to_string(value)
								   + tokens[value == 1 ? one : many];
						};
				// At least two of month, day, or hour is not zero because hour
				// cannot be zero if day and month are both zero.
				if (month > 0) {
					if (day == 0 && hour == 0) {
						// in the remote chance a player finishes on exactly 0
						// hours, 0 days and X month(s)
						displayMessage += tokens[TokenExactly];
					}
					displayMessage += get_token(month, TokenMonth, TokenMonths);
					// add ampersand or comma if there is more to display.
					if (day > 0 && hour > 0) {
						displayMessage += ", ";
					} else if (day > 0 || hour > 0) {
						displayMessage += tokens[TokenAnd];
					}
				}
				if (day > 0) {
					if (month == 0 && hour == 0) {
						displayMessage += tokens[TokenExactly];
					}
					displayMessage += get_token(day, TokenDay, TokenDays);
					// add ampersand only if there is more to display.
					if (hour > 0) {
						displayMessage += tokens[TokenAnd];
					}
				}
				if (hour > 0 || (month == 0 && day == 0)) {
					// if no days, display hours (this would only happen on
					// exactly 1,2,3 etc months) Here so the player doesn't
					// think we didn't track the hours/days. so 112 days at 2am
					// would display "4 months & 2 hours", 113 days at 2am would
					// display "4 months & 1 day"
					if (month == 0 && day == 0) {
						displayMessage += tokens[TokenOnly];
					}
					displayMessage += get_token(hour, TokenHour, TokenHours);
				}
				displayMessage += '.';
			}
			message = displayMessage.c_str();
			end_font->draw_text(
					ibuf, centerx - end_font->get_text_width(message) / 2,
					starty + end_font->get_text_height() * i, message);
		} else {
			end_font->draw_text(
					ibuf, centerx - end_font->get_text_width(message) / 2,
					starty + end_font->get_text_height() * i, message);
		}
	}

	// Fade in for 1 sec (50 cycles)
	pal0->fade(50, 1, 0);

	// Display text for 20 seonds (only 8 at the moment)
	for (unsigned int i = 0; i < 80; i++) {
		if (wait_delay(100)) {
			throw UserSkipException();
		}
	}

	// Fade out for 1 sec (50 cycles)
	pal0->fade(50, 0, 0);
}

const char* xml_root = "Game_config";

void Game::add_shape(const char* name, int shapenum) {
	shapes[name] = shapenum;
}

int Game::get_shape(const char* name) {
	if (xml) {
		const string key = string(xml_root) + "/shapes/" + name;
		int          ret;
		xml->value(key, ret, 0);
		return ret;
	} else {
		return shapes[name];
	}
}

void Game::add_resource(const char* name, const char* str, int num) {
	resources[name].str = str;
	resources[name].num = num;
}

const str_int_pair& Game::get_resource(const char* name) {
	auto it = resources.find(name);
	if (it != resources.end()) {
		return it->second;
	} else if (xml) {
		string key = string(xml_root) + "/resources/" + name;
		string str;
		xml->value(key, str, "");
		char* res = newstrdup(str.c_str());
		// This is used to prevent leaks.
		key += "/num";
		int num;
		xml->value(key, num, 0);
		// Add it for next time.
		resources[name].str = res;
		resources[name].num = num;
		xmlstrings.push_back(res);
		return resources[name];
	} else {
		char buf[250];
		snprintf(
				buf, sizeof(buf),
				"Game::get_resource: Illegal resource requested: '%s'", name);
		throw exult_exception(buf);
	}
}

/*
 *  Write out game resources/shapes to "patch/exultgame.xml".
 */
void Game::write_game_xml() {
	const string name = get_system_path("<PATCH>/exultgame.xml");

	U7mkdir("<PATCH>", 0755);    // Create dir. if not already there.
	if (U7exists(name)) {
		U7remove(name.c_str());
	}
	const string  root = xml_root;
	Configuration xml(name, root);
	for (auto& resource : resources) {
		string key = root;
		key += "/resources/";
		key += resource.first;
		const str_int_pair& val = resource.second;
		if (val.str) {
			xml.set(key.c_str(), val.str, false);
		}
		if (val.num != 0) {
			key += "/num";
			xml.set(key.c_str(), val.num, false);
		}
	}
	for (auto& shape : shapes) {
		string key = root;
		key += "/shapes/";
		key += shape.first;
		const int num = shape.second;
		xml.set(key.c_str(), num, false);
	}
	xml.write_back();
}

/*
 *  Read in game resources/shapes.  First try 'name1', then
 *  "exultgame.xml" in the "patch" and "static" directories.
 *  Output: true if successful.
 */
bool Game::read_game_xml(const char* name1) {
	const char* nm;

	if (name1 && U7exists(name1)) {
		nm = name1;
	} else if (!U7exists(nm = "<PATCH>/exultgame.xml")) {
		if (!U7exists(nm = "<STATIC>/exultgame.xml")) {
			return false;
		}
	}
	xml                  = new Configuration;
	const string namestr = get_system_path(nm);
	xml->read_abs_config_file(namestr);
	std::cout << "Reading game configuration from '" << namestr.c_str() << "'."
			  << std::endl;
	return true;
}

bool Game::show_menu(bool skip) {
	const int menuy = topy + 120;
	// Brand-new game in development?
	if (skip || (is_editing() && !U7exists(MAINSHP_FLX))) {
		const bool first = !U7exists(IDENTITY);
		if (first) {
			set_avname("Newbie");
		}
		return gwin->init_gamedat(first);
	}
	IExultDataSource mouse_data(MAINSHP_FLX, PATCH_MAINSHP, 19);
	menu_mouse   = new Mouse(gwin, mouse_data);
	Mouse::mouse = menu_mouse;

	top_menu();
	MenuList* menu = nullptr;

	constexpr static const std::array menuchoices{0x04, 0x05, 0x08, 0x06,
												  0x11, 0x12, 0x07};

	const Vga_file exult_flx(BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX));
	char           npc_name[16];
	snprintf(npc_name, sizeof(npc_name), "Exult");
	bool play     = false;
	bool fadeout  = true;
	bool exitmenu = false;

	do {
		if (!menu) {
			menu       = new MenuList();
			int offset = 0;
			for (size_t i = 0; i < menuchoices.size(); i++) {
				if ((i != 4 && i != 5)
					|| (i == 4 && U7exists("<SAVEGAME>/quotes.flg"))
					|| (i == 5 && U7exists("<SAVEGAME>/endgame.flg"))) {
					Shape_frame* f0 = menushapes.get_shape(menuchoices[i], 0);
					Shape_frame* f1 = menushapes.get_shape(menuchoices[i], 1);
					assert(f0 != nullptr && f1 != nullptr);
					auto* entry
							= new MenuEntry(f1, f0, centerx, menuy + offset);
					entry->set_id(i);
					menu->add_entry(entry);
					offset += f1->get_ybelow() + 3;
				}
			}
			menu->set_selection(2);
		}

		bool      created = false;
		const int choice  = menu->handle_events(gwin, menu_mouse);
		switch (choice) {
		case -1:    // Exit
#ifdef SDL_PLATFORM_IOS
			break;
#else
			pal->fade_out(c_fade_out_time);
			Audio::get_ptr()->stop_music();
			delete menu_mouse;
			menu_mouse   = nullptr;
			Mouse::mouse = nullptr;
			delete menu;
			throw quit_exception();
#endif
		case 0:    // Intro
			if (game_type == EXULT_DEVEL_GAME) {
				break;
			}
			pal->fade_out(c_fade_out_time);
			play_intro();
			gwin->clear_screen(true);
			top_menu();
			break;
		case 2:    // Journey Onwards
			created = gwin->init_gamedat(false);
			if (!created) {
				show_journey_failed();
				gwin->clear_screen(true);
				top_menu();
				menu->set_selection(1);
				break;
			}
			exitmenu = true;
			fadeout  = true;
			play     = true;
			break;
		case 1:    // New Game
			if (new_game(menushapes)) {
				exitmenu = true;
			} else {
				break;
			}
			fadeout = false;
			play    = true;
			break;
		case 3:    // Credits
			pal->fade_out(c_fade_out_time);
			show_credits();
			delete menu;
			menu = nullptr;
			top_menu();
			break;
		case 4:    // Quotes
			pal->fade_out(c_fade_out_time);
			show_quotes();
			top_menu();
			break;
		case 5:    // End Game
			if (game_type == EXULT_DEVEL_GAME) {
				break;
			}
			pal->fade_out(c_fade_out_time);
			end_game(true, false);
			top_menu();
			break;
		case 6:    // Return to Menu
			play     = false;
			exitmenu = true;
			fadeout  = true;
			break;
		default:
			break;
		}
	} while (!exitmenu);

	if (fadeout) {
		pal->fade_out(c_fade_out_time);
		gwin->clear_screen(true);
	}
	delete menu;
	Audio::get_ptr()->stop_music();
	Mouse::mouse = nullptr;
	delete menu_mouse;
	menu_mouse = nullptr;
	return play;
}

void Game::journey_failed_text() {
	std::shared_ptr<Font> font = fontManager.get_font("MENU_FONT");
	font->center_text(
			ibuf, centerx, centery + 30, "You must start a new game first.");
	font->center_text(ibuf, centerx, centery + 42, "Press ESC to return.");
	pal->fade_in(50);
	while (!wait_delay(10))
		;
	pal->fade_out(50);
}

int Game::waitforspeech() {
	Audio* audio = Audio::get_ptr();
	if (!audio->is_speech_playing()) {
		return -1;
	}
	// uh oh speech from ingame is playing. We will wait for it to finish
	//
	// but first grey out the screen

	win->fill_translucent8(
			0, gwin->get_game_width(), gwin->get_game_height(), 0, 0,
			Game_singletons::sman->get_xform(8));

	gwin->show(true);

	return audio->wait_for_speech([](int ms) {
		// use this as wait function so input terminates the speech
		// immediately.
		return wait_delay(ms);
	});
}

const char* Game::get_avname() {
	if (av_name[0]) {
		return av_name;
	} else {
		return nullptr;
	}
}

int Game::get_avsex() {
	return av_sex;
}

int Game::get_avskin() {
	return av_skin;
}

// Assume safe
void Game::set_avname(const char* name) {
	strcpy(av_name, name);
}

void Game::set_avsex(int sex) {
	av_sex = sex;
}

void Game::set_avskin(int skin) {
	av_skin = skin;
}

void Game::clear_avname() {
	av_name[0]    = 0;
	new_game_flag = false;
}

void Game::clear_avsex() {
	av_sex = -1;
}

void Game::clear_avskin() {
	av_skin = -1;
}

// wait ms milliseconds, while cycling colours startcol to startcol+ncol-1
// return 0 if time passed completly, 1 if user pressed any key or mouse button,
// and 2 if user pressed Return/Enter
int wait_delay(int ms, int startcol, int ncol, int rotspd) {
	SDL_Event     event;
	static uint32 last_b3_click = 0;
	unsigned long delay;
	int           loops;

	const int loopinterval = (ncol == 0) ? 50 : 10;
	if (!ms) {
		ms = 1;
	}
	if (ms <= 2 * loopinterval) {
		delay = ms;
		loops = 1;
	} else {
		delay = loopinterval;
		loops = ms / static_cast<long>(delay);
	}
	Game_window* gwin      = Game_window::get_instance();
	const int    rot_speed = rotspd
						  << (gwin->get_win()->fast_palette_rotate() ? 0 : 1);

	static unsigned long last_rotate = 0;

	for (int i = 0; i < loops; i++) {
		const unsigned long ticks1 = SDL_GetTicks();
		// this may be a bit risky... How fast can events be generated?
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_EVENT_KEY_DOWN:
				switch (event.key.key) {
				case SDLK_RSHIFT:
				case SDLK_LSHIFT:
				case SDLK_RCTRL:
				case SDLK_LCTRL:
				case SDLK_RALT:
				case SDLK_LALT:
				case SDLK_RGUI:
				case SDLK_LGUI:
				case SDLK_NUMLOCKCLEAR:
				case SDLK_CAPSLOCK:
				case SDLK_SCROLLLOCK:
					break;
				case SDLK_S:
					if ((event.key.mod & SDL_KMOD_ALT)
						&& (event.key.mod & SDL_KMOD_CTRL)) {
						make_screenshot(true);
					}
					break;
				case SDLK_ESCAPE:
					return 1;
				case SDLK_SPACE:
				case SDLK_RETURN:
				case SDLK_KP_ENTER:
					return 2;
				default:
					break;
				}
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
				break;
			case SDL_EVENT_MOUSE_BUTTON_UP: {
				if (event.button.button == 3 || event.button.button == 1) {
					if (ticks1 - last_b3_click < 500) {
						return 1;
					}
					last_b3_click = ticks1;
				}
				break;
			}
			default:
				break;
			}
		}
		const unsigned long ticks2 = SDL_GetTicks();
		if (ticks2 - ticks1 > delay) {
			i += (ticks2 - ticks1) / delay - 1;
		} else {
			SDL_Delay(delay - (ticks2 - ticks1));
		}
		if (abs(ncol) > 1 && ticks2 > last_rotate + rot_speed) {
			gwin->get_win()->rotate_colors(startcol, ncol, 1);
			while (ticks2 > last_rotate + rot_speed) {
				last_rotate += rot_speed;
			}
			gwin->get_win()->show();
		}
	}

	return 0;
}
