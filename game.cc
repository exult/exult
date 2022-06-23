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

#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include "menulist.h"
#include "Audio.h"
#include "Configuration.h"
#include "databuf.h"
#include "exult.h"
#include "exult_constants.h"
#include "exult_flx.h"
#include "files/U7fileman.h"
#include "files/U7file.h"
#include "files/utils.h"
#include "font.h"
#include "game.h"
#include "gamemgr/bggame.h"
#include "gamemgr/sigame.h"
#include "gamemgr/devgame.h"
#include "gamewin.h"
#include "keys.h"
#include "mouse.h"
#include "palette.h"
#include "shapeid.h"
#include "gamemgr/modmgr.h"
#include "shapes/miscinf.h"
#include "array_size.h"

#ifdef VITA
#include "vita/vita.h"
#endif


using std::cout;
using std::endl;
using std::ifstream;
using std::strcpy;
using std::string;

bool Game::new_game_flag = false;
bool Game::editing_flag = false;
Game *game = nullptr;
Exult_Game Game::game_type = NONE;
Game_Language Game::language = ENGLISH;
bool Game::expansion = false;
bool Game::sibeta = false;

static char av_name[17] = "";
static int av_sex = -1;
static int av_skin = -1;

std::string Game::gametitle;
std::string Game::modtitle;

unsigned int Game::ticks = 0;

Game::Game() {
	try {               // Okay to fail if development game.
		menushapes.load(MAINSHP_FLX, PATCH_MAINSHP);
	} catch (const exult_exception &e) {
		if (!is_editing())
			throw;
	}
	jive = false;
	gwin = Game_window::get_instance();
	win = gwin->get_win();
	ibuf = win->get_ib8();
	topx = (gwin->get_width() - 320) / 2;
	topy = (gwin->get_height() - 200) / 2;
	centerx = gwin->get_width() / 2;
	centery = gwin->get_height() / 2;
}

Game::~Game() {
	game_type = NONE;
	language = ENGLISH;
	delete xml;
	while (!xmlstrings.empty()) {
		char *str = xmlstrings.back();
		xmlstrings.pop_back();
		delete [] str;
	}
	U7FileManager::get_ptr()->reset();
	Shapeinfo_lookup::reset();
}

Game *Game::create_game(BaseGameInfo *mygame) {
	mygame->setup_game_paths();
	gametitle = mygame->get_cfgname();
	modtitle = mygame->get_mod_title();
	game_type = mygame->get_game_type();
	language = mygame->get_game_language();
	expansion = mygame->have_expansion();
	sibeta = mygame->is_si_beta();
	editing_flag = mygame->being_edited();

	// Need to do this here. Maybe force on for EXULT_DEVEL_GAME too?
	std::string str;
	config->value("config/gameplay/bg_paperdolls", str, "yes");
	sman->set_paperdoll_status(game_type == SERPENT_ISLE || str == "yes");
	config->set("config/gameplay/bg_paperdolls", str, true);
	char buf[256];
	if (!mygame->get_mod_title().empty())
		snprintf(buf, 256, " with the '%s' modification.",
		         mygame->get_mod_title().c_str());
	else
		buf[0] = 0;
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
	if (is_system_path_defined("<PATCH>"))
		cout << "Patch   : " << get_system_path("<PATCH>") << endl;
	else
		cout << "Patch   : none" << endl;
	cout << endl;

	return game;
}

const char *xml_root = "Game_config";

void Game::add_shape(const char *name, int shapenum) {
	shapes[name] = shapenum;
}

int Game::get_shape(const char *name) {
	if (xml) {
		string key = string(xml_root) + "/shapes/" + name;
		int ret;
		xml->value(key, ret, 0);
		return ret;
	} else
		return shapes[name];
}

void Game::add_resource(const char *name, const char *str, int num) {
	resources[name].str = str;
	resources[name].num = num;
}

const str_int_pair &Game::get_resource(const char *name) {
	auto it = resources.find(name);
	if (it != resources.end())
		return it->second;
	else if (xml) {
		string key = string(xml_root) + "/resources/" + name;
		string str;
		xml->value(key, str, "");
		char *res = newstrdup(str.c_str());
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
		snprintf(buf, array_size(buf),
		         "Game::get_resource: Illegal resource requested: '%s'", name);
		throw exult_exception(buf);
	}
}

