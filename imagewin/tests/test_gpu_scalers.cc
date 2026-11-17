#include "imagewin/scalefx_gpu.h"
#include "scaler_reference.h"

#include <iostream>
#include <stdexcept>
#if SDL_VERSION_ATLEAST(3, 4, 0)
void require(bool value, const char* label) {
	if (!value) {
		throw std::runtime_error(std::string(label) + ": " + SDL_GetError());
	}
}

void compare(SDL_Renderer* renderer, ScaleFX::GPU& gpu, const std::vector<Uint32>& cpu, SDL_Rect region, int f) {
	auto* saved = SDL_GetRenderTarget(renderer);
	require(SDL_SetRenderTarget(renderer, gpu.output()), "read target");
	auto* read = SDL_RenderReadPixels(renderer, nullptr);
	require(read != nullptr, "read");
	auto* pixels = SDL_ConvertSurface(read, SDL_PIXELFORMAT_ARGB8888);
	require(pixels != nullptr, "convert");
	for (int y = region.y * f; y < (region.y + region.h) * f; ++y) {
		for (int x = region.x * f; x < (region.x + region.w) * f; ++x) {
			Uint32 actual;
			SDL_memcpy(&actual, static_cast<Uint8*>(pixels->pixels) + y * pixels->pitch + x * 4, 4);
			auto expected = cpu[size_t(y * pixels->w + x)] | 0xFF000000u;
			if (actual != expected) {
				std::cerr << "algorithm " << int(gpu.method()) << ' ' << f << "x mismatch " << x << ',' << y << " GPU " << std::hex
						  << actual << " CPU " << expected << std::dec << '\n';
				throw std::runtime_error("scaler mismatch");
			}
		}
	}
	SDL_DestroySurface(pixels);
	SDL_DestroySurface(read);
	SDL_SetRenderTarget(renderer, saved);
}

int main() {
	try {
		require(SDL_Init(SDL_INIT_VIDEO), "init");
		auto* renderer = SDL_CreateGPURenderer(nullptr, nullptr);
		if (!renderer) {
			std::cerr << SDL_GetError() << '\n';
			return 77;
		}
		auto* anchor = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, 1, 1);
		require(anchor && SDL_SetRenderTarget(renderer, anchor), "anchor");
		Uint32 random = 791;
		auto   next   = [&] {
			random = random * 1664525u + 1013904223u;
			return Uint8(random >> 24);
		};
		for (int algorithm = 1; algorithm <= 7; ++algorithm) {
			for (int f : {2, 3, 4}) {
				if (algorithm <= 3 && f != algorithm + 1) {
					continue;
				}
				if (algorithm >= 4 && algorithm <= 6 && f != 2) {
					continue;
				}
				ScaleFX::GPU gpu(renderer, static_cast<ScaleFX::Algorithm>(algorithm));
				for (int size : {8, 16, 40}) {
					for (int pattern = 0; pattern < 3; ++pattern) {
						auto* source = SDL_CreateSurface(size, size + 8, SDL_PIXELFORMAT_INDEX8);
						require(source != nullptr, "source");
						auto* pal = SDL_CreateSurfacePalette(source);
						require(pal != nullptr, "palette");
						std::array<SDL_Color, 256> colours;
						for (auto& c : colours) {
							c = {next(), next(), next(), 255};
						}
						colours[0] = {0, 0, 0, 255};
						colours[1] = {255, 255, 255, 255};
						require(SDL_SetPaletteColors(pal, colours.data(), 0, 256), "colours");
						for (int y = 0; y < source->h; ++y) {
							for (int x = 0; x < size; ++x) {
								static_cast<Uint8*>(source->pixels)[y * source->pitch + x] = pattern == 0 ? next()
																							 : pattern == 1
																									 ? Uint8(x >= y)
																									 : Uint8((x / 2 + y / 3) % 4);
							}
						}
						SDL_Rect            content{2, 2, size - 4, source->h - 4};
						std::vector<Uint32> cpu(size_t(source->w * source->h * f * f));
						scaler_reference(algorithm, source, content, f, cpu, {colours});
						require(gpu.update(source, content, f, content), "update");
						compare(renderer, gpu, cpu, content, f);
						// Palette-only changes must refresh output using the same native algorithm.
						colours[1] = {240, 37, 11, 255};
						require(SDL_SetPaletteColors(pal, colours.data(), 0, 256), "animate");
						scaler_reference(algorithm, source, content, f, cpu, {colours});
						require(gpu.update(source, content, f, content), "palette update");
						compare(renderer, gpu, cpu, content, f);
						const int px = content.x + content.w / 2, py = content.y + content.h / 2;
						static_cast<Uint8*>(source->pixels)[py * source->pitch + px] ^= 1;
						SDL_Rect dirty{std::max(content.x, px - 4), std::max(content.y, py - 4), 0, 0};
						dirty.w = std::min(content.x + content.w, px + 5) - dirty.x;
						dirty.h = std::min(content.y + content.h, py + 5) - dirty.y;
						scaler_reference(algorithm, source, dirty, f, cpu, {colours, content.y + content.h});
						require(gpu.update(source, dirty, f, content), "dirty update");
						compare(renderer, gpu, cpu, content, f);
						SDL_DestroySurface(source);
					}
				}
			}
		}
		SDL_DestroyTexture(anchor);
		SDL_DestroyRenderer(renderer);
		SDL_Quit();
		std::cout << "HQ2x/HQ3x/HQ4x, 2xSaI/Super2xSaI/SuperEagle and 2x/3x/4xBR GPU output agrees with CPU\n";
		return 0;
	} catch (const std::exception& e) {
		std::cerr << "::error title=GPU conformance::" << e.what() << '\n';
		return 1;
	}
}
#else
int main() {
	return 77;
}
#endif
