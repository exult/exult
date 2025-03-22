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

#include "exultmenu.h"

#include "Audio.h"
#include "Configuration.h"
#include "XMidiFile.h"
#include "bggame.h"
#include "cheat.h"
#include "data/exult_flx.h"
#include "databuf.h"
#include "exult.h"
#include "fnames.h"
#include "font.h"
#include "game.h"
#include "gamemgr/modmgr.h"
#include "gamewin.h"
#include "gumps/Gamemenu_gump.h"
#include "ibuf8.h"
#include "ignore_unused_variable_warning.h"
#include "items.h"
#include "menulist.h"
#include "mouse.h"
#include "palette.h"
#include "shapeid.h"
#include "sigame.h"
#include "span.h"
#include "txtscroll.h"

#include <array>
#include <memory>

#ifdef SDL_PLATFORM_IOS
#	include "ios_utils.h"
#endif

using std::unique_ptr;

#define MAX_GAMES 100

static inline bool handle_menu_click(
		int id, int& first, int last_page, int pagesize) {
	switch (id) {
	case -8:
		first = 0;
		return true;
	case -7:
		first -= pagesize;
		return true;
	case -6:
		first += pagesize;
		return true;
	case -5:
		first = last_page;
		return true;
	default:
		return false;
	}
}

int maximum_size(
		std::shared_ptr<Font> font, tcb::span<const char* const> options,
		int centerx) {
	ignore_unused_variable_warning(centerx);
	int max_width = 0;
	for (const auto* option : options) {
		const int width = font->get_text_width(option);
		if (width > max_width) {
			max_width = width;
		}
	}
	max_width += 16;
	return max_width;
}

void create_scroller_menu(
		MenuList* menu, std::shared_ptr<Font> fonton,
		std::shared_ptr<Font> font, int first, int pagesize, int num_choices,
		int xpos, int ypos) {
	constexpr static const std::array menuscroller{
			"FIRST", "PREVIOUS", "NEXT", "LAST"};
	assert(menuscroller.size() == 4);
	const int max_width = maximum_size(font, menuscroller, xpos);
	xpos                = xpos - max_width * 3 / 2;

	num_choices--;
	const int lastpage = num_choices - num_choices % pagesize;

	for (size_t i = 0; i < menuscroller.size(); i++) {
		// Check to see if this entry is needed at all:
		if ((i >= 2 || first != 0) && (i != 0 || first != pagesize)
			&& (i < 2 || lastpage != first)
			&& (i != 3 || lastpage != first + pagesize)) {
			auto* entry = new MenuTextEntry(
					fonton, font, menuscroller[i], xpos, ypos);
			// These commands have negative ids:
			entry->set_id(i - 8);
			menu->add_entry(entry);
		}
		xpos += max_width;
	}
}

ExultMenu::ExultMenu(Game_window* gw) {
	gwin              = gw;
	ibuf              = gwin->get_win()->get_ib8();
	const char* fname = BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX);
	fontManager.add_font("CREDITS_FONT", fname, EXULT_FLX_FONT_SHP, 1);
	fontManager.add_font("HOT_FONT", fname, EXULT_FLX_FONTON_SHP, 1);
	fontManager.add_font("NAV_FONT", fname, EXULT_FLX_NAVFONT_SHP, 1);
	fontManager.add_font("HOT_NAV_FONT", fname, EXULT_FLX_NAVFONTON_SHP, 1);
	calc_win();
	exult_flx.load(fname);
}

void ExultMenu::calc_win() {
	centerx  = gwin->get_width() / 2;
	centery  = gwin->get_height() / 2;
	auto fnt = font ? font : fontManager.get_font("CREDITS_FONT");
	pagesize = std::max(
			1, 2
					   * ((gwin->get_win()->get_full_height()
						   - 5 * fnt->get_text_height() - 15)
						  / 45));
}