/*
 *  Write out game resources/shapes to "patch/exultgame.xml".
*/
void Game::write_game_xml() {
	string name = get_system_path("<PATCH>/exultgame.xml");

	U7mkdir("<PATCH>", 0755);   // Create dir. if not already there.
	if (U7exists(name))
		U7remove(name.c_str());
	string root = xml_root;
	Configuration xml(name, root);
	for (auto& resource : resources) {
		string key = root;
		key += "/resources/";
		key += resource.first;
		str_int_pair &val = resource.second;
		if (val.str)
			xml.set(key.c_str(), val.str, false);
		if (val.num != 0) {
			string valkey = key;
			valkey += "/num";
			xml.set(valkey.c_str(), val.num, false);
		}
	}
	for (auto& shape : shapes) {
		string key = root;
		key += "/shapes/";
		key += shape.first;
		int num = shape.second;
		xml.set(key.c_str(), num, false);
	}
	xml.write_back();
}

/*
 *  Read in game resources/shapes.  First try 'name1', then
 *  "exultgame.xml" in the "patch" and "static" directories.
 *  Output: true if successful.
 */
bool Game::read_game_xml(const char *name1) {
	const char *nm;

	if (name1 && U7exists(name1))
		nm = name1;
	else if (!U7exists(nm = "<PATCH>/exultgame.xml")) {
		if (!U7exists(nm = "<STATIC>/exultgame.xml"))
			return false;
	}
	xml = new Configuration;
	string namestr = get_system_path(nm);
	xml->read_abs_config_file(namestr);
	std::cout << "Reading game configuration from '" << namestr.c_str() <<
	          "'." << std::endl;
	return true;
}


