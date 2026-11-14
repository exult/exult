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

#include "Mui_painter.h"

#include "Mui_font.h"
#include "ibuf8.h"
#include "vgafile.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>

namespace {
	// Small integer hash for deterministic texture noise.
	uint32 noise(int x, int y) {
		uint32 h = uint32(x) * 374761393u + uint32(y) * 668265263u;
		h        = (h ^ (h >> 13)) * 1274126177u;
		return h ^ (h >> 16);
	}
}    // namespace

Mui_painter::Mui_painter(Image_buffer8* b, const Mui_theme& th, float s) : buf(b), theme(th), scale(s) {
	clips.emplace_back(0, 0, int(buf->get_width()), int(buf->get_height()));
	apply_clip();
}

int Mui_painter::px(float ref) const {
	if (ref <= 0) {
		return 0;
	}
	return std::max(1, int(std::lround(ref * scale)));
}

const Mui_text_font* Mui_painter::font(Mui_font_role role) const {
	const auto& spec = theme.font(role);
	return Mui_fonts::get().pick(spec.face, int(std::lround(spec.px * scale)));
}

void Mui_painter::apply_clip() {
	const TileRect& r = clips.back();
	buf->set_clip(r.x, r.y, std::max(0, r.w), std::max(0, r.h));
}

void Mui_painter::push_clip(const TileRect& r) {
	TileRect c = clips.back().intersect(r);
	if (c.w < 0) {
		c.w = 0;
	}
	if (c.h < 0) {
		c.h = 0;
	}
	clips.push_back(c);
	apply_clip();
}

void Mui_painter::pop_clip() {
	if (clips.size() > 1) {
		clips.pop_back();
	}
	apply_clip();
}

TileRect Mui_painter::get_clip() const {
	return clips.back();
}

void Mui_painter::fill_rect(const TileRect& r, uint8 c) {
	if (r.w > 0 && r.h > 0) {
		buf->fill8(c, r.w, r.h, r.x, r.y);
	}
}

void Mui_painter::span(int x0, int x1, int y, uint8 c) {
	if (x1 >= x0) {
		buf->fill8(c, x1 - x0 + 1, 1, x0, y);
	}
}

void Mui_painter::hline(int x, int y, int w, uint8 c) {
	fill_rect(TileRect(x, y, w, 1), c);
}

void Mui_painter::vline(int x, int y, int h, uint8 c) {
	fill_rect(TileRect(x, y, 1, h), c);
}

void Mui_painter::frame_rect(const TileRect& r, int n, uint8 c) {
	n = std::min({n, r.w / 2, r.h / 2});
	if (n <= 0) {
		return;
	}
	fill_rect(TileRect(r.x, r.y, r.w, n), c);
	fill_rect(TileRect(r.x, r.y + r.h - n, r.w, n), c);
	fill_rect(TileRect(r.x, r.y + n, n, r.h - 2 * n), c);
	fill_rect(TileRect(r.x + r.w - n, r.y + n, n, r.h - 2 * n), c);
}

void Mui_painter::disc(int cx, int cy, int r, uint8 c) {
	for (int dy = -r; dy <= r; dy++) {
		const int dx = int(std::sqrt(double(r * r - dy * dy)) + 0.5);
		span(cx - dx, cx + dx, cy + dy, c);
	}
}

void Mui_painter::line(int x0, int y0, int x1, int y1, int thick, uint8 c) {
	const int dx    = std::abs(x1 - x0);
	const int dy    = std::abs(y1 - y0);
	const int steps = std::max({dx, dy, 1});
	const int r     = std::max(0, thick / 2);
	for (int i = 0; i <= steps; i++) {
		const int x = x0 + (x1 - x0) * i / steps;
		const int y = y0 + (y1 - y0) * i / steps;
		if (r == 0) {
			fill_rect(TileRect(x, y, 1, 1), c);
		} else {
			disc(x, y, r, c);
		}
	}
}

