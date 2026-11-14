/*
Copyright (C) 2026 The Exult Team

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

#ifndef MUI_THEME_H
#define MUI_THEME_H

#include "common_types.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

class Configuration;

// Colour roles of a theme. Widgets only ever use roles, never colours.
enum class Mui_colour : int {
	backdrop = 0,
	window,
	panel,
	panel_alt,
	frame,
	frame_light,
	frame_dark,
	text,
	text_dim,
	text_on_accent,
	text_on_danger,
	title_text,
	accent,
	accent_hover,
	focus,
	danger,
	warning,
	success,
	badge_now,
	badge_restart,
	badge_changed,
	cheat_accent,
	count
};

// Text roles of a theme.
enum class Mui_font_role : int {
	title = 0,
	heading,
	body,
	small,
	mono,
	count
};

// Faces available in mui_fonts.vga (font index = face * sizes + size index).
enum class Mui_face : int {
	sans = 0,
	sans_bold,
	serif,
	serif_bold,
	count
};

/*
 *  A themed menu style, loaded from exult.cfg-style XML (see
 *  data/mui_modern.thm). Colours are resolved to the nearest day-palette
 *  entry (indices 0-223, so no cycling colours).
 */
struct Mui_theme {
	struct Metrics {
		// Reference pixels, times ui_scale() on the display.
		float unit      = 8;
		float gap       = 8;
		float control_h = 32;
		float row_h     = 44;
		float tab_h     = 40;
		float radius    = 6;
		float border    = 1;
		float title_h   = 48;
		float footer_h  = 52;
	};

	struct Font_spec {
		Mui_face face = Mui_face::sans;
		int      px   = 16;
	};

	std::string                                      name = "modern";
	std::string                                      label;    // Display name (already translated).
	bool                                             tabs_left       = true;
	float                                            sidebar_width   = 200;
	int                                              backdrop_alpha  = 170;
	bool                                             frame_stone     = false;    // Stone block frame, else flat.
	bool                                             panel_parchment = false;    // Parchment texture, else plain.
	Metrics                                          metrics;
	std::array<uint8, int(Mui_colour::count)>        idx{};
	std::array<uint32, int(Mui_colour::count)>       rgb{};    // 0xRRGGBB wanted.
	std::array<Font_spec, int(Mui_font_role::count)> fonts{};

	uint8 colour(Mui_colour c) const {
		return idx[int(c)];
	}

	const Font_spec& font(Mui_font_role r) const {
		return fonts[int(r)];
	}

	// Load a theme by name: <PATCH>/mui_themes/<name>.thm, then
	// <DATA>/mui_themes/<name>.thm, then the built-ins in exult.flx. Missing
	// keys fall back to the built-in "modern" values. Returns nullptr when
	// no theme of that name exists.
	static std::shared_ptr<Mui_theme> load(const std::string& name);
	// Built-in themes plus any theme files found.
	static std::vector<std::string> list();
	// Scale for theme metrics: display-height based (1 at 960 px), limited
	// by the width on narrow displays, clamped to 1-4, then times the
	// user's menu size in percent (config/gameplay/menu_scale).
	static float ui_scale(int display_w, int display_h, int percent = 100);

	// Day palette helpers (palettes/0).
	static uint8  nearest_index(uint32 rgb);
	static uint32 index_rgb(uint8 index);
	static uint8  blend_index(uint8 from, uint8 to, int num, int den);

private:
	void apply(const Configuration& conf);
	void resolve_colours();
};

#endif
