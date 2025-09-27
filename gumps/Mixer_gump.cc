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
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
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

// Translatable Strings
class Strings : public GumpStrings {
public:
	static auto Failedtoplaymiditesttrack() {
		return get_text_msg(0x5A0 - msg_file_start);
	}

	static auto Failedtoplayoggtesttrack() {
		return get_text_msg(0x5A1 - msg_file_start);
	}

	static auto Music_() {
		return get_text_msg(0x5A2 - msg_file_start);
	}

	static auto MIDIMusic_() {
		return get_text_msg(0x5A3 - msg_file_start);
	}

	static auto OGGMusic_() {
		return get_text_msg(0x5A4 - msg_file_start);
	}

	static auto SFX_() {
		return get_text_msg(0x5A5 - msg_file_start);
	}

	static auto Speech_() {
		return get_text_msg(0x5A6 - msg_file_start);
	}
};

using Mixer_button     = CallbackButton<Mixer_gump>;
using Mixer_Textbutton = CallbackTextButton<Mixer_gump>;

void Mixer_gump::close() {
	save_settings();
	done = true;
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
	SDL_OpenURL("https://exult.info/docs.html#volume_mixer_gump");
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
	// must always have space forat least 3 sliders b ecause of the disabled
	// messages
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

	TileRect gumprect = TileRect(0, 0, 158, yForRow(num_sliders + 1) + 1);

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
				this,
				label_margin + font->get_text_width(Strings::MIDIMusic_()),
				yForRow(0) - 13, shidleft, shidright, shiddiamond, 0, 100, 1,
				initial_music_midi, slider_width);
	}
	if (initial_music_ogg != -1) {
		oggslider = std::make_shared<Slider_widget>(
				this, label_margin + font->get_text_width(Strings::OGGMusic_()),
				yForRow(num_sliders - 3) - 13, shidleft, shidright, shiddiamond,
				0, 100, 1, initial_music_ogg, slider_width);
	}

	if (initial_sfx != -1) {
		sfxslider = std::make_shared<Slider_widget>(
				this, label_margin + font->get_text_width(Strings::SFX_()),
				yForRow(num_sliders - 2) - 13, shidleft, shidright, shiddiamond,
				0, 100, 1, initial_sfx, slider_width);
	}
	if (initial_speech != -1) {
		speechslider = std::make_shared<Slider_widget>(
				this, label_margin + font->get_text_width(Strings::Speech_()),
				yForRow(num_sliders - 1) - 13, shidleft, shidright, shiddiamond,
				0, 100, 1, initial_speech, slider_width);
	}
	auto Shapediamond = shiddiamond.get_shape();
	if (Shapediamond) {
		slider_height = Shapediamond->get_height();
	}

	// Ok
	buttons[id_ok] = std::make_unique<Mixer_Textbutton>(
			this, &Mixer_gump::close, Strings::OK(), 25, yForRow(num_sliders),
			50);
	// Help
	buttons[id_help] = std::make_unique<Mixer_Textbutton>(
			this, &Mixer_gump::help, Strings::HELP(), 50, yForRow(num_sliders),
			50);

	// Cancel
	buttons[id_cancel] = std::make_unique<Mixer_Textbutton>(
			this, &Mixer_gump::cancel, Strings::CANCEL(), 75,
			yForRow(num_sliders), 50);

	// resize gump and reposition widgets
	ResizeWidthToFitWidgets(tcb::span(&midislider, 4), 28);
	HorizontalArrangeWidgets(tcb::span(buttons.data() + id_ok, 3));
	RightAlignWidgets(tcb::span(&midislider, 4), 28);
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
			midi->SetOggMusicVolume(oggslider->getselection(), true);
		} else if (midislider) {
			midi->SetMidiMusicVolume(midislider->getselection(), true);
		}
	}
	if (sfxslider) {
		audio->set_sfx_volume(sfxslider->getselection(), true);
	}
	if (speechslider) {
		audio->set_speech_volume(speechslider->getselection(), true);
	}

	config->write_back();
}

