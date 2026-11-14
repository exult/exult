// Run from the repository root. See README.md for benchmark and baseline setup.
#include "imagewin/scale_scalefx.h"
#ifdef SCALEFX_BASELINE_HEADER
#	include SCALEFX_BASELINE_HEADER
#endif
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

namespace {
	using Clock = std::chrono::steady_clock;

	template <typename F>
	double timing(F operation) {
		operation();    // Warm caches and allocate retained buffers outside the sample.
		std::array<double, 9> samples;
		for (auto& sample : samples) {
			auto start = Clock::now();
			for (int i = 0; i < 3; ++i) {
				operation();
			}
			sample = std::chrono::duration<double, std::milli>(Clock::now() - start).count() / 3;
		}
		std::sort(samples.begin(), samples.end());
		return samples[samples.size() / 2];
	}

	void ppm(const std::string& path, const std::vector<uint32_t>& pixels, int w, int h) {
		std::ofstream out(path, std::ios::binary);
		out << "P6\n" << w << ' ' << h << "\n255\n";
		for (auto pixel : pixels) {
			const char rgb[] = {char(pixel >> 16), char(pixel >> 8), char(pixel)};
			out.write(rgb, 3);
		}
	}

	std::vector<uint8_t> scene(int w, int h) {
		std::vector<uint8_t> src(size_t(w * h));
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				// High-contrast diagonals of several slopes, curves, line ends, a
				// checkerboard and a coloured tile. Deterministic, no game assets.
				int v = 0;
				if (x < w / 3) {
					v = ((x - y) % 12 + 12) % 12 < 6 ? 1 : 0;
				} else if (x < 2 * w / 3) {
					const int dx = x - w / 2, dy = y - h / 2;
					v = (dx * dx + dy * dy < (h / 3) * (h / 3)) ? 2 : 0;
				} else {
					v = ((x / 8 + y / 8) % 2) ? 3 : 1;
				}
				if (y > h * 3 / 4) {
					v = (x + y / 4) % 17 < 3 ? 1 : 0;
				}
				src[size_t(y * w + x)] = uint8_t(v);
			}
		}
		return src;
	}
}    // namespace

int main(int argc, char** argv) {
	const std::string             prefix = argc > 1 ? argv[1] : "/tmp/scalefx";
	std::array<ScaleFX::RGB, 256> palette{};
	palette[1] = {240, 240, 225};
	palette[2] = {235, 75, 35};
	palette[3] = {45, 145, 230};
	std::array<uint32_t, 256> colours{};
	for (size_t i = 0; i < 256; ++i) {
		const auto p = palette[i];
		colours[i]   = 0xff000000u | (uint32_t(p.r) << 16) | (uint32_t(p.g) << 8) | p.b;
	}
	const int w = 320, h = 200;
	auto      source = scene(w, h);
	std::cout << "implementation,factor,full_ms,dirty_16x16_ms,palette_change_ms\n";
	for (int f : {3, 4, 6}) {
		std::vector<uint32_t> dst(size_t(w * h * f * f));
		ScaleFX::Workspace    workspace;
		auto                  render = [&](int x, int y, int rw, int rh) {
			ScaleFX::scale(source.data(), w, w, h, x, y, rw, rh, palette, dst.data(), w * f, colours, f, workspace);
		};
		double full     = timing([&] {
			render(0, 0, w, h);
		});
		double dirty    = timing([&] {
			source[size_t(100 * w + 160)] ^= 1;
			render(152, 92, 16, 16);
		});
		double animated = timing([&] {
			palette[2].g ^= 1;
			render(0, 0, w, h);
		});
		std::cout << "native," << f << ',' << full << ',' << dirty << ',' << animated << '\n';
	}
#ifdef SCALEFX_BASELINE_HEADER
	std::array<BaselineScaleFX::RGB, 256> baseline_palette{};
	for (size_t i = 0; i < 256; ++i) {
		baseline_palette[i] = {palette[i].r, palette[i].g, palette[i].b};
	}
	std::vector<uint32_t> baseline(size_t(w * h * 9));
	auto                  original = [&](int x, int y, int rw, int rh) {
		BaselineScaleFX::scale(source.data(), w, w, h, x, y, rw, rh, baseline_palette, baseline.data(), w * 3, colours);
	};
	const double full     = timing([&] {
		original(0, 0, w, h);
	});
	const double dirty    = timing([&] {
		source[size_t(100 * w + 160)] ^= 1;
		original(152, 92, 16, 16);
	});
	const double animated = timing([&] {
		baseline_palette[2].g ^= 1;
		original(0, 0, w, h);
	});
	std::cout << "original,3," << full << ',' << dirty << ',' << animated << '\n';
#endif
	// Small illustrations: native 4x/6x beside a nearest enlargement of the
	// unchanged 3x result. This is only a comparison, never the render path.
	const int             vw = 48, vh = 32;
	auto                  visual = scene(vw, vh);
	std::vector<uint32_t> source_rgb(visual.size());
	for (size_t i = 0; i < visual.size(); ++i) {
		source_rgb[i] = colours[visual[i]];
	}
	ppm(prefix + "-source.ppm", source_rgb, vw, vh);
	std::vector<uint32_t> legacy(size_t(vw * vh * 9));
	ScaleFX::scale(visual.data(), vw, vw, vh, 0, 0, vw, vh, palette, legacy.data(), vw * 3, colours);
	for (int f : {3, 4, 6}) {
		std::vector<uint32_t> native(size_t(vw * vh * f * f)), enlarged(native.size());
		ScaleFX::scale(visual.data(), vw, vw, vh, 0, 0, vw, vh, palette, native.data(), vw * f, colours, f);
		size_t different = 0;
		for (int y = 0; y < vh * f; ++y) {
			for (int x = 0; x < vw * f; ++x) {
				const auto i = size_t(y * vw * f + x);
				enlarged[i]  = legacy[size_t((y * 3 / f) * (vw * 3) + x * 3 / f)];
				different += native[i] != enlarged[i];
			}
		}
		ppm(prefix + "-native-" + std::to_string(f) + ".ppm", native, vw * f, vh * f);
		ppm(prefix + "-enlarged-" + std::to_string(f) + ".ppm", enlarged, vw * f, vh * f);
		std::cerr << f << "x: " << different << " native samples differ from enlarged 3x out of " << native.size() << '\n';
	}
}
