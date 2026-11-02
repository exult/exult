/* ScaleFX regression tests. Run from the repository root; see README.md. */
#include "imagewin/scale_scalefx.h"

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
		constexpr Pixel      sentinel = Pixel(~Pixel(0));
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

int main() {
	check_fixture(load("imagewin/tests/slopes-32-24.txt"));
	check_fixture(load("imagewin/tests/noise-17-19.txt"));
	for (int w : {1, 2, 7}) {
		for (int h : {1, 2, 9}) {
			Fixture f{w, h, {}, std::vector<uint8_t>(size_t(w * h), 1), std::vector<uint8_t>(size_t(w * h * 9), 1)};
			f.palette[1] = {255, 255, 255};
			check_fixture(f);
		}
	}
	// Palette changes must be reflected immediately, even with identical indices.
	auto                     f = load("imagewin/tests/noise-17-19.txt");
	std::array<uint8_t, 256> identity{};
	for (size_t i = 0; i < identity.size(); ++i) {
		identity[i] = uint8_t(i);
	}
	std::vector<uint8_t> out(f.expected.size());
	ScaleFX::scale(
			f.source.data(), f.width, f.width, f.height, 0, 0, f.width, f.height, f.palette, out.data(), f.width * 3, identity);
	assert(out == f.expected);
	f.palette.fill({0, 0, 0});
	ScaleFX::scale(
			f.source.data(), f.width, f.width, f.height, 0, 0, f.width, f.height, f.palette, out.data(), f.width * 3, identity);
	for (int y = 0; y < f.height * 3; ++y) {
		for (int x = 0; x < f.width * 3; ++x) {
			assert(out[size_t(y * f.width * 3 + x)] == f.source[size_t((y / 3) * f.width + x / 3)]);
		}
	}
	std::cout << "ScaleFX shader fixtures, pixel formats, boundaries, palette changes and dirty updates passed\n";
}
