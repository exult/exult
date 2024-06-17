/*
 *  Copyright (C) 2024  The Exult Team
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

#include "Audio.h"
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
static const int colx[] = {35, 84, 95, 117, 206, 215, 253};

static const char* oktext     = "OK";
static const char* canceltext = "CANCEL";
static const char* helptext   = "HELP";

using Mixer_button     = CallbackButton<Mixer_gump>;
using Mixer_Textbutton = CallbackTextButton<Mixer_gump>;

void Mixer_gump::close() {
	save_settings();
	done   = true;
	closed = true;
}

void Mixer_gump::cancel() {
	done         = true;
	closed       = true;
	Audio* audio = Audio::get_ptr();
	if (!audio) {
		return;
	}

	if (!audio->is_audio_enabled()) {
		return;
	}
	audio->stop_speech();

	MyMidiPlayer* midi = audio->get_midi();
	if (midi) {
		// do not save config
		if (music_is_ogg) {
			midi->SetOggMusicVolume(initial_music, false);
		} else {
			midi->SetMidiMusicVolume(initial_music, false);
		}
	}
	audio->set_sfx_volume(initial_music, false);
	audio->set_speech_volume(initial_speech, false);
}

void Mixer_gump::help() {
#if SDL_VERSION_ATLEAST(2, 0, 14)
	// Pointing at Audio section for help as there is no item for the mixer
	// yet
	SDL_OpenURL("http://exult.info/docs.php#Audio");
#endif
}

void Mixer_gump::load_settings() {
	Audio* audio = Audio::get_ptr();

	if (audio && !audio->is_audio_enabled()) {
		audio = nullptr;
	}
	// Get initial music volume
	MyMidiPlayer* midi = audio ? audio->get_midi() : nullptr;
	if (audio && audio->is_music_enabled() && midi && midi->can_play()) {
		if (midi->get_ogg_enabled()) {
			if (midi->ogg_is_playing() || midi->get_current_track() == -1) {
				music_is_ogg = true;
			}
		}

		if (music_is_ogg) {
			initial_music = midi->GetOggMusicVolume();
		} else {
			initial_music = midi->GetMidiMusicVolume();
		}

		musicslider->set_val(initial_music);
	} else {
		// Music is disabled, get rid of music slider
		musicslider = nullptr;
		std::cout << "Mixer_gump music is disabled." << std::endl;
	}

	// SFX
	if (audio && audio->are_effects_enabled()) {
		audio->Init_sfx();
		sfxslider->set_val(initial_sfx = audio->get_sfx_volume());
	} else {
		sfxslider = nullptr;
		std::cout << "Mixer_gump sfx are disabled." << std::endl;
	}

	// Voice
	if (audio && audio->is_speech_enabled()) {
		speechslider->set_val(initial_speech = audio->get_speech_volume());
	} else {
		speechslider = nullptr;
		std::cout << "Mixer_gump voice is disabled." << std::endl;
	}
}

std::shared_ptr<Slider_widget> Mixer_gump::GetSlider(int sx, int sy) {
	if (musicslider && musicslider->get_rect().has_point(sx, sy)) {
		return musicslider;
	} else if (sfxslider && sfxslider->get_rect().has_point(sx, sy)) {
		return sfxslider;
	} else if (speechslider && speechslider->get_rect().has_point(sx, sy)) {
		return speechslider;
	}
	return nullptr;
}

Mixer_gump::Mixer_gump()
		: Modal_gump(nullptr, EXULT_FLX_MIXER_SHP, SF_EXULT_FLX) {
	set_object_area(TileRect(0, 0, 0, 0), 9, 62);

	for (auto& btn : buttons) {
		btn.reset();
	}
	musicslider = std::make_shared<Slider_widget>(
			this, colx[1], rowy[0] - 13,
			ShapeID(EXULT_FLX_SCROLL_LEFT_SHP, 0, SF_EXULT_FLX),
			ShapeID(EXULT_FLX_SCROLL_RIGHT_SHP, 0, SF_EXULT_FLX),
			ShapeID(EXULT_FLX_SAV_SLIDER_SHP, 0, SF_EXULT_FLX), 0, 100, 1, 100,
			120);
	sfxslider = std::make_shared<Slider_widget>(
			this, colx[1], rowy[1] - 13,
			ShapeID(EXULT_FLX_SCROLL_LEFT_SHP, 0, SF_EXULT_FLX),
			ShapeID(EXULT_FLX_SCROLL_RIGHT_SHP, 0, SF_EXULT_FLX),
			ShapeID(EXULT_FLX_SAV_SLIDER_SHP, 0, SF_EXULT_FLX), 0, 100, 1, 100,
			120);
	speechslider = std::make_shared<Slider_widget>(
			this, colx[1], rowy[2] - 13,
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

Mixer_gump::~Mixer_gump() {
	// We did not close ourselves, so do a cancel and revert settings
	if (!closed) {
		cancel();
	}
}

void Mixer_gump::save_settings() {
	Audio* audio = Audio::get_ptr();
	if (!audio || !audio->is_audio_enabled()) {
		return;
	}
	audio->stop_speech();

	MyMidiPlayer* midi = audio->get_midi();
	if (midi && musicslider) {
		if (music_is_ogg) {
			midi->SetOggMusicVolume(musicslider->get_val(), true);
		} else {
			midi->SetMidiMusicVolume(musicslider->get_val(), true);
		}
	}
	if (sfxslider) {
		audio->set_sfx_volume(sfxslider->get_val(), true);
	}
	if (speechslider) {
		audio->set_speech_volume(speechslider->get_val(), true);
	}

	config->write_back();
}

void Mixer_gump::paint() {
	Gump::paint();
	for (auto& btn : buttons) {
		if (btn) {
			btn->paint();
		}
	}
	Font* font = fontManager.get_font("SMALL_BLACK_FONT");
	// font is required
	if (!font) {
		std::cerr << "Mixer_gump::paint() unable to get SMALL_BLACK_FONT "
					 "closing gump."
				  << std::endl;
		cancel();
		return;
	}

	Image_window8* iwin = gwin->get_win();
	/* commented out because there isn't enough room
	if (music_is_ogg) {
		font->paint_text(
				iwin->get_ib8(), "OGG Music:", x + colx[0], y + rowy[0]);
	} else {
		font->paint_text(
				iwin->get_ib8(), "MIDI Music:", x + colx[0], y + rowy[0]);
	}
	*/
	font->paint_text(iwin->get_ib8(), "Music:", x + colx[0], y + rowy[0]);
	font->paint_text(iwin->get_ib8(), "SFX:", x + colx[0], y + rowy[1]);
	font->paint_text(iwin->get_ib8(), "Speech:", x + colx[0], y + rowy[2]);
	// Slider
	if (musicslider) {
		musicslider->paint();
		gumpman->paint_num(
				musicslider->get_val(), x + colx[6], y + rowy[0], font);
	} else {
		font->paint_text(iwin->get_ib8(), "disabled", x + colx[2], y + rowy[0]);
	}

	if (sfxslider) {
		sfxslider->paint();
		gumpman->paint_num(
				sfxslider->get_val(), x + colx[6], y + rowy[1], font);
	} else {
		font->paint_text(iwin->get_ib8(), "disabled", x + colx[2], y + rowy[1]);
	}
	if (speechslider) {
		speechslider->paint();
		gumpman->paint_num(
				speechslider->get_val(), x + colx[6], y + rowy[2], font);
	} else {
		font->paint_text(iwin->get_ib8(), "disabled", x + colx[2], y + rowy[2]);
	}

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
	if (inputslider == nullptr) {
		inputslider = GetSlider(mx, my);
	}

	if (inputslider && inputslider->mouse_down(mx, my, button)) {
		return true;
	}
	return Modal_gump::mouse_down(mx, my, button);
}

