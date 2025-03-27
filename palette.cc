/*
 *  Copyright (C) 2000-2022  The Exult Team
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

#include "palette.h"

#include "exceptions.h"
#include "files/U7file.h"
#include "fnames.h"
#include "game.h"
#include "gameclk.h"
#include "gamewin.h"
#include "ibuf8.h"
#include "ignore_unused_variable_warning.h"
#include "utils.h"

#include <algorithm>
#include <cstring>

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

using std::size_t;
using std::string;

unsigned char Palette::border[3] = {0, 0, 0};

Palette::Palette()
		: win(Game_window::get_instance()->get_win()), palette(-1),
		  brightness(100), max_val(63), border255(false), faded_out(false),
		  fades_enabled(true) {
	memset(pal1, 0, 768);
	memset(pal2, 0, 768);
}

Palette::Palette(Palette* pal)
		: win(Game_window::get_instance()->get_win()), max_val(63) {
	take(pal);
}

void Palette::take(Palette* pal) {
	palette       = pal->palette;
	brightness    = pal->brightness;
	faded_out     = pal->faded_out;
	fades_enabled = pal->fades_enabled;
	memcpy(pal1, pal->pal1, 768);
	memcpy(pal2, pal->pal2, 768);
}

/*
 *  Fade the current palette in or out.
 *  Note:  If pal_num != -1, the current palette is set to it.
 */

void Palette::fade(
		int cycles,    // Length of fade.
		int inout,     // 1 to fade in, 0 to fade to black.
		int pal_num    // 0-11, or -1 for current.
) {
	if (pal_num == -1) {
		pal_num = palette;
	}
	palette = pal_num;

	border255 = (palette >= 0 && palette <= 12) && palette != 9;

	load(PALETTES_FLX, PATCH_PALETTES, pal_num);
	if (inout) {
		fade_in(cycles);
	} else {
		fade_out(cycles);
	}
	faded_out = !inout;    // Be sure to set flag.
}

/*
 *  Flash the current palette red.
 */

void Palette::flash_red() {
	const int savepal = palette;
	set(PALETTE_RED);    // Palette 8 is the red one.
	win->show();
	SDL_Delay(100);
	set(savepal);
	Game_window::get_instance()->set_painted();
}

/*
 *  Read in a palette.
 */

void Palette::set(
		int  pal_num,           // 0-11, or -1 to leave unchanged.
		int  new_brightness,    // New percentage, or -1.
		bool repaint) {
	border255 = (pal_num >= 0 && pal_num <= 12) && pal_num != 9;

	if ((palette == pal_num || pal_num == -1)
		&& (brightness == new_brightness || new_brightness == -1)) {
		// Already set.
		return;
	}
	if (pal_num != -1) {
		palette = pal_num;    // Store #.
	}
	if (new_brightness > 0) {
		brightness = new_brightness;
	}
	if (faded_out) {
		return;    // In the black.
	}

	// could throw!
	load(PALETTES_FLX, PATCH_PALETTES, palette);
	set_brightness(brightness);
	apply(repaint);
}

/*
 *  Read in a palette.
 */

void Palette::set(
		const unsigned char palnew[768],
		int                 new_brightness,    // New percentage, or -1.
		bool repaint, bool border255) {
	this->border255 = border255;
	memcpy(pal1, palnew, 768);
	memset(pal2, 0, 768);
	palette = -1;
	if (new_brightness > 0) {
		brightness = new_brightness;
	}
	if (faded_out) {
		return;    // In the black.
	}

	set_brightness(brightness);
	apply(repaint);
}

void Palette::apply(bool repaint) {
	const uint8 r = pal1[255 * 3 + 0];
	const uint8 g = pal1[255 * 3 + 1];
	const uint8 b = pal1[255 * 3 + 2];

	if (border255) {
		pal1[255 * 3 + 0] = border[0] * 63 / 255;
		pal1[255 * 3 + 1] = border[1] * 63 / 255;
		pal1[255 * 3 + 2] = border[2] * 63 / 255;
	}

	win->set_palette(pal1, max_val, brightness);

	pal1[255 * 3 + 0] = r;
	pal1[255 * 3 + 1] = g;
	pal1[255 * 3 + 2] = b;

	if (!repaint) {
		return;
	}
	win->show();
}

