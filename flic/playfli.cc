/**
 ** playfli.cc - Play Autodesk Animator FLIs
 **
 ** Written: 5/5/2000 - TST
 **/

/*
Copyright (C) 2000  Tristan Tarrant
Copyright (C) 2000-2022  The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <cstring>
#include <iostream>
#include "databuf.h"
#include "playfli.h"
#include "utils.h"
#include "gamewin.h"
#include "palette.h"

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#endif    // __GNUC__
#include <SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

using std::ifstream;
using std::memset;
using std::size_t;
using std::cout;
using std::endl;

void playfli::initfli() {
	fli_data.read(fli_name, 8);
	fli_size = fli_data.read4();
	fli_magic = fli_data.read2();
	fli_frames = fli_data.read2();
	fli_width = fli_data.read2();
	fli_height = fli_data.read2();
	fli_depth = fli_data.read2();
	fli_flags = fli_data.read2();
	fli_speed = fli_data.read2();
	fli_data.skip(110);
	streampos = streamstart = fli_data.getPos();
	frame = 0;
	thispal = -1;
	nextpal = 0;
	changepal = false;
}


void playfli::info(fliinfo *fi) {
#ifdef DEBUG
	cout << "Flic name :   " << fli_name << endl;
	cout << "Frame count : " << fli_frames << endl;
	cout << "Width :       " << fli_width << endl;
	cout << "Height :      " << fli_height << endl;
	cout << "Depth :       " << fli_depth << endl;
	cout << "Speed :       " << fli_speed << endl;
#endif
	if (fi) {
		fi->frames = fli_frames;
		fi->width = fli_width;
		fi->height = fli_height;
		fi->depth = fli_depth;
		fi->speed = fli_speed;
	}
}

int playfli::play(Image_window *win, int first_frame, int last_frame, unsigned long ticks, int brightness) {
	int xoffset = (win->get_game_width() - fli_width) / 2;
	int yoffset = (win->get_game_height() - fli_height) / 2;
	bool dont_show = false;

	if (!fli_buf) {
		fli_buf = win->create_buffer(fli_width, fli_height);
	}

	// Set up last frame
	if (first_frame == last_frame) dont_show = true;
	if (first_frame < 0) first_frame += fli_frames;
	if (last_frame < 0) last_frame += 1 + fli_frames;
	if (first_frame == last_frame) last_frame++;
	if (last_frame < 0 || last_frame > fli_frames) last_frame = fli_frames;

	if (!ticks) ticks = SDL_GetTicks();

	if (first_frame < frame) {
		nextpal = 0;
		frame = 0;
		streampos = streamstart;
	}
	auto *pixbuf = new uint8[fli_width];

	if (brightness != palette->get_brightness()) {
		palette->set_brightness(brightness);
		changepal = true;
	}

	// Play frames...
	for (; frame < last_frame; frame++) {
		fli_data.seek(streampos);
		int frame_size = fli_data.read4();
		//frame_magic = fli_data.read2();
		fli_data.skip(2);
		int frame_chunks = fli_data.read2();
		fli_data.skip(8);
		for (int chunk = 0; chunk < frame_chunks; chunk++) {
			//chunk_size = fli_data.read4();
			fli_data.skip(4);
			int chunk_type = fli_data.read2();

			switch (chunk_type) {
			case 11: {
				int packets = fli_data.read2();
				unsigned char colors[3 * 256];

				memset(colors, 0, 3 * 256);
				int current = 0;

				for (int p_count = 0; p_count < packets;
				        p_count++) {
					int skip = fli_data.read1();

					current += skip;
					int change = fli_data.read1();

					if (change == 0)
						change = 256;
					fli_data.read(reinterpret_cast<char *>(&colors[current * 3]), change * 3);
				}
				// Set palette
				palette->set_palette(colors);
				if (thispal != nextpal) {
					thispal = nextpal;
					changepal = true;
				}
				nextpal++;

			}
			break;

			case 12: {

				int skip_lines = fli_data.read2();
				int change_lines = fli_data.read2();
				for (int line = 0; line < change_lines; line++) {
					int packets = fli_data.read1();
					int pixpos = 0;
					for (int p_count = 0; p_count < packets;
					        p_count++) {
						int skip_count = fli_data.read1();
						pixpos += skip_count;
						sint8 size_count = fli_data.read1();
						if (size_count < 0) {
							size_count = -size_count;
							uint8 data = fli_data.read1();
							memset(pixbuf, data, size_count);
							fli_buf->copy8(pixbuf, size_count, 1,
								               pixpos, skip_lines + line);
							pixpos += size_count;

						} else {
							fli_data.read(pixbuf, size_count);
							fli_buf->copy8(pixbuf, size_count, 1,
								               pixpos, skip_lines + line);
							pixpos += size_count;
						}
					}
				}

			}
			break;

			case 13:
				break;

			case 15: {
				for (int line = 0; line < fli_height; line++) {
					int packets = fli_data.read1();
					int pixpos = 0;
					for (int p_count = 0; p_count < packets;
					        p_count++) {
						sint8 size_count = fli_data.read1();
						if (size_count > 0) {
							uint8 data = fli_data.read1();
							memset(&pixbuf[pixpos], data, size_count);
							pixpos += size_count;
						} else {
							fli_data.read(&pixbuf[pixpos],
							               -size_count);
							pixpos -= size_count;
						}
					}
					fli_buf->copy8(pixbuf, fli_width, 1, 0, line);
				}
			}
			break;

			case 16:
				fli_data.skip(fli_width * fli_height);
				break;

			default:
				cout << "UNKNOWN FLIC FRAME" << endl;
				break;
			}
		}

		streampos += frame_size;

		if (changepal) palette->apply(false);
		changepal = false;

		if (frame < first_frame)
			continue;

		// Speed related frame skipping detection
		bool skip_frame = Game_window::get_instance()->get_frame_skipping() && SDL_GetTicks() >= ticks;

		win->put(fli_buf.get(), xoffset, yoffset);

		if (ticks > SDL_GetTicks()) SDL_Delay(ticks - SDL_GetTicks());

		ticks += fli_speed * 10;

		if (!dont_show && !skip_frame) {
			win->show();
		}
	}

	delete[] pixbuf;

	return ticks;
}

void playfli::put_buffer(Image_window *win) {
	int xoffset = (win->get_game_width() - fli_width) / 2;
	int yoffset = (win->get_game_height() - fli_height) / 2;

	win->put(fli_buf.get(), xoffset, yoffset);
}