bool Mixer_gump::mouse_up(int mx, int my, MouseButton button) {
	// Not Pushing a button?
	if (!pushed) {
		if (inputslider && inputslider->mouse_up(mx, my, button)) {
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

template <typename T>
class RAIIPointerClearer {
	T* pointer = nullptr;

public:
	~RAIIPointerClearer() {
		if (pointer) {
			*pointer = nullptr;
		}
	}

	RAIIPointerClearer<T>& operator=(T& ref) {
		pointer = &ref;
		return *this;
	}
};

bool Mixer_gump::mousewheel_up(int mx, int my) {
	RAIIPointerClearer<decltype(inputslider)> clearer;
	if (inputslider == nullptr) {
		inputslider = GetSlider(mx, my);
		clearer     = inputslider;
	}
	if (inputslider && inputslider->mousewheel_up(mx, my)) {
		return true;
	}

	return Modal_gump::mousewheel_up(mx, my);
}

bool Mixer_gump::mousewheel_down(int mx, int my) {
	RAIIPointerClearer<decltype(inputslider)> clearer;
	if (inputslider == nullptr) {
		inputslider = GetSlider(mx, my);
		clearer     = inputslider;
	}
	if (inputslider && inputslider->mousewheel_down(mx, my)) {
		return true;
	}

	return Modal_gump::mousewheel_down(mx, my);
}

bool Mixer_gump::key_down(int chr) {
	switch (chr) {
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		close();
		return true;
	default:
		// Send the input to the current input slider if there is one
		// if there is not an inputslider get the current mouse pos and get the
		// slider under it
		RAIIPointerClearer<decltype(inputslider)> clearer;

		if (!inputslider) {
			inputslider = GetSlider(
					Mouse::mouse->get_mousex(), Mouse::mouse->get_mousey());
			clearer = inputslider;
		}
		if (inputslider && inputslider->key_down(chr)) {
			return true;
		}

		break;
	}
	return Modal_gump::key_down(chr);
}

void Mixer_gump::OnSliderValueChanged(Slider_widget* sender, int newvalue) {
	gwin->add_dirty(get_rect());

	// do nothing if out of range, it should always be in range
	if (newvalue < 0 || newvalue > 100) {
		return;
	}
	// Only accept updates from the current input slider
	if (inputslider.get() != sender) {
		return;
	}

	Audio* audio = Audio::get_ptr();
	if (!audio) {
		return;
	}

	// we do not save to config here, only update Audio with the new values for
	// preview purposes
	// For the Exult menu playing test sounds it will load the data of one of
	// the games and appear as if it is that game so no special casing needs to
	// be done for it here it seems like exult menu assumes the identity of the
	// first game it finds which will be bg if it is configured

	if (sender == musicslider.get()) {
		MyMidiPlayer* midi = audio->get_midi();
		if (midi) {
			if (music_is_ogg) {
				midi->SetOggMusicVolume(newvalue, false);
			} else {
				midi->SetMidiMusicVolume(newvalue, false);
			}
			int cur = midi->get_current_track();
			// play preview song here if not already playing a track
			// Choosing the harpsicord object music track because it is short
			// and available for both games;
			if ((cur == -1 || (gwin->is_background_track(cur) && music_is_ogg))
				&& !sender->is_dragging()) {
				midi->start_music(
						Audio::game_music(57), false,
						!music_is_ogg);    // 57 is bg track number
			}
		}
	} else if (sender == sfxslider.get()) {
		audio->set_sfx_volume(newvalue, false);
		if (!sender->is_dragging()) {
			audio->stop_sound_effects();
			// Play preview sfx here at the new volume

			// Both games use a similar bell chime sfx
			if (GAME_BG) {
				audio->play_sound_effect(19);
			} else if (GAME_SI) {
				audio->play_sound_effect(30);
			}
		}
	} else if (sender == speechslider.get()) {
		audio->set_speech_volume(newvalue, false);
		if (!sender->is_dragging()) {
			// Play preview voice sample here at the new volume
			if (!audio->is_speech_playing()) {
				if (GAME_BG) {
					// BG use speech 1   Guardian: Yes my friend rest and heal
					// so that you are strong and able to face the perils before
					// you. Pleasant Dreams
					audio->start_speech(1);
				} else if (GAME_SI) {
					// SI use speech 21   Guardian: Pleasant Dreams Avatar
					// *laughs*
					audio->start_speech(21);
				}
			}
		}
	}
}