/**
 *  Loads and a xform table and sets palette from a buffer.
 *  @param buf  What to base palette on.
 *  @param xfname   xform file name.
 *  @param xindex   xform index.
 */
void Palette::loadxform(
		const unsigned char* buf, const char* xfname, int& xindex) {
	const U7object xform(xfname, xindex);
	size_t         xlen;
	auto           xbuf = xform.retrieve(xlen);
	if (!xbuf || xlen <= 0) {
		xindex = -1;
	} else {
		for (int i = 0; i < 256; i++) {
			const int ix    = xbuf[i];
			pal1[3 * i]     = buf[3 * ix];
			pal1[3 * i + 1] = buf[3 * ix + 1];
			pal1[3 * i + 2] = buf[3 * ix + 2];
		}
	}
}

/**
 *  Actually loads and sets the palette and xform table.
 *  @param pal  What is being loaded.
 *  @param xfname   xform file name.
 *  @param xindex   xform index.
 */
void Palette::set_loaded(
		const U7multiobject& pal, const char* xfname, int xindex) {
	size_t               len;
	auto                 xfbuf = pal.retrieve(len);
	const unsigned char* buf   = xfbuf.get();
	if (len == 768) {
		// Simple palette
		if (xindex >= 0) {
			// Get xform table.
			loadxform(buf, xfname, xindex);
		}

		if (xindex < 0) {    // Set the first palette
			memcpy(pal1, buf, 768);
		}
		// The second one is black.
		memset(pal2, 0, sizeof(pal2));
	} else if (buf && len >= 1536) {
		// Double palette
		for (int i = 0; i < 768; i++) {
			pal1[i] = buf[i * 2];
			pal2[i] = buf[i * 2 + 1];
		}
	} else {
		// Something went wrong during palette load. This probably
		// happens because a dev is being used, which means that
		// the palette won't be loaded.
		// For now, let's try to avoid overwriting any palette that
		// may be loaded and just cleanup.
		return;
	}
}

/**
 *  Loads a palette from the given spec. Optionally loads a
 *  xform from the desired file.
 *  @param fname0   Specification of pallete to load.
 *  @param index    Index of the palette.
 *  @param xfname   Optional xform file name.
 *  @param xindex   Optional xform index.
 */
void Palette::load(
		const File_spec& fname0, int index, const char* xfname, int xindex) {
	const U7multiobject pal(fname0, index);
	set_loaded(pal, xfname, xindex);
}

/**
 *  Loads a palette from the given spec. Optionally loads a
 *  xform from the desired file.
 *  @param fname0   Specification of first pallete to load (likely <STATIC>).
 *  @param fname1   Specification of second pallete to load (likely <PATCH>).
 *  @param index    Index of the palette.
 *  @param xfname   Optional xform file name.
 *  @param xindex   Optional xform index.
 */
void Palette::load(
		const File_spec& fname0, const File_spec& fname1, int index,
		const char* xfname, int xindex) {
	const U7multiobject pal(fname0, fname1, index);
	set_loaded(pal, xfname, xindex);
}

/**
 *  Loads a palette from the given spec. Optionally loads a
 *  xform from the desired file.
 *  @param fname0   Specification of first pallete to load (likely <STATIC>).
 *  @param fname1   Specification of second pallete to load.
 *  @param fname2   Specification of third pallete to load (likely <PATCH>).
 *  @param index    Index of the palette.
 *  @param xfname   Optional xform file name.
 *  @param xindex   Optional xform index.
 */
void Palette::load(
		const File_spec& fname0, const File_spec& fname1,
		const File_spec& fname2, int index, const char* xfname, int xindex) {
	const U7multiobject pal(fname0, fname1, fname2, index);
	set_loaded(pal, xfname, xindex);
}

void Palette::set_brightness(int bright) {
	brightness = bright;
}

