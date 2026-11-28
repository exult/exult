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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "Mui_theme.h"

#include "Configuration.h"
#include "U7obj.h"
#include "databuf.h"
#include "exceptions.h"
#include "exult_flx.h"
#include "fnames.h"
#include "game.h"
#include "items.h"
#include "listfiles.h"
#include "palette.h"
#include "utils.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>

namespace {
	// Config key names for each colour role (theme/colours/<name>).
	constexpr std::array<const char*, int(Mui_colour::count)> colour_names
			= {"backdrop",   "window",        "panel",         "panel_alt",      "frame",          "frame_light",
			   "frame_dark", "text",          "text_dim",      "text_on_accent", "text_on_danger", "title_text",
			   "accent",     "accent_hover",  "focus",         "danger",         "warning",        "success",
			   "badge_now",  "badge_restart", "badge_changed", "cheat_accent"};

	constexpr std::array<const char*, int(Mui_font_role::count)> font_role_names = {"title", "heading", "body", "small", "mono"};

	constexpr std::array<const char*, int(Mui_face::count)> face_names = {"sans", "sans_bold", "serif", "serif_bold"};

	struct Builtin {
		const char* name;
		int         index;
	};

	const std::array<Builtin, 2> builtins = {
			{{"ultima", EXULT_FLX_MUI_ULTIMA_THM}, {"modern", EXULT_FLX_MUI_MODERN_THM}}
    };

	std::string exult_flx_path() {
		return BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX);
	}

	// Read a theme file or a built-in theme into a string.
	bool read_theme_text(const std::string& name, std::string& out) {
		for (const char* dir : {"<PATCH>/mui_themes/", "<DATA>/mui_themes/"}) {
			const std::string path = std::string(dir) + name + ".thm";
			if (!is_system_path_defined(dir == std::string("<PATCH>/mui_themes/") ? "<PATCH>" : "<DATA>")) {
				continue;
			}
			if (!U7exists(path)) {
				continue;
			}
			try {
				auto in = U7open_in(path.c_str(), true);
				if (in) {
					std::ostringstream ss;
					ss << in->rdbuf();
					out = ss.str();
					return true;
				}
			} catch (const std::exception& e) {
				std::cerr << "Mui_theme: cannot read " << path << ": " << e.what() << std::endl;
			}
		}
		for (const auto& b : builtins) {
			if (name == b.name) {
				try {
					IExultDataSource ds(File_spec(exult_flx_path()), b.index);
					if (!ds.good() || ds.getSize() == 0) {
						return false;
					}
					out.assign(reinterpret_cast<const char*>(ds.getPtr()), ds.getSize());
					return true;
				} catch (const std::exception& e) {
					std::cerr << "Mui_theme: cannot read built-in theme " << name << ": " << e.what() << std::endl;
					return false;
				}
			}
		}
		return false;
	}

	bool parse_theme(const std::string& text, Configuration& conf) {
		const size_t start = text.find("<theme");
		if (start == std::string::npos) {
			return false;
		}
		conf.read_config_string(text.substr(start));
		return conf.key_exists("theme");
	}

	bool parse_rgb(const std::string& s, uint32& rgb) {
		if (s.size() != 7 || s[0] != '#') {
			return false;
		}
		char*               end = nullptr;
		const unsigned long v   = std::strtoul(s.c_str() + 1, &end, 16);
		if (end == nullptr || *end != 0) {
			return false;
		}
		rgb = uint32(v);
		return true;
	}

	// Day palette, loaded on first use.
	struct Day_palette {
		std::array<uint32, 256> rgb{};
		bool                    loaded = false;
		std::map<uint32, uint8> nearest_cache;
		std::map<uint32, uint8> blend_cache;

		void ensure() {
			if (loaded) {
				return;
			}
			loaded = true;
			if (game == nullptr) {
				return;
			}
			Palette             pal;
			const str_int_pair& pal_static = game->get_resource("palettes/0");
			const str_int_pair& pal_patch  = game->get_resource("palettes/patch/0");
			pal.load(pal_static.str, pal_patch.str, pal_static.num);
			const int maxv = std::max(1, pal.get_max_val());
			for (int i = 0; i < 256; i++) {
				const uint32 r = std::min(255, pal.get_red(i) * 255 / maxv);
				const uint32 g = std::min(255, pal.get_green(i) * 255 / maxv);
				const uint32 b = std::min(255, pal.get_blue(i) * 255 / maxv);
				rgb[i]         = (r << 16) | (g << 8) | b;
			}
		}
	};

	Day_palette& day_palette() {
		static Day_palette pal;
		pal.ensure();
		return pal;
	}
}    // namespace