bool Game::show_menu(bool skip) {
	int menuy = topy + 120;
	// Brand-new game in development?
	if (skip || (is_editing() && !U7exists(MAINSHP_FLX))) {
		bool first = !U7exists(IDENTITY);
		if (first)
			set_avname("Newbie");
		return gwin->init_gamedat(first);
	}
	IExultDataSource mouse_data(MAINSHP_FLX, PATCH_MAINSHP, 19);
	menu_mouse = new Mouse(gwin, mouse_data);

	top_menu();
	MenuList *menu = nullptr;


	int menuchoices[] = { 0x04, 0x05, 0x08, 0x06, 0x11, 0x12, 0x07 };
	int num_choices = array_size(menuchoices);

	Vga_file exult_flx(BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX));
	char npc_name[16];
	snprintf(npc_name, 16, "Exult");
	bool play = false;
	bool fadeout = true;
	bool exitmenu = false;

	do {
		if (!menu) {
			menu = new MenuList();
			int offset = 0;
			for (int i = 0; i < num_choices; i++) {
				if ((i != 4 && i != 5) || (i == 4 && U7exists("<SAVEGAME>/quotes.flg")) || (i == 5 && U7exists("<SAVEGAME>/endgame.flg"))) {
					Shape_frame *f0 = menushapes.get_shape(menuchoices[i], 0);
					Shape_frame *f1 = menushapes.get_shape(menuchoices[i], 1);
					assert(f0 != nullptr && f1 != nullptr);
					auto *entry = new MenuEntry(f1, f0, centerx, menuy + offset);
					entry->set_id(i);
					menu->add_entry(entry);
					offset += f1->get_ybelow() + 3;
				}
			}
			menu->set_selection(2);
		}

		bool created = false;
		int choice = menu->handle_events(gwin, menu_mouse);
		switch (choice) {
		case -1: // Exit
#ifdef __IPHONEOS__
			break;
#else
			pal->fade_out(c_fade_out_time);
			Audio::get_ptr()->stop_music();
			delete menu_mouse;
			delete menu;
			throw quit_exception();
#endif
		case 0: // Intro
			if (game_type == EXULT_DEVEL_GAME)
				break;
			pal->fade_out(c_fade_out_time);
			play_intro();
			gwin->clear_screen(true);
			top_menu();
			break;
		case 2: // Journey Onwards
			created = gwin->init_gamedat(false);
			if (!created) {
				show_journey_failed();
				gwin->clear_screen(true);
				top_menu();
				menu->set_selection(1);
				break;
			}
			exitmenu = true;
			fadeout = true;
			play = true;
			break;
		case 1: // New Game
			if (new_game(menushapes))
				exitmenu = true;
			else
				break;
			fadeout = false;
			play = true;
			break;
		case 3: // Credits
			pal->fade_out(c_fade_out_time);
			show_credits();
			delete menu;
			menu = nullptr;
			top_menu();
			break;
		case 4: // Quotes
			pal->fade_out(c_fade_out_time);
			show_quotes();
			top_menu();
			break;
		case 5: // End Game
			if (game_type == EXULT_DEVEL_GAME)
				break;
			pal->fade_out(c_fade_out_time);
			end_game(true);
			top_menu();
			break;
		case 6: // Return to Menu
			play = false;
			exitmenu = true;
			fadeout = true;
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
	delete menu_mouse;
	return play;
}

void Game::journey_failed_text() {
	Font *font = fontManager.get_font("MENU_FONT");
	font->center_text(ibuf, centerx, centery + 30,  "You must start a new game first.");
	font->center_text(ibuf, centerx, centery + 42,  "Press ESC to return.");
	pal->fade_in(50);
	while (!wait_delay(10))
		;
	pal->fade_out(50);
}

const char *Game::get_avname() {
	if (av_name[0])
		return av_name;
	else
		return nullptr;
}

int Game::get_avsex() {
	return av_sex;
}
int Game::get_avskin() {
	return av_skin;
}

// Assume safe
void Game::set_avname(const char *name) {
	strcpy(av_name, name);
}

void Game::set_avsex(int sex) {
	av_sex = sex;
}

void Game::set_avskin(int skin) {
	av_skin = skin;
}

void Game::clear_avname() {
	av_name[0] = 0;
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
	SDL_Event event;
	static uint32 last_b3_click = 0;
	unsigned long delay;
	int loops;

	int loopinterval = (ncol == 0) ? 50 : 10;
	if (!ms) ms = 1;
	if (ms <= 2 * loopinterval) {
		delay = ms;
		loops = 1;
	} else {
		delay = loopinterval;
		loops = ms / static_cast<long>(delay);
	}
	Game_window *gwin = Game_window::get_instance();
	int rot_speed = rotspd << (gwin->get_win()->fast_palette_rotate() ? 0 : 1);

	static unsigned long last_rotate = 0;

	for (int i = 0; i < loops; i++) {
		unsigned long ticks1 = SDL_GetTicks();
		// this may be a bit risky... How fast can events be generated?
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym) {
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
				case SDLK_s:
					if ((event.key.keysym.mod & KMOD_ALT) &&
					        (event.key.keysym.mod & KMOD_CTRL))
						make_screenshot(true);
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
			case SDL_MOUSEBUTTONDOWN:
				break;
			case SDL_MOUSEBUTTONUP: {
				if (event.button.button == 3 || event.button.button == 1) {
					if (ticks1 - last_b3_click < 500)
						return 1;
					last_b3_click = ticks1;
				}
				break;
			}
#ifdef VITA
      case SDL_JOYBUTTONDOWN:
        switch (event.jbutton.button) {
          case VITA_BUTTON_CIRCLE:
            return 1;
          break;
          case VITA_BUTTON_CROSS:
            return 2;
          break;
          case VITA_BUTTON_START:
            return 2;
          break;
          default:
          break;
        }
#endif
			default:
				break;
			}
		}
		unsigned long ticks2 = SDL_GetTicks();
		if (ticks2 - ticks1 > delay)
			i += (ticks2 - ticks1) / delay - 1;
		else
			SDL_Delay(delay - (ticks2 - ticks1));
		if (abs(ncol) > 1 && ticks2 > last_rotate + rot_speed) {
			gwin->get_win()->rotate_colors(startcol, ncol, 1);
			while (ticks2 > last_rotate + rot_speed)
				last_rotate += rot_speed;
			gwin->get_win()->show();
		}
	}

	return 0;
}
