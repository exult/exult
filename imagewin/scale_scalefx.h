/*
 * ScaleFX CPU implementation of the five-pass shader by Sp00kyFox.
 * Reference: libretro/glsl-shaders/scalefx (2017-03-01), default settings:
 * SFX_CLR=0.5, SFX_SAA=1, SFX_SCN=1. Native output is 3x.
 * Intermediate flags and source selectors are kept as integers instead of
 * packing them into normalized shader render targets.
 *
 * Copyright (c) 2016 Sp00kyFox - ScaleFX@web.de

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

#ifndef EXULT_SCALE_SCALEFX_H
#define EXULT_SCALE_SCALEFX_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ScaleFX {
	constexpr int factor = 3;
	// Pass 0 reads one neighbour, passes 1 and 2 add one each, and pass 3
	// examines flags three pixels away. Both dirty updates and input halos
	// must cover this dependency radius.
	constexpr int radius = 6;

	struct RGB {
		uint8_t r, g, b;
	};

	namespace detail {
		template <typename T>
		struct Quad {
			T x{}, y{}, z{}, w{};
			Quad() = default;

			Quad(T a, T b, T c, T d) : x(a), y(b), z(c), w(d) {}

			T operator[](int i) const {
				return i == 0 ? x : i == 1 ? y : i == 2 ? z : w;
			}
		};

		using Float4 = Quad<float>;
		using Bool4  = Quad<bool>;
		using Codes4 = Quad<uint8_t>;

		struct Pair {
			bool x, y;

			Pair(bool a, bool b) : x(a), y(b) {}
		};

		struct Flags {
			Bool4 corner, horizontal, vertical, orientation;
		};

		template <typename T>
		struct Grid {
			int            width, height;
			std::vector<T> data;

			Grid(int w, int h) : width(w), height(h), data(size_t(w) * size_t(h)) {}

			const T& get(int x, int y) const {
				x = std::clamp(x, 0, width - 1);
				y = std::clamp(y, 0, height - 1);
				return data[size_t(y) * size_t(width) + size_t(x)];
			}

			T& at(int x, int y) {
				return data[size_t(y) * size_t(width) + size_t(x)];
			}
		};

		inline float distance(RGB a, RGB b) {
			const float r  = float(a.r + b.r) / 510.0f;
			const float dr = float(a.r) / 255.0f - float(b.r) / 255.0f;
			const float dg = float(a.g) / 255.0f - float(b.g) / 255.0f;
			const float db = float(a.b) / 255.0f - float(b.b) / 255.0f;
			return std::sqrt((2.0f + r) * dr * dr + 4.0f * dg * dg + (3.0f - r) * db * db) / 3.0f;
		}

		inline float strength(float d, float ax, float ay, float bx, float by) {
			const float diff    = ax - ay;
			const float weight1 = std::max(0.5f - d, 0.0f) / 0.5f;
			const float weight2
					= std::clamp((1.0f - d) + (std::min(ax, bx) + ax > std::min(ay, by) + ay ? diff : -diff), 0.0f, 1.0f);
			return (weight1 * weight2) * (ax * ay);
		}

		inline bool clear(float cx, float cy, float ax, float ay, float bx, float by) {
			return cx >= std::max(std::min(ax, ay), std::min(bx, by)) && cy >= std::max(std::min(ax, by), std::min(bx, ay));
		}

		inline Bool4 dominance(const Float4& a, const Float4& b, const Float4& c, const Float4& d) {
			// Each junction meets the SE, SW, NW and NE corners of four pixels.
			const float v[4] = {2 * a.z - (a.y + a.w), 2 * b.w - (b.z + b.x), 2 * c.x - (c.w + c.y), 2 * d.y - (d.x + d.z)};
			auto        vote = [&](int i) {
				return v[i] > 0
					   && ((v[(i + 1) % 4] <= 0 && v[(i + 3) % 4] <= 0) || v[i] + v[(i + 2) % 4] > v[(i + 1) % 4] + v[(i + 3) % 4]);
			};
			return {vote(0), vote(1), vote(2), vote(3)};
		}

		inline bool inject(const Bool4& votes, const Float4& strengths, int i) {
			return votes[i]
				   || (!votes[(i + 1) % 4] && !votes[(i + 3) % 4] && strengths[i] > 0
					   && (votes[(i + 2) % 4]
						   || strengths[i] + strengths[(i + 2) % 4] > strengths[(i + 1) % 4] + strengths[(i + 3) % 4]));
		}

		inline std::array<uint8_t, 9> selectors(const Grid<Flags>& flags, int x, int y) {
			const auto& E  = flags.get(x, y);
			const auto& D  = flags.get(x - 1, y);
			const auto& D0 = flags.get(x - 2, y);
			const auto& D1 = flags.get(x - 3, y);
			const auto& F  = flags.get(x + 1, y);
			const auto& F0 = flags.get(x + 2, y);
			const auto& F1 = flags.get(x + 3, y);
			const auto& B  = flags.get(x, y - 1);
			const auto& B0 = flags.get(x, y - 2);
			const auto& B1 = flags.get(x, y - 3);
			const auto& H  = flags.get(x, y + 1);
			const auto& H0 = flags.get(x, y + 2);
			const auto& H1 = flags.get(x, y + 3);
			const Bool4 Ec = E.corner, Eh = E.horizontal, Ev = E.vertical, Eo = E.orientation;
			const Bool4 Dc = D.corner, Dh = D.horizontal, Do = D.orientation, D0c = D0.corner, D0h = D0.horizontal,
						D1h = D1.horizontal;
			const Bool4 Fc = F.corner, Fh = F.horizontal, Fo = F.orientation, F0c = F0.corner, F0h = F0.horizontal,
						F1h = F1.horizontal;
			const Bool4 Bc = B.corner, Bv = B.vertical, Bo = B.orientation, B0c = B0.corner, B0v = B0.vertical, B1v = B1.vertical;
			const Bool4 Hc = H.corner, Hv = H.vertical, Ho = H.orientation, H0c = H0.corner, H0v = H0.vertical, H1v = H1.vertical;
			// lvl1 corners (hori, vert)
			bool lvl1x = Ec.x;
			bool lvl1y = Ec.y;
			bool lvl1z = Ec.z;
			bool lvl1w = Ec.w;

			// lvl2 mid (left, right / up, down)
			Pair lvl2x = Pair(((Ec.x && Eh.y)) && Dc.z, ((Ec.y && Eh.x)) && Fc.w);
			Pair lvl2y = Pair((Ec.y && Ev.z) && Bc.w, (Ec.z && Ev.y) && Hc.x);
			Pair lvl2z = Pair((Ec.w && Eh.z) && Dc.y, (Ec.z && Eh.w) && Fc.x);
			Pair lvl2w = Pair((Ec.x && Ev.w) && Bc.z, (Ec.w && Ev.x) && Hc.y);

			// lvl3 corners (hori, vert)
			Pair lvl3x = Pair(lvl2x.y && (Dh.y && Dh.x) && Fh.z, lvl2w.y && (Bv.w && Bv.x) && Hv.z);
			Pair lvl3y = Pair(lvl2x.x && (Fh.x && Fh.y) && Dh.w, lvl2y.y && (Bv.z && Bv.y) && Hv.w);
			Pair lvl3z = Pair(lvl2z.x && (Fh.w && Fh.z) && Dh.x, lvl2y.x && (Hv.y && Hv.z) && Bv.x);
			Pair lvl3w = Pair(lvl2z.y && (Dh.z && Dh.w) && Fh.y, lvl2w.x && (Hv.x && Hv.w) && Bv.y);

			// lvl4 corners (hori, vert)
			Pair lvl4x
					= Pair((Dc.x && Dh.y && Eh.x && Eh.y && Fh.x && Fh.y) && (D0c.z && D0h.w),
						   (Bc.x && Bv.w && Ev.x && Ev.w && Hv.x && Hv.w) && (B0c.z && B0v.y));
			Pair lvl4y
					= Pair((Fc.y && Fh.x && Eh.y && Eh.x && Dh.y && Dh.x) && (F0c.w && F0h.z),
						   (Bc.y && Bv.z && Ev.y && Ev.z && Hv.y && Hv.z) && (B0c.w && B0v.x));
			Pair lvl4z
					= Pair((Fc.z && Fh.w && Eh.z && Eh.w && Dh.z && Dh.w) && (F0c.x && F0h.y),
						   (Hc.z && Hv.y && Ev.z && Ev.y && Bv.z && Bv.y) && (H0c.x && H0v.w));
			Pair lvl4w
					= Pair((Dc.w && Dh.z && Eh.w && Eh.z && Fh.w && Fh.z) && (D0c.y && D0h.x),
						   (Hc.w && Hv.x && Ev.w && Ev.x && Bv.w && Bv.x) && (H0c.y && H0v.z));

			// lvl5 mid (left, right / up, down)
			Pair lvl5x = Pair(lvl4x.x && (F0h.x && F0h.y) && (D1h.z && D1h.w), lvl4y.x && (D0h.y && D0h.x) && (F1h.w && F1h.z));
			Pair lvl5y = Pair(lvl4y.y && (H0v.y && H0v.z) && (B1v.w && B1v.x), lvl4z.y && (B0v.z && B0v.y) && (H1v.x && H1v.w));
			Pair lvl5z = Pair(lvl4w.x && (F0h.w && F0h.z) && (D1h.y && D1h.x), lvl4z.x && (D0h.z && D0h.w) && (F1h.x && F1h.y));
			Pair lvl5w = Pair(lvl4x.y && (H0v.x && H0v.w) && (B1v.z && B1v.y), lvl4w.y && (B0v.w && B0v.x) && (H1v.y && H1v.z));

			// lvl6 corners (hori, vert)
			Pair lvl6x = Pair(lvl5x.y && (D1h.y && D1h.x), lvl5w.y && (B1v.w && B1v.x));
			Pair lvl6y = Pair(lvl5x.x && (F1h.x && F1h.y), lvl5y.y && (B1v.z && B1v.y));
			Pair lvl6z = Pair(lvl5z.x && (F1h.w && F1h.z), lvl5y.x && (H1v.y && H1v.z));
			Pair lvl6w = Pair(lvl5z.y && (D1h.z && D1h.w), lvl5w.x && (H1v.x && H1v.w));

			// subpixels - 0 = E, 1 = D, 2 = D0, 3 = F, 4 = F0, 5 = B, 6 = B0, 7 = H, 8 = H0

			Codes4 crn;
			crn.x = ((lvl1x && Eo.x) || (lvl3x.x && Eo.y) || (lvl4x.x && Do.x) || (lvl6x.x && Fo.y)) ? 5
					: (lvl1x || (lvl3x.y && !Eo.w) || (lvl4x.y && !Bo.x) || (lvl6x.y && !Ho.w))      ? 1
					: lvl3x.x                                                                        ? 3
					: lvl3x.y                                                                        ? 7
					: lvl4x.x                                                                        ? 2
					: lvl4x.y                                                                        ? 6
					: lvl6x.x                                                                        ? 4
					: lvl6x.y                                                                        ? 8
																									 : 0;
			crn.y = ((lvl1y && Eo.y) || (lvl3y.x && Eo.x) || (lvl4y.x && Fo.y) || (lvl6y.x && Do.x)) ? 5
					: (lvl1y || (lvl3y.y && !Eo.z) || (lvl4y.y && !Bo.y) || (lvl6y.y && !Ho.z))      ? 3
					: lvl3y.x                                                                        ? 1
					: lvl3y.y                                                                        ? 7
					: lvl4y.x                                                                        ? 4
					: lvl4y.y                                                                        ? 6
					: lvl6y.x                                                                        ? 2
					: lvl6y.y                                                                        ? 8
																									 : 0;
			crn.z = ((lvl1z && Eo.z) || (lvl3z.x && Eo.w) || (lvl4z.x && Fo.z) || (lvl6z.x && Do.w)) ? 7
					: (lvl1z || (lvl3z.y && !Eo.y) || (lvl4z.y && !Ho.z) || (lvl6z.y && !Bo.y))      ? 3
					: lvl3z.x                                                                        ? 1
					: lvl3z.y                                                                        ? 5
					: lvl4z.x                                                                        ? 4
					: lvl4z.y                                                                        ? 8
					: lvl6z.x                                                                        ? 2
					: lvl6z.y                                                                        ? 6
																									 : 0;
			crn.w = ((lvl1w && Eo.w) || (lvl3w.x && Eo.z) || (lvl4w.x && Do.w) || (lvl6w.x && Fo.z)) ? 7
					: (lvl1w || (lvl3w.y && !Eo.x) || (lvl4w.y && !Ho.w) || (lvl6w.y && !Bo.x))      ? 1
					: lvl3w.x                                                                        ? 3
					: lvl3w.y                                                                        ? 5
					: lvl4w.x                                                                        ? 2
					: lvl4w.y                                                                        ? 8
					: lvl6w.x                                                                        ? 4
					: lvl6w.y                                                                        ? 6
																									 : 0;

			Codes4 mid;
			mid.x = ((lvl2x.x && Eo.x) || (lvl2x.y && Eo.y) || (lvl5x.x && Do.x) || (lvl5x.y && Fo.y)) ? 5
					: lvl2x.x                                                                          ? 1
					: lvl2x.y                                                                          ? 3
					: lvl5x.x                                                                          ? 2
					: lvl5x.y                                                                          ? 4
					: ((Ec.x && Dc.z && Ec.y && Fc.w))                                                 ? (Eo.x ? Eo.y ? 5 : 3 : 1)
																									   : 0;
			mid.y = ((lvl2y.x && !Eo.y) || (lvl2y.y && !Eo.z) || (lvl5y.x && !Bo.y) || (lvl5y.y && !Ho.z)) ? 3
					: lvl2y.x                                                                              ? 5
					: lvl2y.y                                                                              ? 7
					: lvl5y.x                                                                              ? 6
					: lvl5y.y                                                                              ? 8
					: ((Ec.y && Bc.w && Ec.z && Hc.x)) ? (!Eo.y ? !Eo.z ? 3 : 7 : 5)
													   : 0;
			mid.z = ((lvl2z.x && Eo.w) || (lvl2z.y && Eo.z) || (lvl5z.x && Do.w) || (lvl5z.y && Fo.z)) ? 7
					: lvl2z.x                                                                          ? 1
					: lvl2z.y                                                                          ? 3
					: lvl5z.x                                                                          ? 2
					: lvl5z.y                                                                          ? 4
					: ((Ec.z && Fc.x && Ec.w && Dc.y))                                                 ? (Eo.z ? Eo.w ? 7 : 1 : 3)
																									   : 0;
			mid.w = ((lvl2w.x && !Eo.x) || (lvl2w.y && !Eo.w) || (lvl5w.x && !Bo.x) || (lvl5w.y && !Ho.w)) ? 1
					: lvl2w.x                                                                              ? 5
					: lvl2w.y                                                                              ? 7
					: lvl5w.x                                                                              ? 6
					: lvl5w.y                                                                              ? 8
					: ((Ec.w && Hc.y && Ec.x && Bc.z)) ? (!Eo.w ? !Eo.x ? 1 : 5 : 7)
													   : 0;

			return {crn.x, mid.x, crn.y, mid.w, 0, mid.y, crn.w, mid.z, crn.z};
		}
	}    // namespace detail

	// The input and output rectangles use absolute surface coordinates. Strides
	// are in pixels, not bytes. Texture reads clamp to the actual image boundary,
	// never row padding. No source colour is synthesized or blended.
	template <typename Pixel>
	void scale(
			const uint8_t* src, int stride, int width, int height, int x, int y, int w, int h, const std::array<RGB, 256>& palette,
			Pixel* dst, int dst_stride, const std::array<Pixel, 256>& colours) {
		if (width <= 0 || height <= 0 || w <= 0 || h <= 0) {
			return;
		}
		const int end_x = std::min(width, x + w), end_y = std::min(height, y + h);
		x = std::max(0, x);
		y = std::max(0, y);
		if (x >= end_x || y >= end_y) {
			return;
		}
		const int                    left = std::max(0, x - radius), top = std::max(0, y - radius);
		const int                    right = std::min(width, end_x + radius), bottom = std::min(height, end_y + radius);
		const int                    gw = right - left, gh = bottom - top;
		detail::Grid<detail::Float4> metric(gw, gh), strengths(gw, gh);
		detail::Grid<detail::Flags>  flags(gw, gh);
		auto                         sample = [&](int sx, int sy) {
			return src[size_t(std::clamp(sy, 0, height - 1)) * size_t(stride) + size_t(std::clamp(sx, 0, width - 1))];
		};
		// Cache palette-pair metrics only for the duration of this call, so palette
		// animations and multiple windows cannot reuse stale colours.
		std::array<float, 256 * 256> distances;
		distances.fill(-1.0f);
		auto dist = [&](uint8_t a, uint8_t b) {
			auto& value = distances[size_t(std::min(a, b)) * 256 + std::max(a, b)];
			if (value < 0) {
				value = detail::distance(palette[a], palette[b]);
			}
			return value;
		};
		// Pass 0: colour distances to NW, N, NE and E.
		for (int j = 0; j < gh; ++j) {
			for (int i = 0; i < gw; ++i) {
				const int  sx = left + i, sy = top + j;
				const auto e = sample(sx, sy);
				metric.at(i, j)
						= {dist(e, sample(sx - 1, sy - 1)), dist(e, sample(sx, sy - 1)), dist(e, sample(sx + 1, sy - 1)),
						   dist(e, sample(sx + 1, sy))};
			}
		}
		// Pass 1: strengths for NW, NE, SE and SW corners.
		for (int j = 0; j < gh; ++j) {
			for (int i = 0; i < gw; ++i) {
				const auto &A = metric.get(i - 1, j - 1), &B = metric.get(i, j - 1), &D = metric.get(i - 1, j),
						   &E = metric.get(i, j), &F = metric.get(i + 1, j), &G = metric.get(i - 1, j + 1),
						   &H = metric.get(i, j + 1), &I = metric.get(i + 1, j + 1);
				strengths.at(i, j)
						= {detail::strength(D.z, D.w, E.y, A.w, D.y), detail::strength(F.x, E.w, E.y, B.w, F.y),
						   detail::strength(H.z, E.w, H.y, H.w, I.y), detail::strength(H.x, D.w, H.y, G.w, G.y)};
			}
		}
		// Pass 2: resolve junction dominance, then mark orthogonal edges.
		for (int j = 0; j < gh; ++j) {
			for (int i = 0; i < gw; ++i) {
				const auto &A = metric.get(i - 1, j - 1), &B = metric.get(i, j - 1), &D = metric.get(i - 1, j),
						   &E = metric.get(i, j), &F = metric.get(i + 1, j), &G = metric.get(i - 1, j + 1),
						   &H = metric.get(i, j + 1), &I = metric.get(i + 1, j + 1);
				const auto &As = strengths.get(i - 1, j - 1), &Bs = strengths.get(i, j - 1), &Cs = strengths.get(i + 1, j - 1),
						   &Ds = strengths.get(i - 1, j), &Es = strengths.get(i, j), &Fs = strengths.get(i + 1, j),
						   &Gs = strengths.get(i - 1, j + 1), &Hs = strengths.get(i, j + 1), &Is = strengths.get(i + 1, j + 1);
				const auto          jx = detail::dominance(As, Bs, Es, Ds), jy = detail::dominance(Bs, Cs, Fs, Es),
									jz = detail::dominance(Es, Fs, Is, Hs), jw = detail::dominance(Ds, Es, Hs, Gs);
				const detail::Bool4 res
						= {detail::inject(jx, {As.z, Bs.w, Es.x, Ds.y}, 2), detail::inject(jy, {Bs.z, Cs.w, Fs.x, Es.y}, 3),
						   detail::inject(jz, {Es.z, Fs.w, Is.x, Hs.y}, 0), detail::inject(jw, {Ds.z, Es.w, Hs.x, Gs.y}, 1)};
				const detail::Bool4 corner
						= {res.x && (jx.z || !(res.w && res.y)), res.y && (jy.w || !(res.x && res.z)),
						   res.z && (jz.x || !(res.y && res.w)), res.w && (jw.y || !(res.z && res.x))};
				const detail::Bool4 clr
						= {detail::clear(D.z, E.x, D.w, E.y, A.w, D.y), detail::clear(F.x, E.z, E.w, E.y, B.w, F.y),
						   detail::clear(H.z, I.x, E.w, H.y, H.w, I.y), detail::clear(H.x, G.z, D.w, H.y, G.w, G.y)};
				const detail::Float4 horizontal = {std::min(D.w, A.w), std::min(E.w, B.w), std::min(E.w, H.w), std::min(D.w, G.w)},
									 vertical   = {std::min(E.y, D.y), std::min(E.y, F.y), std::min(H.y, I.y), std::min(H.y, G.y)};
				const detail::Bool4  hori       = {horizontal.x < vertical.x && clr.x, horizontal.y < vertical.y && clr.y,
												   horizontal.z < vertical.z && clr.z, horizontal.w < vertical.w && clr.w},
									 vert       = {horizontal.x > vertical.x && clr.x, horizontal.y > vertical.y && clr.y,
												   horizontal.z > vertical.z && clr.z, horizontal.w > vertical.w && clr.w},
									 orientation
									 = {horizontal.x + D.w > vertical.x + E.y, horizontal.y + E.w > vertical.y + E.y,
										horizontal.z + E.w > vertical.z + H.y, horizontal.w + D.w > vertical.w + H.y};
				flags.at(i, j) = {corner, hori, vert, orientation};
			}
		}
		// Passes 3 and 4: identify edges through level 6 and choose one of the
		// original palette colours for each of the nine output subpixels.
		constexpr int dx[9] = {0, -1, -2, 1, 2, 0, 0, 0, 0}, dy[9] = {0, 0, 0, 0, 0, -1, -2, 1, 2};
		for (int sy = y; sy < end_y; ++sy) {
			for (int sx = x; sx < end_x; ++sx) {
				const auto selected = detail::selectors(flags, sx - left, sy - top);
				for (int row = 0; row < factor; ++row) {
					for (int col = 0; col < factor; ++col) {
						const auto code = selected[size_t(row * factor + col)];
						dst[size_t(sy * factor + row) * size_t(dst_stride) + size_t(sx * factor + col)]
								= colours[sample(sx + dx[code], sy + dy[code])];
					}
				}
			}
		}
	}
}    // namespace ScaleFX
#endif
