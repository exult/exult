/*
 *  Copyright (C) 2000-2013  The Exult Team
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

#include "menulist.h"
#include "exultmenu.h"
#include "exult.h"
#include "mouse.h"
#include "gamewin.h"
#include "ibuf8.h"
#include "game.h"
#include "bggame.h"
#include "sigame.h"
#include "Audio.h"
#include "font.h"
#include "cheat.h"
#include "Configuration.h"
#include "txtscroll.h"
#include "data/exult_flx.h"
#include "palette.h"
#include "shapeid.h"
#include "XMidiFile.h"
#include "databuf.h"
#include "fnames.h"
#include "gamemgr/modmgr.h"
#include "gumps/Gamemenu_gump.h"
#include "shapeid.h"
#include "ignore_unused_variable_warning.h"
#include "array_size.h"
#ifdef __IPHONEOS__
#  include "ios_utils.h"
 #endif
#include <memory>
using std::unique_ptr;
using std::make_unique;

#define MAX_GAMES 100

static inline bool handle_menu_click(
    int id,
    int &first,
    int last_page,
    int pagesize
) {
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

int maximum_size(Font *font, const char *options[], int num_choices, int centerx) {
	ignore_unused_variable_warning(centerx);
	int max_width = 0;
	int width;
	for (int i = 0; i < num_choices; i++) {
		width = font->get_text_width(options[i]);
		if (width > max_width)
			max_width = width;
	}
	max_width += 16;
	return max_width;
}

void create_scroller_menu(MenuList *menu, Font *fonton, Font *font, int first, int pagesize, int num_choices, int xpos, int ypos) {
	const char *menuscroller[] = {
		"FIRST",
		"PREVIOUS",
		"NEXT",
		"LAST"
	};
	int ncount = array_size(menuscroller);
	assert(ncount == 4);
	int max_width = maximum_size(font, menuscroller, ncount, xpos);
	xpos = xpos - max_width * 3 / 2;

	num_choices--;
	int lastpage = num_choices - num_choices % pagesize;

	for (int i = 0; i < ncount; i++) {
		//Check to see if this entry is needed at all:
		if (!((i < 2 && first == 0) ||
		        (i == 0 && first == pagesize) ||
		        (i >= 2 && lastpage == first) ||
		        (i == 3 && lastpage == first + pagesize))) {
			MenuTextEntry *entry = new MenuTextEntry(fonton, font, menuscroller[i],
			        xpos, ypos);
			//These commands have negative ids:
			entry->set_id(i - 8);
			menu->add_entry(entry);
		}
		xpos += max_width;
	}

}

ExultMenu::ExultMenu(Game_window *gw)
	: font(nullptr), fonton(nullptr), navfont(nullptr), navfonton(nullptr) {
	gwin = gw;
	ibuf = gwin->get_win()->get_ib8();
	const char *fname = BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX);
	fontManager.add_font("CREDITS_FONT", fname, EXULT_FLX_FONT_SHP, 1);
	fontManager.add_font("HOT_FONT", fname, EXULT_FLX_FONTON_SHP, 1);
	fontManager.add_font("NAV_FONT", fname, EXULT_FLX_NAVFONT_SHP, 1);
	fontManager.add_font("HOT_NAV_FONT", fname, EXULT_FLX_NAVFONTON_SHP, 1);
	calc_win();
	exult_flx.load(fname);
}

void ExultMenu::calc_win() {
	centerx = gwin->get_width() / 2;
	centery = gwin->get_height() / 2;
	Font *fnt = font ? font : fontManager.get_font("CREDITS_FONT");
	pagesize = 2 * ((gwin->get_win()->get_full_height() - 5 * fnt->get_text_height() - 15) / 45);
}


void ExultMenu::setup() {
	ModManager *mm = gamemanager->get_bg();
	if (!mm) mm = gamemanager->get_si();
	if (!mm) mm = gamemanager->get_game(0);
	if (!mm) {
		std::cerr << "No games found. Unable to show gumps in Exult menu." << std::endl;
		return;
	}
	//ModManager mm_exult_menu_game (*mm);
	//mm_exult_menu_game.set_game_type(EXULT_MENU_GAME);

	Game *exult_menu_game = Game::create_game(mm);

	if (!Shape_manager::get_instance()->load_gumps_minimal()) {
		std::cerr << "Unable to show gumps in Exult menu." << std::endl;
		return;
	}

	Mouse::mouse = menu_mouse;

	gwin->clear_screen(true);

	Palette *gpal = gwin->get_pal();
	gpal->fade(0, 1, 0);

	gwin->set_in_exult_menu(true);
	Gamemenu_gump::do_exult_menu();
	gwin->set_in_exult_menu(false);

	Mouse::mouse = nullptr;
	delete exult_menu_game;
	game = exult_menu_game = nullptr;

	gwin->clear_screen(true);
	gpal->load(BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX), EXULT_FLX_EXULT0_PAL);
	gpal->apply();

}

MenuList *ExultMenu::create_main_menu(int first) {
	MenuList *menu = new MenuList();

	int ypos = 15 + gwin->get_win()->get_start_y();
	Shape_frame *fr = exult_flx.get_shape(EXULT_FLX_SFX_ICON_SHP, 0);
	assert(fr != nullptr);
	int xpos = (gwin->get_win()->get_full_width() / 2 + fr->get_width()) / 2;
	std::vector<ModManager> &game_list = gamemanager->get_game_list();
	int num_choices = game_list.size();
	int last = num_choices > first + pagesize ? first + pagesize : num_choices;
	for (int i = first; i < last; i++) {
		int menux = xpos + (i % 2) * gwin->get_win()->get_full_width() / 2 + gwin->get_win()->get_start_x();
		ModManager &exultgame = game_list[i];
		bool have_sfx = Audio::have_config_sfx(exultgame.get_cfgname()) ||
		                Audio::have_roland_sfx(exultgame.get_game_type()) ||
		                Audio::have_sblaster_sfx(exultgame.get_game_type()) ||
		                Audio::have_midi_sfx();

		Shape_frame *sfxicon = exult_flx.get_shape(EXULT_FLX_SFX_ICON_SHP,
		                       have_sfx ? 1 : 0);
		MenuGameEntry *entry = new MenuGameEntry(fonton, font,
		        exultgame.get_menu_string().c_str(),
		        sfxicon, menux, ypos);
		entry->set_id(i);
		menu->add_entry(entry);
		if (exultgame.has_mods()) {
			MenuTextEntry *mod_entry = new MenuTextEntry(navfonton, navfont, "SHOW MODS",
			        menux, ypos + entry->get_height() + 4);
			mod_entry->set_id(i + MAX_GAMES);
			menu->add_entry(mod_entry);
		}
		if (i % 2)
			ypos += 45;
	}

	create_scroller_menu(menu, navfonton, navfont, first, pagesize, num_choices,
	                     centerx, ypos = gwin->get_win()->get_end_y() - 5 * font->get_text_height());

	const char *menuchoices[] = {
		"SETUP",
		"CREDITS",
		"QUOTES",
#ifdef __IPHONEOS__
		"HELP"
#else
		"EXIT"
#endif
	};
	int num_entries = array_size(menuchoices);
	int max_width = maximum_size(font, menuchoices, num_entries, centerx);
	xpos = centerx - max_width * (num_entries - 1) / 2;
	ypos = gwin->get_win()->get_end_y() - 3 * font->get_text_height();
	for (int i = 0; i < 4; i++) {
		MenuTextEntry *entry = new MenuTextEntry(fonton, font, menuchoices[i],
		        xpos, ypos);
		//These commands have negative ids:
		entry->set_id(i - 4);
		menu->add_entry(entry);
		xpos += max_width;
	}
	return menu;
}

MenuList *ExultMenu::create_mods_menu(ModManager *selgame, int first) {
	MenuList *menu = new MenuList();

	int ypos = 15 + gwin->get_win()->get_start_y();
	int xpos = gwin->get_win()->get_full_width() / 4;

	std::vector<ModInfo> &mod_list = selgame->get_mod_list();
	int num_choices = mod_list.size();
	int last = num_choices > first + pagesize ? first + pagesize : num_choices;
	for (int i = first; i < last; i++) {
		int menux = xpos + (i % 2) * gwin->get_win()->get_full_width() / 2 + gwin->get_win()->get_start_x();
		ModInfo &exultmod = mod_list[i];
		MenuGameEntry *entry = new MenuGameEntry(fonton, font,
		        exultmod.get_menu_string().c_str(),
		        nullptr, menux, ypos);
		entry->set_id(i);
		entry->set_enabled(exultmod.is_mod_compatible());
		menu->add_entry(entry);

		if (!exultmod.is_mod_compatible()) {
			MenuGameEntry *incentry = new MenuGameEntry(navfonton, navfont, "WRONG EXULT VERSION",
			        nullptr, menux, ypos + entry->get_height() + 4);
			// Accept no clicks:
			incentry->set_enabled(false);
			menu->add_entry(incentry);
		}
		if (i % 2)
			ypos += 45;
	}

	create_scroller_menu(menu, navfonton, navfont, first, pagesize, num_choices,
	                     centerx, ypos = gwin->get_win()->get_end_y() - 5 * font->get_text_height());

	const char *menuchoices[] = {
		"RETURN TO MAIN MENU"
	};
	int num_entries = array_size(menuchoices);
	int max_width = maximum_size(font, menuchoices, num_entries, centerx);
	xpos = centerx - max_width * (num_entries - 1) / 2;
	ypos = gwin->get_win()->get_end_y() - 3 * font->get_text_height();
	for (int i = 0; i < num_entries; i++) {
		MenuTextEntry *entry = new MenuTextEntry(fonton, font, menuchoices[i],
		        xpos, ypos);
		//These commands have negative ids:
		entry->set_id(i - 4);
		menu->add_entry(entry);
		xpos += max_width;
	}
	return menu;
}

BaseGameInfo *ExultMenu::show_mods_menu(ModManager *selgame) {
	Palette *gpal = gwin->get_pal();
	Shape_manager *sman = Shape_manager::get_instance();

	gwin->clear_screen(true);
	gpal->load(BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX), EXULT_FLX_EXULT0_PAL);
	gpal->apply();

	int first_mod = 0;
	int num_choices = selgame->get_mod_list().size() - 1;
	int last_page = num_choices - num_choices % pagesize;
	MenuList *menu = create_mods_menu(selgame, first_mod);
	menu->set_selection(0);
	BaseGameInfo *sel_mod = nullptr;

	Shape_frame *exultlogo = exult_flx.get_shape(EXULT_FLX_EXULT_LOGO_SHP, 1);
	assert(exultlogo != nullptr);
	int logox;
	int logoy;
	logox = centerx - exultlogo->get_width() / 2;
	logoy = centery - exultlogo->get_height() / 2;

	do {
		// Interferes with the menu.
		sman->paint_shape(logox, logoy, exultlogo);
		font->draw_text(gwin->get_win()->get_ib8(),
						gwin->get_win()->get_end_x() - font->get_text_width(VERSION),
						gwin->get_win()->get_end_y() - font->get_text_height() - 5, VERSION);
		int choice = menu->handle_events(gwin, menu_mouse);
		switch (choice) {
		case -10: // The incompatibility notice; do nothing
			break;
		case -4: // Return to main menu
			gpal->fade_out(c_fade_out_time / 2);
			wait_delay(c_fade_out_time / 2);
			gwin->clear_screen(true);
			delete menu;
			return nullptr;
		default:
			if (choice >= 0) {
				// Load the game:
				gpal->fade_out(c_fade_out_time);
				sel_mod = selgame->get_mod(choice);
				break;
			} else if (handle_menu_click(choice, first_mod, last_page, pagesize)) {
				delete menu;
				menu = create_mods_menu(selgame, first_mod);
				gwin->clear_screen(true);
			}
		}
	} while (sel_mod == nullptr);
	delete menu;

	gwin->clear_screen(true);
	return sel_mod;
}

BaseGameInfo *ExultMenu::run() {
	Palette *gpal = gwin->get_pal();
	Shape_manager *sman = Shape_manager::get_instance();

	gwin->clear_screen(true);
	gpal->load(BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX), EXULT_FLX_EXULT0_PAL);
	font = fontManager.get_font("CREDITS_FONT");
	fonton = fontManager.get_font("HOT_FONT");
	navfont = fontManager.get_font("NAV_FONT");
	navfonton = fontManager.get_font("HOT_NAV_FONT");

	if (!gamemanager->get_game_count()) {
		int topy = centery - 25;
		font->center_text(gwin->get_win()->get_ib8(),
		                  centerx, topy + 20, "WARNING");
		font->center_text(gwin->get_win()->get_ib8(),
		                  centerx, topy + 40, "Could not find the static data for either");
		font->center_text(gwin->get_win()->get_ib8(),
		                  centerx, topy + 50, "\"The Black Gate\" or \"Serpent Isle\".");
#ifndef __IPHONEOS__
		const char games_missing_msg[] = "Please edit the configuration file";
#else
		const char games_missing_msg[] = "Please add the games in iTunes File Sharing";
#endif
		font->center_text(gwin->get_win()->get_ib8(),
		                  centerx, topy + 60, games_missing_msg);
		font->center_text(gwin->get_win()->get_ib8(),
		                  centerx, topy + 70, "and restart Exult.");
#ifdef __IPHONEOS__
		font->center_text(gwin->get_win()->get_ib8(),
		                  centerx, topy + 100, "Touch screen for help!");
#endif
		gpal->apply();
		while (!wait_delay(200)) {
		}
#ifndef __IPHONEOS__
		throw quit_exception(1);
#else
		// Never quits because Apple doesn't allow you to.
		ios_open_url("http://exult.sourceforge.net/docs.php#ios_games");
		while (1) {
			wait_delay(1000);
		}
#endif
	}
	IExultDataSource mouse_data(BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX),
	                           EXULT_FLX_POINTERS_SHP);
	menu_mouse = new Mouse(gwin, mouse_data);

	//Must check this or it will crash as midi
	//may not be initialised
	if (Audio::get_ptr()->is_audio_enabled()) {
		// Make sure timbre library is correct!
		//Audio::get_ptr()->get_midi()->set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_GM);
		Audio::get_ptr()->start_music(EXULT_FLX_MEDITOWN_MID, true, EXULT_FLX);
	}

	Shape_frame *exultlogo = exult_flx.get_shape(EXULT_FLX_EXULT_LOGO_SHP, 0);
	assert(exultlogo != nullptr);
	int logox = centerx - exultlogo->get_width() / 2;
	int logoy = centery - exultlogo->get_height() / 2;
	sman->paint_shape(logox, logoy, exultlogo);
	gpal->fade_in(c_fade_in_time);
	wait_delay(2000);

	exultlogo = exult_flx.get_shape(EXULT_FLX_EXULT_LOGO_SHP, 1);

	int first_game = 0;
	int num_choices = gamemanager->get_game_count() - 1;
	int last_page = num_choices - num_choices % pagesize;
	// Erase the old logo.
	gwin->clear_screen(true);

	MenuList *menu = create_main_menu(first_game);;
	BaseGameInfo *sel_game = nullptr;
	menu->set_selection(0);

	do {
		// Interferes with the menu.
		sman->paint_shape(logox, logoy, exultlogo);
		font->draw_text(gwin->get_win()->get_ib8(),
						gwin->get_win()->get_end_x() - font->get_text_width(VERSION),
						gwin->get_win()->get_end_y() - font->get_text_height() - 5, VERSION);
		int choice = menu->handle_events(gwin, menu_mouse);
		switch (choice) {
		case -4: // Setup
			gpal->fade_out(c_fade_out_time);
			setup();
			delete menu;
			if (Audio::get_ptr()->is_audio_enabled()) {
				// Make sure timbre library is correct!
				//Audio::get_ptr()->get_midi()->set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_GM);
				Audio::get_ptr()->start_music(EXULT_FLX_MEDITOWN_MID, true, EXULT_FLX);
			}

			calc_win();
			logox = centerx - exultlogo->get_width() / 2;
			logoy = centery - exultlogo->get_height() / 2;
			first_game = 0;
			menu = create_main_menu(first_game);
			menu->set_selection(0);
			break;
		case -3: { // Exult Credits
			gpal->fade_out(c_fade_out_time);
			TextScroller credits(
			    BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX),
			    EXULT_FLX_CREDITS_TXT,
			    fontManager.get_font("CREDITS_FONT"),
			    exult_flx.extract_shape(EXULT_FLX_EXTRAS_SHP));
			credits.run(gwin);
			gwin->clear_screen(true);
			gpal->apply();
		}
		break;
		case -2: { // Exult Quotes
			gpal->fade_out(c_fade_out_time);
			TextScroller quotes(BUNDLE_CHECK(
			                        BUNDLE_EXULT_FLX, EXULT_FLX),
			                    EXULT_FLX_QUOTES_TXT,
			                    fontManager.get_font("CREDITS_FONT"),
			                    exult_flx.extract_shape(EXULT_FLX_EXTRAS_SHP));
			quotes.run(gwin);
			gwin->clear_screen(true);
			gpal->apply();
		}
		break;
		case -1: // Exit
#ifdef __IPHONEOS__
			ios_open_url("http://exult.sourceforge.net/docs.php#iOS%20Guide");
			break;
#else
			gpal->fade_out(c_fade_out_time);
			Audio::get_ptr()->stop_music();
			delete menu;
			delete menu_mouse;
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
			} else if (handle_menu_click(choice, first_game, last_page, pagesize)) {
				delete menu;
				menu = create_main_menu(first_game);
				gwin->clear_screen(true);
			}
			break;
		}
	} while (sel_game == nullptr);
	delete menu;
	gwin->clear_screen(true);
	Audio::get_ptr()->stop_music();
	delete menu_mouse;
	return sel_game;
}