void Palette::fade_in(int cycles) {
	if (cycles && fades_enabled) {
		unsigned char fade_pal[768];
		unsigned int  ticks = SDL_GetTicks() + 20;
		for (int i = 0; i <= cycles; i++) {
			const uint8 r = pal1[255 * 3 + 0];
			const uint8 g = pal1[255 * 3 + 1];
			const uint8 b = pal1[255 * 3 + 2];

			if (border255) {
				pal1[255 * 3 + 0] = border[0] * 63 / 255;
				pal1[255 * 3 + 1] = border[1] * 63 / 255;
				pal1[255 * 3 + 2] = border[2] * 63 / 255;
			}

			for (int c = 0; c < 768; c++) {
				fade_pal[c] = ((pal1[c] - pal2[c]) * i) / cycles + pal2[c];
			}

			pal1[255 * 3 + 0] = r;
			pal1[255 * 3 + 1] = g;
			pal1[255 * 3 + 2] = b;

			win->set_palette(fade_pal, max_val, brightness);

			// Frame skipping on slow systems
			if (i == cycles || ticks >= SDL_GetTicks()
				|| !Game_window::get_instance()->get_frame_skipping()) {
				win->show();
			}
			while (ticks >= SDL_GetTicks())
				;
			ticks += 20;
		}
	} else {
		const uint8 r = pal1[255 * 3 + 0];
		const uint8 g = pal1[255 * 3 + 1];
		const uint8 b = pal1[255 * 3 + 2];

		if ((palette >= 0 && palette <= 12) && palette != 9) {
			pal1[255 * 3 + 0] = border[0] * 63 / 255;
			pal1[255 * 3 + 1] = border[1] * 63 / 255;
			pal1[255 * 3 + 2] = border[2] * 63 / 255;
		}

		win->set_palette(pal1, max_val, brightness);

		pal1[255 * 3 + 0] = r;
		pal1[255 * 3 + 1] = g;
		pal1[255 * 3 + 2] = b;

		win->show();
	}
}

void Palette::fade_out(int cycles) {
	faded_out = true;    // Be sure to set flag.
	if (cycles && fades_enabled) {
		unsigned char fade_pal[768];
		unsigned int  ticks = SDL_GetTicks() + 20;
		for (int i = cycles; i >= 0; i--) {
			const uint8 r = pal1[255 * 3 + 0];
			const uint8 g = pal1[255 * 3 + 1];
			const uint8 b = pal1[255 * 3 + 2];

			if (border255) {
				pal1[255 * 3 + 0] = border[0] * 63 / 255;
				pal1[255 * 3 + 1] = border[1] * 63 / 255;
				pal1[255 * 3 + 2] = border[2] * 63 / 255;
			}

			for (int c = 0; c < 768; c++) {
				fade_pal[c] = ((pal1[c] - pal2[c]) * i) / cycles + pal2[c];
			}

			pal1[255 * 3 + 0] = r;
			pal1[255 * 3 + 1] = g;
			pal1[255 * 3 + 2] = b;

			win->set_palette(fade_pal, max_val, brightness);
			// Frame skipping on slow systems
			if (i == 0 || ticks >= SDL_GetTicks()
				|| !Game_window::get_instance()->get_frame_skipping()) {
				win->show();
			}
			while (ticks >= SDL_GetTicks())
				;
			ticks += 20;
		}
	} else {
		win->set_palette(pal2, max_val, brightness);
		win->show();
	}
	// Messes up sleep.          win->set_palette(pal1, max_val, brightness);
}

//	Find index (0-255) of closest color (r,g,b < 64).
int Palette::find_color(int r, int g, int b, int last) const {
	int  best_index    = -1;
	long best_distance = 0xfffffff;
	// But don't search rotating colors.
	for (int i = 0; i < last; i++) {
		// Get deltas.
		const long dr = r - pal1[3 * i];
		const long dg = g - pal1[3 * i + 1];
		const long db = b - pal1[3 * i + 2];
		// Figure distance-squared.
		const long dist = dr * dr + dg * dg + db * db;
		if (dist < best_distance) {    // Better than prev?
			best_index    = i;
			best_distance = dist;
		}
	}
	return best_index;
}

/*
 *  Creates a translation table between two palettes.
 */

void Palette::create_palette_map(const Palette* to, unsigned char* buf) const {
	// Assume buf has 256 elements
	for (int i = 0; i < 256; i++) {
		buf[i] = to->find_color(
				pal1[3 * i], pal1[3 * i + 1], pal1[3 * i + 2], 256);
	}
}

/*
 *  Creates a palette in-between two palettes.
 */

