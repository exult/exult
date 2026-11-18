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

#include "Mui_font.h"

#include "U7obj.h"
#include "databuf.h"
#include "exult_flx.h"
#include "fnames.h"
#include "font.h"
#include "gamewin.h"
#include "ibuf8.h"
#include "utils.h"
#include "vgafile.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace {
	constexpr std::array<int, 7> font_sizes = {13, 16, 20, 24, 30, 36, 48};
	constexpr int                num_levels = 4;
}    // namespace

Mui_text_font::Mui_text_font(std::unique_ptr<Shape_file> shp, int size) : shapes(std::move(shp)), px(size) {
	int above = 0;
	int below = 0;
	for (int i = 0; i < shapes->get_num_frames(); i++) {
		Shape_frame* f = shapes->get_frame(i);
		if (f && f->is_rle()) {
			above = std::max(above, f->get_yabove());
			below = std::max(below, f->get_ybelow());
		}
	}
	base   = above;
	line_h = above + below + 1;
}

Mui_text_font::Mui_text_font(std::shared_ptr<Font> font) : classic(std::move(font)) {
	if (classic) {
		line_h = classic->get_text_height();
		base   = classic->get_text_baseline();
		px     = line_h;
	}
}

Mui_text_font::~Mui_text_font() = default;

int Mui_text_font::width(const char* text, size_t len) const {
	if (classic) {
		return classic->get_text_width(text, int(len));
	}
	if (!shapes) {
		return 0;
	}
	int w = 0;
	for (size_t i = 0; i < len; i++) {
		Shape_frame* f = shapes->get_frame(static_cast<unsigned char>(text[i]));
		if (f && f->is_rle()) {
			w += f->get_width();
		}
	}
	return w;
}

int Mui_text_font::width(const std::string& text) const {
	return width(text.data(), text.size());
}

const unsigned char* Mui_text_font::trans_for(uint8 fg, uint8 bg) const {
	const uint16 key = uint16((fg << 8) | bg);
	auto         it  = trans_cache.find(key);
	if (it != trans_cache.end()) {
		return it->second.data();
	}
	std::array<unsigned char, 256> table;
	for (int i = 0; i < 256; i++) {
		table[i] = static_cast<unsigned char>(i);
	}
	for (int lvl = 1; lvl <= num_levels; lvl++) {
		table[lvl] = Mui_theme::blend_index(bg, fg, lvl, num_levels);
	}
	return trans_cache.emplace(key, table).first->second.data();
}

int Mui_text_font::draw(Image_buffer8* buf, const std::string& text, int x, int y, uint8 fg, uint8 bg) const {
	if (classic) {
		// The classic font draws black (0) pixels; remap them to the text colour.
		std::array<unsigned char, 256> table;
		for (int i = 0; i < 256; i++) {
			table[i] = static_cast<unsigned char>(i);
		}
		table[0]             = fg;
		Game_window*   gwin  = Game_window::get_instance();
		Image_buffer8* prev  = gwin->push_render_target(buf);
		const int      drawn = classic->paint_text(buf, text.c_str(), x, y, table.data());
		gwin->pop_render_target(prev);
		return drawn;
	}
	if (!shapes) {
		return 0;
	}
	const unsigned char* trans = trans_for(fg, bg);
	int                  cx    = x;
	const int            by    = y + base;
	for (const char ch : text) {
		Shape_frame* f = shapes->get_frame(static_cast<unsigned char>(ch));
		if (!f || !f->is_rle()) {
			continue;
		}
		if (f->get_width() > 0) {
			f->paint_rle_remapped(buf, cx, by, trans);
		}
		cx += f->get_width();
	}
	return cx - x;
}

std::string Mui_text_font::elide(const std::string& text, int max_w) const {
	if (width(text) <= max_w) {
		return text;
	}
	const std::string dots = "...";
	const int         dw   = width(dots);
	std::string       out  = text;
	while (!out.empty() && width(out) + dw > max_w) {
		out.pop_back();
	}
	while (!out.empty() && out.back() == ' ') {
		out.pop_back();
	}
	return out + dots;
}

