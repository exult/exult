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
#	include <config.h>
#endif

#include "playfli.h"

#include "databuf.h"
#include "endianio.h"
#include "exceptions.h"
#include "gamewin.h"
#include "palette.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

using std::cout;
using std::endl;
using std::ifstream;
using std::size_t;

void playfli::initfli() {
	size_t pos = fli_data.getPos();
	fli_data.skip(4);
	fli_magic = fli_data.read2();
	fli_data.seek(pos);
	if (fli_magic != 0xaf11 && fli_magic != 0xaf12) {
		// This is a U7-style flic with a 8-byte name prefixed.
		fli_data.read(fli_name, 8);
	}
	fli_size  = fli_data.read4();
	fli_magic = fli_data.read2();
	if (fli_magic != 0xaf11 && fli_magic != 0xaf12) {
		throw exult_exception("Not a valid FLI file");
	}
	fli_frames = fli_data.read2();
	fli_width  = fli_data.read2();
	fli_height = fli_data.read2();
	fli_depth  = fli_data.read2();
	fli_flags  = fli_data.read2();
	fli_speed  = fli_data.read2();
	fli_data.skip(110);
	streampos = streamstart = fli_data.getPos();
	frame                   = 0;
	thispal                 = -1;
	nextpal                 = 0;
	changepal               = false;
}

void playfli::info(fliinfo* fi) const {
#ifdef DEBUG
	if (fli_name[0] == 0) {
		cout << "Flic name :   (none)" << endl;
	} else {
		cout << "Flic name :   " << fli_name << endl;
	}
	cout << "Frame count : " << fli_frames << endl;
	cout << "Width :       " << fli_width << endl;
	cout << "Height :      " << fli_height << endl;
	cout << "Depth :       " << fli_depth << endl;
	cout << "Speed :       " << fli_speed << endl;
#endif
	if (fi != nullptr) {
		fi->frames = fli_frames;
		fi->width  = fli_width;
		fi->height = fli_height;
		fi->depth  = fli_depth;
		fi->speed  = fli_speed;
	}
}

