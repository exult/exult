/*
 *  Copyright (C) 2000-2025  The Exult Team
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

#ifndef PALETTE_H
#define PALETTE_H

#include "common_types.h"
class Image_window8;
struct File_spec;
class U7multiobject;

#include <memory>

/*
 *  Palette #'s in 'palettes.flx':
 */
const int PALETTE_DAY       = 0;
const int PALETTE_DUSK      = 1;
const int PALETTE_DAWN      = 1;    // Think this is it.
const int PALETTE_NIGHT     = 2;
const int PALETTE_INVISIBLE = 3;    // When Avatar is invisible.
const int PALETTE_OVERCAST  = 4;    // When raining or overcast during daytime
const int PALETTE_FOG       = 5;
const int PALETTE_SPELL     = 6;    // light spell.
const int PALETTE_CANDLE    = 7;    // is somewhat warmer, candles.
const int PALETTE_RED       = 8;    // Used when hit in combat.
// 9 has lots of black.
const int PALETTE_LIGHTNING    = 10;
const int PALETTE_SINGLE_LIGHT = 11;
const int PALETTE_MANY_LIGHTS  = 12;

class Palette {
	Image_window8* win;
	unsigned char  pal1[768];
	unsigned char  pal2[768];
	int            palette;    // Palette #.
	int            brightness;
	int            max_val;
	bool           border255;
	bool           faded_out;    // true if faded palette to black.
	bool           fades_enabled;
	void set_loaded(const U7multiobject& pal, const char* xfname, int xindex);
	void loadxform(const unsigned char* buf, const char* xfname, int& xindex);

	static unsigned char border[3];

public:
	Palette();
	Palette(Palette* pal);      // "Copy" constructor.
	void take(Palette* pal);    // Copies a palette into another.
	// Fade palette in/out.
	void fade(int cycles, int inout, int pal_num = -1);

	bool is_faded_out() const {
		return faded_out;
	}

	void flash_red();    // Flash red for a moment.
	// Set desired palette.
	void set(int pal_num, int new_brightness = -1, bool repaint = true);
	void set(
			const unsigned char palnew[768], int new_brightness = -1,
			bool repaint = true, bool border255 = false);

	int get_brightness() const {    // Percentage:  100 = normal.
		return brightness;
	}

	//   the user.
	void set_fades_enabled(bool f) {
		fades_enabled = f;
	}

	bool get_fades_enabled() const {
		return fades_enabled;
	}

	void apply(bool repaint = true);
	void load(
			const File_spec& fname0, int index, const char* xfname = nullptr,
			int xindex = -1);
	void load(
			const File_spec& fname0, const File_spec& fname1, int index,
			const char* xfname = nullptr, int xindex = -1);
	void load(
			const File_spec& fname0, const File_spec& fname1,
			const File_spec& fname2, int index, const char* xfname = nullptr,
			int xindex = -1);
	void set_brightness(int bright);

	void set_max_val(int max) {
		max_val = max;
	}

	int get_max_val() const {
		return max_val;
	}

	void fade_in(int cycles);
	void fade_out(int cycles);
	int  find_color(int r, int g, int b, int last = 0xe0) const;
	void create_palette_map(const Palette* to, unsigned char* buf) const;
	std::unique_ptr<Palette> create_intermediate(
			const Palette& to, int nsteps, int pos) const;
	void create_trans_table(
			unsigned char br, unsigned bg, unsigned bb, int alpha,
			unsigned char* table) const;
	void show();

	void set_color(int nr, int r, int g, int b);

	unsigned char get_red(int nr) const {
		return pal1[3 * nr];
	}

	unsigned char get_green(int nr) const {
		return pal1[3 * nr + 1];
	}

	unsigned char get_blue(int nr) const {
		return pal1[3 * nr + 2];
	}

	void set_palette(const unsigned char palnew[768]);

	int get_palette_index() const {
		return palette;
	}

	static void set_border(int r, int g, int b) {
		border[0] = r;
		border[1] = g;
		border[2] = b;
	}

	unsigned char get_border_index() const {
		return border255 ? 255 : 0;
	}

	struct Ramp {
		uint8 start;
		uint8 end;
	};

	//! Get details of colour ramps in the palette
	//! \param[out] num_ramps The number of ramps in the returned array
	//! \returns Pointer to array of Ramp. A start index of 0 is used as a terminator.
	//! \remarks Colour index 0 is never included in a ramp. In general the u7 palette has 17 generally usuable ramps
	//! Ramp 17 onwards are palette cycling colours So cannot be reasonably used
	//! This generates the ramp information on first call so first callmight be
	//! slow in some cases and fast in others In particular this will always
	//! generate ramps from palette 0. If paleete 0 is not current it will
	//! attempt to load if it fails to load palette 0 it will return zero ramps
	const Ramp* get_ramps(unsigned int& num_ramps);

	//! Get the ramp number this colour belongs to
	//! \param colindex The colour to lookup
	//! \returns The ramp the colour is in
	int get_ramp_for_index(uint8 colindex);

	//! Remap a single colour to another ramp maintaining relative brightness
	//! \param colindex The colour to remap
	//! \param newramp the ramp the colour shouldbe remapped to
	//! \returns the bew colour if remapped or the colindex unchanged if
	//! remapping failed
	uint8 remap_colour_to_ramp(uint8 colindex, unsigned int newramp);

	//! Generate an xform table to remap colours to a specific ramp
	//! \param[out] table array to recieve generated table
	//! \param[in] remaps array to indicate which ramps should get remapped to which other ramp. i = remaps[i], use -1 as terminator
	void Generate_remap_xformtable(uint8 table[256], const int* remaps);
};

/*
 *  Smooth palette transition.
 */

class Palette_transition {
	Palette                  start, end;
	std::unique_ptr<Palette> current;
	int                      step, max_steps;
	int                      start_hour, start_minute, start_ticks;
	int                      rate;

public:
	Palette_transition(
			int from, int to, int ch, int cm, int ct, int, int nsteps, int sh,
			int smin, int stick);
	Palette_transition(
			Palette* from, Palette* to, int ch, int cm, int ct, int r,
			int nsteps, int sh, int smin, int stick);
	Palette_transition(
			Palette* from, int to, int ch, int cm, int ct, int r, int nsteps,
			int sh, int smin, int stick);

	int get_step() const {
		return step;
	}

	bool set_step(int hour, int min, int tick);

	Palette* get_current_palette() const {
		return current.get();
	}
};

#endif
