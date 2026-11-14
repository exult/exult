// GPU UI consensus matting versus the existing CPU algorithm, without game data.
#include "imagewin/scalefx_gpu_ui.h"
#include "scaler_reference.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#if SDL_VERSION_ATLEAST(3, 4, 0)
static void require(bool ok, const char* message) {
	if (!ok) {
		throw std::runtime_error(std::string(message) + ": " + SDL_GetError());
	}
}

static void compare(
		SDL_Renderer* renderer, ScaleFX::UIGPU& gpu, SDL_Surface* source, int f, int transparent,
		const std::array<SDL_Color, 256>& base, const std::array<SDL_Color, 256>& original, SDL_Rect content) {
	std::array<std::vector<Uint32>, 3> passes;
	const int                          w = source->w * f, h = source->h * f;
	for (int i = 0; i < 3; ++i) {
		std::array<ScaleFX::RGB, 256> rgb;
		std::array<Uint32, 256>       packed;
		for (int j = 0; j < 256; ++j) {
			auto c = base[j];
			if (j == transparent) {
				c = {Uint8(i == 0 ? 255 : 0), Uint8(i == 1 ? 255 : 0), Uint8(i == 2 ? 255 : 0), 255};
			}
			rgb[j]    = {c.r, c.g, c.b};
			packed[j] = (Uint32(c.r) << 16) | (Uint32(c.g) << 8) | c.b;
		}
		passes[i].resize(size_t(w * h));
		if (gpu.method() == ScaleFX::Algorithm::ScaleFX) {
			ScaleFX::scale(
					static_cast<const Uint8*>(source->pixels), source->pitch, source->w, source->h, 0, 0, source->w, source->h, rgb,
					passes[i].data(), w, packed, f);
		} else {
			auto palette         = base;
			palette[transparent] = {Uint8(i == 0 ? 255 : 0), Uint8(i == 1 ? 255 : 0), Uint8(i == 2 ? 255 : 0), 255};
			scaler_reference(int(gpu.method()), source, content, f, passes[i], {palette});
		}
	}
	bool needs3  = false;
	auto channel = [](Uint32 p, int shift) {
		return int((p >> shift) & 255);
	};
	for (int y = content.y * f; y < (content.y + content.h) * f; ++y) {
		for (int x = content.x * f; x < (content.x + content.w) * f; ++x) {
			auto a = passes[0][y * w + x], b = passes[1][y * w + x];
			if (std::abs((channel(a, 16) - channel(b, 16)) - (channel(b, 8) - channel(a, 8))) > 96) {
				needs3 = true;
			}
		}
	}
	auto* saved = SDL_GetRenderTarget(renderer);
	require(SDL_SetRenderTarget(renderer, gpu.output()), "UI target");
	auto* read = SDL_RenderReadPixels(renderer, nullptr);
	require(read != nullptr, "UI read");
	auto* rgba = SDL_ConvertSurface(read, SDL_PIXELFORMAT_RGBA32);
	require(rgba != nullptr, "UI convert");
	for (int y = content.y * f; y < (content.y + content.h) * f; ++y) {
		for (int x = content.x * f; x < (content.x + content.w) * f; ++x) {
			const auto a = passes[0][y * w + x], b = passes[1][y * w + x], c = passes[2][y * w + x];
			int        r1 = channel(a, 16), g1 = channel(a, 8), b1 = channel(a, 0), r2 = channel(b, 16), g2 = channel(b, 8),
					   b2 = channel(b, 0);
			int        ta = r1 - r2, tb = g2 - g1, sum = ta + tb, best = std::abs(ta - tb), use = 1;
			if (needs3) {
				int t13a = r1 - channel(c, 16), t13b = channel(c, 0) - b1, t23a = g2 - channel(c, 8), t23b = channel(c, 0) - b2;
				if (std::abs(t13a - t13b) < best) {
					best = std::abs(t13a - t13b);
					sum  = t13a + t13b;
					use  = 1;
				}
				if (std::abs(t23a - t23b) < best) {
					best = std::abs(t23a - t23b);
					sum  = t23a + t23b;
					use  = 2;
				}
			}
			const int index = static_cast<const Uint8*>(source->pixels)[(y / f) * source->pitch + x / f];
			auto      orig  = original[index];
			int       coverage;
			int       r, g, blue;
			if (best > 96) {
				coverage = index == transparent ? 0 : 255;
				r        = orig.r;
				g        = orig.g;
				blue     = orig.b;
			} else {
				coverage = 255 - std::clamp((sum + 1) / 2, 0, 255);
				if (coverage <= 8) {
					coverage = 0;
					r = g = blue = 0;
				} else {
					r    = std::clamp(((use == 1 ? r1 : r2) - (use == 1 ? 255 - coverage : 0)) * 255 / coverage, 0, 255);
					g    = std::clamp(((use == 1 ? g1 : g2) - (use == 2 ? 255 - coverage : 0)) * 255 / coverage, 0, 255);
					blue = std::clamp((use == 1 ? b1 : b2) * 255 / coverage, 0, 255);
				}
			}
			const int   alpha = coverage * (index == transparent ? 255 : orig.a) / 255;
			const auto* pixel = static_cast<const Uint8*>(rgba->pixels) + y * rgba->pitch + x * 4;
			if (pixel[0] != r || pixel[1] != g || pixel[2] != blue || pixel[3] != alpha) {
				std::cerr << f << "x UI difference at " << x << "," << y << " expected " << r << "," << g << "," << blue << ","
						  << alpha << " actual " << int(pixel[0]) << "," << int(pixel[1]) << "," << int(pixel[2]) << ","
						  << int(pixel[3]) << '\n';
				throw std::runtime_error("GPU/CPU UI mismatch");
			}
		}
	}
	SDL_DestroySurface(rgba);
	SDL_DestroySurface(read);
	SDL_SetRenderTarget(renderer, saved);
}

