/*
 *  Copyright (C) 2000-2025 The Exult Team
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
#include "Midi.h"
#include "Scroll_gump.h"
#include "XMidiEvent.h"
#include "XMidiEventList.h"
#include "XMidiFile.h"
#include "exult.h"
#include "font.h"
#include "gamewin.h"
#include "gump_utils.h"
#include "items.h"
#include "mouse.h"
#include "soundtest.h"
#include "tqueue.h"

#include <iomanip>
#include <sstream>

namespace {

	// Only the strings used in dynamic display lines
	class Strings {
	public:
		static auto Music() {
			return get_text_msg(0x89C - msg_file_start);
		}

		static auto Sfx() {
			return get_text_msg(0x89D - msg_file_start);
		}

		static auto Voice() {
			return get_text_msg(0x89E - msg_file_start);
		}

		static auto Repeat() {
			return get_text_msg(0x89F - msg_file_start);
		}
	};

}    // namespace

void SoundTester::test_sound() {
	Game_window*          gwin = Game_window::get_instance();
	Image_buffer8*        ibuf = gwin->get_win()->get_ib8();
	std::shared_ptr<Font> font = Shape_manager::get_instance()->get_font(4);

	Audio*                 audio  = Audio::get_ptr();
	MyMidiPlayer*          player = audio->get_midi();
	Pentagram::AudioMixer* mixer  = Pentagram::AudioMixer::get_instance();

	char      buf[256];
	bool      looping = true;
	bool      redraw  = true;
	SDL_Event event;

	const int centerx    = gwin->get_width() / 2;
	const int centery    = gwin->get_height() / 2;
	const int left       = centerx - 68;
	const int first_line = centery - 53;
	const int height     = 6;
	const int width      = 6;
	int       sfx_id     = -1;

	Mouse::mouse()->hide();

	gwin->get_tqueue()->pause(SDL_GetTicks());

	int line;
	do {
		if (redraw) {
			Scroll_gump scroll;
			scroll.set_from_help(true);
			scroll.add_text(" ~");
			scroll.paint();
			// Update our repeat flag to match the player if it is playing out
			// song
			if (player && player->is_track_playing(song)) {
				repeat = player->is_repeating();
			}
			line = first_line;
			// Paint controls from exultmsg.txt block 0x890-0x89B
#ifdef DEBUG
			// "D - Dump Music Track"
			constexpr int msg_end = 0x89B;
#else
			constexpr int msg_end = 0x89A;
#endif
			for (int msgid = 0x890; msgid <= msg_end; ++msgid) {
				font->paint_text_fixedwidth(ibuf, get_text_msg(msgid - msg_file_start), left, line, width);
				line += height;
			}

			{
				std::ostringstream oss;
				oss << (active == 0 ? "->" : "  ") << " " << Strings::Music() << " " << (active == 0 ? '<' : ' ')
					<< ((player && player->is_track_playing(song)) ? '*' : ' ') << std::setw(3) << song
					<< ((player && player->is_track_playing(song)) ? '*' : ' ') << (active == 0 ? '>' : ' ')
					<< (repeat ? Strings::Repeat() : "");
				line += height * 2;
				font->paint_text_fixedwidth(ibuf, oss.str().c_str(), left, line, width);
			}
			line += height;
			if (player && player->get_current_track() != -1) {
				formattime(buf, sizeof(buf), player->get_track_position(), player->get_track_length());
				font->paint_text_fixedwidth(ibuf, buf, left, line, width);
			}
			line += height;
			{
				std::ostringstream oss;
				oss << (active == 1 ? "->" : "  ") << " " << Strings::Sfx() << "   " << (active == 1 ? '<' : ' ') << " "
					<< std::setw(3) << sfx << " " << (active == 1 ? '>' : ' ');
				font->paint_text_fixedwidth(ibuf, oss.str().c_str(), left, line, width);
			}
			line += height;
			if (sfx_id != -1 && mixer->isPlaying(sfx_id)) {
				formattime(buf, sizeof(buf), mixer->GetPlaybackPosition(sfx_id), mixer->GetPlaybackLength(sfx_id));
				font->paint_text_fixedwidth(ibuf, buf, left, line, width);
			}
			line += height;

			{
				std::ostringstream oss;
				oss << (active == 2 ? "->" : "  ") << " " << Strings::Voice() << " " << (active == 2 ? '<' : ' ') << " "
					<< std::setw(3) << voice << " " << (active == 2 ? '>' : ' ');
				font->paint_text_fixedwidth(ibuf, oss.str().c_str(), left, line, width);
			}
			line += height;
			if (audio->is_speech_playing()) {
				int speech_id = audio->get_speech_id();
				if (speech_id != -1) {
					formattime(buf, sizeof(buf), mixer->GetPlaybackPosition(speech_id), mixer->GetPlaybackLength(speech_id));
					font->paint_text_fixedwidth(ibuf, buf, left, line, width);
				}
			}

			gwin->show();
			redraw = false;
		}
		auto repainttime = SDL_GetTicks() + 10;
		while (looping && !redraw && SDL_GetTicks() < repainttime) {
			Delay();
			while (looping && !redraw && SDL_PollEvent(&event)) {
				if (event.type == SDL_EVENT_KEY_DOWN) {
					redraw = true;
					switch (event.key.key) {
					case SDLK_ESCAPE:
						looping = false;
						break;

					case SDLK_RETURN:
					case SDLK_KP_ENTER:
					case SDLK_SPACE:
						if (active == 0) {
							audio->stop_music();
							audio->start_music(song, repeat);
						} else if (active == 1) {
							audio->stop_sound_effects();
							sfx_id = audio->play_sound_effect(sfx);
						} else if (active == 2) {
							audio->stop_speech();
							audio->start_speech(voice, false);
						}
						break;
#ifdef DEBUG
					case SDLK_D: {
						std::string flex = player && player->is_adlib() ? MAINMUS_AD : MAINMUS;

						std::unique_ptr<IDataSource> mid_data = open_music_flex(flex, song);
						int                          convert  = XMIDIFILE_CONVERT_NOCONVERSION;
						std::string_view             driver_name{};

						if (player) {
							convert     = player->setup_timbre_for_track(flex);
							driver_name = player->get_actual_midi_driver_name();
						}

						if (mid_data && mid_data->good()) {
							XMidiFile midfile(mid_data.get(), convert, driver_name);
							for (int i = 0; i < midfile.number_of_tracks(); i++) {
								std::cout << " track " << i << std::endl;
								midfile.GetEventList(i)->events->DumpText(std::cout);
							}
						}
					} break;
#endif
					case SDLK_R:
						repeat = !repeat;
						// Change repeat flag of the player to match us if it is
						// playing our selected song
						if (player && player->is_track_playing(song)) {
							player->set_repeat(repeat);
						}
						break;
					case SDLK_S:
						if ((event.key.mod & SDL_KMOD_ALT) && (event.key.mod & SDL_KMOD_CTRL)) {
							make_screenshot(true);
						} else {
							if (active == 0) {
								audio->stop_music();
							} else if (active == 1) {
								audio->stop_sound_effects();
							} else if (active == 2) {
								audio->stop_speech();
							}
						}
						break;
					case SDLK_I:
					case SDLK_UP:
						active = (active + 2) % 3;
						break;
					case SDLK_K:
					case SDLK_DOWN:
						active = (active + 1) % 3;
						break;
					case SDLK_J:
					case SDLK_LEFT:
						if (active == 0) {
							song--;
							if (song < 0) {
								song = 255;
							}

						} else if (active == 1) {
							sfx--;
							if (sfx < 0) {
								sfx = 255;
							}
						} else if (active == 2) {
							voice--;
							if (voice < 0) {
								voice = 255;
							}
						}
						break;
					case SDLK_RIGHT:
					case SDLK_L:
						if (active == 0) {
							song++;
							if (song > 255) {
								song = 0;
							}
						} else if (active == 1) {
							sfx++;
							if (sfx > 255) {
								sfx = 0;
							}
						} else if (active == 2) {
							voice++;
							if (voice > 255) {
								voice = 0;
							}
						}
						break;
					default:
						break;
					}
				}
			}
			redraw = redraw || SDL_GetTicks() > repainttime;
		}
	} while (looping);

	gwin->get_tqueue()->resume(SDL_GetTicks());

	gwin->paint();
	Mouse::mouse()->show();
	gwin->show();
}

size_t SoundTester::formattime(char* buffer, size_t buffersize, uint32 position, uint32 length) {
	int seconds = position / 1000;

	size_t count = snprintf(buffer, buffersize, "%01d:%02d.%03d", seconds / 60, seconds % 60, position % 1000);
	buffersize -= count;
	buffer += count;
	if (buffersize > 0) {
		seconds = length / 1000;

		count += snprintf(buffer, buffersize, " / %01d:%02d.%03d", seconds / 60, seconds % 60, length % 1000);
	}
	return count;
}
