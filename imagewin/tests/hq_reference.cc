#include "scaler_reference.h"
#if TEST_HQ_FACTOR == 2
#	define USE_HQ2X_SCALER
#	include "imagewin/scale_hq2x.h"

void hq_reference2(SDL_Surface* s, SDL_Rect r, std::vector<Uint32>& dest, const ReferencePixels& pixels) {
	Scale_Hq2x(
			static_cast<Uint8*>(s->pixels), r.x, r.y, r.w, r.h, s->pitch, (pixels.source_height ? pixels.source_height : r.y + r.h),
			dest.data(), s->w * 2, pixels);
}
#elif TEST_HQ_FACTOR == 3
#	define USE_HQ3X_SCALER
#	include "imagewin/scale_hq3x.h"

void hq_reference3(SDL_Surface* s, SDL_Rect r, std::vector<Uint32>& dest, const ReferencePixels& pixels) {
	Scale_Hq3x(
			static_cast<Uint8*>(s->pixels), r.x, r.y, r.w, r.h, s->pitch, (pixels.source_height ? pixels.source_height : r.y + r.h),
			dest.data(), s->w * 3, pixels);
}
#elif TEST_HQ_FACTOR == 4
#	define USE_HQ4X_SCALER
#	include "imagewin/scale_hq4x.h"

void hq_reference4(SDL_Surface* s, SDL_Rect r, std::vector<Uint32>& dest, const ReferencePixels& pixels) {
	Scale_Hq4x(
			static_cast<Uint8*>(s->pixels), r.x, r.y, r.w, r.h, s->pitch, (pixels.source_height ? pixels.source_height : r.y + r.h),
			dest.data(), s->w * 4, pixels);
}
#endif
