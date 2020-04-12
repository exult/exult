/*
 *  Copyright (C) 2000-2013  The Exult Team
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
#  include <config.h>
#endif

#include <cstring>

#include "files/U7file.h"
#include "palette.h"
#include "ibuf8.h"
#include "utils.h"
#include "fnames.h"
#include "gamewin.h"
#include "exceptions.h"
#include "ignore_unused_variable_warning.h"

#include "SDL_timer.h"

using std::memcpy;
using std::memset;
using std::size_t;
using std::string;

unsigned char Palette::border[3] = {
	0, 0, 0
};

Palette::Palette()
	: win(Game_window::get_instance()->get_win()),
	  palette(-1), brightness(100), max_val(63), border255(false),
	  faded_out(false), fades_enabled(true) {
	memset(pal1, 0, 768);
	memset(pal2, 0, 768);
}

Palette::Palette(Palette *pal)
	: win(Game_window::get_instance()->get_win()), max_val(63) {
	take(pal);
}

void Palette::take(Palette *pal) {
	palette = pal->palette;
	brightness = pal->brightness;
	faded_out = pal->faded_out;
	fades_enabled = pal->fades_enabled;
	memcpy(pal1, pal->pal1, 768);
	memcpy(pal2, pal->pal2, 768);
}

/*
 *  Fade the current palette in or out.
 *  Note:  If pal_num != -1, the current palette is set to it.
 */

void Palette::fade(
    int cycles,         // Length of fade.
    int inout,          // 1 to fade in, 0 to fade to black.
    int pal_num         // 0-11, or -1 for current.
) {
	if (pal_num == -1) pal_num = palette;
	palette = pal_num;

	border255 = (palette >= 0 && palette <= 12) && palette != 9;

	load(PALETTES_FLX, PATCH_PALETTES, pal_num);
	if (inout)
		fade_in(cycles);
	else
		fade_out(cycles);
	faded_out = !inout;     // Be sure to set flag.
}

/*
 *  Flash the current palette red.
 */

void Palette::flash_red(
) {
	int savepal = palette;
	set(PALETTE_RED);       // Palette 8 is the red one.
	win->show();
	SDL_Delay(100);
	set(savepal);
	Game_window::get_instance()->set_painted();
}

/*
 *  Read in a palette.
 */

void Palette::set(
    int pal_num,            // 0-11, or -1 to leave unchanged.
    int new_brightness,     // New percentage, or -1.
    bool repaint
) {
	border255 = (pal_num >= 0 && pal_num <= 12) && pal_num != 9;

	if ((palette == pal_num || pal_num == -1) &&
	        (brightness == new_brightness || new_brightness == -1))
		// Already set.
		return;
	if (pal_num != -1)
		palette = pal_num;  // Store #.
	if (new_brightness > 0)
		brightness = new_brightness;
	if (faded_out)
		return;         // In the black.

	// could throw!
	load(PALETTES_FLX, PATCH_PALETTES, palette);
	set_brightness(brightness);
	apply(repaint);
}

/*
 *  Read in a palette.
 */

void Palette::set(
    unsigned char palnew[768],
    int new_brightness,     // New percentage, or -1.
    bool repaint,
    bool border255
) {
	this->border255 = border255;
	memcpy(pal1, palnew, 768);
	memset(pal2, 0, 768);
	palette = -1;
	if (new_brightness > 0)
		brightness = new_brightness;
	if (faded_out)
		return;         // In the black.

	set_brightness(brightness);
	apply(repaint);
}

void Palette::apply(bool repaint) {
	uint8 r = pal1[255 * 3 + 0];
	uint8 g = pal1[255 * 3 + 1];
	uint8 b = pal1[255 * 3 + 2];

	if (border255) {
		pal1[255 * 3 + 0] = border[0] * 63 / 255;
		pal1[255 * 3 + 1] = border[1] * 63 / 255;
		pal1[255 * 3 + 2] = border[2] * 63 / 255;
	}

	win->set_palette(pal1, max_val, brightness);

	pal1[255 * 3 + 0] = r;
	pal1[255 * 3 + 1] = g;
	pal1[255 * 3 + 2] = b;

	if (!repaint)
		return;
	win->show();
}

/**
 *  Loads and a xform table and sets palette from a buffer.
 *  @param buf  What to base palette on.
 *  @param xfname   xform file name.
 *  @param xindex   xform index.
 */