void ExultMenu::setup() {
	ModManager* mm = gamemanager->get_bg();
	if (!mm) {
		mm = gamemanager->get_si();
	}
	if (!mm) {
		mm = gamemanager->get_game(0);
	}
	if (!mm) {
		std::cerr << "No games found. Unable to show gumps in Exult menu."
				  << std::endl;
		return;
	}
	// ModManager mm_exult_menu_game (*mm);
	// mm_exult_menu_game.set_game_type(EXULT_MENU_GAME);

	Game* exult_menu_game = Game::create_game(mm);

	// Load text messages if needed
	if (!get_num_text_msgs()) {
		Setup_text(GAME_SI, Game::has_expansion(), GAME_SIB);
	}

	if (!Shape_manager::get_instance()->load_gumps_minimal()) {
		std::cerr << "Unable to show gumps in Exult menu." << std::endl;
		return;
	}

	Mouse::mouse = menu_mouse;

	gwin->clear_screen(true);

	Palette* gpal = gwin->get_pal();
	gpal->fade(0, 1, 0);

	gwin->set_in_exult_menu(true);
	Gamemenu_gump::do_exult_menu();
	gwin->set_in_exult_menu(false);

	Mouse::mouse = nullptr;
	delete exult_menu_game;
	game = nullptr;

	gwin->clear_screen(true);
	gpal->load(BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX), EXULT_FLX_EXULT0_PAL);
	gpal->apply();
}

std::unique_ptr<MenuList> ExultMenu::create_main_menu(int first) {
	auto menu = std::make_unique<MenuList>();

	int          ypos = 15 + gwin->get_win()->get_start_y();
	Shape_frame* fr   = exult_flx.get_shape(EXULT_FLX_SFX_ICON_SHP, 0);
	if (fr == nullptr) {
		std::cerr << "Exult.flx file is corrupted. Please reinstall Exult."
				  << std::endl;
		throw quit_exception();
	}
	int xpos = (gwin->get_win()->get_full_width() / 2 + fr->get_width()) / 2;
	std::vector<ModManager>& game_list   = gamemanager->get_game_list();
	const int                num_choices = game_list.size();
	const int                last
			= num_choices > first + pagesize ? first + pagesize : num_choices;
	for (int i = first; i < last; i++) {
		const int menux = xpos + (i % 2) * gwin->get_win()->get_full_width() / 2
						  + gwin->get_win()->get_start_x();
		const ModManager& exultgame = game_list[i];
		const bool        have_sfx
				= Audio::have_config_sfx(exultgame.get_cfgname())
				  || Audio::have_roland_sfx(exultgame.get_game_type())
				  || Audio::have_sblaster_sfx(exultgame.get_game_type())
				  || Audio::have_midi_sfx();

		Shape_frame* sfxicon
				= exult_flx.get_shape(EXULT_FLX_SFX_ICON_SHP, have_sfx ? 1 : 0);
		auto* entry = new MenuGameEntry(
				fonton, font, exultgame.get_menu_string().c_str(), sfxicon,
				menux, ypos);
		entry->set_id(i);
		menu->add_entry(entry);
		if (exultgame.has_mods()) {
			auto* mod_entry = new MenuTextEntry(
					navfonton, navfont, "SHOW MODS", menux,
					ypos + entry->get_height() + 4);
			mod_entry->set_id(i + MAX_GAMES);
			menu->add_entry(mod_entry);
		}
		if (i % 2) {
			ypos += 45;
		}
	}

	create_scroller_menu(
			menu.get(), navfonton, navfont, first, pagesize, num_choices,
			centerx,
			gwin->get_win()->get_end_y() - 5 * font->get_text_height());

	constexpr static const std::array menuchoices{
			"SETUP", "CREDITS", "QUOTES",
#ifdef SDL_PLATFORM_IOS
			"HELP"
#else
			"EXIT"
#endif
	};
	const int max_width = maximum_size(font, menuchoices, centerx);
	xpos                = centerx - max_width * (menuchoices.size() - 1) / 2;
	ypos = gwin->get_win()->get_end_y() - 3 * font->get_text_height();
	for (size_t i = 0; i < menuchoices.size(); i++) {
		auto* entry
				= new MenuTextEntry(fonton, font, menuchoices[i], xpos, ypos);
		// These commands have negative ids:
		entry->set_id(i - 4);
		menu->add_entry(entry);
		xpos += max_width;
	}
	menu->set_selection(0);
	return menu;
}

