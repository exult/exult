/*
 *  Copyright (C) 2001-2022  The Exult Team
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

#include <cstring>
#include <iostream>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "Configuration.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "Mixer_gump.h"
#include "Text_button.h"
#include "exult.h"
#include "exult_flx.h"
#include "font.h"
#include "game.h"
#include "gamewin.h"

using std::string;

static const int rowy[] = {8, 21, 34, 48};
static const int colx[] = {35, 83, 95, 117, 206, 215, 253};

// Scrollbar and Slider Info
const short Mixer_gump::sliderw = 7;    // Width of Slider
int         Mixer_gump::max_val = 100;
int         Mixer_gump::min_val = 0;
short Mixer_gump::xmin = colx[2] + 2;    // Min., max. positions of diamond.
short Mixer_gump::xmax = colx[5] - Mixer_gump::sliderw;

static const char* oktext     = "OK";
static const char* canceltext = "CANCEL";
static const char* helptext   = "HELP";

// the slider bar is longer than 100 pixels, so we calculate its length and
// divide by 100
float Mixer_gump::factor
		= static_cast<float>(colx[5] - colx[2] - 2 - Mixer_gump::sliderw)
		  / static_cast<float>(100.0);

using Mixer_button     = CallbackButton<Mixer_gump>;
using Mixer_Textbutton = CallbackTextButton<Mixer_gump>;

void Mixer_gump::close() {
	save_settings();
	done = true;
}

void Mixer_gump::cancel() {
	done = true;
}

void Mixer_gump::help() {
#if SDL_VERSION_ATLEAST(2, 0, 14)
	// Pointing at Audio section for help as there is no item for the mixer yet
	SDL_OpenURL("http://exult.info/docs.php#Audio");
#endif
}

void Mixer_gump::load_settings() {
	music_volume  = max_val;
	sfx_volume    = max_val;
	speech_volume = max_val;
	config->value("config/audio/midi/music_volume", music_volume, music_volume);
	config->value("config/audio/effects/sfx_volume", sfx_volume, sfx_volume);
	config->value(
			"config/audio/speech/speech_volume", speech_volume, speech_volume);
}

void Mixer_gump::set_val(int newval) {
	val                    = newval;
	static const int xdist = xmax - xmin;
	sliderx = xmin + ((val - min_val) * xdist) / (max_val - min_val);
}

Mixer_gump::Mixer_gump()
		: Modal_gump(nullptr, EXULT_FLX_MIXER_SHP, SF_EXULT_FLX) {
	set_object_area(TileRect(0, 0, 0, 0), 9, 62);
	music_slider  = ShapeID(EXULT_FLX_SAV_SLIDER_SHP, 0, SF_EXULT_FLX);
	sfx_slider    = ShapeID(EXULT_FLX_SAV_SLIDER_SHP, 0, SF_EXULT_FLX);
	speech_slider = ShapeID(EXULT_FLX_SAV_SLIDER_SHP, 0, SF_EXULT_FLX);
	for (auto& btn : buttons) {
		btn.reset();
	}
	buttons[id_music_left] = std::make_unique<Mixer_button>(
			this, &Mixer_gump::scroll_left, EXULT_FLX_SCROLL_LEFT_SHP, colx[1],
			rowy[0] - 3, SF_EXULT_FLX);
	buttons[id_music_right] = std::make_unique<Mixer_button>(
			this, &Mixer_gump::scroll_right, EXULT_FLX_SCROLL_RIGHT_SHP,
			colx[5], rowy[0] - 3, SF_EXULT_FLX);
	buttons[id_sfx_left] = std::make_unique<Mixer_button>(
			this, &Mixer_gump::scroll_left, EXULT_FLX_SCROLL_LEFT_SHP, colx[1],
			rowy[1] - 3, SF_EXULT_FLX);
	buttons[id_sfx_right] = std::make_unique<Mixer_button>(
			this, &Mixer_gump::scroll_right, EXULT_FLX_SCROLL_RIGHT_SHP,
			colx[5], rowy[1] - 3, SF_EXULT_FLX);
	buttons[id_voc_left] = std::make_unique<Mixer_button>(
			this, &Mixer_gump::scroll_left, EXULT_FLX_SCROLL_LEFT_SHP, colx[1],
			rowy[2] - 3, SF_EXULT_FLX);
	buttons[id_voc_right] = std::make_unique<Mixer_button>(
			this, &Mixer_gump::scroll_right, EXULT_FLX_SCROLL_RIGHT_SHP,
			colx[5], rowy[2] - 3, SF_EXULT_FLX);

	load_settings();

	// Ok
	buttons[id_ok] = std::make_unique<Mixer_Textbutton>(
			this, &Mixer_gump::close, oktext, colx[0] - 2, rowy[3], 50);
	// Cancel
	buttons[id_cancel] = std::make_unique<Mixer_Textbutton>(
			this, &Mixer_gump::cancel, canceltext, colx[4], rowy[3], 50);
#if SDL_VERSION_ATLEAST(2, 0, 14)
	// Help
	buttons[id_help] = std::make_unique<Mixer_Textbutton>(
			this, &Mixer_gump::help, helptext, colx[3], rowy[3], 50);
#endif
}

void Mixer_gump::scroll_left() {
	move_slider(-step_val);
}

void Mixer_gump::scroll_right() {
	move_slider(step_val);
}

void Mixer_gump::move_slider(int dir) {
	int newval = val;
	newval += dir;
	if (newval < min_val) {
		newval = min_val;
	}
	if (newval > max_val) {
		newval = max_val;
	}

	set_val(newval);
	paint();
	gwin->set_painted();
}

void Mixer_gump::save_settings() {
	config->set("config/audio/midi/music_volume", music_volume, false);
	config->set("config/audio/effects/sfx_volume", sfx_volume, false);
	config->set("config/audio/speech/speech_volume", speech_volume, false);
	config->write_back();

	// Now initialize Audio anew with the saved settings?
	// commented for now until it actually does something
	/*Audio::get_ptr()->Init_sfx();
	// restart music track if one was playing and isn't anymore
	if (midi && Audio::get_ptr()->is_music_enabled()
		&& midi->get_current_track() != track_playing
		&& (!gwin->is_bg_track(track_playing) || midi->get_ogg_enabled()
			|| midi->is_mt32())) {
		if (gwin->is_in_exult_menu()) {
			Audio::get_ptr()->start_music(
					EXULT_FLX_MEDITOWN_MID, true, EXULT_FLX);
		} else {
			Audio::get_ptr()->start_music(track_playing, looping);
		}
	}*/
}

