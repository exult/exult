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

#ifndef PALETTE_H
#define PALETTE_H

class Image_window8;
struct File_spec;
class U7multiobject;

/*
 *  Palette #'s in 'palettes.flx':
 */
const int PALETTE_DAY = 0;
const int PALETTE_DUSK = 1;
const int PALETTE_DAWN = 1;     // Think this is it.
const int PALETTE_NIGHT = 2;
const int PALETTE_INVISIBLE = 3;    // When Avatar is invisible.
const int PALETTE_OVERCAST = 4;     // When raining or overcast during daytime
const int PALETTE_FOG = 5;
const int PALETTE_SPELL = 6; // light spell.
//const int PALETTE_CANDLE = 7; // is somewhat warmer, candles. unused in code yet.
const int PALETTE_RED = 8;      // Used when hit in combat.
// 9 has lots of black.
const int PALETTE_LIGHTNING = 10;
const int PALETTE_SINGLE_LIGHT = 11;
const int PALETTE_MANY_LIGHTS = 12;



class Palette {
	Image_window8 *win;
	unsigned char pal1[768];
	unsigned char pal2[768];
	int palette;        // Palette #.
	int brightness;
	int max_val;
	bool border255;
	bool faded_out;     // true if faded palette to black.
	bool fades_enabled;
	void set_loaded(const U7multiobject &pal, const char *xfname, int xindex);
	void loadxform(const char *buf, const char *xfname, int &xindex);

	static unsigned char border[3];
public:
	Palette();
	Palette(Palette *pal);      // "Copy" constructor.
	void take(Palette *pal);    // Copies a palette into another.
	// Fade palette in/out.
	void fade(int cycles, int inout, int pal_num = -1);
	bool is_faded_out() {
		return faded_out;
	}
	void flash_red();   // Flash red for a moment.
	// Set desired palette.
	void set(int pal_num, int new_brightness = -1,
	         bool repaint = true);
	void set(unsigned char palnew[768], int new_brightness = -1,
	         bool repaint = true, bool border255 = false);
	int get_brightness() {  // Percentage:  100 = normal.
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
	void load(const File_spec &fname0, int index,
	          const char *xfname = nullptr, int xindex = -1);
	void load(const File_spec &fname0, const File_spec &fname1,
	          int index, const char *xfname = nullptr, int xindex = -1);
	void load(const File_spec &fname0, const File_spec &fname1,
	          const File_spec &fname2, int index,
	          const char *xfname = nullptr, int xindex = -1);
	void set_brightness(int bright);
	void set_max_val(int max);
	int get_max_val();
	void fade_in(int cycles);
	void fade_out(int cycles);
	int find_color(int r, int g, int b, int last = 0xe0);
	void create_palette_map(Palette *to, unsigned char *&buf);
	Palette *create_intermediate(Palette *to, int nsteps, int pos);
	void create_trans_table(unsigned char br, unsigned bg,
	                        unsigned bb, int alpha, unsigned char *table);
	void show();

	void set_color(int nr, int r, int g, int b);
	unsigned char get_red(int nr) {
		return pal1[3 * nr];
	}
	unsigned char get_green(int nr) {
		return pal1[3 * nr + 1];
	}
	unsigned char get_blue(int nr) {
		return pal1[3 * nr + 2];
	}
	void set_palette(unsigned char palnew[768]);
	static void set_border(int r, int g, int b) {
		border[0] = r;
		border[1] = g;
		border[2] = b;
	}

	unsigned char get_border_index() {

		return border255 ? 255 : 0;
	}
};

/*
 *  Smooth palette transition.
 */

class Palette_transition {
	Palette *start, *end, *current;
	int step, max_steps;
	int start_hour, start_minute, rate;
public:
	Palette_transition(int from, int to, int ch = 0, int cm = 0, int r = 4,
	                   int nsteps = 15, int sh = 0, int smin = 0);
	Palette_transition(Palette *from, Palette *to, int ch = 0, int cm = 0,
	                   int r = 4, int nsteps = 15, int sh = 0, int smin = 0);
	Palette_transition(Palette *from, int to, int ch = 0, int cm = 0,
	                   int r = 4, int nsteps = 15, int sh = 0, int smin = 0);
	~Palette_transition();
	int get_step() const {
		return step;
	}
	bool set_step(int hour, int min);
	Palette *get_current_palette() {
		return current;
	}
};

#endif