std::unique_ptr<MenuList> ExultMenu::create_mods_menu(
		ModManager* selgame, int first) {
	auto menu = std::make_unique<MenuList>();

	int ypos = 15 + gwin->get_win()->get_start_y();
	int xpos = gwin->get_win()->get_full_width() / 4;

	std::vector<ModInfo>& mod_list    = selgame->get_mod_list();
	const int             num_choices = mod_list.size();
	const int             last
			= num_choices > first + pagesize ? first + pagesize : num_choices;
	for (int i = first; i < last; i++) {
		const int menux = xpos + (i % 2) * gwin->get_win()->get_full_width() / 2
						  + gwin->get_win()->get_start_x();
		const ModInfo& exultmod = mod_list[i];
		auto*          entry    = new MenuGameEntry(
                fonton, font, exultmod.get_menu_string().c_str(), nullptr,
                menux, ypos);
		entry->set_id(i);
		entry->set_enabled(exultmod.is_mod_compatible());
		menu->add_entry(entry);

		if (!exultmod.is_mod_compatible()) {
			auto* incentry = new MenuGameEntry(
					navfonton, navfont, "WRONG EXULT VERSION", nullptr, menux,
					ypos + entry->get_height() + 4);
			// Accept no clicks:
			incentry->set_enabled(false);
			menu->add_entry(incentry);
		}
		if (i % 2) {
			ypos += 45;
		}
	}

	create_scroller_menu(
			menu.get(), navfonton, navfont, first, pagesize, num_choices,
			centerx,
			gwin->get_win()->get_end_y() - 5 * font->get_text_height());

	constexpr static const std::array menuchoices{"RETURN TO MAIN MENU"};
	const int max_width = maximum_size(font, menuchoices, centerx);
	xpos                = centerx - max_width * (menuchoices.size() - 1) / 2;
	ypos = gwin->get_win()->get_end_y() - 3 * font->get_text_height();
	for (size_t i = 0; i < menuchoices.size(); i++) {
		auto* entry
				= new MenuTextEntry(fonton, font, menuchoices[i], xpos, ypos);
		// These commands have negative ids:
		entry->set_id(i - 4);
		menu->add_entry(entry);
		xpos += max_width;
	}
	menu->set_selection(0);
	menu->set_cancel(-4);
	return menu;
}

BaseGameInfo* ExultMenu::show_mods_menu(ModManager* selgame) {
	Palette*       gpal = gwin->get_pal();
	Shape_manager* sman = Shape_manager::get_instance();

	gwin->clear_screen(true);
	gpal->load(BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX), EXULT_FLX_EXULT0_PAL);
	gpal->apply();

	int           first_mod   = 0;
	const int     num_choices = selgame->get_mod_list().size() - 1;
	const int     last_page   = num_choices - num_choices % pagesize;
	auto          menu        = create_mods_menu(selgame, first_mod);
	BaseGameInfo* sel_mod     = nullptr;

	Shape_frame* exultlogo = exult_flx.get_shape(EXULT_FLX_EXULT_LOGO_SHP, 1);
	if (exultlogo == nullptr) {
		std::cerr << "Exult.flx file is corrupted. Please reinstall Exult."
				  << std::endl;
		throw quit_exception();
	}
	int logox;
	int logoy;
	logox = centerx - exultlogo->get_width() / 2;
	logoy = centery - exultlogo->get_height() / 2;

	do {
		// Interferes with the menu.
		sman->paint_shape(logox, logoy, exultlogo);
		font->draw_text(
				gwin->get_win()->get_ib8(),
				gwin->get_win()->get_end_x() - font->get_text_width(VERSION),
				gwin->get_win()->get_end_y() - font->get_text_height() - 5,
				VERSION);
		const int choice = menu->handle_events(gwin, menu_mouse);
		switch (choice) {
		case -10:    // The incompatibility notice; do nothing
			break;
		case -4:    // Return to main menu
			gpal->fade_out(c_fade_out_time / 2);
			wait_delay(c_fade_out_time / 2);
			gwin->clear_screen(true);
			return nullptr;
		default:
			if (choice >= 0) {
				// Load the game:
				gpal->fade_out(c_fade_out_time);
				sel_mod = selgame->get_mod(choice);
				break;
			} else if (handle_menu_click(
							   choice, first_mod, last_page, pagesize)) {
				menu = create_mods_menu(selgame, first_mod);
				gwin->clear_screen(true);
			}
		}
	} while (sel_mod == nullptr);

	gwin->clear_screen(true);
	return sel_mod;
}