std::unique_ptr<Palette> Palette::create_intermediate(
		const Palette& to, int nsteps, int pos) const {
	auto newpal = std::make_unique<Palette>();
	if (fades_enabled) {
		for (int c = 0; c < 768; c++) {
			newpal->pal1[c] = ((to.pal1[c] - pal1[c]) * pos) / nsteps + pal1[c];
		}
	} else {
		const unsigned char* palold;
		if (2 * pos >= nsteps) {
			palold = to.pal1;
		} else {
			palold = pal1;
		}
		memcpy(newpal->pal1, palold, 768);
	}

	// Reset palette data and set.
	memset(newpal->pal2, 0, 768);
	newpal->border255 = true;
	newpal->apply(true);
	return newpal;
}

/*
 *  Create a translucency table for this palette seen through a given
 *  color.  (Based on a www.gamedev.net article by Jesse Towner.)
 */

void Palette::create_trans_table(
		// Color to blend with:
		unsigned char br, unsigned bg, unsigned bb,
		int            alpha,    // 0-255, applied to 'blend' color.
		unsigned char* table     // 256 indices are stored here.
) const {
	for (int i = 0; i < 256; i++) {
		const int newr
				= (static_cast<int>(br) * alpha) / 255
				  + (static_cast<int>(pal1[i * 3]) * (255 - alpha)) / 255;
		const int newg
				= (static_cast<int>(bg) * alpha) / 255
				  + (static_cast<int>(pal1[i * 3 + 1]) * (255 - alpha)) / 255;
		const int newb
				= (static_cast<int>(bb) * alpha) / 255
				  + (static_cast<int>(pal1[i * 3 + 2]) * (255 - alpha)) / 255;
		table[i] = find_color(newr, newg, newb);
	}
}

void Palette::show() {
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			win->fill8(y * 16 + x, 8, 8, x * 8, y * 8);
		}
	}
}

void Palette::set_color(int nr, int r, int g, int b) {
	pal1[nr * 3]     = r;
	pal1[nr * 3 + 1] = g;
	pal1[nr * 3 + 2] = b;
}

void Palette::set_palette(const unsigned char palnew[768]) {
	memcpy(pal1, palnew, 768);
	memset(pal2, 0, 768);
}

const Palette::Ramp* Palette::get_ramps(unsigned int& num_ramps) {
	static Ramp ramps[32] = {
			{0, 0}
    };
	static uint32 rampgame = 0;

	uint32 currentgame = Game::Get_unique_gamecode();

	// it is zeroed or game changed  so need to scan for ramps
	if (ramps[0].start == 0 || rampgame != currentgame) {
		// If palette is not 0, load palette 0 and use that instead
		if (palette != 0) {
			Palette palette0;
			try {
				palette0.load(PALETTES_FLX, PATCH_PALETTES, 0);
				palette0.palette = 0;
				return palette0.get_ramps(num_ramps);
			} catch (...) {
				num_ramps = 0;
				return ramps;
			}
		}
		unsigned int r    = 0;
		int          last = pal1[3] + pal1[4] + pal1[5];
		ramps[0].start    = 1;
		// Where the palette sycling ramps start
		uint8 palette_cycling[] = {
				0xe0, 0xe8, 0xf0, 0xf4, 0xf8, 0xfc, 0xff,
		};

		for (unsigned int c = 2; c < 256; c++) {
			int brightness = pal1[c * 3] + pal1[c * 3 + 1] + pal1[c * 3 + 2];

			// Big change in brightness means start of a ramp (note 6 bit colour
			// so 48 is equiv to 192 in 8 bit)
			// Special handling for palette cycling ranges so they behave better
			if ((c < palette_cycling[0] && std::abs(brightness - last) > 48)
				|| c
						   == *std::find(
								   palette_cycling,
								   palette_cycling + std::size(palette_cycling)
										   - 1,
								   c)) {
				// set the end point of the previous ramp
				ramps[r].end = c - 1;
				r++;
				// too many ramps
				if (r >= std::size(ramps)) {
					break;
				}
				// Start of a ramp
				ramps[r].start = c;
			}
			last = brightness;
		}
		if (r > 0) {
			if (r < std::size(ramps)) {
				num_ramps    = r + 1;
				ramps[r].end = 255;
			} else {
				num_ramps = r;
			}
		} else {
			ramps[0].start = 0;
			num_ramps      = 0;
		}

		rampgame = currentgame;
	} else {
		unsigned num = 0;
		while (ramps[num].start != 0 && ramps[num].end != 0) {
			num++;
		}
		num_ramps = num;
	}
	// terminator
	ramps[std::size(ramps) - 1].start = 0;
	return &ramps[0];
}

