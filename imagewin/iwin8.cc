/**
 ** Iwin8.h - 8-bit image window.
 **
 ** Written: 8/13/98 - JSF
 **/

/*
Copyright (C) 1998 Jeffrey S. Freedman
Copyright (C) 2000-2022 The Exult Team

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

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

#include "common_types.h"
#include "gamma.h"
#include "iwin8.h"

#include <algorithm>
#include <climits>
#include <cstring>

using std::make_unique;
using std::rotate;
using std::unique_ptr;

GammaTable<uint8> Image_window8::GammaRed(256);
GammaTable<uint8> Image_window8::GammaBlue(256);
GammaTable<uint8> Image_window8::GammaGreen(256);

Image_window8::Image_window8(
		unsigned int w, unsigned int h, unsigned int gwidth, unsigned int gheight, int scl, bool fs, int sclr,
		Image_window::FillMode fillmode, unsigned int fillsclr)
		: Image_window(new Image_buffer8(0, 0, nullptr), w, h, gwidth, gheight, scl, fs, sclr, fillmode, fillsclr) {
	ib8 = static_cast<Image_buffer8*>(ibuf);
}

void Image_window8::get_gamma(double& r, double& g, double& b) {
	r = GammaRed.get_gamma();
	g = GammaGreen.get_gamma();
	b = GammaBlue.get_gamma();
}

void Image_window8::set_gamma(double r, double g, double b) {
	GammaRed.set_gamma(r);
	GammaGreen.set_gamma(g);
	GammaBlue.set_gamma(b);
}

/*
 *  Convert rgb value.
 */

inline unsigned char Get_color8(
		unsigned char val, int maxval,
		int brightness    // 100=normal.
) {
	const uint32 c = (static_cast<uint32>(val) * brightness * 255L) / (100 * maxval);
	return c <= 255L ? static_cast<unsigned char>(c) : 255;
}

/*
 *  Set palette.
 */

void Image_window8::set_palette(
		const unsigned char* rgbs,         // 256 3-byte entries.
		int                  maxval,       // Highest val. for each color.
		int                  brightness    // Brightness control (100 = normal).
) {
	// Get the colors.
	SDL_Color colors2[256];
	for (int i = 0; i < 256; i++) {
		colors2[i].r = colors[i * 3] = GammaRed[Get_color8(rgbs[3 * i], maxval, brightness)];
		colors2[i].g = colors[i * 3 + 1] = GammaGreen[Get_color8(rgbs[3 * i + 1], maxval, brightness)];
		colors2[i].b = colors[i * 3 + 2] = GammaBlue[Get_color8(rgbs[3 * i + 2], maxval, brightness)];
	}
	if (paletted_surface) {
		if (SDL_Palette* paletted_surface_palette = SDL_GetSurfacePalette(paletted_surface)) {
			SDL_SetPaletteColors(paletted_surface_palette, colors2, 0, 256);
		}
	}
	if (paletted_surface != draw_surface && draw_surface) {
		if (SDL_Palette* draw_surface_palette = SDL_GetSurfacePalette(draw_surface)) {
			SDL_SetPaletteColors(draw_surface_palette, colors2, 0, 256);
		}
	}
	// Layer textures are palette snapshots; refresh them.
	mark_all_layers_dirty();
}

void Image_window8::apply_gamma_palette(
		const unsigned char* rgbs, int maxval, int brightness, std::array<unsigned char, 768>& out) const {
	for (int i = 0; i < 256; i++) {
		out[3 * i]     = GammaRed[Get_color8(rgbs[3 * i], maxval, brightness)];
		out[3 * i + 1] = GammaGreen[Get_color8(rgbs[3 * i + 1], maxval, brightness)];
		out[3 * i + 2] = GammaBlue[Get_color8(rgbs[3 * i + 2], maxval, brightness)];
	}
}

/*
 *  Rotate a range of colors.
 */

