/*
 *  Copyright (C) 2001-2013  The Exult Team
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

#include <iostream>
#include <cstring>

#include "SDL_events.h"

#include "Gump_manager.h"
#include "Configuration.h"
#include "Gump_button.h"
#include "Gump_ToggleButton.h"
#include "MiscOptions_gump.h"
#include "exult.h"
#include "exult_flx.h"
#include "gamewin.h"
#include "combat_opts.h"
#include "Text_button.h"
#include "Enabled_button.h"
#include "font.h"
#include "gamewin.h"
#include "Notebook_gump.h"
#include "ShortcutBar_gump.h"
using std::string;

static const int rowy[] = { 4, 16, 28, 40, 52, 64, 76, 88, 100, 112, 124, 136, 148, 160, 172 };
static const int colx[] = { 35, 50, 120, 170, 192, 211 };

static const char *oktext = "OK";
static const char *canceltext = "CANCEL";

using MiscOptions_button = CallbackTextButton<MiscOptions_gump>;
using MiscTextToggle = CallbackToggleTextButton<MiscOptions_gump>;
using MiscEnabledToggle = CallbackEnabledButton<MiscOptions_gump>;

void MiscOptions_gump::close() {
	save_settings();
	done = true;
}

void MiscOptions_gump::cancel() {
	done = true;
}

void MiscOptions_gump::build_buttons() {
	const std::vector<std::string> yesNo = {"No", "Yes"};

	int y_index = 0;
	int small_size = 44;
	int large_size = 85;
#ifndef __IPHONEOS__
	buttons[id_scroll_mouse] = std::make_unique<MiscTextToggle>(this, &MiscOptions_gump::toggle_scroll_mouse,
	        yesNo, scroll_mouse, colx[5], rowy[y_index], small_size);
#endif
	buttons[id_menu_intro] = std::make_unique<MiscTextToggle>(this, &MiscOptions_gump::toggle_menu_intro,
	        yesNo, menu_intro, colx[5], rowy[++y_index], small_size);
	buttons[id_usecode_intro] = std::make_unique<MiscTextToggle>(this, &MiscOptions_gump::toggle_usecode_intro,
	        yesNo, usecode_intro, colx[5], rowy[++y_index], small_size);
	buttons[id_alternate_drop] = std::make_unique<MiscTextToggle>(this, &MiscOptions_gump::toggle_alternate_drop,
	        yesNo, alternate_drop, colx[5], rowy[++y_index], small_size);
	buttons[id_allow_autonotes] = std::make_unique<MiscTextToggle>(this, &MiscOptions_gump::toggle_allow_autonotes,
	        yesNo, allow_autonotes, colx[5], rowy[++y_index], small_size);

	std::vector<std::string> sc_enabled_txt = {"No", "transparent", "Yes"};
	buttons[id_sc_enabled] = std::make_unique<MiscTextToggle>(this, &MiscOptions_gump::toggle_sc_enabled,
	        std::move(sc_enabled_txt), sc_enabled, colx[3], rowy[++y_index], large_size);

	// keep in order of Pixel_colors
	// "No" needs to be last.
	sc_outline_txt = std::vector<std::string>{"green", "white", "yellow", "blue", "red", "purple", "black", "No"};
	buttons[id_sc_outline] = std::make_unique<MiscTextToggle>(this, &MiscOptions_gump::toggle_sc_outline,
	        sc_outline_txt, sc_outline, colx[5], rowy[++y_index], small_size);
	buttons[id_sb_hide_missing] = std::make_unique<MiscTextToggle>(this, &MiscOptions_gump::toggle_sb_hide_missing,
	        yesNo, sb_hide_missing, colx[5], rowy[++y_index], small_size);
	// two row gap
	std::vector<std::string> diffs = {
	    "Easiest (-3)", "Easier (-2)", "Easier (-1)",
		"Normal",
		"Harder (+1)", "Harder (+2)", "Hardest (+3)"};
	buttons[id_difficulty] = std::make_unique<MiscTextToggle>(this, &MiscOptions_gump::toggle_difficulty,
	        std::move(diffs), difficulty, colx[3], rowy[y_index+=3], large_size);
	buttons[id_show_hits] = std::make_unique<MiscEnabledToggle>(this, &MiscOptions_gump::toggle_show_hits,
	        show_hits, colx[3], rowy[++y_index], large_size);

	std::vector<std::string> modes = {"Original", "Space pauses"};
	buttons[id_mode] = std::make_unique<MiscTextToggle>(this, &MiscOptions_gump::toggle_mode,
	        std::move(modes), mode, colx[3], rowy[++y_index], large_size);

	std::vector<std::string> charmedDiff = {"Normal", "Hard"};
	buttons[id_charmDiff] = std::make_unique<MiscTextToggle>(this, &MiscOptions_gump::toggle_charmDiff,
	        std::move(charmedDiff), charmDiff, colx[3], rowy[++y_index], large_size);
	// Ok
	buttons[id_ok] = std::make_unique<MiscOptions_button>(this, &MiscOptions_gump::close,
	        oktext, colx[0], rowy[++y_index]);
	// Cancel
	buttons[id_cancel] = std::make_unique<MiscOptions_button>(this, &MiscOptions_gump::cancel,
	        canceltext, colx[4], rowy[y_index]);
}

void MiscOptions_gump::load_settings() {
	string yn;
	scroll_mouse = gwin->can_scroll_with_mouse();
	config->value("config/gameplay/skip_intro", yn, "no");
	usecode_intro = (yn == "yes");
	config->value("config/gameplay/skip_splash", yn, "no");
	menu_intro = (yn == "yes");

	sc_enabled = gwin->get_shortcutbar_type();
	sc_outline = gwin->get_outline_color();
	sb_hide_missing = gwin->sb_hide_missing_items();

	difficulty = Combat::difficulty;
	if (difficulty < -3)
		difficulty = -3;
	else if (difficulty > 3)
		difficulty = 3;
	difficulty += 3;        // Scale to choices (0-6).
	show_hits = Combat::show_hits ? 1 : 0;
	mode = static_cast<int>(Combat::mode);
	if (mode < 0 || mode > 1)
		mode = 0;
	charmDiff = Combat::charmed_more_difficult;
	alternate_drop = gwin->get_alternate_drop();
	allow_autonotes = gwin->get_allow_autonotes();
}

MiscOptions_gump::MiscOptions_gump()
	: Modal_gump(nullptr, EXULT_FLX_MISCOPTIONS_SHP, SF_EXULT_FLX) {
	set_object_area(Rectangle(0, 0, 0, 0), 8, 184);//++++++ ???

	load_settings();
	build_buttons();
}

void MiscOptions_gump::save_settings() {
#ifndef __IPHONEOS__
	config->set("config/gameplay/scroll_with_mouse",
	            scroll_mouse ? "yes" : "no", false);
	gwin->set_mouse_with_scroll(scroll_mouse);
#endif
	config->set("config/gameplay/skip_intro",
	            usecode_intro ? "yes" : "no", false);
	config->set("config/gameplay/skip_splash",
	            menu_intro ? "yes" : "no", false);

	string str = "no";
	if(sc_enabled == 1)
		str = "transparent";
	else if(sc_enabled == 2)
		str = "yes";
	config->set("config/shortcutbar/use_shortcutbar", str, false);
	config->set("config/shortcutbar/use_outline_color", sc_outline_txt[sc_outline], false);
	config->set("config/shortcutbar/hide_missing_items", sb_hide_missing ? "yes" : "no", false);

	gwin->set_outline_color(static_cast<Pixel_colors>(sc_outline));
	gwin->set_sb_hide_missing_items(sb_hide_missing);
	gwin->set_shortcutbar(static_cast<uint8>(sc_enabled));
	if(g_shortcutBar)
		g_shortcutBar->set_changed();

	Combat::difficulty = difficulty - 3;
	config->set("config/gameplay/combat/difficulty",
	            Combat::difficulty, false);
	Combat::show_hits = (show_hits != 0);
	config->set("config/gameplay/combat/show_hits",
	            show_hits ? "yes" : "no", false);
	Combat::mode = static_cast<Combat::Mode>(mode);
	str = Combat::mode == Combat::keypause ? "keypause"
	                  : "original";
	config->set("config/gameplay/combat/mode", str, false);
	Combat::charmed_more_difficult = charmDiff;
	config->set("config/gameplay/combat/charmDifficulty",
	            charmDiff ? "hard" : "normal", false);
	gwin->set_alternate_drop(alternate_drop);
	config->set("config/gameplay/alternate_drop",
	            alternate_drop ? "yes" : "no", false);
	gwin->set_allow_autonotes(allow_autonotes);
	config->set("config/gameplay/allow_autonotes",
	            allow_autonotes ? "yes" : "no", false);
	config->write_back();
}

void MiscOptions_gump::paint() {
	Gump::paint();
	for (auto& btn : buttons)
		if (btn)
			btn->paint();
	Font *font = fontManager.get_font("SMALL_BLACK_FONT");
	Image_window8 *iwin = gwin->get_win();
	int y_index = 0;
#ifndef __IPHONEOS__
	font->paint_text(iwin->get_ib8(), "Scroll game view with mouse:", x + colx[0], y + rowy[y_index] + 1);
#endif
	font->paint_text(iwin->get_ib8(), "Skip intro:", x + colx[0], y + rowy[++y_index] + 1);
	font->paint_text(iwin->get_ib8(), "Skip scripted first scene:", x + colx[0], y + rowy[++y_index] + 1);
	font->paint_text(iwin->get_ib8(), "Alternate drag'n'drop:", x + colx[0], y + rowy[++y_index] + 1);
	font->paint_text(iwin->get_ib8(), "Allow Autonotes:", x + colx[0], y + rowy[++y_index] + 1);
	font->paint_text(iwin->get_ib8(), "Use ShortcutBar :", x + colx[0], y + rowy[++y_index] + 1);
	font->paint_text(iwin->get_ib8(), "Use outline color :", x + colx[1], y + rowy[++y_index] + 1);
	font->paint_text(iwin->get_ib8(), "Hide missing items:", x + colx[1], y + rowy[++y_index] + 1);
	// 1 row gap
	font->paint_text(iwin->get_ib8(), "Combat Options:", x + colx[0], y + rowy[y_index+=2] + 1);
	font->paint_text(iwin->get_ib8(), "Difficulty:", x + colx[1], y + rowy[++y_index] + 1);
	font->paint_text(iwin->get_ib8(), "Show Hits:", x + colx[1], y + rowy[++y_index] + 1);
	font->paint_text(iwin->get_ib8(), "Mode:", x + colx[1], y + rowy[++y_index] + 1);
	font->paint_text(iwin->get_ib8(), "Charmed Difficulty:", x + colx[1], y + rowy[++y_index] + 1);
	gwin->set_painted();
}

bool MiscOptions_gump::mouse_down(int mx, int my, int button) {
	// Only left and right buttons
	if (button != 1 && button != 3) return false;

	// We'll eat the mouse down if we've already got a button down
	if (pushed) return true;

	// First try checkmark
	pushed = Gump::on_button(mx, my);

	// Try buttons at bottom.
	if (!pushed) {
		for (auto& btn : buttons) {
			if (btn && btn->on_button(mx, my)) {
				pushed = btn.get();
				break;
			}
		}
	}

	if (pushed && !pushed->push(button))            // On a button?
		pushed = nullptr;

	return button == 1 || pushed != nullptr;
}

bool MiscOptions_gump::mouse_up(int mx, int my, int button) {
	// Not Pushing a button?
	if (!pushed) return false;

	if (pushed->get_pushed() != button) return button == 1;

	bool res = false;
	pushed->unpush(button);
	if (pushed->on_button(mx, my))
		res = pushed->activate(button);
	pushed = nullptr;
	return res;
}