BaseGameInfo* ExultMenu::run() {
	Palette*       gpal = gwin->get_pal();
	Shape_manager* sman = Shape_manager::get_instance();

	gwin->clear_screen(true);
	gpal->load(BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX), EXULT_FLX_EXULT0_PAL);
	font      = fontManager.get_font("CREDITS_FONT");
	fonton    = fontManager.get_font("HOT_FONT");
	navfont   = fontManager.get_font("NAV_FONT");
	navfonton = fontManager.get_font("HOT_NAV_FONT");

	if (!gamemanager->get_game_count()) {
// OS Specific messages
#ifdef SDL_PLATFORM_IOS
		const char game_missing_msg[]
				= "Please add the games in File Sharing";
		const char close_screen_msg[] = "Touch screen for help!";
#else
		const char game_missing_msg[] = "Please edit the configuration file.";
		const char close_screen_msg[] = "Press ESC to exit.";
#endif
		// Create our message and programatically center it.
		const char* const message[8] = {
				"Welcome to Exult",
				"",
				"To play you need the static data for either",
				R"("The Black Gate" or "Serpent Isle".)",
				game_missing_msg,
				"",
				"",
				close_screen_msg,
		};
		const int total_lines
				= sizeof(message)
				  / sizeof(message[0]);    // While this method is no longer
										   // "proper" it fits the rest of the
										   // coding style.
		const int topy = centery - (total_lines * 10) / 2;
		for (int line_num = 0; line_num < total_lines; line_num++) {
			font->center_text(
					gwin->get_win()->get_ib8(), centerx, topy + (line_num * 10),
					message[line_num]);
		}
		gpal->apply();
		while (!wait_delay(200)) {
		}
#ifdef SDL_PLATFORM_IOS
		// Never quits because Apple doesn't allow you to.
		SDL_OpenURL("https://exult.info/docs.php#ios_games");
		while (1) {
			wait_delay(1000);
		}
#else
		throw quit_exception(1);
#endif
	}
	IExultDataSource mouse_data(
			BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX), EXULT_FLX_POINTERS_SHP);
	Mouse mouse(gwin, mouse_data);
	menu_mouse = &mouse;

	// Must check this or it will crash as midi
	// may not be initialised
	if (Audio::get_ptr()->is_audio_enabled()) {
		// Make sure timbre library is correct!
		// Audio::get_ptr()->get_midi()->set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_GM);
		Audio::get_ptr()->start_music(
				EXULT_FLX_MEDITOWN_MID, true, MyMidiPlayer::Force_None,
				EXULT_FLX);
	}

	Shape_frame* exultlogo = exult_flx.get_shape(EXULT_FLX_EXULT_LOGO_SHP, 0);
	if (exultlogo == nullptr) {
		std::cerr << "Exult.flx file is corrupted. Please reinstall Exult."
				  << std::endl;
		throw quit_exception();
	}
	int logox = centerx - exultlogo->get_width() / 2;
	int logoy = centery - exultlogo->get_height() / 2;
	sman->paint_shape(logox, logoy, exultlogo);
	gpal->fade_in(c_fade_in_time);
	wait_delay(2000);

	exultlogo = exult_flx.get_shape(EXULT_FLX_EXULT_LOGO_SHP, 1);

	int       first_game  = 0;
	const int num_choices = gamemanager->get_game_count() - 1;
	const int last_page   = num_choices - num_choices % pagesize;
	// Erase the old logo.
	gwin->clear_screen(true);

	auto          menu     = create_main_menu(first_game);
	BaseGameInfo* sel_game = nullptr;

	do {
		// Interferes with the menu.
		sman->paint_shape(logox, logoy, exultlogo);
		font->draw_text(
				gwin->get_win()->get_ib8(),
				gwin->get_win()->get_end_x() - font->get_text_width(VERSION),
				gwin->get_win()->get_end_y() - font->get_text_height() - 5,
				VERSION);
		const int choice = menu->handle_events(gwin, menu_mouse);

		switch (choice) {
		case -4:    // Setup
			gpal->fade_out(c_fade_out_time);
			setup();
			if (quitting_time == QUIT_TIME_YES) {
				throw quit_exception();
			}

			if (Audio::get_ptr()->is_audio_enabled()) {
				// Make sure timbre library is correct!
				// Audio::get_ptr()->get_midi()->set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_GM);
				Audio::get_ptr()->start_music(
						EXULT_FLX_MEDITOWN_MID, true, MyMidiPlayer::Force_None,
						EXULT_FLX);
			}

			calc_win();
			logox      = centerx - exultlogo->get_width() / 2;
			logoy      = centery - exultlogo->get_height() / 2;
			first_game = 0;
			menu       = create_main_menu(first_game);
			break;
		case -3: {    // Exult Credits
			gpal->fade_out(c_fade_out_time);
			TextScroller credits(
					BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX),
					EXULT_FLX_CREDITS_TXT, fontManager.get_font("CREDITS_FONT"),
					exult_flx.extract_shape(EXULT_FLX_EXTRAS_SHP));
			credits.run(gwin);
			gwin->clear_screen(true);
			gpal->apply();
		} break;
		case -2: {    // Exult Quotes
			gpal->fade_out(c_fade_out_time);
			TextScroller quotes(
					BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX),
					EXULT_FLX_QUOTES_TXT, fontManager.get_font("CREDITS_FONT"),
					exult_flx.extract_shape(EXULT_FLX_EXTRAS_SHP));
			quotes.run(gwin);
			gwin->clear_screen(true);
			gpal->apply();
		} break;
		case -1:    // Exit
#ifdef SDL_PLATFORM_IOS
			SDL_OpenURL("https://exult.info/docs.php#iOS%20Guide");
			break;
#else
			gpal->fade_out(c_fade_out_time);
			Audio::get_ptr()->stop_music();
			throw quit_exception();
#endif
		default:
			if (choice >= 0 && choice < MAX_GAMES) {
				// Load the game:
				gpal->fade_out(c_fade_out_time);
				sel_game = gamemanager->get_game(choice);
			} else if (choice >= MAX_GAMES && choice < 2 * MAX_GAMES) {
				// Show the mods for the game:
				gpal->fade_out(c_fade_out_time / 2);
				sel_game = show_mods_menu(
						gamemanager->get_game(choice - MAX_GAMES));
				gwin->clear_screen(true);
				gpal->apply();
			} else if (handle_menu_click(
							   choice, first_game, last_page, pagesize)) {
				menu = create_main_menu(first_game);
				gwin->clear_screen(true);
			}
			break;
		}
	} while (sel_game == nullptr);
	gwin->clear_screen(true);
	Audio::get_ptr()->stop_music();
	return sel_game;
}