void Image_window8::rotate_colors(
		int first,    // Palette index of 1st.
		int num,      // # in range.
		int upd       // 1 to update hardware palette.
) {
	first *= 3;
	num *= 3;
	const int      cnt    = abs(num);
	unsigned char* start  = colors + first;
	unsigned char* finish = start + cnt;
	if (num > 0) {
		// Shift upward.
		rotate(start, finish - 3, finish);
	} else {
		// Shift downward.
		rotate(start, start + 3, finish);
	}

	if (upd) {    // Take effect now?
		SDL_Color colors2[256];
		for (int i = 0; i < 256; i++) {
			colors2[i].r = colors[i * 3];
			colors2[i].g = colors[i * 3 + 1];
			colors2[i].b = colors[i * 3 + 2];
		}
		if (paletted_surface) {
			if (SDL_Palette* paletted_surface_palette = SDL_GetSurfacePalette(paletted_surface)) {
				SDL_SetPaletteColors(paletted_surface_palette, colors2, 0, 256);
			}
		}
		if (paletted_surface != draw_surface && draw_surface) {
			if (SDL_Palette* draw_surface_palette = SDL_GetSurfacePalette(draw_surface)) {
				SDL_SetPaletteColors(draw_surface_palette, colors2, 0, 256);
			}
		}
	}
	// Layer textures are palette snapshots; refresh them. The dirty
	// flag coalesces, so several rotate calls cost only one re-convert.
	mark_all_layers_dirty();
}

static inline int pow2(int x) {
	return x * x;
}

// a nearest-average-colour 1/3 scaler
unique_ptr<unsigned char[]> Image_window8::mini_screenshot() {
	int i;
	if (!paletted_surface) {
		return nullptr;
	}

	auto                 buf    = make_unique<Uint8[]>(96 * 60);
	const int            w      = 3 * 96;
	const int            h      = 3 * 60;
	const unsigned char* pixels = ibuf->get_bits();
	const int            pitch  = ibuf->get_line_width();

	for (int y = 0; y < h; y += 3) {
		for (int x = 0; x < w; x += 3) {
			// calculate average colour
			int r = 0;
			int g = 0;
			int b = 0;
			for (i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					const int pix = pixels[pitch * (j + y + (get_game_height() - h) / 2) + i + x + (get_game_width() - w) / 2];
					r += colors[3 * pix + 0];
					g += colors[3 * pix + 1];
					b += colors[3 * pix + 2];
				}
			}
			r = r / 9;
			g = g / 9;
			b = b / 9;

			// find nearest-colour in non-rotating palette
			int bestdist  = INT_MAX;
			int bestindex = -1;
			for (i = 0; i < 224; i++) {
				const int dist = pow2(colors[3 * i + 0] - r) + pow2(colors[3 * i + 1] - g) + pow2(colors[3 * i + 2] - b);
				if (dist < bestdist) {
					bestdist  = dist;
					bestindex = i;
				}
			}
			buf[y * w / 9 + x / 3] = bestindex;
		}
	}

	return buf;
}

/*
 *  ARGB for one palette index within a layer.
 */
uint32 Image_window8::layer_argb_pixel(const Layer& layer, unsigned char p) const {
	if (!layer.is_opaque() && p == layer.get_transparent()) {
		return 0;    // Fully transparent.
	}
	if (!layer.index_argb.empty() && layer.index_argb[p] != 0) {
		// Translucent colour: keep its alpha, gamma-correct the RGB so it
		// matches the (gamma-corrected) opaque colours on screen.
		const uint32 argb = layer.index_argb[p];
		const uint32 a    = (argb >> 24) & 0xff;
		const uint32 r    = GammaRed[(argb >> 16) & 0xff];
		const uint32 g    = GammaGreen[(argb >> 8) & 0xff];
		const uint32 b    = GammaBlue[argb & 0xff];
		return (a << 24) | (r << 16) | (g << 8) | b;
	}
	// Use the layer's fixed-palette override if one is set, else the live one.
	const std::vector<unsigned char>& ov  = get_ui_cfg(layer.ui_kind).ui_palette_colors;
	const unsigned char*              pal = ov.empty() ? colors : ov.data();
	return (static_cast<uint32>(0xff) << 24) | (static_cast<uint32>(pal[3 * p]) << 16) | (static_cast<uint32>(pal[3 * p + 1]) << 8)
		   | static_cast<uint32>(pal[3 * p + 2]);
}

/*
 *  Convert a layer's 8-bit paletted pixels into its ARGB texture,
 *  using the current palette.  The layer's transparent index is written as
 *  fully transparent so the scene shows through.
 */
