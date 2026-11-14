#include "scaler_reference.h"

#include "imagewin/scale_2xSaI.h"
#define USE_XBR_SCALER
#include "imagewin/scale_xbr.h"

void scaler_reference(int algorithm, SDL_Surface* s, SDL_Rect r, int f, std::vector<Uint32>& dest, const ReferencePixels& pixels) {
	if (algorithm == 1) {
		return hq_reference2(s, r, dest, pixels);
	}
	if (algorithm == 2) {
		return hq_reference3(s, r, dest, pixels);
	}
	if (algorithm == 3) {
		return hq_reference4(s, r, dest, pixels);
	}
	auto* input = static_cast<Uint8*>(s->pixels);
	switch (algorithm) {
	case 4:
		Scale_2xSaI(
				input, r.x, r.y, r.w, r.h, s->pitch, (pixels.source_height ? pixels.source_height : r.y + r.h), dest.data(),
				s->w * f, pixels);
		break;
	case 5:
		Scale_Super2xSaI(
				input, r.x, r.y, r.w, r.h, s->pitch, (pixels.source_height ? pixels.source_height : r.y + r.h), dest.data(),
				s->w * f, pixels);
		break;
	case 6:
		Scale_SuperEagle(
				input, r.x, r.y, r.w, r.h, s->pitch, (pixels.source_height ? pixels.source_height : r.y + r.h), dest.data(),
				s->w * f, pixels);
		break;
	case 7:
		if (f == 2) {
			Scale_xBR<Uint32, ReferencePixels, Scaler2xBR>(
					input, r.x, r.y, r.w, r.h, s->pitch, (pixels.source_height ? pixels.source_height : r.y + r.h), dest.data(),
					s->w * f, pixels);
		} else if (f == 3) {
			Scale_xBR<Uint32, ReferencePixels, Scaler3xBR>(
					input, r.x, r.y, r.w, r.h, s->pitch, (pixels.source_height ? pixels.source_height : r.y + r.h), dest.data(),
					s->w * f, pixels);
		} else {
			Scale_xBR<Uint32, ReferencePixels, Scaler4xBR>(
					input, r.x, r.y, r.w, r.h, s->pitch, (pixels.source_height ? pixels.source_height : r.y + r.h), dest.data(),
					s->w * f, pixels);
		}
	}
}
