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
#include "items.h"
using std::string;

static const int rowy[] = {8, 21, 34, 47, 61, 74};
static const int colx[] = {35, 84, 95, 117, 206, 215, 253};

static const char* oktext     = "OK";
static const char* canceltext = "CANCEL";
static const char* helptext   = "HELP";

using Mixer_button     = CallbackButton<Mixer_gump>;
using Mixer_Textbutton = CallbackTextButton<Mixer_gump>;

void Mixer_gump::close() {
	save_settings();
	done   = true;
}

void Mixer_gump::cancel() {
	done         = true;
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
		if (initial_music_ogg != -1) {
			midi->SetOggMusicVolume(initial_music_ogg, false);
		}
		if (initial_music_midi != -1) {
			midi->SetMidiMusicVolume(initial_music_midi, false);
		}
	}
	if (initial_sfx != -1) {
		audio->set_sfx_volume(initial_sfx, false);
	}
	if (initial_speech != -1) {
		audio->set_speech_volume(initial_speech, false);
	}
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
	num_sliders = 2;
	// Get initial music volume
	MyMidiPlayer* midi = audio ? audio->get_midi() : nullptr;
	if (audio && audio->is_music_enabled() && midi) {
		if (midi->get_ogg_enabled()) {
			initial_music_ogg = midi->GetOggMusicVolume();
			num_sliders++;
		} else {
			std::cout << "Mixer_gump ogg is disabled." << std::endl;
			oggslider         = nullptr;
			initial_music_ogg = -1;
		}

		if (midi->can_play_midi()) {
			initial_music_midi = midi->GetMidiMusicVolume();
			num_sliders++;
		} else {
			std::cout << "Mixer_gump midi is disabled." << std::endl;
			midislider         = nullptr;
			initial_music_midi = -1;
		}

	} else {
		// Music is disabled, get rid of music slider
		initial_music_ogg  = -1;
		initial_music_midi = -1;
		oggslider          = nullptr;
		midislider         = nullptr;
		std::cout << "Mixer_gump music is disabled." << std::endl;
	}

	// SFX
	if (audio && audio->are_effects_enabled()) {
		audio->Init_sfx();
		initial_sfx = audio->get_sfx_volume();
	} else {
		initial_sfx = -1;
		sfxslider   = nullptr;
		std::cout << "Mixer_gump sfx are disabled." << std::endl;
	}

	// Voice
	if (audio && audio->is_speech_enabled()) {
		initial_speech = audio->get_speech_volume();
	} else {
		initial_speech = -1;
		speechslider   = nullptr;
		std::cout << "Mixer_gump speech is disabled." << std::endl;
	}
	// must always have space forat least 3 sliders b ecause of the disabled messages
	if (num_sliders < 3) {
		num_sliders = 3;
	}
}

std::shared_ptr<Slider_widget> Mixer_gump::GetSlider(int sx, int sy) {
	if (midislider && midislider->get_rect().has_point(sx, sy)) {
		return midislider;
	} else if (oggslider && oggslider->get_rect().has_point(sx, sy)) {
		return oggslider;
	} else if (sfxslider && sfxslider->get_rect().has_point(sx, sy)) {
		return sfxslider;
	} else if (speechslider && speechslider->get_rect().has_point(sx, sy)) {
		return speechslider;
	}
	return nullptr;
}