void Image_window8::refresh_layer(Layer& layer) {
	if (layer.texture == nullptr || !layer.buf) {
		return;
	}
	if (layer.render_scale > 1 && refresh_layer_scaled(layer, layer.render_scale)) {
		return;
	}
	const int            w      = layer.get_width();
	const int            h      = layer.get_height();
	Image_buffer*        b      = layer.get_ibuf();
	const unsigned char* src    = b->get_bits();
	const int            pitch  = static_cast<int>(b->get_line_width());
	auto                 pixels = make_unique<uint32[]>(static_cast<size_t>(w) * h);
	for (int y = 0; y < h; y++) {
		const unsigned char* srow = src + static_cast<size_t>(y) * pitch;
		uint32*              drow = pixels.get() + static_cast<size_t>(y) * w;
		for (int x = 0; x < w; x++) {
			drow[x] = layer_argb_pixel(layer, srow[x]);
		}
	}
	SDL_UpdateTexture(layer.texture, nullptr, pixels.get(), w * static_cast<int>(sizeof(uint32)));
}

/*
 *  Pre-scale a layer with the current (member) game scaler by 'factor' and
 *  upload the result, preserving transparency and translucency. The colour
 *  is scaled by the scaler (via a guard-banded 8-bit surface whose palette
 *  carries the translucent blend colours); the alpha is sampled from the
 *  source so it stays crisp and masks any colour bleed at transparent edges.
 */