void Mui_painter::rounded_fill(const TileRect& r, int radius, uint8 c) {
	radius = std::clamp(radius, 0, std::min(r.w, r.h) / 2);
	if (radius == 0) {
		fill_rect(r, c);
		return;
	}
	for (int row = 0; row < r.h; row++) {
		int inset = 0;
		int dy    = -1;
		if (row < radius) {
			dy = radius - row;
		} else if (row >= r.h - radius) {
			dy = row - (r.h - radius - 1);
		}
		if (dy > 0) {
			const double fy = dy - 0.5;
			inset           = radius - int(std::sqrt(std::max(0.0, double(radius * radius) - fy * fy)) + 0.5);
		}
		span(r.x + inset, r.x + r.w - 1 - inset, r.y + row, c);
	}
}

void Mui_painter::rounded_frame(const TileRect& r, int radius, int n, uint8 c) {
	if (radius <= 0) {
		frame_rect(r, n, c);
		return;
	}
	// Outline = outer shape minus inner shape, row by row.
	radius        = std::clamp(radius, 0, std::min(r.w, r.h) / 2);
	auto inset_at = [](int row, int h, int rad) {
		int dy = -1;
		if (row < rad) {
			dy = rad - row;
		} else if (row >= h - rad) {
			dy = row - (h - rad - 1);
		}
		if (dy <= 0) {
			return 0;
		}
		const double fy = dy - 0.5;
		return rad - int(std::sqrt(std::max(0.0, double(rad * rad) - fy * fy)) + 0.5);
	};
	const int irad = std::max(0, radius - n);
	for (int row = 0; row < r.h; row++) {
		const int oi = inset_at(row, r.h, radius);
		const int x0 = r.x + oi;
		const int x1 = r.x + r.w - 1 - oi;
		if (row < n || row >= r.h - n) {
			span(x0, x1, r.y + row, c);
			continue;
		}
		const int ii = n + inset_at(row - n, r.h - 2 * n, irad);
		span(x0, r.x + ii - 1, r.y + row, c);
		span(r.x + r.w - ii, x1, r.y + row, c);
	}
}

void Mui_painter::bevel(const TileRect& r, int n, uint8 light, uint8 dark) {
	for (int i = 0; i < n && i < r.w / 2 && i < r.h / 2; i++) {
		hline(r.x + i, r.y + i, r.w - 2 * i, light);
		vline(r.x + i, r.y + i, r.h - 2 * i, light);
		hline(r.x + i, r.y + r.h - 1 - i, r.w - 2 * i, dark);
		vline(r.x + r.w - 1 - i, r.y + i, r.h - 2 * i, dark);
	}
}

void Mui_painter::texture_parchment(const TileRect& r, uint8 base, uint8 light, uint8 dark) {
	const TileRect c = get_clip().intersect(r);
	if (c.w <= 0 || c.h <= 0) {
		return;
	}
	fill_rect(c, base);
	// Sparse fibres and blotches: low-frequency noise at 4-px cells picks
	// the shade, single-pixel noise breaks up the cells.
	const int cell = std::max(2, px(3));
	for (int y = c.y; y < c.y + c.h; y++) {
		for (int x = c.x; x < c.x + c.w; x++) {
			const uint32 coarse = noise(x / cell, y / (cell * 2)) & 0xff;
			const uint32 fine   = noise(x, y) & 0xff;
			if (coarse < 40 && fine < 110) {
				buf->put_pixel8(dark, x, y);
			} else if (coarse > 235 && fine < 90) {
				buf->put_pixel8(light, x, y);
			} else if (fine < 3) {
				buf->put_pixel8(dark, x, y);
			}
		}
	}
}