Mixer_gump::Mixer_gump() : Modal_gump(nullptr, -1) {
	load_settings();

	TileRect gumprect = TileRect(29, 0, 227, rowy[num_sliders + 1] + 1);

	// If both ogg and midi make it slightly wider to the left
	if ((initial_music_midi != -1) && (initial_music_ogg != -1)) {
		gumprect.x -= 12;
		gumprect.w += 12;
	}
	int colourramp = -1;
	SetProceduralBackground(gumprect, colourramp);

	procedural_colours.RemapColours(colourramp);

	slider_track_color = procedural_colours.Background + 1;

	for (auto& btn : buttons) {
		btn.reset();
	}
	auto shiddiamond = ShapeID(EXULT_FLX_SAV_SLIDER_SHP, 0, SF_EXULT_FLX);
	auto shidleft    = ShapeID(EXULT_FLX_SCROLL_LEFT_SHP, 0, SF_EXULT_FLX);
	auto shidright   = ShapeID(EXULT_FLX_SCROLL_RIGHT_SHP, 0, SF_EXULT_FLX);
	if (initial_music_midi != -1) {
		midislider = std::make_shared<Slider_widget>(
				this, colx[1], rowy[0] - 13, shidleft, shidright, shiddiamond,
				0, 100, 1, initial_music_midi, slider_width);
	}
	if (initial_music_ogg != -1) {
		oggslider = std::make_shared<Slider_widget>(
				this, colx[1], rowy[num_sliders - 3] - 13, shidleft, shidright,
				shiddiamond, 0, 100, 1, initial_music_ogg, slider_width);
	}

	if (initial_sfx != -1) {
		sfxslider = std::make_shared<Slider_widget>(
				this, colx[1], rowy[num_sliders - 2] - 13, shidleft, shidright,
				shiddiamond, 0, 100, 1, initial_sfx, slider_width);
	}
	if (initial_speech != -1) {
		speechslider = std::make_shared<Slider_widget>(
				this, colx[1], rowy[num_sliders - 1] - 13, shidleft, shidright,
				shiddiamond, 0, 100, 1, initial_speech, slider_width);
	}
	auto Shapediamond = shiddiamond.get_shape();
	if (Shapediamond) {
		slider_height = Shapediamond->get_height();
	}

	// Ok
	buttons[id_ok] = std::make_unique<Mixer_Textbutton>(
			this, &Mixer_gump::close, oktext, colx[0] - 2, rowy[num_sliders],
			50);
	// Cancel
	buttons[id_cancel] = std::make_unique<Mixer_Textbutton>(
			this, &Mixer_gump::cancel, canceltext, colx[4], rowy[num_sliders],
			50);
#if SDL_VERSION_ATLEAST(2, 0, 14)
	// Help
	buttons[id_help] = std::make_unique<Mixer_Textbutton>(
			this, &Mixer_gump::help, helptext, colx[3], rowy[num_sliders], 50);
#endif
}

