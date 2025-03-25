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
#include "mouse.h"
#include "soundtest.h"
#include "tqueue.h"

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
	const int left       = centerx - 65;
	const int first_line = centery - 53;
	const int height     = 6;
	const int width      = 6;
	int       sfx_id     = -1;

	Mouse::mouse->hide();

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
			font->paint_text_fixedwidth(
					ibuf, "Sound Tester", left, line, width);
			line += height;
			font->paint_text_fixedwidth(
					ibuf, "------------", left, line, width);

			line += height * 2;
			font->paint_text_fixedwidth(
					ibuf, "   Up - Previous Type", left, line, width);

			line += height;
			font->paint_text_fixedwidth(
					ibuf, " Down - Next Type", left, line, width);

			line += height;
			font->paint_text_fixedwidth(
					ibuf, " Left - Previous Number", left, line, width);

			line += height;
			font->paint_text_fixedwidth(
					ibuf, "Right - Next Number", left, line, width);

			line += height;
			font->paint_text_fixedwidth(
					ibuf, "Enter - Play it", left, line, width);

			line += height;
			font->paint_text_fixedwidth(
					ibuf, "  ESC - Leave", left, line, width);

			line += height;
			font->paint_text_fixedwidth(
					ibuf, "    R - Repeat Music", left, line, width);

			line += height;
			font->paint_text_fixedwidth(
					ibuf, "    S - Stop Music", left, line, width);

#ifdef DEBUG
			line += height;
			font->paint_text_fixedwidth(
					ibuf, "    D - Dump Music Track ", left, line, width);
#endif

			snprintf(
					buf, sizeof(buf), "%2s Music %c%c%3i%c%c %s",
					active == 0 ? "->" : "", active == 0 ? '<' : ' ',
					player->is_track_playing(song) ? '*' : ' ', song,
					player->is_track_playing(song) ? '*' : ' ',
					active == 0 ? '>' : ' ', repeat ? "- Repeat" : "");
			line += height * 2;
			font->paint_text_fixedwidth(ibuf, buf, left, line, width);
			line += height;
			if (player && player->get_current_track() != -1) {
				formattime(
						buf, sizeof(buf), player->get_track_position(),
						player->get_track_length());
				font->paint_text_fixedwidth(ibuf, buf, left, line, width);
			}
			line += height;
			snprintf(
					buf, sizeof(buf), "%2s SFX   %c %3i %c",
					active == 1 ? "->" : "", active == 1 ? '<' : ' ', sfx,
					active == 1 ? '>' : ' ');

			font->paint_text_fixedwidth(ibuf, buf, left, line, width);
			line += height;
			if (sfx_id != -1 && mixer->isPlaying(sfx_id)) {
				formattime(
						buf, sizeof(buf), mixer->GetPlaybackPosition(sfx_id),
						mixer->GetPlaybackLength(sfx_id));
				font->paint_text_fixedwidth(ibuf, buf, left, line, width);
			}
			line += height;

			snprintf(
					buf, sizeof(buf), "%2s Voice %c %3i %c",
					active == 2 ? "->" : "", active == 2 ? '<' : ' ', voice,
					active == 2 ? '>' : ' ');

			font->paint_text_fixedwidth(ibuf, buf, left, line, width);
			line += height;
			if (audio->is_speech_playing()) {
				int speech_id = audio->get_speech_id();
				if (speech_id != -1) {
					formattime(
							buf, sizeof(buf),
							mixer->GetPlaybackPosition(speech_id),
							mixer->GetPlaybackLength(speech_id));
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
					switch (event.key.keysym.sym) {
					case SDLK_ESCAPE:
						looping = false;
						break;

					case SDLK_RETURN:
					case SDLK_KP_ENTER:
						if (active == 0) {
							audio->stop_music();
							audio->start_music(song, repeat);
						} else if (active == 1) {
							sfx_id = audio->play_sound_effect(sfx);
						} else if (active == 2) {
							audio->stop_speech();
							audio->start_speech(voice, false);
						}
						break;
#ifdef DEBUG
					case SDLK_D: {
						std::string flex = player && player->is_adlib()
												   ? MAINMUS_AD
												   : MAINMUS;

						std::unique_ptr<IDataSource> mid_data
								= open_music_flex(flex, song);
						int convert = XMIDIFILE_CONVERT_NOCONVERSION;
						std::string_view driver_name{};

						if (player) {
							convert     = player->setup_timbre_for_track(flex);
							driver_name = player->get_actual_midi_driver_name();
						}

						if (mid_data && mid_data->good()) {
							XMidiFile midfile(
									mid_data.get(), convert, driver_name);
							for (int i = 0; i < midfile.number_of_tracks();
								 i++) {
								std::cout << " track " << i << std::endl;
								midfile.GetEventList(i)->events->DumpText(
										std::cout);
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
						if ((event.key.keysym.mod & SDL_KMOD_ALT)
							&& (event.key.keysym.mod & SDL_KMOD_CTRL)) {
							make_screenshot(true);
						} else {
							audio->stop_music();
						}
						break;
					case SDLK_UP:
						active = (active + 2) % 3;
						break;
					case SDLK_DOWN:
						active = (active + 1) % 3;
						break;
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
	Mouse::mouse->show();
	gwin->show();
}

size_t SoundTester::formattime(
		char* buffer, size_t buffersize, uint32 position, uint32 length) {
	int seconds = position / 1000;

	size_t count = snprintf(
			buffer, buffersize, "%01d:%02d.%03d", seconds / 60, seconds % 60,
			position % 1000);
	buffersize -= count;
	buffer += count;
	if (buffersize > 0) {
		seconds = length / 1000;

		count += snprintf(
				buffer, buffersize, " / %01d:%02d.%03d", seconds / 60,
				seconds % 60, length % 1000);
	}
	return count;
}