void Mui_painter::texture_stone(const TileRect& r, uint8 base, uint8 light, uint8 dark, uint8 mortar) {
	const TileRect c = get_clip().intersect(r);
	if (c.w <= 0 || c.h <= 0) {
		return;
	}
	const int bh = std::max(6, px(14));    // Block height.
	const int bw = bh * 2;                 // Block width.
	for (int y = c.y; y < c.y + c.h; y++) {
		const int ry  = y - r.y;
		const int row = ry / bh;
		const int iy  = ry % bh;
		for (int x = c.x; x < c.x + c.w; x++) {
			const int rx  = x - r.x + (row & 1) * (bw / 2);
			const int col = rx / bw;
			const int ix  = rx % bw;
			uint8     pix;
			if (iy == 0 || ix == 0) {
				pix = mortar;
			} else if (iy == 1 || ix == 1) {
				pix = light;    // Lit top/left edge of each block.
			} else if (iy == bh - 1 || ix == bw - 1) {
				pix = dark;
			} else {
				const uint32 n     = noise(x, y) & 0xff;
				const uint32 shade = noise(col, row) & 3;    // Per-block tint.
				if (n < 18 + shade * 6) {
					pix = dark;
				} else if (n > 236 - shade * 4) {
					pix = light;
				} else {
					pix = base;
				}
			}
			buf->put_pixel8(pix, x, y);
		}
	}
}

void Mui_painter::divider(int x, int y, int w) {
	hline(x, y, w, col(Mui_colour::frame_dark));
	if (theme.frame_stone) {
		hline(x, y + 1, w, col(Mui_colour::frame_light));
	}
}

void Mui_painter::icon(Mui_icon which, const TileRect& r, uint8 c) {
	const int s  = std::min(r.w, r.h);
	const int cx = r.x + r.w / 2;
	const int cy = r.y + r.h / 2;
	const int h  = std::max(2, s * 3 / 10);    // Half extent.
	const int t  = std::max(2, s / 8);         // Stroke.
	switch (which) {
	case Mui_icon::check:
		line(cx - h, cy, cx - h / 3, cy + h * 2 / 3, t, c);
		line(cx - h / 3, cy + h * 2 / 3, cx + h, cy - h * 2 / 3, t, c);
		break;
	case Mui_icon::arrow_left:
	case Mui_icon::arrow_right:
	case Mui_icon::arrow_up:
	case Mui_icon::arrow_down:
	case Mui_icon::chevron_down: {
		// Solid triangles for arrows; the chevron is an open "v".
		const int hh = h * 2 / 3;
		if (which == Mui_icon::chevron_down) {
			line(cx - h, cy - hh / 2, cx, cy + hh / 2, t, c);
			line(cx, cy + hh / 2, cx + h, cy - hh / 2, t, c);
			break;
		}
		for (int i = 0; i <= hh; i++) {
			const int len = i * h / std::max(1, hh);
			switch (which) {
			case Mui_icon::arrow_up:
				span(cx - len, cx + len, cy - hh / 2 + i, c);
				break;
			case Mui_icon::arrow_down:
				span(cx - len, cx + len, cy + hh / 2 - i, c);
				break;
			case Mui_icon::arrow_left:
				fill_rect(TileRect(cx - hh / 2 + i, cy - len, 1, 2 * len + 1), c);
				break;
			default:
				fill_rect(TileRect(cx + hh / 2 - i, cy - len, 1, 2 * len + 1), c);
				break;
			}
		}
		break;
	}
	case Mui_icon::close:
		line(cx - h, cy - h, cx + h, cy + h, t, c);
		line(cx - h, cy + h, cx + h, cy - h, t, c);
		break;
	case Mui_icon::search: {
		const int rr = h * 2 / 3;
		const int ox = cx - h / 4;
		const int oy = cy - h / 4;
		for (int a = 0; a < 64; a++) {
			const double ang = a * 2 * 3.14159265358979 / 64;
			fill_rect(
					TileRect(
							ox + int(std::lround(rr * std::cos(ang))) - t / 2, oy + int(std::lround(rr * std::sin(ang))) - t / 2, t,
							t),
					c);
		}
		line(ox + rr * 7 / 10, oy + rr * 7 / 10, cx + h, cy + h, t, c);
		break;
	}
	case Mui_icon::plus:
		fill_rect(TileRect(cx - h, cy - t / 2, 2 * h + 1, t), c);
		fill_rect(TileRect(cx - t / 2, cy - h, t, 2 * h + 1), c);
		break;
	case Mui_icon::minus:
		fill_rect(TileRect(cx - h, cy - t / 2, 2 * h + 1, t), c);
		break;
	}
}