Mixer_gump::~Mixer_gump() {
	// We did not close ourselves, so do a cancel and revert settings
	if (!done) {
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
	if (midi) {
		if (oggslider) {
			midi->SetOggMusicVolume(oggslider->get_val(), true);
		} else if (midislider) {
			midi->SetMidiMusicVolume(midislider->get_val(), true);
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
	Modal_gump::paint();
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
	auto           ib8  = iwin->get_ib8();

	bool use3dslidertrack = true;

	// if don't have both music sliders
	if (!midislider || !oggslider) {
		font->paint_text_right_aligned(ib8, "Music:", x + colx[1], y + rowy[0]);
	}
	// if have neither
	if (!midislider && !oggslider) {
		font->paint_text(
				iwin->get_ib8(), " disabled", x + colx[1], y + rowy[0]);
	}
	// midi Slider
	if (midislider) {
		if (oggslider) {
			font->paint_text_right_aligned(
					iwin->get_ib8(), "MIDI Music:", x + colx[1], y + rowy[0]);
		}

		if (use3dslidertrack) {
			ib8->draw_beveled_box(
					x + colx[2] + 1, y + rowy[0], slider_width, slider_height,
					1, slider_track_color, slider_track_color + 2,
					slider_track_color + 4, slider_track_color - 2,
					slider_track_color - 4);
		} else {
			ib8->draw_box(
					x + colx[2] + 1, y + rowy[0], slider_width, slider_height,
					0, slider_track_color, 0xff);
		}

		midislider->paint();
		gumpman->paint_num(
				midislider->get_val(), x + colx[6], y + rowy[0], font);
	}
	if (oggslider) {
		if (midislider) {
			font->paint_text_right_aligned(
					iwin->get_ib8(), "OGG Music:", x + colx[1],
					y + rowy[num_sliders - 3]);
		}

		if (use3dslidertrack) {
			ib8->draw_beveled_box(
					x + colx[2] + 1, y + rowy[num_sliders - 3], slider_width,
					slider_height, 1, slider_track_color,
					slider_track_color + 2, slider_track_color + 4,
					slider_track_color - 2, slider_track_color - 4);
		} else {
			ib8->draw_box(
					x + colx[2] + 1, y + rowy[num_sliders - 3], slider_width,
					slider_height, 0, slider_track_color, 0xff);
		}
		oggslider->paint();
		gumpman->paint_num(
				oggslider->get_val(), x + colx[6], y + rowy[num_sliders - 3],
				font);
	}
	font->paint_text_right_aligned(
			ib8, "SFX:", x + colx[1], y + rowy[num_sliders - 2]);
	if (sfxslider) {
		if (use3dslidertrack) {
			ib8->draw_beveled_box(
					x + colx[2] + 1, y + rowy[num_sliders - 2], slider_width,
					slider_height, 1, slider_track_color,
					slider_track_color + 2, slider_track_color + 4,
					slider_track_color - 2, slider_track_color - 4);
		} else {
			ib8->draw_box(
					x + colx[2] + 1, y + rowy[num_sliders - 2], slider_width,
					slider_height, 0, slider_track_color, 0xff);
		}
		sfxslider->paint();
		gumpman->paint_num(
				sfxslider->get_val(), x + colx[6], y + rowy[num_sliders - 2],
				font);
	} else {
		font->paint_text(
				iwin->get_ib8(), " disabled", x + colx[1],
				y + rowy[num_sliders - 2]);
	}
	font->paint_text_right_aligned(
			ib8, "Speech:", x + colx[1], y + rowy[num_sliders - 1]);
	if (speechslider) {
		if (use3dslidertrack) {
			ib8->draw_beveled_box(
					x + colx[2] + 1, y + rowy[num_sliders - 1], slider_width,
					slider_height, 1, slider_track_color,
					slider_track_color + 2, slider_track_color + 4,
					slider_track_color - 2, slider_track_color - 4);
		} else {
			ib8->draw_box(
					x + colx[2] + 1, y + rowy[num_sliders - 1], slider_width,
					slider_height, 0, slider_track_color, 0xff);
		}
		speechslider->paint();
		gumpman->paint_num(
				speechslider->get_val(), x + colx[6], y + rowy[num_sliders - 1],
				font);
	} else {
		font->paint_text(
				iwin->get_ib8(), " disabled", x + colx[1],
				y + rowy[num_sliders - 1]);
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

//! \brief A simple class that sets a pointer to nullptr when the instance of this class goes out of scope.
//! Used for cleanup when a smart pointer that wont go out of scope needs to
//! have a temporary lifetime General usage is as follows
//! RAIIPointerClearer<decltype(globalpointer)> clearer;
//! if(!globalpointer){
//! globalpointer = temporary;
//! clearer = globalpointer;
//! }
//! do_something_with_globalpointer;
//! \tparam T Type of pointer to be cleared
//!
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

	int test_music_track = 57;    // bg number

	if (sender == midislider.get()) {
		MyMidiPlayer* midi = audio->get_midi();
		if (midi) {
			midi->SetMidiMusicVolume(newvalue, false);

			int cur = !midi->ogg_is_playing() ? midi->get_current_track() : -1;
			// play preview song here if not already playing a track

			if ((cur == -1
				 || (gwin->is_background_track(cur)
					 && !gwin->is_in_exult_menu()))
				&& !sender->is_dragging()) {
				if (!midi->start_music(
							Audio::game_music(test_music_track), false,
							MyMidiPlayer::Force_Midi)) {
					auto msg = get_text_msg(mixergump_midi_test_failed);
					SetPopupMessage(

							msg + (" #" + std::to_string(test_music_track)));
				}
			}
		}
	} else if (sender == oggslider.get()) {
		MyMidiPlayer* midi = audio->get_midi();
		if (midi) {
			midi->SetOggMusicVolume(newvalue, false);

			int cur = midi->ogg_is_playing() ? midi->get_current_track() : -1;
			// play preview song here if not already playing a track
			if ((cur == -1
				 || (gwin->is_background_track(cur)
					 && !gwin->is_in_exult_menu()))
				&& !sender->is_dragging()) {
				if (!midi->start_music(
							Audio::game_music(test_music_track), false,
							MyMidiPlayer::Force_Ogg)) {
					auto msg = get_text_msg(mixergump_ogg_test_failed);
					SetPopupMessage(msg + (" " + midi->GetOggFailed()));
				}
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