std::vector<std::string> Mui_text_font::wrap(const std::string& text, int max_w) const {
	std::vector<std::string> lines;
	std::string              line;
	size_t                   i = 0;
	while (i <= text.size()) {
		if (i == text.size() || text[i] == '\n') {
			lines.push_back(line);
			line.clear();
			i++;
			continue;
		}
		// Next word (with its leading spaces).
		size_t j = i;
		while (j < text.size() && text[j] == ' ') {
			j++;
		}
		while (j < text.size() && text[j] != ' ' && text[j] != '\n') {
			j++;
		}
		std::string word = text.substr(i, j - i);
		if (!line.empty() && width(line + word) > max_w) {
			lines.push_back(line);
			const auto b = word.find_first_not_of(' ');
			line         = b == std::string::npos ? std::string() : word.substr(b);
		} else {
			line += word;
		}
		// A single word wider than the line: hard-break it.
		while (width(line) > max_w && line.size() > 1) {
			size_t n = line.size();
			while (n > 1 && width(line.data(), n) > max_w) {
				n--;
			}
			lines.push_back(line.substr(0, n));
			line = line.substr(n);
		}
		i = j;
	}
	while (lines.size() > 1 && lines.back().empty()) {
		lines.pop_back();
	}
	return lines;
}

Mui_fonts& Mui_fonts::get() {
	static Mui_fonts instance;
	return instance;
}

Mui_fonts::Mui_fonts() {
	const std::string flx   = BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX);
	const int         total = int(Mui_face::count) * int(font_sizes.size());
	try {
		IExultDataSource ds(File_spec(flx), EXULT_FLX_MUI_FONTS_VGA);
		if (ds.good() && ds.getSize() > 0) {
			// mui_fonts.vga is itself a Flex of font shapes.
			const auto*                                 data = ds.getPtr();
			IBufferDataView                             flex(data, ds.getSize());
			const size_t                                size = ds.getSize();
			std::vector<std::unique_ptr<Mui_text_font>> loaded;
			flex.seek(0x50);
			const uint32 magic = flex.read4();
			const uint32 count = flex.read4();
			if (magic == 0xFFFF1A00u && count >= uint32(total)) {
				for (int i = 0; i < total; i++) {
					flex.seek(0x80 + 8 * i);
					const uint32 off = flex.read4();
					const uint32 len = flex.read4();
					if (off == 0 || len == 0 || size_t(off) + len > size) {
						break;
					}
					IBufferDataView glyphs(data + off, len);
					auto            shp = std::make_unique<Shape_file>(&glyphs);
					loaded.push_back(std::make_unique<Mui_text_font>(std::move(shp), font_sizes[i % font_sizes.size()]));
				}
			}
			if (int(loaded.size()) == total) {
				fonts = std::move(loaded);
			}
		}
	} catch (const std::exception& e) {
		std::cerr << "Mui_fonts: " << e.what() << std::endl;
	}
	if (fonts.empty()) {
		std::cerr << "Mui_fonts: mui_fonts.vga missing from exult.flx; using SMALL_BLACK_FONT" << std::endl;
		fallback = std::make_unique<Mui_text_font>(fontManager.get_font("SMALL_BLACK_FONT"));
	}
}

const Mui_text_font* Mui_fonts::pick(Mui_face face, int want_px) {
	if (fonts.empty()) {
		return fallback.get();
	}
	int best = 0;
	for (size_t i = 1; i < font_sizes.size(); i++) {
		if (std::abs(font_sizes[i] - want_px) < std::abs(font_sizes[best] - want_px)) {
			best = int(i);
		}
	}
	const int f = std::clamp(int(face), 0, int(Mui_face::count) - 1);
	return fonts[size_t(f) * font_sizes.size() + size_t(best)].get();
}