void Palette::loadxform(const char *buf, const char *xfname, int &xindex) {
	U7object xform(xfname, xindex);
	size_t xlen;
	auto xbuf = xform.retrieve(xlen);
	if (!xbuf || xlen <= 0) {
		xindex = -1;
	} else {
		for (int i = 0; i < 256; i++) {
			int ix = xbuf[i];
			pal1[3 * i] = buf[3 * ix];
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
    const U7multiobject &pal,
    const char *xfname,
    int xindex
) {
	size_t len;
	auto xfbuf = pal.retrieve(len);
	const char *buf = reinterpret_cast<const char*>(xfbuf.get());
	if (len == 768) {
		// Simple palette
		if (xindex >= 0)
			// Get xform table.
			loadxform(buf, xfname, xindex);

		if (xindex < 0)     // Set the first palette
			memcpy(pal1, buf, 768);
		// The second one is black.
		memset(pal2, 0, 768);
	} else if (buf && len > 0) {
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
    const File_spec &fname0,
    int index,
    const char *xfname,
    int xindex
) {
	U7multiobject pal(fname0, index);
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
    const File_spec &fname0,
    const File_spec &fname1,
    int index,
    const char *xfname,
    int xindex
) {
	U7multiobject pal(fname0, fname1, index);
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
    const File_spec &fname0,
    const File_spec &fname1,
    const File_spec &fname2,
    int index,
    const char *xfname,
    int xindex
) {
	U7multiobject pal(fname0, fname1, fname2, index);
	set_loaded(pal, xfname, xindex);
}

void Palette::set_brightness(int bright) {
	brightness = bright;
}

void Palette::fade_in(int cycles) {
	if (cycles && fades_enabled) {
		unsigned char fade_pal[768];
		unsigned int ticks = SDL_GetTicks() + 20;
		for (int i = 0; i <= cycles; i++) {
			uint8 r = pal1[255 * 3 + 0];
			uint8 g = pal1[255 * 3 + 1];
			uint8 b = pal1[255 * 3 + 2];

			if (border255) {
				pal1[255 * 3 + 0] = border[0] * 63 / 255;
				pal1[255 * 3 + 1] = border[1] * 63 / 255;
				pal1[255 * 3 + 2] = border[2] * 63 / 255;
			}

			for (int c = 0; c < 768; c++)
				fade_pal[c] = ((pal1[c] - pal2[c]) * i) / cycles + pal2[c];

			pal1[255 * 3 + 0] = r;
			pal1[255 * 3 + 1] = g;
			pal1[255 * 3 + 2] = b;

			win->set_palette(fade_pal, max_val, brightness);

			// Frame skipping on slow systems
			if (i == cycles || ticks >= SDL_GetTicks() ||
			        !Game_window::get_instance()->get_frame_skipping())
				win->show();
			while (ticks >= SDL_GetTicks())
				;
			ticks += 20;
		}
	} else {
		uint8 r = pal1[255 * 3 + 0];
		uint8 g = pal1[255 * 3 + 1];
		uint8 b = pal1[255 * 3 + 2];

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
	faded_out = true;       // Be sure to set flag.
	if (cycles && fades_enabled) {
		unsigned char fade_pal[768];
		unsigned int ticks = SDL_GetTicks() + 20;
		for (int i = cycles; i >= 0; i--) {
			uint8 r = pal1[255 * 3 + 0];
			uint8 g = pal1[255 * 3 + 1];
			uint8 b = pal1[255 * 3 + 2];

			if (border255) {
				pal1[255 * 3 + 0] = border[0] * 63 / 255;
				pal1[255 * 3 + 1] = border[1] * 63 / 255;
				pal1[255 * 3 + 2] = border[2] * 63 / 255;
			}

			for (int c = 0; c < 768; c++)
				fade_pal[c] = ((pal1[c] - pal2[c]) * i) / cycles + pal2[c];

			pal1[255 * 3 + 0] = r;
			pal1[255 * 3 + 1] = g;
			pal1[255 * 3 + 2] = b;

			win->set_palette(fade_pal, max_val, brightness);
			// Frame skipping on slow systems
			if (i == 0 || ticks >= SDL_GetTicks() ||
			        !Game_window::get_instance()->get_frame_skipping())
				win->show();
			while (ticks >= SDL_GetTicks())
				;
			ticks += 20;
		}
	} else {
		win->set_palette(pal2, max_val, brightness);
		win->show();
	}
//Messes up sleep.          win->set_palette(pal1, max_val, brightness);
}

//	Find index (0-255) of closest color (r,g,b < 64).
int Palette::find_color(int r, int g, int b, int last) {
	int best_index = -1;
	long best_distance = 0xfffffff;
	// But don't search rotating colors.
	for (int i = 0; i < last; i++) {
		// Get deltas.
		long dr = r - pal1[3 * i];
		long dg = g - pal1[3 * i + 1];
		long db = b - pal1[3 * i + 2];
		// Figure distance-squared.
		long dist = dr * dr + dg * dg + db * db;
		if (dist < best_distance) { // Better than prev?
			best_index = i;
			best_distance = dist;
		}
	}
	return best_index;
}

/*
 *  Creates a translation table between two palettes.
 */

void Palette::create_palette_map(Palette *to, unsigned char *&buf) {
	// Assume buf has 256 elements
	for (int i = 0; i < 256; i++)
		buf[i] = to->find_color(pal1[3 * i], pal1[3 * i + 1], pal1[3 * i + 2], 256);
}

/*
 *  Creates a palette in-between two palettes.
 */

Palette *Palette::create_intermediate(Palette *to, int nsteps, int pos) {
	unsigned char palnew[768];
	if (fades_enabled) {
		for (int c = 0; c < 768; c++)
			palnew[c] = ((to->pal1[c] - pal1[c]) * pos) / nsteps + pal1[c];
	} else {
		unsigned char *palold;
		if (2 * pos >= nsteps)
			palold = to->pal1;
		else
			palold = pal1;
		memcpy(palnew, palold, 768);
	}
	Palette *ret = new Palette();
	ret->set(palnew, -1, true, true);
	return ret;
}

/*
 *  Create a translucency table for this palette seen through a given
 *  color.  (Based on a www.gamedev.net article by Jesse Towner.)
 */

void Palette::create_trans_table(
    // Color to blend with:
    unsigned char br, unsigned bg, unsigned bb,
    int alpha,          // 0-255, applied to 'blend' color.
    unsigned char *table        // 256 indices are stored here.
) {
	for (int i = 0; i < 256; i++) {
		int newr = (static_cast<int>(br) * alpha) / 255 +
		           (static_cast<int>(pal1[i * 3]) * (255 - alpha)) / 255;
		int newg = (static_cast<int>(bg) * alpha) / 255 +
		           (static_cast<int>(pal1[i * 3 + 1]) * (255 - alpha)) / 255;
		int newb = (static_cast<int>(bb) * alpha) / 255 +
		           (static_cast<int>(pal1[i * 3 + 2]) * (255 - alpha)) / 255;
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
	pal1[nr * 3] = r;
	pal1[nr * 3 + 1] = g;
	pal1[nr * 3 + 2] = b;
}

void Palette::set_palette(unsigned char palnew[768]) {
	memcpy(pal1, palnew, 768);
	memset(pal2, 0, 768);
}

void Palette::set_max_val(int max) {
	max_val = max;
}

int Palette::get_max_val() {
	return max_val;
}

Palette_transition::Palette_transition(
    int from, int to,
    int ch, int cm,
    int r,
    int nsteps,
    int sh, int smin
)
	: current(nullptr), step(0), max_steps(nsteps),
	  start_hour(sh), start_minute(smin), rate(r) {
	start = new Palette();
	start->load(PALETTES_FLX, PATCH_PALETTES, from);
	end = new Palette();
	end->load(PALETTES_FLX, PATCH_PALETTES, to);
	set_step(ch, cm);
}

Palette_transition::Palette_transition(
    Palette *from, int to,
    int ch, int cm,
    int r,
    int nsteps,
    int sh, int smin
)
	: current(nullptr), step(0), max_steps(nsteps),
	  start_hour(sh), start_minute(smin), rate(r) {
	start = new Palette(from);
	end = new Palette();
	end->load(PALETTES_FLX, PATCH_PALETTES, to);
	set_step(ch, cm);
}

Palette_transition::Palette_transition(
    Palette *from, Palette *to,
    int ch, int cm,
    int r,
    int nsteps,
    int sh, int smin
)
	: current(nullptr), step(0), max_steps(nsteps),
	  start_hour(sh), start_minute(smin), rate(r) {
	start = new Palette(from);
	end = new Palette(to);
	set_step(ch, cm);
}

bool Palette_transition::set_step(int hour, int min) {
	int new_step = 60 * (hour - start_hour) + min - start_minute;
	while (new_step < 0)
		new_step += 60;
	new_step /= rate;

	Game_window *gwin = Game_window::get_instance();
	if (gwin->get_pal()->is_faded_out())
		return false;

	if (!current || new_step != step) {
		step = new_step;
		delete current;
		current = start->create_intermediate(end, max_steps, step);
	}

	if (current)
		current->apply(true);
	return step < max_steps;
}

Palette_transition::~Palette_transition(
) {
	delete start;
	delete end;
	delete current;
}