static void benchmark(SDL_Renderer* renderer) {
	auto* source = SDL_CreateSurface(420, 263, SDL_PIXELFORMAT_INDEX8);
	require(source != nullptr, "benchmark source");
	std::array<SDL_Color, 256> base, original;
	for (int i = 0; i < 256; ++i) {
		base[i] = original[i] = {Uint8(i), Uint8(i * 3), Uint8(i * 7), 255};
	}
	original[255] = {0, 0, 0, 0};
	for (int y = 0; y < 263; ++y) {
		for (int x = 0; x < 420; ++x) {
			static_cast<Uint8*>(source->pixels)[y * source->pitch + x] = Uint8((x + y) % 30 < 15 ? 255 : 1 + (x / 12) % 8);
		}
	}
	auto median = [](auto operation) {
		operation();
		std::array<double, 3> times;
		for (auto& t : times) {
			auto start = std::chrono::steady_clock::now();
			operation();
			t = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		}
		std::sort(times.begin(), times.end());
		return times[1];
	};
	std::cout << "factor,cpu_three_scaling_passes_ms,gpu_UI_completed_ms,gpu_UI_cached_completed_ms\n";
	for (int f : {3, 4, 6}) {
		ScaleFX::UIGPU      gpu(renderer);
		ScaleFX::Workspace  workspace;
		std::vector<Uint32> output(size_t(420 * 263 * f * f));
		auto                cpu    = median([&] {
			for (int pass = 0; pass < 3; ++pass) {
				std::array<ScaleFX::RGB, 256> rgb;
				std::array<Uint32, 256>       packed;
				for (int i = 0; i < 256; ++i) {
					auto c = base[i];
					if (i == 255) {
						c = {Uint8(pass == 0 ? 255 : 0), Uint8(pass == 1 ? 255 : 0), Uint8(pass == 2 ? 255 : 0), 255};
					}
					rgb[i]    = {c.r, c.g, c.b};
					packed[i] = (Uint32(c.r) << 16) | (Uint32(c.g) << 8) | c.b;
				}
				ScaleFX::scale(
						static_cast<const Uint8*>(source->pixels), source->pitch, 420, 263, 0, 0, 420, 263, rgb, output.data(),
						420 * f, packed, f, workspace);
			}
		});
		auto*               device = SDL_GetGPURendererDevice(renderer);
		auto                full   = median([&] {
			auto* pixel = static_cast<Uint8*>(source->pixels);
			*pixel      = *pixel == 255 ? 1 : 255;
			require(gpu.update(source, f, 255, base, original, {0, 0, 420, 263}), "benchmark UI update");
			require(SDL_WaitForGPUIdle(device), "benchmark GPU completion");
		});
		auto                cached = median([&] {
			require(gpu.update(source, f, 255, base, original, {0, 0, 420, 263}), "benchmark UI cache");
			require(SDL_WaitForGPUIdle(device), "benchmark cached completion");
		});
		std::cout << f << ',' << cpu << ',' << full << ',' << cached << '\n';
	}
	SDL_DestroySurface(source);
}