int Palette::get_ramp_for_index(uint8 colindex) {
	unsigned int num_ramps = 0;
	auto         ramps     = get_ramps(num_ramps);

	for (unsigned int r = 0; r < num_ramps; r++) {
		if (ramps[r].start <= colindex && ramps[r].end >= colindex) {
			return r;
		}
	}

	return -1;
}

uint8 Palette::remap_colour_to_ramp(uint8 colindex, unsigned int newramp) {
	unsigned int num_ramps = 0;
	auto         ramps     = get_ramps(num_ramps);

	if (colindex == 0) {
		return 0;
	}
	// New ramp is out of range, do nothing
	if (newramp > num_ramps) {
		return colindex;
	}

	// Find the ramp the existing colour is in and what offset it has in that
	// ramp
	int offset = 0;
	for (unsigned int r = 0; r < num_ramps; r++) {
		if (ramps[r].start <= colindex && ramps[r].end >= colindex) {
			offset = ((colindex - ramps[r].start) * 256)
					 / (ramps[r].end - ramps[r].start);
			break;
		}
	}

	int rampsize = ramps[newramp].end - ramps[newramp].start;
	// New ramp is too small, just return the centre colour index
	int newoffset = (offset * rampsize) / 256;
	return ramps[newramp].start + newoffset;
}

void Palette::Generate_remap_xformtable(uint8 table[256], const int* remaps) {
	unsigned int num_ramps = 0;
	auto         ramps     = get_ramps(num_ramps);

	// First set table to do nothing
	for (int i = 0; i < 256; i++) {
		table[i] = i;
	}

	for (unsigned int r = 0; remaps[r] >= 0; r++) {
		unsigned int to = remaps[r];
		// Do nothing is invalid ramp or remap to self
		if (to > num_ramps || to == r) {
			continue;
		}
		auto fromramp = ramps[r];
		auto toramp   = ramps[to];
		int  fromsize = fromramp.end - fromramp.start;
		int  tosize   = toramp.end - toramp.start;
		if (fromramp.start) {
			for (std::uint_fast16_t c = fromramp.start; c <= fromramp.end;
				 c++) {
				int offset = fromsize
									 ? ((c - fromramp.start) * 256) / (fromsize)
									 : 0;

				table[c] = toramp.start + (offset * tosize) / 256;
			}
		}
	}
}

Palette_transition::Palette_transition(
		int from, int to, int ch, int cm, int ct, int r, int nsteps, int sh,
		int smin, int stick)
		: current(nullptr), step(0), max_steps(nsteps), start_hour(sh),
		  start_minute(smin), start_ticks(stick), rate(r) {
	start.load(PALETTES_FLX, PATCH_PALETTES, from);
	end.load(PALETTES_FLX, PATCH_PALETTES, to);
	set_step(ch, cm, ct);
}

Palette_transition::Palette_transition(
		Palette* from, int to, int ch, int cm, int ct, int r, int nsteps,
		int sh, int smin, int stick)
		: start(from), current(nullptr), step(0), max_steps(nsteps),
		  start_hour(sh), start_minute(smin), start_ticks(stick), rate(r) {
	end.load(PALETTES_FLX, PATCH_PALETTES, to);
	set_step(ch, cm, ct);
}

Palette_transition::Palette_transition(
		Palette* from, Palette* to, int ch, int cm, int ct, int r, int nsteps,
		int sh, int smin, int stick)
		: start(from), end(to), current(nullptr), step(0), max_steps(nsteps),
		  start_hour(sh), start_minute(smin), start_ticks(stick), rate(r) {
	set_step(ch, cm, ct);
}

bool Palette_transition::set_step(int hour, int min, int tick) {
	int       new_step = ticks_per_minute * (60 * hour + min) + tick;
	const int old_step
			= ticks_per_minute * (60 * start_hour + start_minute) + start_ticks;
	new_step -= old_step;
	while (new_step < 0) {
		new_step += 60 * ticks_per_minute;
	}
	new_step /= rate;

	Game_window* gwin = Game_window::get_instance();
	if (gwin->get_pal()->is_faded_out()) {
		return false;
	}

	if (!current || new_step != step) {
		step    = new_step;
		current = start.create_intermediate(end, max_steps, step);
	}

	if (current) {
		current->apply(true);
	}
	return step < max_steps;
}
