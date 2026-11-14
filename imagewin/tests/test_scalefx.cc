/* ScaleFX regression tests. Run from the repository root; see README.md. */
#include "imagewin/scale_scalefx.h"
#ifdef SCALEFX_BASELINE_HEADER
#	include SCALEFX_BASELINE_HEADER
#endif

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

namespace {
	struct Fixture {
		int                           width, height;
		std::array<ScaleFX::RGB, 256> palette{};
		std::vector<uint8_t>          source, expected;
	};

	Fixture load(const std::string& path) {
		std::ifstream input(path);
		Fixture       result{};
		assert(input >> result.width >> result.height);
		for (int i = 0; i < 4; ++i) {
			unsigned r, g, b;
			assert(input >> r >> g >> b);
			result.palette[i] = {uint8_t(r), uint8_t(g), uint8_t(b)};
		}
		auto read = [&](std::vector<uint8_t>& data, int width, int height) {
			for (int y = 0; y < height; ++y) {
				std::string row;
				assert(input >> row);
				assert(row.size() == size_t(width));
				for (char c : row) {
					assert(c >= '0' && c <= '3');
					data.push_back(uint8_t(c - '0'));
				}
			}
		};
		read(result.source, result.width, result.height);
		read(result.expected, result.width * 3, result.height * 3);
		return result;
	}

	template <typename Pixel>
	void check(const Fixture& f, const std::array<Pixel, 256>& colours) {
		// Padded input rows must never be treated as image pixels. Output row
		// padding and red zones must remain untouched, including on tiny images.
		const int            src_stride = f.width + 7, dst_stride = f.width * 3 + 5;
		constexpr Pixel      sentinel = Pixel(0x5a5a);
		std::vector<uint8_t> src(size_t(src_stride * f.height), 255);
		for (int y = 0; y < f.height; ++y) {
			std::copy_n(f.source.begin() + y * f.width, f.width, src.begin() + y * src_stride);
		}
		std::vector<Pixel> full(size_t(dst_stride * f.height * 3 + 16), sentinel);
		auto               render = [&](std::vector<Pixel>& target, int x, int y, int w, int h) {
			ScaleFX::scale(
					src.data(), src_stride, f.width, f.height, x, y, w, h, f.palette, target.data() + 8, dst_stride, colours);
		};
		render(full, 0, 0, f.width, f.height);
		for (int y = 0; y < f.height * 3; ++y) {
			for (int x = 0; x < dst_stride; ++x) {
				const Pixel expected = x < f.width * 3 ? colours[f.expected[size_t(y * f.width * 3 + x)]] : sentinel;
				assert(full[size_t(8 + y * dst_stride + x)] == expected);
			}
		}
		for (int i = 0; i < 8; ++i) {
			assert(full[size_t(i)] == sentinel);
			assert(full[full.size() - 1 - i] == sentinel);
		}

		// Arbitrarily tiled partial updates must agree with the shader reference
		// everywhere, including tile seams and physical source boundaries.
		std::vector<Pixel> tiled(full.size(), sentinel);
		for (int y = 0; y < f.height; y += 3) {
			for (int x = 0; x < f.width; x += 5) {
				render(tiled, x, y, std::min(5, f.width - x), std::min(3, f.height - y));
			}
		}
		assert(tiled == full);

		// Drawing one rectangle must leave every pixel outside it unchanged.
		if (f.width > 2 && f.height > 2) {
			auto partial = std::vector<Pixel>(full.size(), sentinel);
			render(partial, 1, 1, 1, 1);
			for (int y = 0; y < f.height * 3; ++y) {
				for (int x = 0; x < dst_stride; ++x) {
					const auto i = size_t(8 + y * dst_stride + x);
					assert(partial[i] == ((x >= 3 && x < 6 && y >= 3 && y < 6) ? full[i] : sentinel));
				}
			}
		}

		// A source edit can affect output six pixels away. Repaint that damage
		// region and compare against a complete rendering of the edited frame.
		const int cx = f.width / 2, cy = f.height / 2;
		src[size_t(cy * src_stride + cx)] ^= 1;
		auto edited = std::vector<Pixel>(full.size(), sentinel);
		render(edited, 0, 0, f.width, f.height);
		const int left = std::max(0, cx - ScaleFX::radius), top = std::max(0, cy - ScaleFX::radius);
		render(tiled, left, top, std::min(f.width, cx + ScaleFX::radius + 1) - left,
			   std::min(f.height, cy + ScaleFX::radius + 1) - top);
		assert(tiled == edited);
	}