uint8 Mui_theme::nearest_index(uint32 want) {
	Day_palette& pal = day_palette();
	auto         it  = pal.nearest_cache.find(want);
	if (it != pal.nearest_cache.end()) {
		return it->second;
	}
	const int wr   = int((want >> 16) & 0xff);
	const int wg   = int((want >> 8) & 0xff);
	const int wb   = int(want & 0xff);
	long      best = -1;
	uint8     idx  = 0;
	// Only the non-cycling colours (0xe0 and up rotate).
	for (int i = 0; i < 0xe0; i++) {
		const int r = int((pal.rgb[i] >> 16) & 0xff);
		const int g = int((pal.rgb[i] >> 8) & 0xff);
		const int b = int(pal.rgb[i] & 0xff);
		// Weighted distance, closer to perceived difference than plain RGB.
		const long dr = r - wr;
		const long dg = g - wg;
		const long db = b - wb;
		const long d  = 3 * dr * dr + 4 * dg * dg + 2 * db * db;
		if (best < 0 || d < best) {
			best = d;
			idx  = uint8(i);
		}
	}
	pal.nearest_cache[want] = idx;
	return idx;
}

uint32 Mui_theme::index_rgb(uint8 index) {
	return day_palette().rgb[index];
}

uint8 Mui_theme::blend_index(uint8 from, uint8 to, int num, int den) {
	if (den <= 0 || num <= 0) {
		return from;
	}
	if (num >= den) {
		return to;
	}
	Day_palette& pal = day_palette();
	const uint32 key = (uint32(from) << 24) | (uint32(to) << 16) | (uint32(num) << 8) | uint32(den);
	auto         it  = pal.blend_cache.find(key);
	if (it != pal.blend_cache.end()) {
		return it->second;
	}
	const uint32 a   = pal.rgb[from];
	const uint32 b   = pal.rgb[to];
	auto         mix = [&](int shift) {
        const int ca = int((a >> shift) & 0xff);
        const int cb = int((b >> shift) & 0xff);
        return uint32(ca + (cb - ca) * num / den) << shift;
	};
	const uint8 idx      = nearest_index(mix(16) | mix(8) | mix(0));
	pal.blend_cache[key] = idx;
	return idx;
}

float Mui_theme::ui_scale(int display_w, int display_h, int percent) {
	// Grow with the shorter side (a 1120x660 window takes about 70% of the
	// height from 1440x960 up), never wider than the display allows, and
	// never below the reference size on small displays. The width only
	// limits narrow displays, so an ultrawide gets the same size as a 16:9
	// display of the same height.
	const float base = std::clamp(std::min(display_h / 960.f, display_w / 1440.f), 1.f, 4.f);
	return std::clamp(base * float(std::clamp(percent, 50, 200)) / 100.f, 0.5f, 6.f);
}

void Mui_theme::apply(const Configuration& conf) {
	std::string s;
	auto        get = [&](const char* key, std::string& out) {
        const std::string k = std::string("theme/") + key;
        if (!conf.key_exists(k)) {
            return false;
        }
        conf.value(k, out, "");
        // Trim whitespace left by multi-line values.
        const auto b = out.find_first_not_of(" \t\r\n");
        const auto e = out.find_last_not_of(" \t\r\n");
        out          = b == std::string::npos ? std::string() : out.substr(b, e - b + 1);
        return true;
	};
	auto get_float = [&](const char* key, float& out) {
		std::string v;
		if (get(key, v) && !v.empty()) {
			out = std::strtof(v.c_str(), nullptr);
		}
	};
	if (get("meta/name", s) && !s.empty()) {
		name = s;
	}
	if (get("meta/label", s) && !s.empty()) {
		if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
			const unsigned long id = std::strtoul(s.c_str(), nullptr, 16);
			const char*         m  = id > unsigned(msg_file_start) ? get_text_msg(unsigned(id - msg_file_start)) : nullptr;
			label                  = m && *m ? m : name;
		} else {
			label = s;
		}
	}
	if (get("layout/tabs", s)) {
		tabs_left = s == "left";
	}
	get_float("layout/sidebar_width", sidebar_width);
	get_float("metrics/unit", metrics.unit);
	get_float("metrics/gap", metrics.gap);
	get_float("metrics/control_h", metrics.control_h);
	get_float("metrics/row_h", metrics.row_h);
	get_float("metrics/tab_h", metrics.tab_h);
	get_float("metrics/radius", metrics.radius);
	get_float("metrics/border", metrics.border);
	get_float("metrics/title_h", metrics.title_h);
	get_float("metrics/footer_h", metrics.footer_h);
	for (int i = 0; i < int(Mui_colour::count); i++) {
		uint32 v;
		if (get((std::string("colours/") + colour_names[i]).c_str(), s) && parse_rgb(s, v)) {
			rgb[i] = v;
		}
	}
	if (get("colours/backdrop_alpha", s) && !s.empty()) {
		backdrop_alpha = std::clamp(std::atoi(s.c_str()), 0, 255);
	}
	for (int i = 0; i < int(Mui_font_role::count); i++) {
		const std::string base = std::string("fonts/") + font_role_names[i];
		if (get((base + "/face").c_str(), s)) {
			for (int f = 0; f < int(Mui_face::count); f++) {
				if (s == face_names[f]) {
					fonts[i].face = Mui_face(f);
				}
			}
		}
		if (get((base + "/px").c_str(), s) && !s.empty()) {
			fonts[i].px = std::clamp(std::atoi(s.c_str()), 8, 96);
		}
	}
	if (get("frame/style", s)) {
		frame_stone = s == "stone";
		frame_gump  = s == "gump";
	}
	if (get("panel/texture", s)) {
		panel_parchment = s == "parchment";
	}
}

