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
#include <Audio.h>

using std::string;

static const int rowy[] = {8, 21, 34, 48};
static const int colx[] = {35, 84, 95, 117, 206, 215, 253};


static const char* oktext     = "OK";
static const char* canceltext = "CANCEL";
static const char* helptext   = "HELP";



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
//	config->value("config/audio/midi/music_volume", music_volume, music_volume);
	//config->value("config/audio/effects/sfx_volume", sfx_volume, sfx_volume);
	//config->value(
		//	"config/audio/speech/speech_volume", speech_volume, speech_volume);
}


std::shared_ptr<Slider_widget> Mixer_gump::GetSlider(int sx, int sy) {
	if (musicslider->get_rect().has_point(sx, sy)) {
		return musicslider;
	} else if (sfxslider->get_rect().has_point(sx, sy)) {
		return sfxslider;
	} else if (voiceslider->get_rect().has_point(sx, sy)) {
		return voiceslider;
	}
	return nullptr;
}

Mixer_gump::Mixer_gump()
		: Modal_gump(nullptr, EXULT_FLX_MIXER_SHP, SF_EXULT_FLX) {
	set_object_area(TileRect(0, 0, 0, 0), 9, 62);

	for (auto& btn : buttons) {
		btn.reset();
	}
	musicslider = std::make_unique<Slider_widget>(
			this, colx[1], rowy[0]-13,
			ShapeID(EXULT_FLX_SCROLL_LEFT_SHP, 0, SF_EXULT_FLX),
			ShapeID(EXULT_FLX_SCROLL_RIGHT_SHP, 0, SF_EXULT_FLX),
			ShapeID(EXULT_FLX_SAV_SLIDER_SHP, 0, SF_EXULT_FLX), 0, 100, 1, 100,
			120);
	sfxslider = std::make_unique<Slider_widget>(
			this, colx[1], rowy[1]-13,
			ShapeID(EXULT_FLX_SCROLL_LEFT_SHP, 0, SF_EXULT_FLX),
			ShapeID(EXULT_FLX_SCROLL_RIGHT_SHP, 0, SF_EXULT_FLX),
			ShapeID(EXULT_FLX_SAV_SLIDER_SHP, 0, SF_EXULT_FLX), 0, 100, 1, 100,
			120);
	voiceslider = std::make_unique<Slider_widget>(
			this, colx[1], rowy[2]-13,
			ShapeID(EXULT_FLX_SCROLL_LEFT_SHP, 0, SF_EXULT_FLX),
			ShapeID(EXULT_FLX_SCROLL_RIGHT_SHP, 0, SF_EXULT_FLX),
			ShapeID(EXULT_FLX_SAV_SLIDER_SHP, 0, SF_EXULT_FLX), 0, 100, 1, 100,
			120);
			/*buttons[id_music_left]
			= std::make_unique<Mixer_button>(
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
			*/

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



void Mixer_gump::save_settings() {
	//config->set("config/audio/midi/music_volume", music_volume, false);
	//config->set("config/audio/effects/sfx_volume", sfx_volume, false);
	//config->set("config/audio/speech/speech_volume", speech_volume, false);
	///config->write_back();

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
	musicslider->paint();
	sfxslider->paint();
	voiceslider->paint();
	// Numbers
	gumpman->paint_num(musicslider->get_val(), x + colx[6], y + rowy[0], font);
	gumpman->paint_num(sfxslider->get_val(), x + colx[6], y + rowy[1], font);
	gumpman->paint_num(voiceslider->get_val(), x + colx[6], y + rowy[2], font);

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

	if (pushed && pushed->push(button)) {    // On a button?
		return true;
	}

	pushed = nullptr;
	if (inputslider == nullptr) 
		inputslider = GetSlider(mx, my);

	if (inputslider && inputslider->mouse_down(mx, my, button)) {
		return true;
	}
	return Modal_gump::mouse_down(mx, my, button);
}

bool Mixer_gump::mouse_up(int mx, int my, MouseButton button) {
	// Not Pushing a button?
	if (!pushed) {

			if (inputslider && inputslider->mouse_up(mx, my, button))
			{
					inputslider = nullptr;
					return true;
			}
			inputslider = nullptr;

			return Modal_gump::mouse_up(mx, my, button);
			
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

	if (inputslider && inputslider->mouse_drag(mx, my)) {
		return true;
	} 

	return Modal_gump::mouse_drag(mx, my);
	
}

bool Mixer_gump::mousewheel_up(int mx, int my) {

	bool clear = false;
	if (inputslider == nullptr) {
		inputslider = GetSlider(mx, my);
		clear       = true;
	}
		if (inputslider && inputslider->mousewheel_up(mx, my)) {
		if (clear) {
			inputslider = nullptr;
		}
		return true;
	}

	return Modal_gump::mousewheel_up(mx, my);
}

bool Mixer_gump::mousewheel_down(int mx, int my) {

	bool clear = false;
	if (inputslider == nullptr) {
		inputslider = GetSlider(mx, my);
		clear       = true;
	}
	if (inputslider && inputslider->mousewheel_down(mx, my)) {
		if (clear) {
			inputslider = nullptr;
		}
		return true;
	}

	return Modal_gump::mousewheel_down(mx, my);
}

void Mixer_gump::OnSliderValueChanged(Slider_widget* sender, int newvalue) {
	gwin->add_dirty(get_rect());
	
	// do nothing if out of range, it should always be in range
	if (newvalue < 0 || newvalue > 100) {
		return;
	}

	Audio* audio = Audio::get_ptr();
	if (sender == musicslider.get()) {
		MyMidiPlayer* midi = audio?audio->get_midi():nullptr;
		if (midi) {
			// do not save config
			midi->SetMidiMusicVolume(newvalue,false);
		}
	}
	else if (sender == sfxslider.get()) {
	}
	else if (sender == voiceslider.get()) {
	}
}