	void check_fixture(const Fixture& f) {
		std::array<uint32_t, 256> rgba{};
		std::array<uint16_t, 256> rgb565{}, rgb555{}, bgr555{};
		for (size_t i = 0; i < f.palette.size(); ++i) {
			const auto c = f.palette[i];
			rgba[i]      = 0xff000000u | (uint32_t(c.r) << 16) | (uint32_t(c.g) << 8) | c.b;
			rgb565[i]    = uint16_t((c.r >> 3) << 11 | (c.g >> 2) << 5 | (c.b >> 3));
			rgb555[i]    = uint16_t((c.r >> 3) << 10 | (c.g >> 3) << 5 | (c.b >> 3));
			bgr555[i]    = uint16_t((c.b >> 3) << 10 | (c.g >> 3) << 5 | (c.r >> 3));
		}
		check(f, rgba);
		check(f, rgb565);
		check(f, rgb555);
		check(f, bgr555);
	}
}    // namespace

namespace {
	template <typename Pixel>
	void native_checks(const Fixture& f, int factor, const std::array<Pixel, 256>& colours) {
		const int            stride = f.width + 5, pitch = f.width * factor + 7;
		constexpr Pixel      sentinel = Pixel(0x5a5a);
		std::vector<uint8_t> source(size_t(stride * f.height), 255);
		for (int y = 0; y < f.height; ++y) {
			std::copy_n(f.source.begin() + y * f.width, f.width, source.begin() + y * stride);
		}
		std::vector<Pixel> full(size_t(pitch * f.height * factor + 16), sentinel), tiled(full);
		ScaleFX::Workspace workspace;
		auto               render = [&](std::vector<Pixel>& out, int x, int y, int w, int h) {
			ScaleFX::scale(
					source.data(), stride, f.width, f.height, x, y, w, h, f.palette, out.data() + 8, pitch, colours, factor,
					workspace);
		};
		render(full, 0, 0, f.width, f.height);
		const auto* metric          = workspace.metric.data.data();
		const auto* flags           = workspace.flags.data.data();
		const auto  metric_capacity = workspace.metric.data.capacity();
		// A warm workspace keeps its storage, including across different factors.
		render(tiled, 0, 0, f.width, f.height);
		assert(workspace.metric.data.data() == metric && workspace.flags.data.data() == flags);
		assert(workspace.metric.data.capacity() == metric_capacity);
		assert(full == tiled);
		std::fill(tiled.begin(), tiled.end(), sentinel);
		for (int y = 0; y < f.height; y += 2) {
			for (int x = 0; x < f.width; x += 3) {
				render(tiled, x, y, std::min(3, f.width - x), std::min(2, f.height - y));
			}
		}
		assert(tiled == full);
		for (int y = 0; y < f.height * factor; ++y) {
			for (int x = f.width * factor; x < pitch; ++x) {
				assert(full[size_t(8 + y * pitch + x)] == sentinel);
			}
		}
		for (int i = 0; i < 8; ++i) {
			assert(full[size_t(i)] == sentinel);
			assert(full[full.size() - 1 - i] == sentinel);
		}
		const int cx = f.width / 2, cy = f.height / 2;
		source[size_t(cy * stride + cx)] ^= 1;
		std::vector<Pixel> edited(full.size(), sentinel);
		render(edited, 0, 0, f.width, f.height);
		const int left = std::max(0, cx - ScaleFX::radius), top = std::max(0, cy - ScaleFX::radius);
		render(tiled, left, top, std::min(f.width, cx + ScaleFX::radius + 1) - left,
			   std::min(f.height, cy + ScaleFX::radius + 1) - top);
		assert(tiled == edited);
		std::fill(tiled.begin(), tiled.end(), sentinel);
		render(tiled, -1, -1, 0, 0);
		assert(std::all_of(tiled.begin(), tiled.end(), [&](Pixel p) {
			return p == sentinel;
		}));
		// Every output value comes from the supplied palette, including in
		// contradictory junctions; there is no colour mixing or filtering blur.
		for (int y = 0; y < f.height * factor; ++y) {
			for (int x = 0; x < f.width * factor; ++x) {
				const auto value = full[size_t(8 + y * pitch + x)];
				assert(std::find(colours.begin(), colours.end(), value) != colours.end());
			}
		}
	}

