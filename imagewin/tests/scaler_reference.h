#ifndef EXULT_TEST_SCALER_REFERENCE_H
#define EXULT_TEST_SCALER_REFERENCE_H
#include <SDL3/SDL.h>

#include <array>
#include <vector>

struct ReferencePixels {
	using uintD = Uint32;
	const std::array<SDL_Color, 256>& colours;
	int                               source_height = 0;

	void split_source(Uint8 index, unsigned& r, unsigned& g, unsigned& b) const {
		auto c = colours[index];
		r      = c.r;
		g      = c.g;
		b      = c.b;
	}

	static Uint32 rgb(unsigned r, unsigned g, unsigned b) {
		return (r << 16) | (g << 8) | b;
	}

	void copy(Uint32& dest, Uint8 index) const {
		auto c = colours[index];
		dest   = rgb(c.r, c.g, c.b);
	}
};

void hq_reference2(SDL_Surface*, SDL_Rect, std::vector<Uint32>&, const ReferencePixels&);
void hq_reference3(SDL_Surface*, SDL_Rect, std::vector<Uint32>&, const ReferencePixels&);
void hq_reference4(SDL_Surface*, SDL_Rect, std::vector<Uint32>&, const ReferencePixels&);
void scaler_reference(int, SDL_Surface*, SDL_Rect, int, std::vector<Uint32>&, const ReferencePixels&);
#endif
