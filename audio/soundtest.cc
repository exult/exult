/*
 *  Copyright (C) 2000-2022 The Exult Team
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
#include <SDL.h>
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
#include "mouse.h"
#include "soundtest.h"
#include "tqueue.h"

void SoundTester::test_sound() {
	Game_window*   gwin = Game_window::get_instance();
	Image_buffer8* ibuf = gwin->get_win()->get_ib8();
	Font*          font = Shape_manager::get_instance()->get_font(4);

	Audio* audio = Audio::get_ptr();

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

	Mouse::mouse->hide();

	gwin->get_tqueue()->pause(SDL_GetTicks());

	int line;
	do {
		if (redraw) {
			Scroll_gump scroll;
			scroll.add_text(" ~");
			scroll.paint();

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
					buf, sizeof(buf), "%2s Music %c %3i %c %s",
					active == 0 ? "->" : "", active == 0 ? '<' : ' ', song,
					active == 0 ? '>' : ' ', repeat ? "- Repeat" : "");
			line += height * 2;
			font->paint_text_fixedwidth(ibuf, buf, left, line, width);

			snprintf(
					buf, sizeof(buf), "%2s SFX   %c %3i %c",
					active == 1 ? "->" : "", active == 1 ? '<' : ' ', sfx,
					active == 1 ? '>' : ' ');

			line += height * 2;
			font->paint_text_fixedwidth(ibuf, buf, left, line, width);

			snprintf(
					buf, sizeof(buf), "%2s Voice %c %3i %c",
					active == 2 ? "->" : "", active == 2 ? '<' : ' ', voice,
					active == 2 ? '>' : ' ');

			line += height * 2;
			font->paint_text_fixedwidth(ibuf, buf, left, line, width);

			gwin->show();
			redraw = false;
		}
		SDL_WaitEvent(&event);
		if (event.type == SDL_KEYDOWN) {
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
					audio->play_sound_effect(sfx);
				} else if (active == 2) {
					audio->stop_speech();
					audio->start_speech(voice, false);
				}
				break;
#ifdef DEBUG
			case SDLK_d: {
				MyMidiPlayer* player = Audio::get_ptr()->get_midi();
				std::string   flex
						= player && player->is_adlib() ? MAINMUS_AD : MAINMUS;

				std::unique_ptr<IDataSource> mid_data
						= open_music_flex(flex, song);
				int convert = XMIDIFILE_CONVERT_NOCONVERSION;
				if (player) {
					convert = player->setup_timbre_for_track(flex);
				}

				if (mid_data && mid_data->good()) {
					XMidiFile midfile(mid_data.get(), convert);
					for (int i = 0; i < midfile.number_of_tracks(); i++) {
						std::cout << " track " << i << std::endl;
						midfile.GetEventList(i)->events->DumpText(std::cout);
					}
				}
			} break;
#endif
			case SDLK_r:
				repeat = !repeat;
				break;
			case SDLK_s:
				if ((event.key.keysym.mod & KMOD_ALT)
					&& (event.key.keysym.mod & KMOD_CTRL)) {
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
	} while (looping);

	gwin->get_tqueue()->resume(SDL_GetTicks());

	gwin->paint();
	Mouse::mouse->show();
	gwin->show();
}