int playfli::play(
		Image_window* win, int first_frame, int last_frame, unsigned long ticks,
		int brightness) {
	const int xoffset   = (win->get_game_width() - fli_width) / 2;
	const int yoffset   = (win->get_game_height() - fli_height) / 2;
	bool      dont_show = false;

	if (!fli_buf) {
		fli_buf = win->create_buffer(fli_width, fli_height);
	}

	// Set up last frame
	if (first_frame == last_frame) {
		dont_show = true;
	}
	if (first_frame < 0) {
		first_frame += fli_frames;
	}
	if (last_frame < 0) {
		last_frame += 1 + fli_frames;
	}
	if (first_frame == last_frame) {
		last_frame++;
	}
	if (last_frame < 0 || last_frame > fli_frames) {
		last_frame = fli_frames;
	}

	if (ticks == 0u) {
		ticks = SDL_GetTicks();
	}

	if (first_frame < frame) {
		nextpal   = 0;
		frame     = 0;
		streampos = streamstart;
	}

	std::vector<uint8> pixbuf(fli_width);

	if (brightness != palette->get_brightness()) {
		palette->set_brightness(brightness);
		changepal = true;
	}

	auto read_palette = [this]() {
		const size_t packets = fli_data.read2();

		std::array<unsigned char, 768> colors{};

		auto* current = colors.data();

		for (size_t p_count = 0; p_count < packets; p_count++) {
			const size_t skip = fli_data.read1();

			current += skip;
			size_t change = fli_data.read1();

			if (change == 0) {
				change = 256;
			}
			fli_data.read(current, change * 3);
		}
		return colors;
	};

	auto update_palette = [this](const std::array<unsigned char, 768>& colors) {
		palette->set_palette(colors.data());
		if (thispal != nextpal) {
			thispal   = nextpal;
			changepal = true;
		}
		nextpal++;
	};

	// Play frames...
	for (; frame < last_frame; frame++) {
		fli_data.seek(streampos);
		const int frame_size = fli_data.read4();
		// Skip frame_magic
		fli_data.skip(2);
		const int frame_chunks = fli_data.read2();
		fli_data.skip(8);
		for (int chunk = 0; chunk < frame_chunks; chunk++) {
			// Skip chunk_size
			fli_data.skip(4);
			const auto chunk_type = static_cast<FlicChunks>(fli_data.read2());

			switch (chunk_type) {
			case FLI_COLOR256: {
				auto colors = read_palette();
				for (auto& color : colors) {
					color /= 4;
				}
				update_palette(colors);
				break;
			}

			case FLI_COLOR: {
				auto colors = read_palette();
				update_palette(colors);
				break;
			}

			case FLI_LC: {
				const int skip_lines   = fli_data.read2();
				const int change_lines = fli_data.read2();
				for (int line = 0; line < change_lines; line++) {
					const int packets = fli_data.read1();
					int       pix_pos = 0;
					for (int p_count = 0; p_count < packets; p_count++) {
						const int skip_count = fli_data.read1();
						pix_pos += skip_count;
						const int size_count
								= static_cast<sint8>(fli_data.read1());
						if (size_count < 0) {
							const int  pix_count = std::abs(size_count);
							const auto data
									= static_cast<uint8>(fli_data.read1());
							fli_buf->fill_hline8(
									data, pix_count, pix_pos,
									skip_lines + line);
							pix_pos += pix_count;
						} else {
							fli_data.read(pixbuf.data(), size_count);
							fli_buf->copy_hline8(
									pixbuf.data(), size_count, pix_pos,
									skip_lines + line);
							pix_pos += size_count;
						}
					}
				}
				break;
			}

			case FLI_BLACK:
				fli_buf->fill8(0);
				break;

			case FLI_BRUN: {
				for (int line = 0; line < fli_height; line++) {
					const int packets = fli_data.read1();
					auto*     pix_pos = pixbuf.data();
					for (int p_count = 0; p_count < packets; p_count++) {
						const int size_count
								= static_cast<sint8>(fli_data.read1());
						if (size_count > 0) {
							const auto data
									= static_cast<uint8>(fli_data.read1());
							pix_pos = std::fill_n(pix_pos, size_count, data);
						} else {
							const int pix_count = std::abs(size_count);
							fli_data.read(pix_pos, pix_count);
							pix_pos += pix_count;
						}
					}
					fli_buf->copy_hline8(pixbuf.data(), fli_width, 0, line);
				}
				break;
			}

			case FLI_COPY: {
				for (int line = 0; line < fli_height; line++) {
					fli_data.read(pixbuf.data(), fli_width);
					fli_buf->copy_hline8(pixbuf.data(), fli_width, 0, line);
				}
				break;
			}

			case FLI_SS2: {
				const int change_lines = fli_data.read2();
				for (int line = 0; line < change_lines; line++) {
					int packets = fli_data.read2();
					while ((packets & 0x8000U) != 0U) {
						// flag word
						if ((packets & 0x4000U) != 0U) {
							// skip lines
							line += std::abs(static_cast<sint16>(packets));
						} else {
							// Set last pixel of current line (used if line
							// width is odd)
							fli_buf->put_pixel8(
									packets & 0xff, fli_width - 1, line);
						}
						packets = fli_data.read2();
					}
					int pix_pos = 0;
					for (int p_count = 0; p_count < packets; p_count++) {
						const int skip_count = fli_data.read1();
						pix_pos += skip_count;
						const int size_count
								= static_cast<sint8>(fli_data.read1());
						if (size_count < 0) {
							const uint16 data       = fli_data.read2();
							auto*        pixptr     = pixbuf.data();
							const int    word_count = std::abs(size_count);
							for (int i = 0; i < word_count; i++) {
								little_endian::Write2(pixptr, data);
							}
							fli_buf->copy_hline8(
									pixbuf.data(), 2 * word_count, pix_pos,
									line);
							pix_pos += 2 * word_count;
						} else {
							fli_data.read(pixbuf.data(), 2 * size_count);
							fli_buf->copy_hline8(
									pixbuf.data(), 2 * size_count, pix_pos,
									line);
							pix_pos += 2 * size_count;
						}
					}
				}
				break;
			}

			case FLI_PSTAMP: {
				std::cerr << "FLIC ERROR: Postage Stamp Image not supported "
							 "(chunk type FLI_PSTAMP == 18)"
						  << endl;
				size_t skip = fli_data.read4();
				fli_data.skip(skip - 4);
				break;
			}

			default:
				std::cerr << "FLIC ERROR: Invalid chunk type: "
						  << static_cast<int>(chunk_type) << endl;
				break;
			}
		}

		streampos += frame_size;

		if (changepal) {
			palette->apply(false);
		}
		changepal = false;

		if (frame < first_frame) {
			continue;
		}

		// Speed related frame skipping detection
		const bool skip_frame
				= Game_window::get_instance()->get_frame_skipping()
				  && SDL_GetTicks() >= ticks;

		win->put(fli_buf.get(), xoffset, yoffset);

		if (ticks > SDL_GetTicks()) {
			SDL_Delay(ticks - SDL_GetTicks());
		}

		ticks += fli_speed * 10;

		if (!dont_show && !skip_frame) {
			win->show();
		}
	}

	return ticks;
}

void playfli::put_buffer(Image_window* win) const {
	const int xoffset = (win->get_game_width() - fli_width) / 2;
	const int yoffset = (win->get_game_height() - fli_height) / 2;

	win->put(fli_buf.get(), xoffset, yoffset);
}