bool Image_window8::refresh_layer_scaled(Layer& layer, int factor) {
	const int            gb     = guard_band;    // Scaler reads a padded border.
	const int            logw   = layer.get_width();
	const int            logh   = layer.get_height();
	Image_buffer*        b      = layer.get_ibuf();
	const unsigned char* src    = b->get_bits();
	const int            spitch = static_cast<int>(b->get_line_width());
	const unsigned char  transp = layer.get_transparent();
	const bool           has_ov = !layer.index_argb.empty();
	const uint32*        ov     = has_ov ? layer.index_argb.data() : nullptr;
	const int            tex_w  = logw * factor;
	const int            tex_h  = logh * factor;
	// Fixed-palette override for this layer, if any (else the live palette).
	const std::vector<unsigned char>& pal_ov      = get_ui_cfg(layer.ui_kind).ui_palette_colors;
	const unsigned char*              palette_rgb = pal_ov.empty() ? colors : pal_ov.data();

	// A fully opaque layer (a full-screen scene) has no transparent index and no
	// translucency, so skip the transparency matting entirely: scale the colours
	// once through the game scaler and mark every pixel opaque. Otherwise the
	// scene's legitimate use of the 'transparent' index (e.g. index 255 in an
	// FLI frame) would be matted away to see-through.
	if (layer.is_opaque()) {
		const int    ossw  = ((logw + 3) & ~3) + 2 * gb;
		const int    ossh  = logh + 2 * gb;
		SDL_Surface* osrc8 = SDL_CreateSurface(ossw, ossh, SDL_PIXELFORMAT_INDEX8);
		SDL_Surface* odst  = SDL_CreateSurface(ossw * factor, ossh * factor, SDL_PIXELFORMAT_ARGB8888);
		bool         done  = false;
		if (osrc8 && odst) {
			SDL_Palette* sdl_pal = SDL_CreateSurfacePalette(osrc8);
			if (sdl_pal) {
				SDL_Color cols[256];
				for (int i = 0; i < 256; i++) {
					cols[i].r = palette_rgb[3 * i];
					cols[i].g = palette_rgb[3 * i + 1];
					cols[i].b = palette_rgb[3 * i + 2];
					cols[i].a = 255;
				}
				SDL_SetPaletteColors(sdl_pal, cols, 0, 256);
				uint8*    sp       = static_cast<uint8*>(osrc8->pixels);
				const int sp_pitch = osrc8->pitch;
				// Copy content at (gb,gb), replicating edge pixels into the guard
				// band so the scaler does not bleed a stray colour at the border.
				for (int y = 0; y < logh; y++) {
					uint8*               drow = sp + static_cast<size_t>(y + gb) * sp_pitch;
					const unsigned char* srow = src + static_cast<size_t>(y) * spitch;
					memset(drow, srow[0], gb);
					memcpy(drow + gb, srow, logw);
					memset(drow + gb + logw, srow[logw - 1], ossw - gb - logw);
				}
				for (int y = 0; y < gb; y++) {
					memcpy(sp + static_cast<size_t>(y) * sp_pitch, sp + static_cast<size_t>(gb) * sp_pitch, ossw);
				}
				for (int y = logh + gb; y < ossh; y++) {
					memcpy(sp + static_cast<size_t>(y) * sp_pitch, sp + static_cast<size_t>(logh + gb - 1) * sp_pitch, ossw);
				}
				if (scale_layer_color(layer, osrc8, logw, logh, odst)) {
					auto          texpix = make_unique<uint32[]>(static_cast<size_t>(tex_w) * tex_h);
					const uint32* pix    = static_cast<const uint32*>(odst->pixels);
					const size_t  dpitch = static_cast<size_t>(odst->pitch) / sizeof(uint32);
					const size_t  sgb    = static_cast<size_t>(factor) * gb;
					for (int y = 0; y < tex_h; y++) {
						const uint32* row  = pix + (static_cast<size_t>(y) + sgb) * dpitch + sgb;
						uint32*       trow = texpix.get() + static_cast<size_t>(y) * tex_w;
						for (int x = 0; x < tex_w; x++) {
							trow[x] = 0xff000000u | (row[x] & 0x00ffffffu);
						}
					}
					SDL_UpdateTexture(layer.texture, nullptr, texpix.get(), tex_w * static_cast<int>(sizeof(uint32)));
					done = true;
				}
			}
		}
		if (odst) {
			SDL_DestroySurface(odst);
		}
		if (osrc8) {
			SDL_DestroySurface(osrc8);
		}
		if (done) {
			return true;
		}
		// If the fast path failed, fall through to the normal (matting) path.
	}

	// Guard-banded 8-bit source (content at (gb,gb)) and a scaled 32-bit dest.
	const int    ssw    = ((logw + 3) & ~3) + 2 * gb;
	const int    ssh    = logh + 2 * gb;
	SDL_Surface* src8   = SDL_CreateSurface(ssw, ssh, SDL_PIXELFORMAT_INDEX8);
	SDL_Surface* dst32  = SDL_CreateSurface(ssw * factor, ssh * factor, SDL_PIXELFORMAT_ARGB8888);
	SDL_Surface* dst32b = SDL_CreateSurface(ssw * factor, ssh * factor, SDL_PIXELFORMAT_ARGB8888);
	SDL_Surface* dst32c = nullptr;
	bool         ok     = false;
	bool         have3  = false;
	if (src8 && dst32 && dst32b) {
		SDL_Palette* sdl_pal = SDL_CreateSurfacePalette(src8);
		if (sdl_pal) {
			SDL_Color cols[256];
			for (int i = 0; i < 256; i++) {
				if (has_ov && ov[i] != 0) {    // Translucent slot -> blend colour.
					cols[i].r = GammaRed[(ov[i] >> 16) & 0xff];
					cols[i].g = GammaGreen[(ov[i] >> 8) & 0xff];
					cols[i].b = GammaBlue[ov[i] & 0xff];
				} else {
					cols[i].r = palette_rgb[3 * i];
					cols[i].g = palette_rgb[3 * i + 1];
					cols[i].b = palette_rgb[3 * i + 2];
				}
				cols[i].a = 255;
			}
			SDL_SetPaletteColors(sdl_pal, cols, 0, 256);
			// Pad with the transparent index, then copy the content.
			uint8*    sp       = static_cast<uint8*>(src8->pixels);
			const int sp_pitch = src8->pitch;
			for (int y = 0; y < ssh; y++) {
				memset(sp + static_cast<size_t>(y) * sp_pitch, transp, ssw);
			}
			for (int y = 0; y < logh; y++) {
				memcpy(sp + static_cast<size_t>(y + gb) * sp_pitch + gb, src + static_cast<size_t>(y) * spitch, logw);
			}
			// CONSENSUS DIFFERENCE MATTING: run the colour scaler with a pure
			// red, then a pure green fill under the transparent index. Wherever
			// the two outputs differ, the difference measures how much
			// transparent colour the scaler mixed into that pixel; from that we
			// recover the TRUE un-bled colour and the scaler's own smooth edge
			// coverage. Two passes are enough when they agree everywhere; only
			// if some pixel disagrees (the shape contains a colour close to red
			// or green, making the edge-directed scalers branch differently in
			// that pass) do we run a THIRD pass with a blue fill, and each
			// pixel then picks whichever PAIR of passes agrees best. So the
			// common case costs two scaler runs, and tricky shapes three.
			SDL_Color fill;
			fill.a = 255;
			fill.r = 255;
			fill.g = 0;
			fill.b = 0;    // Pass 1: red under transparency.
			SDL_SetPaletteColors(sdl_pal, &fill, transp, 1);
			const bool ok1 = scale_layer_color(layer, src8, logw, logh, dst32);
			fill.r         = 0;
			fill.g         = 255;
			fill.b         = 0;    // Pass 2: green under transparency.
			SDL_SetPaletteColors(sdl_pal, &fill, transp, 1);
			const bool ok2 = scale_layer_color(layer, src8, logw, logh, dst32b);
			ok             = ok1 && ok2;
			if (ok) {
				// Pre-scan: do the red/green passes disagree anywhere? (Early
				// exit on the first such pixel; this is far cheaper than an
				// unconditional third scaler run.)
				const uint32* pix1             = static_cast<const uint32*>(dst32->pixels);
				const uint32* pix2             = static_cast<const uint32*>(dst32b->pixels);
				const size_t  dpitch           = static_cast<size_t>(dst32->pitch) / sizeof(uint32);
				const size_t  scaled_guardband = static_cast<size_t>(factor) * static_cast<size_t>(gb);
				bool          needs3           = false;
				for (int y = 0; y < tex_h && !needs3; y++) {
					const size_t  roff = (static_cast<size_t>(y) + scaled_guardband) * dpitch + scaled_guardband;
					const uint32* row1 = pix1 + roff;
					const uint32* row2 = pix2 + roff;
					for (int x = 0; x < tex_w; x++) {
						const uint32 p1   = row1[x];
						const uint32 p2   = row2[x];
						const int    t12a = static_cast<int>((p1 >> 16) & 0xff) - static_cast<int>((p2 >> 16) & 0xff);
						const int    t12b = static_cast<int>((p2 >> 8) & 0xff) - static_cast<int>((p1 >> 8) & 0xff);
						if (std::abs(t12a - t12b) > 96) {
							needs3 = true;
							break;
						}
					}
				}
				if (needs3) {
					dst32c = SDL_CreateSurface(ssw * factor, ssh * factor, SDL_PIXELFORMAT_ARGB8888);
					if (dst32c) {
						fill.r = 0;
						fill.g = 0;
						fill.b = 255;    // Pass 3: blue under transparency.
						SDL_SetPaletteColors(sdl_pal, &fill, transp, 1);
						have3 = scale_layer_color(layer, src8, logw, logh, dst32c);
					}
				}
			}
		}
	}

	auto texpix = make_unique<uint32[]>(static_cast<size_t>(tex_w) * tex_h);
	if (ok) {
		const uint32* pix1             = static_cast<const uint32*>(dst32->pixels);
		const uint32* pix2             = static_cast<const uint32*>(dst32b->pixels);
		const uint32* pix3             = have3 ? static_cast<const uint32*>(dst32c->pixels) : nullptr;
		const size_t  dpitch           = static_cast<size_t>(dst32->pitch) / sizeof(uint32);
		const size_t  scaled_guardband = static_cast<size_t>(factor) * static_cast<size_t>(gb);
		for (int y = 0; y < tex_h; y++) {
			const int            sy   = y / factor;
			const size_t         roff = (static_cast<size_t>(y) + scaled_guardband) * dpitch + scaled_guardband;
			const uint32*        row1 = pix1 + roff;
			const uint32*        row2 = pix2 + roff;
			const uint32*        row3 = have3 ? pix3 + roff : nullptr;
			const unsigned char* srow = src + static_cast<size_t>(sy) * spitch;
			uint32*              trow = texpix.get() + static_cast<size_t>(y) * tex_w;
			for (int x = 0; x < tex_w; x++) {
				const unsigned char idx = srow[x / factor];
				const uint32        p1  = row1[x];
				const uint32        p2  = row2[x];
				const int           r1  = (p1 >> 16) & 0xff;
				const int           g1  = (p1 >> 8) & 0xff;
				const int           b1  = p1 & 0xff;
				const int           r2  = (p2 >> 16) & 0xff;
				const int           g2  = (p2 >> 8) & 0xff;
				const int           b2  = p2 & 0xff;
				// For each pair of passes, two independent estimates of the
				// transparent fraction t (0..255) from the channels where the
				// fills differ by 255. If a pair's estimates agree, both its
				// passes made identical scaling decisions and its t is exact.
				// Pass fills: 1=red, 2=green, 3=blue (pass 3 only when needed).
				const int t12a = r1 - r2, t12b = g2 - g1;    // pair (1,2)
				int       t2sum = t12a + t12b;               // Sum of the two estimates (= 2*t).
				int       use   = 1;                         // Which pass to un-blend from (1 or 2).
				int       sbest = std::abs(t12a - t12b);
				if (have3) {
					const uint32 p3   = row3[x];
					const int    r3   = (p3 >> 16) & 0xff;
					const int    g3   = (p3 >> 8) & 0xff;
					const int    b3   = p3 & 0xff;
					const int    t13a = r1 - r3, t13b = b3 - b1;    // pair (1,3)
					const int    t23a = g2 - g3, t23b = b3 - b2;    // pair (2,3)
					const int    s13 = std::abs(t13a - t13b);
					const int    s23 = std::abs(t23a - t23b);
					if (s13 < sbest) {
						sbest = s13;
						t2sum = t13a + t13b;
						use   = 1;
					}
					if (s23 < sbest) {
						sbest = s23;
						t2sum = t23a + t23b;
						use   = 2;
					}
				}
				int    cov;
				uint32 rgb;
				if (sbest > 96) {
					// All pairs disagreed: fall back to the crisp source pixel.
					cov = idx == transp ? 0 : 255;
					rgb = layer_argb_pixel(layer, idx) & 0x00ffffffu;
				} else {
					const int t = std::max(0, std::min(255, (t2sum + 1) / 2));
					cov         = 255 - t;
					if (cov <= 8) {    // (Nearly) fully transparent.
						cov = 0;
						rgb = 0;
					} else {
						// Un-blend: out = c*cov/255 + fill*t/255, fill is pure
						// red (pass 1) or pure green (pass 2).
						const int t255 = 255 - cov;
						int       cR, cG, cB;
						if (use == 1) {
							cR = (r1 - t255) * 255 / cov;
							cG = g1 * 255 / cov;
							cB = b1 * 255 / cov;
						} else {
							cR = r2 * 255 / cov;
							cG = (g2 - t255) * 255 / cov;
							cB = b2 * 255 / cov;
						}
						cR  = std::max(0, std::min(255, cR));
						cG  = std::max(0, std::min(255, cG));
						cB  = std::max(0, std::min(255, cB));
						rgb = (static_cast<uint32>(cR) << 16) | (static_cast<uint32>(cG) << 8) | static_cast<uint32>(cB);
					}
				}
				// Intrinsic (translucency) alpha of the underlying source pixel,
				// so translucent layers stay translucent. The coverage keeps the
				// scaler's own smooth (anti-aliased) edge.
				int intrinsic;
				if (idx != transp && has_ov && ov[idx] != 0) {
					intrinsic = (ov[idx] >> 24) & 0xff;
				} else {
					intrinsic = 255;    // Opaque (or an outer AA pixel).
				}
				const uint32 a = static_cast<uint32>(cov * intrinsic / 255);
				trow[x]        = (a << 24) | rgb;
			}
		}
	} else {
		// Fallback: nearest-neighbour upscale of the 1:1 conversion.
		for (int y = 0; y < tex_h; y++) {
			const unsigned char* srow = src + static_cast<size_t>(y / factor) * spitch;
			uint32*              trow = texpix.get() + static_cast<size_t>(y) * tex_w;
			for (int x = 0; x < tex_w; x++) {
				trow[x] = layer_argb_pixel(layer, srow[x / factor]);
			}
		}
	}
	SDL_UpdateTexture(layer.texture, nullptr, texpix.get(), tex_w * static_cast<int>(sizeof(uint32)));
	if (dst32c) {
		SDL_DestroySurface(dst32c);
	}
	if (dst32b) {
		SDL_DestroySurface(dst32b);
	}
	if (dst32) {
		SDL_DestroySurface(dst32);
	}
	if (src8) {
		SDL_DestroySurface(src8);
	}
	return true;
}