int main(int argc, char**) {
	try {
		require(SDL_Init(SDL_INIT_VIDEO), "init");
		auto* renderer = SDL_CreateGPURenderer(nullptr, nullptr);
		if (!renderer) {
			std::cerr << "GPU unavailable: " << SDL_GetError() << '\n';
			return 77;
		}
		auto* anchor = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, 1, 1);
		require(anchor && SDL_SetRenderTarget(renderer, anchor), "anchor");
		Uint32 random = 761;
		auto   next   = [&] {
			random = random * 1664525u + 1013904223u;
			return Uint8(random >> 24);
		};
		{
			for (auto method :
				 {ScaleFX::Algorithm::ScaleFX, ScaleFX::Algorithm::HQ2x, ScaleFX::Algorithm::HQ3x, ScaleFX::Algorithm::HQ4x,
				  ScaleFX::Algorithm::SaI, ScaleFX::Algorithm::SuperSaI, ScaleFX::Algorithm::SuperEagle, ScaleFX::Algorithm::XBR}) {
				ScaleFX::UIGPU gpu(renderer, method);
				for (int size : {1, 9, 32}) {
					for (int f : {2, 3, 4, 6}) {
						if (method == ScaleFX::Algorithm::ScaleFX && f == 2) {
							continue;
						}
						if (method != ScaleFX::Algorithm::ScaleFX && f == 6) {
							continue;
						}
						const int a = int(method);
						if (a >= 1 && a <= 3 && f != a + 1) {
							continue;
						}
						if (a >= 4 && a <= 6 && f != 2) {
							continue;
						}
						for (int pattern = 0; pattern < 4; ++pattern) {
							auto* source = SDL_CreateSurface(size + 4, size + 6, SDL_PIXELFORMAT_INDEX8);
							require(source != nullptr, "source");
							std::array<SDL_Color, 256> base, original;
							int                        transparent = pattern == 3 ? 0 : 255;
							for (int i = 0; i < 256; ++i) {
								base[i]       = {next(), next(), next(), 255};
								original[i]   = base[i];
								original[i].a = next();
							}
							original[transparent] = {0, 0, 0, 0};
							base[1]               = {255, 0, 0, 255};
							base[2]               = {0, 255, 0, 255};
							base[3]               = {0, 0, 255, 255};
							original[1]           = base[1];
							original[2]           = base[2];
							original[3]           = base[3];
							SDL_Rect content{2, 2, size, size + 2};
							for (int frame = 0; frame < 3; ++frame) {
								for (int y = 0; y < source->h; ++y) {
									for (int x = 0; x < source->w; ++x) {
										Uint8 index = pattern == 0   ? Uint8(transparent)
													  : pattern == 1 ? Uint8(next() % 2 ? transparent : 1)
													  : pattern == 2 ? Uint8(x > y ? transparent : 1 + (x + y) % 3)
																	 : next();
										static_cast<Uint8*>(source->pixels)[y * source->pitch + x] = index;
									}
								}
								// Edits, palette animation and alpha-only changes on retained resources.
								base[4]       = {next(), next(), next(), 255};
								original[4]   = base[4];
								original[4].a = next();
								require(gpu.update(source, f, transparent, base, original, content), "GPU UI update");
								compare(renderer, gpu, source, f, transparent, base, original, content);
								// Repeated damage with unchanged pixels takes the cache
								// path; alpha-only overrides must still invalidate it.
								require(gpu.update(source, f, transparent, base, original, content), "cached UI update");
								compare(renderer, gpu, source, f, transparent, base, original, content);
								original[1].a = Uint8(original[1].a / 2);
								require(gpu.update(source, f, transparent, base, original, content), "alpha-only UI update");
								compare(renderer, gpu, source, f, transparent, base, original, content);
							}
							SDL_DestroySurface(source);
						}
					}
				}
			}
		}
		if (argc > 1) {
			benchmark(renderer);
		}
		// A non-GPU renderer must leave the existing CPU matting path usable.
		auto* surface = SDL_CreateSurface(8, 8, SDL_PIXELFORMAT_RGBA32);
		require(surface != nullptr, "software surface");
		auto* software = SDL_CreateSoftwareRenderer(surface);
		require(software != nullptr, "software renderer");
		{
			ScaleFX::UIGPU             fallback(software);
			auto*                      indexed = SDL_CreateSurface(4, 4, SDL_PIXELFORMAT_INDEX8);
			std::array<SDL_Color, 256> colours{};
			require(!fallback.update(indexed, 3, 255, colours, colours, {0, 0, 4, 4}) && !fallback.output(), "CPU UI fallback");
			SDL_DestroySurface(indexed);
		}
		SDL_DestroyRenderer(software);
		SDL_DestroySurface(surface);
		SDL_DestroyTexture(anchor);
		SDL_DestroyRenderer(renderer);
		SDL_Quit();
		std::cout << "GPU scaler UI consensus, transparency, intrinsic alpha, palettes and resize agree with CPU\n";
		return 0;
	} catch (const std::exception& e) {
		std::cerr << "::error title=GPU conformance::" << e.what() << '\n';
		return 1;
	}
}
#else
int main() {
	std::cerr << "GPU UI test requires SDL 3.4+\n";
	return 77;
}
#endif
