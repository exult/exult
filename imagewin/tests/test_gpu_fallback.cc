// CPU-only SDL builds and machines without a GPU must remain usable.
#include "imagewin/scalefx_gpu_ui.h"

#include <iostream>

int main() {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cerr << SDL_GetError() << '\n';
		return 1;
	}
	auto* canvas   = SDL_CreateSurface(32, 32, SDL_PIXELFORMAT_RGBA32);
	auto* renderer = canvas ? SDL_CreateSoftwareRenderer(canvas) : nullptr;
	auto* source   = SDL_CreateSurface(8, 8, SDL_PIXELFORMAT_INDEX8);
	if (!renderer || !source || !SDL_CreateSurfacePalette(source)) {
		std::cerr << SDL_GetError() << '\n';
		return 1;
	}
	SDL_memset(source->pixels, 0, size_t(source->pitch) * source->h);
	std::array<SDL_Color, 256> palette{};
	bool                       ok = !ScaleFX::GPU::available(renderer);
	for (int method = 0; method <= 7; ++method) {
		const auto     algorithm = static_cast<ScaleFX::Algorithm>(method);
		const int      factor    = method == 0 ? 3 : method <= 3 ? method + 1 : 2;
		ScaleFX::GPU   world(renderer, algorithm);
		ScaleFX::UIGPU ui(renderer, algorithm);
		ok = ok && !world.update(source, {0, 0, 8, 8}, factor) && !world.output();
		ok = ok && !ui.update(source, factor, 255, palette, palette, {0, 0, 8, 8}) && !ui.output();
	}
	SDL_DestroySurface(source);
	SDL_DestroyRenderer(renderer);
	SDL_DestroySurface(canvas);
	SDL_Quit();
	if (!ok) {
		std::cerr << "CPU renderer was incorrectly accepted as a GPU scaler\n";
		return 1;
	}
	std::cout << "CPU-only world/UI fallback passed for all scalers\n";
	return 0;
}