	void all_native_checks(const Fixture& f) {
		std::array<uint32_t, 256> colours32{};
		std::array<uint16_t, 256> colours16{};
		for (size_t i = 0; i < 256; ++i) {
			colours32[i] = uint32_t(0xff000000u + i);
			colours16[i] = uint16_t(i + 128);
		}
		for (int factor : {3, 4, 6}) {
			native_checks(f, factor, colours32);
			native_checks(f, factor, colours16);
		}
	}

	void diagonal_checks() {
		constexpr int                 w = 17, h = 17;
		std::array<ScaleFX::RGB, 256> palette{};
		palette[1] = {255, 255, 255};
		std::array<uint8_t, 256> colours{};
		for (size_t i = 0; i < 256; ++i) {
			colours[i] = uint8_t(i);
		}
		std::vector<uint8_t> source(w * h);
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				source[size_t(y * w + x)] = uint8_t(x >= y);
			}
		}
		std::vector<uint8_t> legacy(w * h * 9);
		ScaleFX::scale(source.data(), w, w, h, 0, 0, w, h, palette, legacy.data(), w * 3, colours);
		for (int f : {4, 6}) {
			std::vector<uint8_t> out(size_t(w * h * f * f));
			ScaleFX::scale(source.data(), w, w, h, 0, 0, w, h, palette, out.data(), w * f, colours, f);
			int native_error = 0, enlarged_error = 0, different = 0;
			// Independent analytic 45-degree contour through the pixel's lower
			// left corner, away from clamped image boundaries.
			for (int y = 6 * f; y < 11 * f; ++y) {
				for (int x = 6 * f; x < 11 * f; ++x) {
					const int  expected = (y - x) < f / 2;
					const auto value    = out[size_t(y * w * f + x)];
					native_error += value != expected;
					const auto enlarged = legacy[size_t((y * 3 / f) * (w * 3) + x * 3 / f)];
					enlarged_error += enlarged != expected;
					different += value != enlarged;
				}
			}
			assert(native_error == 0 && enlarged_error > native_error && different > 0);
			// Reflection/rotation must not introduce a preferred edge direction.
			for (int rotation = 0; rotation < 4; ++rotation) {
				auto transformed = source;
				auto map         = [&](int x, int y, int size) {
					if (rotation == 0) {
						return std::array<int, 2>{size - 1 - x, y};
					}
					if (rotation == 1) {
						return std::array<int, 2>{y, size - 1 - x};
					}
					if (rotation == 2) {
						return std::array<int, 2>{size - 1 - x, size - 1 - y};
					}
					return std::array<int, 2>{x, size - 1 - y};
				};
				for (int y = 0; y < h; ++y) {
					for (int x = 0; x < w; ++x) {
						auto m                         = map(x, y, w);
						transformed[size_t(y * w + x)] = source[size_t(m[1] * w + m[0])];
					}
				}
				std::vector<uint8_t> other(out.size());
				ScaleFX::scale(transformed.data(), w, w, h, 0, 0, w, h, palette, other.data(), w * f, colours, f);
				for (int y = 6 * f; y < 11 * f; ++y) {
					for (int x = 6 * f; x < 11 * f; ++x) {
						auto m = map(x, y, w * f);
						assert(other[size_t(y * w * f + x)] == out[size_t(m[1] * w * f + m[0])]);
					}
				}
			}
		}
	}

	void palette_checks() {
		auto                     f = load("imagewin/tests/noise-17-19.txt");
		std::array<uint8_t, 256> colours{};
		for (size_t i = 0; i < 256; ++i) {
			colours[i] = uint8_t(i);
		}
		ScaleFX::Workspace workspace;
		for (int factor : {3, 4, 6}) {
			std::vector<uint8_t> out(size_t(f.width * f.height * factor * factor));
			ScaleFX::scale(
					f.source.data(), f.width, f.width, f.height, 0, 0, f.width, f.height, f.palette, out.data(), f.width * factor,
					colours, factor, workspace);
			auto flat = f.palette;
			flat.fill({0, 0, 0});
			ScaleFX::scale(
					f.source.data(), f.width, f.width, f.height, 0, 0, f.width, f.height, flat, out.data(), f.width * factor,
					colours, factor, workspace);
			for (int y = 0; y < f.height * factor; ++y) {
				for (int x = 0; x < f.width * factor; ++x) {
					assert(out[size_t(y * f.width * factor + x)] == f.source[size_t((y / factor) * f.width + x / factor)]);
				}
			}
			// Restoring a palette must restore all its geometric edge decisions.
			std::vector<uint8_t> restored(out.size()), fresh(out.size());
			ScaleFX::scale(
					f.source.data(), f.width, f.width, f.height, 0, 0, f.width, f.height, f.palette, restored.data(),
					f.width * factor, colours, factor, workspace);
			ScaleFX::scale(
					f.source.data(), f.width, f.width, f.height, 0, 0, f.width, f.height, f.palette, fresh.data(), f.width * factor,
					colours, factor);
			assert(restored == fresh);
		}
	}
}    // namespace
#ifdef SCALEFX_BASELINE_HEADER
void baseline_checks() {
	uint32_t rng  = 0x13579u;
	auto     next = [&] {
		rng = rng * 1664525u + 1013904223u;
		return uint8_t(rng >> 24);
	};
	std::array<ScaleFX::RGB, 256>         palette;
	std::array<BaselineScaleFX::RGB, 256> old_palette;
	std::array<uint32_t, 256>             colours;
	for (size_t i = 0; i < 256; ++i) {
		const auto r = next(), g = next(), b = next();
		palette[i]     = {r, g, b};
		old_palette[i] = {r, g, b};
		colours[i]     = uint32_t(i);
	}
	ScaleFX::Workspace workspace;
	for (int w : {1, 7, 32, 41}) {
		for (int h : {1, 9, 24}) {
			std::vector<uint8_t> source(size_t((w + 5) * h));
			for (auto& v : source) {
				v = next();
			}
			std::vector<uint32_t> now(size_t((w * 3 + 7) * h * 3), 0xabab), before(now);
			ScaleFX::scale(source.data(), w + 5, w, h, 0, 0, w, h, palette, now.data(), w * 3 + 7, colours, 3, workspace);
			BaselineScaleFX::scale(source.data(), w + 5, w, h, 0, 0, w, h, old_palette, before.data(), w * 3 + 7, colours);
			assert(now == before);
			palette[5].r ^= 255;
			old_palette[5].r ^= 255;
			ScaleFX::scale(source.data(), w + 5, w, h, 0, 0, w, h, palette, now.data(), w * 3 + 7, colours, 3, workspace);
			BaselineScaleFX::scale(source.data(), w + 5, w, h, 0, 0, w, h, old_palette, before.data(), w * 3 + 7, colours);
			assert(now == before);
		}
	}
}
#endif