void Mixer_gump::PaintSlider(
		Image_window8* iwin, Slider_widget* slider, const char* label,
		bool use3dslidertrack) {
	auto rect = slider->get_rect();
	font->paint_text_right_aligned(iwin->get_ib8(), label, rect.x, rect.y+2);

	if (use3dslidertrack) {
		iwin->get_ib8()->draw_beveled_box(
				rect.x + 12, rect.y+2, slider_width, slider_height, 1,
				slider_track_color, slider_track_color + 2,
				slider_track_color + 4, slider_track_color - 2,
				slider_track_color - 4);
	} else {
		iwin->get_ib8()->draw_box(
				rect.x + 12, y + yForRow(0), slider_width, slider_height, 0,
				slider_track_color, 0xff);
	}

	slider->paint();

	gumpman->paint_num(
			slider->getselection(), rect.x + rect.w + 24, rect.y + 2, font);
}

void Mixer_gump::paint() {
	Modal_gump::paint();
	for (auto& btn : buttons) {
		if (btn) {
			btn->paint();
		}
	}

	Image_window8* iwin = gwin->get_win();

	int disabled_text_pos
			= x + std::max(procedural_background.w - slider_width - 68, 84);

	// if have neither
	if (!midislider && !oggslider) {
		font->paint_text_right_aligned(
				iwin->get_ib8(), Strings::Music_(), disabled_text_pos,
				y + yForRow(0));

		font->paint_text(
				iwin->get_ib8(), Strings::disabled(), disabled_text_pos,
				y + yForRow(0));
	}
	// midi Slider
	if (midislider) {
		PaintSlider(
				iwin, midislider.get(),
				oggslider ? Strings::MIDIMusic_() : Strings::Music_());
	}
	if (oggslider) {
		PaintSlider(
				iwin, oggslider.get(),
				midislider ? Strings::OGGMusic_() : Strings::Music_());
	}
	if (sfxslider) {
		PaintSlider(iwin, sfxslider.get(), Strings::SFX_());
	} else {
		font->paint_text_right_aligned(
				iwin->get_ib8(), Strings::SFX_(), disabled_text_pos,
				y + yForRow(num_sliders - 2));
		font->paint_text(
				iwin->get_ib8(), Strings::disabled(), disabled_text_pos,
				y + yForRow(num_sliders - 2));
	}
	if (speechslider) {
		PaintSlider(iwin, speechslider.get(), Strings::Speech_());
	} else {
		font->paint_text_right_aligned(
				iwin->get_ib8(), Strings::Speech_(), disabled_text_pos,
				y + yForRow(num_sliders - 1));
		font->paint_text(
				iwin->get_ib8(), Strings::disabled(), disabled_text_pos,
				y + yForRow(num_sliders - 1));
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

bool Mixer_gump::key_down(SDL_Keycode chr, SDL_Keycode unicode) {
	switch (chr) {
	case SDLK_RETURN:
		close();
		return true;
	default:
		// Send the input to the current input slider if there is one
		// if there is not an inputslider get the current mouse pos and get the
		// slider under it
		RAIIPointerClearer<decltype(inputslider)> clearer;

		if (!inputslider) {
			inputslider = GetSlider(
					Mouse::mouse()->get_mousex(), Mouse::mouse()->get_mousey());
			clearer = inputslider;
		}
		if (inputslider && inputslider->key_down(chr, unicode)) {
			return true;
		}

		break;
	}
	return Modal_gump::key_down(chr, unicode);
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
					SetPopupMessage(
							Strings::Failedtoplaymiditesttrack()
							+ (" #" + std::to_string(test_music_track)));
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
					SetPopupMessage(
							Strings::Failedtoplayoggtesttrack()
							+ (" " + midi->GetOggFailed()));
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