void Mui_theme::resolve_colours() {
	// The test driver logs each theme's palette mapping once.
	static std::vector<std::string> reported;
	const bool                      report
			= std::getenv("EXULT_MUI_TEST") != nullptr && std::find(reported.begin(), reported.end(), name) == reported.end();
	if (report) {
		reported.push_back(name);
	}
	for (int i = 0; i < int(Mui_colour::count); i++) {
		idx[i] = nearest_index(rgb[i]);
		if (report) {
			char line[96];
			snprintf(
					line, sizeof(line), "MUI-TEST info theme %s %s #%06x -> %u #%06x", name.c_str(), colour_names[i],
					unsigned(rgb[i]), unsigned(idx[i]), unsigned(index_rgb(idx[i])));
			std::cout << line << std::endl;
		}
	}
}

std::shared_ptr<Mui_theme> Mui_theme::load(const std::string& want) {
	auto theme = std::make_shared<Mui_theme>();
	// Compiled-in values match data/mui_modern.thm, used if exult.flx lacks it.
	const std::array<uint32, int(Mui_colour::count)> defaults
			= {0x000000, 0x161a20, 0x232a35, 0x2c3442, 0x3a4352, 0x4b5566, 0x11151b, 0xe6e9ef, 0x9aa3b2, 0xffffff, 0xffffff,
			   0xe6e9ef, 0x3d7eff, 0x5a92ff, 0x8fb4ff, 0xe0483e, 0xe0a030, 0x3fb06a, 0x3fb06a, 0xe0a030, 0x3d7eff, 0xe0483e};
	theme->rgb   = defaults;
	theme->fonts = {
			{{Mui_face::sans_bold, 24},
			 {Mui_face::sans_bold, 16},
			 {Mui_face::sans, 16},
			 {Mui_face::sans, 13},
			 {Mui_face::sans, 13}}
    };
	std::string text;
	if (read_theme_text("modern", text)) {
		Configuration base("", "theme");
		if (parse_theme(text, base)) {
			theme->apply(base);
		}
	}
	if (want != "modern") {
		text.clear();
		Configuration conf("", "theme");
		if (!read_theme_text(want, text) || !parse_theme(text, conf)) {
			return nullptr;
		}
		theme->label.clear();
		theme->apply(conf);
		theme->name = want;
	}
	if (theme->label.empty()) {
		theme->label = theme->name;
	}
	theme->resolve_colours();
	return theme;
}

std::vector<std::string> Mui_theme::list() {
	std::vector<std::string> names;
	for (const auto& b : builtins) {
		names.emplace_back(b.name);
	}
	for (const char* dir : {"<PATCH>", "<DATA>"}) {
		if (!is_system_path_defined(dir)) {
			continue;
		}
		FileList files;
		U7ListFiles(std::string(dir) + "/mui_themes/*.thm", files, true);
		for (const auto& f : files) {
			std::string n     = f;
			const auto  slash = n.find_last_of("/\\");
			if (slash != std::string::npos) {
				n = n.substr(slash + 1);
			}
			if (n.size() > 4) {
				n.resize(n.size() - 4);
			}
			if (std::find(names.begin(), names.end(), n) == names.end()) {
				names.push_back(n);
			}
		}
	}
	return names;
}