int main() {
	const auto slopes = load("imagewin/tests/slopes-32-24.txt"), noise = load("imagewin/tests/noise-17-19.txt");
	check_fixture(slopes);
	check_fixture(noise);
	all_native_checks(slopes);
	all_native_checks(noise);
	for (int w : {1, 2, 7}) {
		for (int h : {1, 2, 9}) {
			Fixture f{w, h, {}, std::vector<uint8_t>(size_t(w * h), 1), std::vector<uint8_t>(size_t(w * h * 9), 1)};
			f.palette[1] = {255, 255, 255};
			check_fixture(f);
			all_native_checks(f);
		}
	}
	uint32_t state = 42;
	for (const auto dims : {
				 std::array<int, 2>{ 1,  9},
                  { 9,  1},
                  { 5,  7},
                  {37, 23}
    }) {
		Fixture f{dims[0], dims[1], {}, std::vector<uint8_t>(size_t(dims[0] * dims[1])), {}};
		auto    next = [&] {
			state = 1664525u * state + 1013904223u;
			return uint8_t(state >> 24);
		};
		for (auto& c : f.palette) {
			c = {next(), next(), next()};
		}
		for (auto& c : f.source) {
			c = next();
		}
		all_native_checks(f);
	}
	diagonal_checks();
	palette_checks();
#ifdef SCALEFX_BASELINE_HEADER
	baseline_checks();
#endif
	assert(ScaleFX::normalize_factor(2) == 3 && ScaleFX::normalize_factor(4) == 4 && ScaleFX::normalize_factor(6) == 6);
	std::cout << "ScaleFX: 3x shader fixtures; native 4x/6x diagonals; all factors, formats, dirty updates, palettes and workspace "
				 "reuse passed\n";
}