void Mui_painter::focus_ring(const TileRect& r, int radius) {
	const int n = std::max(2, px(2));
	TileRect  o = r;
	o.enlarge(n + 1);
	rounded_frame(o, radius > 0 ? radius + n + 1 : 0, n, col(Mui_colour::focus));
}

void Mui_painter::blit_scaled(const unsigned char* pixels, int w, int h, int pitch, const TileRect& r, int transparent) {
	if (!pixels || w <= 0 || h <= 0 || r.w <= 0 || r.h <= 0) {
		return;
	}
	const TileRect c = get_clip().intersect(r);
	for (int y = c.y; y < c.y + c.h; y++) {
		const int sy = (y - r.y) * h / r.h;
		for (int x = c.x; x < c.x + c.w; x++) {
			const int           sx  = (x - r.x) * w / r.w;
			const unsigned char pix = pixels[sy * pitch + sx];
			if (int(pix) != transparent) {
				buf->put_pixel8(pix, x, y);
			}
		}
	}
}

void Mui_painter::blit_shape(Shape_frame* frame, const TileRect& r, bool keep_aspect) {
	if (!frame || r.w <= 0 || r.h <= 0) {
		return;
	}
	const int fw = frame->get_width();
	const int fh = frame->get_height();
	if (fw <= 0 || fh <= 0) {
		return;
	}
	// Paint the frame 1:1 into a scratch buffer, then scale it.
	Image_buffer8 tmp{unsigned(fw), unsigned(fh)};
	tmp.fill8(255);
	if (frame->is_rle()) {
		frame->paint_rle(&tmp, frame->get_xleft(), frame->get_yabove());
	} else {
		frame->paint(&tmp, frame->get_xleft(), frame->get_yabove());
	}
	TileRect dst = r;
	if (keep_aspect) {
		const float s = std::min(float(r.w) / fw, float(r.h) / fh);
		dst.w         = std::max(1, int(fw * s));
		dst.h         = std::max(1, int(fh * s));
		dst.x         = r.x + (r.w - dst.w) / 2;
		dst.y         = r.y + (r.h - dst.h) / 2;
	}
	blit_scaled(tmp.get_bits(), fw, fh, int(tmp.get_line_width()), dst, 255);
}

int Mui_painter::text_idx(const std::string& s, int x, int y, Mui_font_role role, uint8 fg, uint8 bg) {
	const Mui_text_font* f = font(role);
	return f ? f->draw(buf, s, x, y, fg, bg) : 0;
}

int Mui_painter::text(const std::string& s, int x, int y, Mui_font_role role, Mui_colour fg, Mui_colour bg) {
	return text_idx(s, x, y, role, col(fg), col(bg));
}

void Mui_painter::text_in_idx(const std::string& s, const TileRect& r, Mui_font_role role, uint8 fg, uint8 bg, Mui_align align) {
	const Mui_text_font* f = font(role);
	if (!f) {
		return;
	}
	const std::string t = f->elide(s, r.w);
	const int         w = f->width(t);
	int               x = r.x;
	if (align == Mui_align::centre) {
		x = r.x + (r.w - w) / 2;
	} else if (align == Mui_align::right) {
		x = r.x + r.w - w;
	}
	const int y = r.y + (r.h - f->height()) / 2;
	push_clip(r);
	f->draw(buf, t, x, y, fg, bg);
	pop_clip();
}

void Mui_painter::text_in(
		const std::string& s, const TileRect& r, Mui_font_role role, Mui_colour fg, Mui_colour bg, Mui_align align) {
	text_in_idx(s, r, role, col(fg), col(bg), align);
}

int Mui_painter::text_width(const std::string& s, Mui_font_role role) const {
	const Mui_text_font* f = font(role);
	return f ? f->width(s) : 0;
}

int Mui_painter::line_height(Mui_font_role role) const {
	const Mui_text_font* f = font(role);
	return f ? f->height() : 0;
}