void Mixer_gump::paint() {
	Gump::paint();
	for (auto& btn : buttons) {
		if (btn) {
			btn->paint();
		}
	}
	Font*          font = fontManager.get_font("SMALL_BLACK_FONT");
	Image_window8* iwin = gwin->get_win();
	font->paint_text(iwin->get_ib8(), "Music:", x + colx[0], y + rowy[0]);
	font->paint_text(iwin->get_ib8(), "SFX:", x + colx[0], y + rowy[1]);
	font->paint_text(iwin->get_ib8(), "Speech:", x + colx[0], y + rowy[2]);
	// Slider
	music_slider.paint_shape(
			x + colx[2] + 2 + music_volume * factor, y + rowy[0]);
	sfx_slider.paint_shape(x + colx[2] + 2 + sfx_volume * factor, y + rowy[1]);
	speech_slider.paint_shape(
			x + colx[2] + 2 + speech_volume * factor, y + rowy[2]);
	// Numbers
	gumpman->paint_num(music_volume, x + colx[6], y + rowy[0]);
	gumpman->paint_num(sfx_volume, x + colx[6], y + rowy[1]);
	gumpman->paint_num(speech_volume, x + colx[6], y + rowy[2]);

	gwin->set_painted();
}

bool Mixer_gump::mouse_down(int mx, int my, MouseButton button) {
	// Only left and right buttons
	if (button != MouseButton::Left && button != MouseButton::Right) {
		return false;
	}

	// We'll eat the mouse down if we've already got a button down
	if (pushed) {
		return true;
	}

	dragging = 0;
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

	if (pushed && !pushed->push(button)) {    // On a button?
		pushed = nullptr;
	}

	return button == MouseButton::Left || pushed != nullptr;
}

bool Mixer_gump::mouse_up(int mx, int my, MouseButton button) {
	// Not Pushing a button?
	if (!pushed) {
		return false;
	}

	if (pushed->get_pushed() != button) {
		return button == MouseButton::Left;
	}

	bool res = false;
	pushed->unpush(button);
	if (pushed->on_button(mx, my)) {
		res = pushed->activate(button);
	}
	pushed = nullptr;
	return res;
}

bool Mixer_gump::mouse_drag(
		int mx, int my    // Where mouse is.
) {
	return false;
}

bool Mixer_gump::mousewheel_up(int mx, int my) {
	const SDL_Keymod mod = SDL_GetModState();
	/*	if (mod & KMOD_ALT) {
			move_diamond(-10 * step_val);
		} else {
			move_diamond(-step_val);
		}*/
	return false;
}

bool Mixer_gump::mousewheel_down(int mx, int my) {
	const SDL_Keymod mod = SDL_GetModState();
	/*	if (mod & KMOD_ALT) {
			move_diamond(10 * step_val);
		} else {
			move_diamond(step_val);
		}*/

	return false;
}