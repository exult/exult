/**
**  Imagewin.cc - A window to blit images into.
**
**  Written: 8/13/98 - JSF
**/

/*
Copyright (C) 1998 Jeffrey S. Freedman
Copyright (C) 2000-2022 The Exult Team

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "imagewin.h"

#include "BilinearScaler.h"
#include "Configuration.h"
#include "PointScaler.h"
#include "common_types.h"
#include "exceptions.h"
#include "gamewin.h"
#include "istring.h"
#include "items.h"
#include "manip.h"
#include "mouse.h"
#include "perf.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

// Simulate HighDPI mode without OS or Display Support for it
// uncomment the define and set to a value greater than 1.0 to multiply the
// fullscreen render surface resolution This should only be used for testing and
// development and will likely degrade performance and quality
// #define SIMULATE_HIDPI 2.0f

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

bool SaveIMG_RW(SDL_Surface* saveme, SDL_IOStream* dst, bool freedst, int guardband);

using std::cerr;
using std::cout;
using std::endl;
using std::exit;

#define SCALE_BIT(factor) (1 << ((factor) - 1))

const Image_window::ScalerConst Image_window::point("Point");
const Image_window::ScalerConst Image_window::interlaced("Interlaced");
const Image_window::ScalerConst Image_window::bilinear("Bilinear");
const Image_window::ScalerConst Image_window::BilinearPlus("BilinearPlus");
const Image_window::ScalerConst Image_window::SaI("2xSaI");
const Image_window::ScalerConst Image_window::SuperEagle("SuperEagle");
const Image_window::ScalerConst Image_window::Super2xSaI("Super2xSaI");
const Image_window::ScalerConst Image_window::Scale2x("Scale2x");
const Image_window::ScalerConst Image_window::Hq2x("Hq2X");
const Image_window::ScalerConst Image_window::Hq3x("Hq3x");
const Image_window::ScalerConst Image_window::Hq4x("Hq4x");
const Image_window::ScalerConst Image_window::_2xBR("2xBR");
const Image_window::ScalerConst Image_window::_3xBR("3xBR");
const Image_window::ScalerConst Image_window::_4xBR("4xBR");
const Image_window::ScalerConst Image_window::SDLScaler("SDLScaler");
const Image_window::ScalerConst Image_window::NumScalers(nullptr);

Image_window::ScalerVector        Image_window::p_scalers;
const Image_window::ScalerVector& Image_window::Scalers = Image_window::p_scalers;

std::map<uint32, Image_window::Resolution>        Image_window::p_resolutions;
const std::map<uint32, Image_window::Resolution>& Image_window::Resolutions = Image_window::p_resolutions;
bool                                              Image_window::any_res_allowed;
const bool&                                       Image_window::AnyResAllowed = Image_window::any_res_allowed;

int Image_window::force_bpp     = 0;
int Image_window::desktop_depth = 0;
int Image_window::windowed      = 0;
// When HighDPI is enabled we will end up with a different native scale factor,
// so we need to define the default
float Image_window::nativescale = 1.0f;

const SDL_PixelFormatDetails* ManipBase::fmt;            // Format of dest. pixels (and src for rgb src).
SDL_Color                     ManipBase::colors[256];    // Palette for source window.

// Constructor for the ScalerVector, setup the list
Image_window::ScalerVector::ScalerVector() {
	reserve(14);

	// This is all the names of the scalers. It needs to match the ScalerType
	// enum
	const ScalerInfo point = {
			"Point", 0x69A - msg_file_start, 0xFFFFFFFF, new Pentagram::PointScaler(), nullptr, nullptr, nullptr, nullptr, nullptr};
	push_back(point);

	const ScalerInfo Interlaced
			= {"Interlaced",
			   0x69B - msg_file_start,
			   0xFFFFFFFE,
			   nullptr,
			   &Image_window::show_scaled8to565_interlace,
			   &Image_window::show_scaled8to555_interlace,
			   &Image_window::show_scaled8to16_interlace,
			   &Image_window::show_scaled8to32_interlace,
			   &Image_window::show_scaled8to8_interlace};
	push_back(Interlaced);

	const ScalerInfo Bilinear = {"Bilinear", 0x69C - msg_file_start,
								 0xFFFFFFFF, new Pentagram::BilinearScaler::Scaler(),
								 nullptr,    nullptr,
								 nullptr,    nullptr,
								 nullptr};
	push_back(Bilinear);

	const ScalerInfo BilinearPlus
			= {"BilinearPlus",
			   0x69D - msg_file_start,
			   SCALE_BIT(2),
			   nullptr,
			   &Image_window::show_scaled8to565_BilinearPlus,
			   &Image_window::show_scaled8to555_BilinearPlus,
			   &Image_window::show_scaled8to16_BilinearPlus,
			   &Image_window::show_scaled8to32_BilinearPlus,
			   nullptr};
	push_back(BilinearPlus);

	const ScalerInfo _2xSaI
			= {"2xSaI",
			   0,
			   SCALE_BIT(2),
			   nullptr,
			   &Image_window::show_scaled8to565_2xSaI,
			   &Image_window::show_scaled8to555_2xSaI,
			   &Image_window::show_scaled8to16_2xSaI,
			   &Image_window::show_scaled8to32_2xSaI,
			   nullptr};
	push_back(_2xSaI);

	const ScalerInfo SuperEagle
			= {"SuperEagle",
			   0,
			   SCALE_BIT(2),
			   nullptr,
			   &Image_window::show_scaled8to565_SuperEagle,
			   &Image_window::show_scaled8to555_SuperEagle,
			   &Image_window::show_scaled8to16_SuperEagle,
			   &Image_window::show_scaled8to32_SuperEagle,
			   nullptr};
	push_back(SuperEagle);

	const ScalerInfo Super2xSaI
			= {"Super2xSaI",
			   0,
			   SCALE_BIT(2),
			   nullptr,
			   &Image_window::show_scaled8to565_Super2xSaI,
			   &Image_window::show_scaled8to555_Super2xSaI,
			   &Image_window::show_scaled8to16_Super2xSaI,
			   &Image_window::show_scaled8to32_Super2xSaI,
			   nullptr};
	push_back(Super2xSaI);

	const ScalerInfo Scale2X
			= {"Scale2X",
			   0,
			   SCALE_BIT(2),
			   nullptr,
			   &Image_window::show_scaled8to565_2x_noblur,
			   &Image_window::show_scaled8to555_2x_noblur,
			   &Image_window::show_scaled8to16_2x_noblur,
			   &Image_window::show_scaled8to32_2x_noblur,
			   &Image_window::show_scaled8to8_2x_noblur};
	push_back(Scale2X);

#ifdef USE_HQ2X_SCALER
	const ScalerInfo Hq2x
			= {"Hq2x",
			   0,
			   SCALE_BIT(2),
			   nullptr,
			   &Image_window::show_scaled8to565_Hq2x,
			   &Image_window::show_scaled8to555_Hq2x,
			   &Image_window::show_scaled8to16_Hq2x,
			   &Image_window::show_scaled8to32_Hq2x,
			   nullptr};
	push_back(Hq2x);
#endif

#ifdef USE_HQ3X_SCALER
	const ScalerInfo Hq3x
			= {"Hq3x",
			   0,
			   SCALE_BIT(3),
			   nullptr,
			   &Image_window::show_scaled8to565_Hq3x,
			   &Image_window::show_scaled8to555_Hq3x,
			   &Image_window::show_scaled8to16_Hq3x,
			   &Image_window::show_scaled8to32_Hq3x,
			   nullptr};
	push_back(Hq3x);
#endif

#ifdef USE_HQ4X_SCALER
	const ScalerInfo Hq4x
			= {"Hq4x",
			   0,
			   SCALE_BIT(4),
			   nullptr,
			   &Image_window::show_scaled8to565_Hq4x,
			   &Image_window::show_scaled8to555_Hq4x,
			   &Image_window::show_scaled8to16_Hq4x,
			   &Image_window::show_scaled8to32_Hq4x,
			   nullptr};
	push_back(Hq4x);
#endif

#ifdef USE_XBR_SCALER
	const ScalerInfo _2xbr
			= {"2xBR",
			   0,
			   SCALE_BIT(2),
			   nullptr,
			   &Image_window::show_scaled8to565_2xBR,
			   &Image_window::show_scaled8to555_2xBR,
			   &Image_window::show_scaled8to16_2xBR,
			   &Image_window::show_scaled8to32_2xBR,
			   nullptr};
	push_back(_2xbr);
	const ScalerInfo _3xbr
			= {"3xBR",
			   0,
			   SCALE_BIT(3),
			   nullptr,
			   &Image_window::show_scaled8to565_3xBR,
			   &Image_window::show_scaled8to555_3xBR,
			   &Image_window::show_scaled8to16_3xBR,
			   &Image_window::show_scaled8to32_3xBR,
			   nullptr};
	push_back(_3xbr);
	const ScalerInfo _4xbr
			= {"4xBR",
			   0,
			   SCALE_BIT(4),
			   nullptr,
			   &Image_window::show_scaled8to565_4xBR,
			   &Image_window::show_scaled8to555_4xBR,
			   &Image_window::show_scaled8to16_4xBR,
			   &Image_window::show_scaled8to32_4xBR,
			   nullptr};
	push_back(_4xbr);
#endif
	const ScalerInfo SDLScaler = {"SDLScaler", 0, 0xFFFFFFFF, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
	push_back(SDLScaler);
}

Image_window::ScalerVector::~ScalerVector() {
	for (auto& it : *this) {
		delete it.arb;
	}
}

Image_window::ScalerType Image_window::get_scaler_for_name(const char* scaler) {
	for (int s = 0; s < NumScalers; s++) {
		if (!Pentagram::strcasecmp(scaler, Scalers[s].name)) {
			return s;
		}
	}

	return NoScaler;
}

const char* Image_window::get_displayname_for_scaler(int num) {
	if (num < 0 || static_cast<size_t>(num) >= Scalers.size()) {
		return get_name_for_scaler(num);    // Safe default name.
	}
	return Scalers[num].displayname_msg_index ? get_text_msg(Scalers[num].displayname_msg_index) : get_name_for_scaler(num);
}

/*
 * Image_window::Get_best_bpp
 *
 * Get the bpp for a scaled surface of the desired scaled video mode
 */

int Image_window::Get_best_bpp(int w, int h, int bpp) {
	if (w == 0 || h == 0) {
		return 0;
	}

	auto best_bpp = VideoModeOK(w, h, fullscreen);

	// Explicit BPP required
	if (bpp != 0) {
		auto forced_bpp = VideoModeOK(w, h, fullscreen, bpp);
		if (!(fullscreen)) {
			if (windowed != 0 && windowed == bpp) {
				return windowed;
			}
		}

		if (forced_bpp != 0) {
			return forced_bpp;
		}
		if (best_bpp != 0) {
			return best_bpp;
		}

		cerr << "SDL Reports " << w << "x" << h << " " << bpp << " bpp " << ((fullscreen) ? "fullscreen" : "windowed")
			 << " surface is not OK. Attempting to use " << bpp << " bpp anyway." << endl;
		return bpp;
	}

	if (!(fullscreen)) {
		if (desktop_depth == 16 && windowed != 0) {
			return 16;
		} else if (desktop_depth == 32 && windowed != 0) {
			return 32;
		} else if (windowed == 16) {
			return 16;
		} else if (windowed == 32) {
			return 32;
		} else if (windowed != 0) {
			return windowed;
		}
	}

	if ((desktop_depth == 16 || desktop_depth == 32) && best_bpp != 0) {
		return desktop_depth;
	}

	if (best_bpp == 16) {
		return 16;
	} else if (best_bpp == 32) {
		return 32;
	} else if (best_bpp != 0) {
		return best_bpp;
	}

	cerr << "SDL Reports " << w << "x" << h << " " << ((fullscreen) ? "fullscreen" : "windowed")
		 << " surfaces are not OK. Attempting to use 32 bpp. anyway" << endl;
	return 32;
}

/*
 *   Destroy window.
 */
void Image_window::static_init() {
	static bool done = false;
	if (done) {
		return;
	}
	done = true;

	cout << "Checking rendering support" << std::endl;

	const SDL_DisplayMode* dispmode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
	int                    bpp;
	Uint32                 Rmask;
	Uint32                 Gmask;
	Uint32                 Bmask;
	Uint32                 Amask;
	if (SDL_GetMasksForPixelFormat(dispmode->format, &bpp, &Rmask, &Gmask, &Bmask, &Amask)) {
		desktop_displaymode = *dispmode;
		desktop_depth       = bpp;
	} else {
		desktop_depth = 0;
		cout << "Error: Couldn't get desktop display depth!" << std::endl;
	}
	windowed = VideoModeOK(640, 400, false);

	cout << ' ' << "Windowed" << '\t';
	if (windowed) {
		cout << ' ' << windowed << ' ' << "bpp ok";
	}
	cout << std::endl;

	/* Get available fullscreen/hardware modes */
	SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(SDL_GetPrimaryDisplay(), nullptr);
	for (int j = 0; modes[j]; j++) {
		const Resolution res                          = {modes[j]->w, modes[j]->h};
		p_resolutions[(res.width << 16) | res.height] = res;
	}
	SDL_free(modes);

	// It's empty, so add in some basic resolutions that would be nice to
	// support
	if (p_resolutions.empty()) {
		Resolution res = {0, 0};

		res.width                                     = 640;
		res.height                                    = 480;
		p_resolutions[(res.width << 16) | res.height] = res;

		res.width                                     = 800;
		res.height                                    = 600;
		p_resolutions[(res.width << 16) | res.height] = res;

		res.width                                     = 800;
		res.height                                    = 600;
		p_resolutions[(res.width << 16) | res.height] = res;

		res.width                                     = 1024;
		res.height                                    = 768;
		p_resolutions[(res.width << 16) | res.height] = res;

		res.width                                     = 1280;
		res.height                                    = 720;
		p_resolutions[(res.width << 16) | res.height] = res;

		res.width                                     = 1280;
		res.height                                    = 768;
		p_resolutions[(res.width << 16) | res.height] = res;

		res.width                                     = 1280;
		res.height                                    = 800;
		p_resolutions[(res.width << 16) | res.height] = res;

		res.width                                     = 1280;
		res.height                                    = 960;
		p_resolutions[(res.width << 16) | res.height] = res;

		res.width                                     = 1280;
		res.height                                    = 1024;
		p_resolutions[(res.width << 16) | res.height] = res;

		res.width                                     = 1360;
		res.height                                    = 768;
		p_resolutions[(res.width << 16) | res.height] = res;

		res.width                                     = 1366;
		res.height                                    = 768;
		p_resolutions[(res.width << 16) | res.height] = res;

		res.width                                     = 1920;
		res.height                                    = 1080;
		p_resolutions[(res.width << 16) | res.height] = res;

		res.width                                     = 1920;
		res.height                                    = 1200;
		p_resolutions[(res.width << 16) | res.height] = res;
	}

	bool mode_ok = false;

	for (auto it = p_resolutions.begin(); it != p_resolutions.end();) {
		const Image_window::Resolution& res = it->second;
		bool                            ok  = false;

		int bpp = VideoModeOK(res.width, res.height, true);
		if (bpp) {
			mode_ok = true;
			ok      = true;
		}

		if (!ok) {
			p_resolutions.erase(it++);
		} else {
			cout << ' ' << res.width << "x" << res.height << '\t';
			cout << ' ' << bpp << ' ' << "bpp ok";
			cout << std::endl;
			++it;
		}
	}

#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID)
	if (windowed == 0) {
		cerr << "SDL Reports 640x400 windowed surfaces are not OK. Windowed "
				"scalers may not work properly."
			 << endl;
	}
#endif

	if (!mode_ok) {
		cerr << "SDL Reports no usable fullscreen resolutions." << endl;
	}

	// [SDL 3] force_bpp is set to 32, discard the config/video/force_bpp

	if (force_bpp != 0 && force_bpp != 16 && force_bpp != 8 && force_bpp != 32) {
		force_bpp = 0;
	} else if (force_bpp != 0) {
		cout << "Forcing bit depth to " << force_bpp << " bpp" << endl;
	}
}

/*
 *   Destroy window.
 */

Image_window::~Image_window() {
	free_surface();
	delete ibuf;

	// Clean up the SDL window.  Not particularly important for standalone
	// executable builds, but on android, if the app remains in memory after
	// exiting the game and you try to restart the game, the previous run's
	// window will still be allocated if we don't clean it up here and the
	// subsequent attempt to call SDL_CreateWindow() will crash.
	if (screen_renderer != nullptr) {
		SDL_DestroyRenderer(screen_renderer);
	}
	if (screen_window != nullptr) {
		SDL_DestroyWindow(screen_window);
	}
	screen_renderer = nullptr;
	screen_window   = nullptr;
}

/*
 *   Create the surface.
 */
void Image_window::create_surface(unsigned int w, unsigned int h) {
	uses_palette = true;
	free_surface();

	if (!Scalers[fill_scaler].arb && fill_scaler != SDLScaler) {
		if (Scalers[scaler].arb || scaler == SDLScaler) {
			fill_scaler = scaler;
		} else {
			fill_scaler = point;
		}
	}

	get_draw_dims(w, h, scale, fill_mode, game_width, game_height, inter_width, inter_height);
	saved_game_height = game_height;
	saved_game_width  = game_width;
	if (!try_scaler(w, h)) {
		// Try fallback to point scaler if it failed, if it doesn't work, we
		// probably can't run
		scaler = point;
		if (!try_scaler(w, h)) {
			throw exult_exception("Failed to creat Display Sorfaces");
		}
	}

	ibuf->width  = draw_surface->w;
	ibuf->height = draw_surface->h;

	ibuf->width -= guard_band * 2;
	ibuf->height -= guard_band * 2;

	// Update line size in words.
	ibuf->line_width = draw_surface->pitch / ibuf->pixel_size;
	// Offset it set to the top left pixel if the game window
	ibuf->offset_x = (get_full_width() - get_game_width()) / 2;
	ibuf->offset_y = (get_full_height() - get_game_height()) / 2;
	ibuf->bits     = static_cast<unsigned char*>(draw_surface->pixels) - get_start_x() - get_start_y() * ibuf->line_width;
	// Scaler guardband is in effect
	ibuf->bits += guard_band + ibuf->line_width * guard_band;
}

/*
 *   Set up surfaces for scaling.
 *   Output: False if error (reported).
 */

bool Image_window::create_scale_surfaces(int w, int h, int bpp) {
	int    hwdepth = bpp;
	uint32 flags   = SDL_WINDOW_HIGH_PIXEL_DENSITY;
	hwdepth        = Get_best_bpp(w, h, hwdepth);
	if (!hwdepth) {
		return false;
	}

	if (screen_window != nullptr) {
		SDL_SetWindowSize(screen_window, w, h);
#if 0
		{
			SDL_DisplayMode closest_mode;
			// This does not appear to have any effect
			if (SDL_GetClosestFullscreenDisplayMode(
						SDL_GetPrimaryDisplay(), w, h, 0.0, true,
						&closest_mode)) {
				SDL_SetWindowFullscreenMode(screen_window, &closest_mode);
			}
		}
#endif
		SDL_SetWindowFullscreen(screen_window, fullscreen);
	} else {
		screen_window = SDL_CreateWindow("", w, h, flags);
#if 0
		{
			SDL_DisplayMode closest_mode;
			// This does not appear to have any effect
			if (SDL_GetClosestFullscreenDisplayMode(
						SDL_GetPrimaryDisplay(), w, h, 0.0, true,
						&closest_mode)) {
				SDL_SetWindowFullscreenMode(screen_window, &closest_mode);
			}
		}
#endif
		SDL_SetWindowFullscreen(screen_window, fullscreen);
	}
	if (screen_window == nullptr) {
		cout << "Couldn't create window: " << SDL_GetError() << std::endl;
	}
	if (screen_renderer == nullptr) {
		screen_renderer = SDL_CreateRenderer(screen_window, nullptr);
	}
	if (screen_renderer == nullptr) {
		cout << "Couldn't create renderer: " << SDL_GetError() << std::endl;
	}
	int vsync = 1;
	config->value("config/video/vsync", vsync, vsync);
	SDL_SetRenderVSync(screen_renderer, vsync);

	SDL_DisplayID original_displayID = SDL_GetDisplayForWindow(screen_window);

	if (fullscreen) {
		int dw;
		int dh;
		// with HighDPi this returns the higher resolutions
		SDL_GetCurrentRenderOutputSize(screen_renderer, &dw, &dh);
#ifdef SIMULATE_HIDPI
		constexpr float simulated = SIMULATE_HIDPI + 0.f;
		if (simulated && dw == w && dh == h) {
			dw = w * simulated;
			dh = h * simulated;
		}
#endif
#ifdef SDL_PLATFORM_IOS
		SDL_GetWindowSizeInPixels(screen_window, &dw, &dh);
#endif
		w                            = dw;
		h                            = dh;
		const Resolution res         = {w, h};
		p_resolutions[(w << 16) | h] = res;
		// getting new native scale when highdpi is active
		int sw;
		SDL_GetWindowSize(screen_window, &sw, nullptr);
		nativescale = float(dw) / sw;
		// high resolution fullscreen needs this to make the whole screen
		// available
		SDL_SetRenderLogicalPresentation(screen_renderer, w, h, SDL_LOGICAL_PRESENTATION_LETTERBOX);
	} else {
		// make sure the window has the right dimensions
		SDL_SetWindowSize(screen_window, w, h);
		// center the window on the screen
		SDL_SetWindowPosition(
				screen_window, SDL_WINDOWPOS_CENTERED_DISPLAY(original_displayID),
				SDL_WINDOWPOS_CENTERED_DISPLAY(original_displayID));
		SDL_SetRenderLogicalPresentation(screen_renderer, w, h, SDL_LOGICAL_PRESENTATION_LETTERBOX);
	}
	display_width  = w;
	display_height = h;

	// Do an initial draw/fill
	SDL_SetRenderDrawColor(screen_renderer, 0, 0, 0, 255);
	SDL_RenderClear(screen_renderer);
	// Clear screen_texture_a too
	SDL_SetRenderTarget(screen_renderer, screen_texture_a);
	SDL_RenderClear(screen_renderer);
	SDL_SetRenderTarget(screen_renderer, nullptr);
	SDL_RenderPresent(screen_renderer);
	int    sbpp;
	Uint32 sRmask;
	Uint32 sGmask;
	Uint32 sBmask;
	Uint32 sAmask;
	SDL_GetMasksForPixelFormat(desktop_displaymode.format, &sbpp, &sRmask, &sGmask, &sBmask, &sAmask);

	if (screen_texture == nullptr) {
		screen_texture = SDL_CreateTexture(
				screen_renderer, desktop_displaymode.format, SDL_TEXTUREACCESS_STREAMING,
				std::max(inter_width, w) + 2 * guard_band * scale, std::max(inter_height, h) + 2 * guard_band * scale);
	}
	if (screen_texture == nullptr) {
		cout << "Couldn't create screen_texture: " << SDL_GetError() << std::endl;
	}
	// This is just to make sure that rects in screen_texture get copied exactly to screen_texture_a
	SDL_SetTextureScaleMode(screen_texture, SDL_SCALEMODE_NEAREST);

	if (screen_texture_a == nullptr) {
		screen_texture_a = SDL_CreateTexture(
				screen_renderer, desktop_displaymode.format, SDL_TEXTUREACCESS_TARGET, screen_texture->w, screen_texture->h);
	}
	if (screen_texture_a == nullptr) {
		cout << "Couldn't create screen_texture_a: " << SDL_GetError() << std::endl;
	}
	SDL_SetTextureBlendMode(screen_texture, SDL_BLENDMODE_NONE);
	SDL_SetTextureBlendMode(screen_texture_a, SDL_BLENDMODE_NONE);

	int draw_width  = inter_width / scale + 2 * guard_band;
	int draw_height = inter_height / scale + 2 * guard_band;
	if (!(draw_surface = SDL_CreateSurface(draw_width, draw_height, SDL_GetPixelFormatForMasks(ibuf->depth, 0, 0, 0, 0)))) {
		cerr << "Couldn't create draw surface" << endl;
		free_surface();
		return false;
	}
	if (!(SDL_CreateSurfacePalette(draw_surface))) {
		cerr << "Couldn't create draw surface" << endl;
		free_surface();
		return false;
	}

	// Scale using 'fill_scaler' only
	if (scaler == fill_scaler || scale == 1 || (fill_scaler == SDLScaler && (scaler == point || scaler == bilinear))) {
		// Use nearest scale mode if the scaler is point otherwise use linear
		SDL_SetTextureScaleMode(
				screen_texture_a, (fill_scaler == SDLScaler && scaler == point) ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);
		inter_surface = draw_surface;
	} else if (inter_width != w || inter_height != h) {
		int i_width  = inter_width + 2 * scale * guard_band;
		int i_height = inter_height + 2 * scale * guard_band;
		if (!(inter_surface = SDL_CreateSurface(i_width, i_height, SDL_PIXELFORMAT_ARGB8888))) {
			cerr << "Couldn't create inter surface: " << SDL_GetError() << endl;
			free_surface();
			return false;
		}
		// inter_surface could have an alpha channel but we don't want blending
		// so disable it
		SDL_SetSurfaceBlendMode(inter_surface, SDL_BLENDMODE_NONE);
	}
	// Scale using 'scaler' only
	else {
		inter_surface = nullptr;
	}

	paletted_surface = draw_surface;

	return true;
}

/*
 *   Set up surfaces and scaler to call.
 */

bool Image_window::try_scaler(int w, int h) {
	const ScalerInfo* info;

	if (scaler < 0 || scaler >= NumScalers || scale == 1) {
		info = &Scalers[point];
	} else {
		info = &Scalers[scaler];
	}

	// Is the size supported, if not, default to point scaler
	if (!(info->size_mask & SCALE_BIT(scale))) {
		return false;
	}

	bool has16 = ibuf->depth == 8 && info->fun8to16 && (force_bpp == 0 || force_bpp == 16);
	bool has32 = ibuf->depth == 8 && info->fun8to32 && (force_bpp == 0 || force_bpp == 32);

	if (info->arb) {
		has16 |= (force_bpp == 0 || force_bpp == 16) && info->arb->Support16bpp(ibuf->depth);
		has32 |= (force_bpp == 0 || force_bpp == 32) && info->arb->Support32bpp(ibuf->depth);
	}

	// 16 and 32 bit both un supported means this scaler is unuusable so bail
	if (!has16 && !has32) {
		return false;
	}
	// If both 16 and 32 bit supported allow create_scale_surfaces to select bpp
	// otherwise try 16 or 32 as suppoprted by the scaler
	return create_scale_surfaces(w, h, has16 && has32 ? 0 : has16 ? 16 : 32);
}

/*
 *   Free the surface.
 */

void Image_window::free_surface() {
	if (inter_surface != nullptr && inter_surface != draw_surface) {
		SDL_DestroySurface(inter_surface);
	}
	if (draw_surface != nullptr) {
		SDL_DestroySurface(draw_surface);
	}
	if (screen_texture != nullptr) {
		SDL_DestroyTexture(screen_texture);
	}
	if (screen_texture_a != nullptr) {
		SDL_DestroyTexture(screen_texture_a);
	}
	screen_texture   = nullptr;
	screen_texture_a = nullptr;
	paletted_surface = nullptr;
	inter_surface    = nullptr;
	draw_surface     = nullptr;
	ibuf->bits       = nullptr;
	free_layer_textures();
	if (screen_renderer != nullptr) {
		SDL_DestroyRenderer(screen_renderer);
	}
	screen_renderer = nullptr;
}

/*
 *   Create a compatible buffer.
 */

std::unique_ptr<Image_buffer> Image_window::create_buffer(
		int w, int h    // Dimensions.
) {
	return ibuf->create_another(w, h);
}

/*
 *   Window was resized.
 */

void Image_window::resized(
		unsigned int neww, unsigned int newh, bool newfs, unsigned int newgw, unsigned int newgh, int newsc, int newscaler,
		FillMode fmode, int fillsclr) {
	if (paletted_surface) {
		/* if (neww == display_surface->w && newh == display_surface->h &&
			newsc == scale && scaler == newscaler &&
			newgw == game_width && newgh == game_height)
			return; */     // Nothing changed.
		free_surface();    // Delete old image.
	}
	scale       = newsc;
	scaler      = newscaler;
	fullscreen  = newfs;
	game_width  = newgw;
	game_height = newgh;
	fill_mode   = fmode;
	fill_scaler = fillsclr;
	create_surface(neww, newh);    // Create new one.
}

/*
 *   Repaint portion of window.
 */

void Image_window::show(int x, int y, int w, int h) {
	PerformanceTimer::paintPerfMetrics();
	SDL_ClearError();

	auto perftimer = PerformanceTimer::GetScopedPerfTimer(__func__);
	if (!ready()) {
		return;
	}
	// While a full-screen scene layer is driving the display, the caller paints
	// into the scene buffer (not the game buffer) and blits via the normal path
	// (show(), palette apply/fade, FLI player). Re-upload the layers each show so
	// the animated scene refreshes.
	if (scene_mode) {
		mark_all_layers_dirty();
	}
	// call EndPaintIntoGuardBand just in case. It is safe to call it when not
	// needed
	EndPaintIntoGuardBand();

	int srcx = 0;
	int srcy = 0;
	if (!ibuf->clip(srcx, srcy, w, h, x, y)) {
		return;
	}
	x -= get_start_x();
	y -= get_start_y();

	// we can only include guard band in the buffersize if inter_surface is being used
	// otherwise scalers will write out of bounds. A separate
	// inter_surface has a guardband but screen_texture does not
	int gb = guard_band;
	//  Include guardband when comparing to width and height so the whole buffer
	//  can still be used if it is not a muliple of 4
	int buffer_w = get_full_width() + gb;
	int buffer_h = get_full_height() + gb;
	// Increase the area by 4 pixels
	increase_area(x, y, w, h, 4, 4, 4, 4, buffer_w, buffer_h);

	// Make it 4 pixel aligned too, needed for more advanced scalers that work
	// best with groups of 4 pixels, othwerwise we can get discontinuities
	// around the mouse cursor when it moves.
	const int dx = x & 3;
	const int dy = y & 3;
	x -= dx;
	w += dx;
	y -= dy;
	h += dy;

	// Round up to multiple of 4
	if (w & 3) {
		w += 4 - (w & 3);
	}
	if (h & 3) {
		h += 4 - (h & 3);
	}

	// Clip rounded up sizes to the buffer size
	if (w + x > buffer_w) {
		w = buffer_w - x;
	}

	if (h + y > buffer_h) {
		h = buffer_h - y;
	}
	// Create temporary display_surface from screen_texture;
	SDL_Surface* display_surface;

	// The region of screen_texture that was changed
	SDL_FRect dirtyrect = {0, 0, 0, 0};
	// The Region of the screen_texture that needs to be rendered to the screen
	SDL_FRect fullrect = {0, 0, 0, 0};
	{
		auto perftimer_s_ltts = PerformanceTimer::GetScopedPerfTimer(__func__, " SDL_LockTextureToSurface");

		// Lock the texture as a surface so we can draw to it. The Lock operation imvalidates anything already in the texture
		if (!SDL_LockTextureToSurface(screen_texture, nullptr, &display_surface)) {
			const char* err = SDL_GetError();
			std::cerr << "SDL_LockTextureToSurface failed: " << (err ? err : "") << std::endl;
			SDL_ClearError();
		}
	}

	// inter_surface is not null use that, otherwise set it to the temporary display_surface
	inter_surface = inter_surface ? inter_surface : display_surface;

	// No scaling if ibuf and draw_surface are not the same size
	if (ibuf->width + 2 * guard_band == draw_surface->w && ibuf->height + 2 * guard_band == draw_surface->h) {
		// Phase 1 blit from draw_surface to inter_surface. This only scales the input rectangle
		const SDL_PixelFormatDetails* inter_surface_format  = SDL_GetPixelFormatDetails(inter_surface->format);
		const size_t                  inter_bytes_per_pixel = inter_surface_format->bits_per_pixel / 8;
		if (draw_surface != inter_surface) {
			auto              perftimer_s = PerformanceTimer::GetScopedPerfTimer(__func__, " scaler");
			const ScalerInfo& sel_scaler  = Scalers[scaler];

			if (sel_scaler.arb) {
				if (!sel_scaler.arb->Scale(
							draw_surface, x + guard_band, y + guard_band, w, h, inter_surface, scale * (x + guard_band),
							scale * (y + guard_band), scale * w, scale * h, false)) {
					Scalers[point].arb->Scale(
							draw_surface, x + guard_band, y + guard_band, w, h, inter_surface, scale * (x + guard_band),
							scale * (y + guard_band), scale * w, scale * h, false);
				}
			} else {
				scalefun show_scaled;
				if (inter_surface_format->bits_per_pixel == 16 || inter_surface_format->bits_per_pixel == 15) {
					const int r = inter_surface_format->Rmask;
					const int g = inter_surface_format->Gmask;
					const int b = inter_surface_format->Bmask;

					show_scaled = (r == 0xf800 && g == 0x7e0 && b == 0x1f) || (b == 0xf800 && g == 0x7e0 && r == 0x1f)
										  ? (sel_scaler.fun8to565 != nullptr ? sel_scaler.fun8to565 : sel_scaler.fun8to16)
								  : (r == 0x7c00 && g == 0x3e0 && b == 0x1f) || (b == 0x7c00 && g == 0x3e0 && r == 0x1f)
										  ? (sel_scaler.fun8to555 != nullptr ? sel_scaler.fun8to555 : sel_scaler.fun8to16)
										  : sel_scaler.fun8to16;
				} else if (inter_surface_format->bits_per_pixel == 32) {
					show_scaled = sel_scaler.fun8to32;
				} else {
					show_scaled = sel_scaler.fun8to8;
				}

				(this->*show_scaled)(x, y, w, h);
			}
			x += guard_band;
			y += guard_band;
			x *= scale;
			y *= scale;
			w *= scale;
			h *= scale;

			fullrect.x = guard_band * scale;
			fullrect.y = guard_band * scale;
			fullrect.w = (draw_surface->w - 2 * guard_band) * scale;
			fullrect.h = (draw_surface->h - 2 * guard_band) * scale;
		}

		// Phase 2 blit from inter_surface to display_surface (this scales the entire surface)
		if (inter_surface != display_surface && fill_scaler != SDLScaler) {
			auto              perftimer_fs = PerformanceTimer::GetScopedPerfTimer(__func__, " fill scaler");
			const ScalerInfo& sel_scaler   = Scalers[fill_scaler];

			// Just scale entire surfaces
			if (inter_surface == draw_surface) {
				x = guard_band;
				y = guard_band;
				w = get_full_width();
				h = get_full_height();
			} else {
				x = guard_band * scale;
				y = guard_band * scale;
				w = inter_width;
				h = inter_height;
			}
			int dw = display_surface->w - 2 * guard_band * scale;
			int dh = display_surface->h - 2 * guard_band * scale;

			if (!sel_scaler.arb || !sel_scaler.arb->Scale(inter_surface, x, y, w, h, display_surface, 0, 0, dw, dh, false)) {
				Scalers[point].arb->Scale(inter_surface, x, y, w, h, display_surface, 0, 0, dw, dh, false);
			}

			x          = 0;
			y          = 0;
			w          = dw;
			h          = dh;
			fullrect.x = x;
			fullrect.y = y;
			fullrect.w = w;
			fullrect.h = h;
		}

		// Copy the entire inter surface including guardband to the display surface with no scaling here.
		else if (fill_scaler == SDLScaler && inter_surface != display_surface) {
			auto perfcounter_sfs = PerformanceTimer::GetScopedPerfTimer(__func__, " sdl fill scaler");
			// display_surface is always as big as or bigger than inter_surface so we copy the entire inter_surface to
			// display_surface So the scaling can be done when rendering the texture in UpdateRect

			if (inter_surface == draw_surface) {
				x = guard_band;
				y = guard_band;
				w = get_full_width();
				h = get_full_height();
			} else {
				x = guard_band * scale;
				y = guard_band * scale;
				w = inter_width;
				h = inter_height;
			}
			// Copy everything using memcpy if the surfaces have the same format
			if (inter_surface->format == display_surface->format) {
				char* src = reinterpret_cast<char*>(inter_surface->pixels) + x * inter_bytes_per_pixel
							+ y * size_t(inter_surface->pitch);
				char* end = src + h * inter_surface->pitch;
				char* dst = reinterpret_cast<char*>(display_surface->pixels) + x * inter_bytes_per_pixel
							+ y * size_t(display_surface->pitch);

				while (src != end) {
					memcpy(dst, src, w * inter_bytes_per_pixel);
					src += inter_surface->pitch;
					dst += display_surface->pitch;
				}
			} else {
				// Fallback to SDL if pixel format conversion is needed
				if (!SDL_BlitSurface(inter_surface, nullptr, display_surface, nullptr)) {
					const char* err = SDL_GetError();
					std::cerr << "SDL_BlitSurface failed: " << (err ? err : "") << std::endl;
					SDL_ClearError();
				}
			}
			fullrect.x = x;
			fullrect.y = y;
			fullrect.w = w;
			fullrect.h = h;
		}
	}
	dirtyrect.x = x;
	dirtyrect.y = y;
	dirtyrect.w = w;
	dirtyrect.h = h;

	// Phase 3 call UpdateRect to render screen_texture

	// reset inter_surface back to nullptr if it was set to the temporary display_surface that is about to be destroyed
	if (inter_surface == display_surface) {
		inter_surface = nullptr;
	}
	// Unlocking the texture destroys display_surface
	SDL_UnlockTexture(screen_texture);
	UpdateRect(&dirtyrect, &fullrect);

	const char* err = SDL_GetError();
	if (err && *err) {
		std::cerr << "A SDL error occurred: " << err << std::endl;
	}
}

/*
 *   Toggle fullscreen.
 */
void Image_window::toggle_fullscreen() {
	int w;
	int h;

	w = display_height;
	h = display_height;

	if (fullscreen) {
		cout << "Switching to windowed mode." << endl;
	} else {
		cout << "Switching to fullscreen mode." << endl;
	}
	/* First see if it's allowed.
	 * for now this is preventing the switch to fullscreen
	 *if ( VideoModeOK(w, h, !fullscreen) )
	 */
	{
		free_surface();    // Delete old.
		fullscreen = !fullscreen;
		create_surface(w, h);    // Create new.
	}
}

void Image_window::BeginPaintIntoGuardBand(int* x, int* y, int* w, int* h) {
	int tx = 0, ty = 0, tw = 0, th = 0;

	if (!x) {
		x = &tx;
	}
	if (!y) {
		y = &ty;
	}
	if (!w) {
		w = &tw;
	}
	if (!h) {
		h = &th;
	}

	// adjust ibuf and game dimension things so we can draw into the right and
	// bottom guardband to avoid blacklines when scalers read the guardband.
	// Only apply these temporary changes once until EndPaintIntoGuardBand().
	if (ShouldPaintIntoGuardband() && !guardband_paint_active) {
		// Adjust clip rect on the buffer
		int cx, cy, cw, ch;
		ibuf->get_clip(cx, cy, cw, ch);
		// expand and clip incoming rect to new buffer size and do the same to
		// the clipping rect on the buffer but only if the buffer is set to the
		// same size as the game screen
		if (game_width == ibuf->width) {
			if ((*x + *w) >= ibuf->width) {
				*w = ibuf->width + guard_band / 2 - *x;
			}
			if (cx + cw == ibuf->width) {
				cw = ibuf->width + guard_band / 2 - cx;
			}
		}
		if (game_height == ibuf->height) {
			if ((*y + *h) >= ibuf->height) {
				*h = ibuf->height + guard_band / 2 - *y;
			}
			if ((cy + ch) >= ibuf->height) {
				ch = ibuf->height + guard_band / 2 - cy;
			}
		}
		if (saved_game_width == ibuf->width) {
			game_width = saved_game_width + guard_band / 2;
		}

		if (saved_game_height == ibuf->height) {
			game_height = saved_game_height + guard_band / 2;
		}

		// we only allow drawing into half the guardband on the right and botom
		// and not at all on the top and left as none of the scalers will try
		// reading out of bounds to the left or up
		// draw surface includes 2 guard bands of extrapixels so to allow
		// drawing into half a guardband we need to subtract 1.5 guardbands from
		// the draw surface dimensions
		ibuf->width  = draw_surface->w - 3 * guard_band / 2;
		ibuf->height = draw_surface->h - 3 * guard_band / 2;
		// set the new clipping rectangle after updating the buffer dimensions
		ibuf->set_clip(cx, cy, cw, ch);
		guardband_paint_active = true;
	}
	// clip the rectangle
	if (*x + *w > ibuf->width) {
		*w = ibuf->width - *x;
	}
	if (*y + *h > ibuf->height) {
		*h = ibuf->height - *y;
	}
}

void Image_window::EndPaintIntoGuardBand() {
	if (!guardband_paint_active) {
		return;
	}
	// Restore ibuf and game screen dimensions after temporary guardband paint
	// expansion, regardless of whether game_width/game_height were modified.
	ibuf->width            = draw_surface->w - 2 * guard_band;
	ibuf->height           = draw_surface->h - 2 * guard_band;
	game_width             = saved_game_width;
	game_height            = saved_game_height;
	guardband_paint_active = false;
}

void Image_window::FillGuardband() {
	// In scene mode drawing goes to a full-screen scene layer buffer that has no
	// guard band; the game window's guard band must not be filled from it.
	if (scene_mode) {
		return;
	}
	auto pixels = static_cast<uint8*>(draw_surface->pixels) + guard_band + guard_band * draw_surface->pitch;
	// Bottom
	auto read  = pixels + (ibuf->height - 1) * draw_surface->pitch;
	auto write = read + draw_surface->pitch;
	auto end   = write + guard_band * draw_surface->pitch;
	while (write != end) {
		memcpy(write, read, ibuf->width);
		write += draw_surface->pitch;
	}
	// right
	auto ptr = pixels + ibuf->width - 1;
	end      = ptr + (ibuf->height + guard_band) * draw_surface->pitch;

	while (ptr != end) {
		// guard_band is a constant and should be 4
		// Compilers should be smart enough to optimize the memset away into a
		// 32bit write so no performance impact using memset here
		memset(ptr + 1, *ptr, guard_band);
		ptr += draw_surface->pitch;
	}
}

bool Image_window::screenshot(SDL_IOStream* dst) {
	if (!paletted_surface) {
		return false;
	}
	return SaveIMG_RW(draw_surface, dst, true, guard_band);
}

void Image_window::set_title(const char* title) {
	SDL_SetWindowTitle(screen_window, title);
}

void Image_window::screen_to_game(int sx, int sy, bool fast, int& gx, int& gy) {
	// While a full-screen scene layer owns the display, map through it so mouse
	// hit-testing lines up with widgets drawn in the (scaled) scene.
	if (scene_mode && active_scene_layer >= 0) {
		screen_to_layer(active_scene_layer, sx, sy, gx, gy);
		return;
	}
	if (fast) {
		gx = sx + get_start_x();
		gy = sy + get_start_y();
		if (Mouse::mouse()) {
			Mouse::mouse()->apply_fast_offset(gx, gy);
		}
	} else {
		gx = (sx * inter_width) / (scale * get_display_width()) + get_start_x();
		gy = (sy * inter_height) / (scale * get_display_height()) + get_start_y();
	}
}

void Image_window::game_to_screen(int gx, int gy, bool fast, int& sx, int& sy) {
	// Inverse of screen_to_game while a full-screen scene layer owns the display
	// (keeps the cursor aligned with the scaled scene).
	if (scene_mode && active_scene_layer >= 0) {
		layer_to_screen(active_scene_layer, gx, gy, sx, sy);
		return;
	}
	if (fast) {
		if (Mouse::mouse()) {
			Mouse::mouse()->unapply_fast_offset(gx, gy);
		}
		sx = gx - get_start_x();
		sy = gy - get_start_y();
	} else {
		sx = ((gx - get_start_x()) * scale * get_display_width()) / inter_width;
		sy = ((gy - get_start_y()) * scale * get_display_height()) / inter_height;
	}
}

bool Image_window::get_draw_dims(int sw, int sh, int scale, FillMode fillmode, int& gw, int& gh, int& iw, int& ih) {
	// Handle each type separately

	if (fillmode == Fill) {
		if (gw == 0 || gh == 0) {
			gw = sw / scale;
			gh = sh / scale;
		}

		iw = gw * scale;

		ih = gh * scale;
	} else if (fillmode == Fit) {
		if (gw == 0 || gh == 0) {
			gw = sw / scale;
			gh = sh / scale;
		}

		// Height determines the scaling factor
		if (sw * gh >= sh * gw) {
			ih = gh * scale;
			// iw = sw * gh * scale / sh >= gw * scale => L&R bands
			iw = (sw * ih) / (sh);
		}
		// Width determines the scaling factor
		else {
			iw = gw * scale;
			// ih = sh * gw * scale / sw >  gh * scale => T&B bands
			ih = (sh * iw) / (sw);
		}
	} else if (fillmode == AspectCorrectFit) {
		if (gw == 0 || gh == 0) {
			gw = sw / scale;
			gh = (sh * 5) / (scale * 6);
		}

		// Height determines the scaling factor
		if (sw * gh * 6 >= sh * gw * 5) {
			if (gh == (sh * 5) / (scale * 6)) {
				ih = (sh * 5) / 6;
			} else {
				ih = gh * scale;
			}
			// iw = 6 * sw * gh * scale / 5 * sh >= gw * scale => L&R bands
			iw = (sw * ih * 6) / (sh * 5);
		}
		// Width determines the scaling factor
		else {
			if (gw == sw / scale) {
				iw = sw;
			} else {
				iw = gw * scale;
			}
			// ih = 5 * sh * gw * scale / 6 * sw >  gh * scale => T&B bands
			ih = (sh * iw * 5) / (sw * 6);
		}
	} else if (fillmode >= Centre && fillmode < (1 << 16)) {
		const int  factor        = 2 + ((fillmode - Centre) / 2);
		const int  aspect_factor = (fillmode & 1) ? 5 : 6;
		const bool auto_game     = (gw == 0 || gh == 0);

		if (auto_game) {
			gw = (sw * 2) / (factor * scale);
			gh = (sh * aspect_factor) / (3 * factor * scale);
		}

		iw = (sw * 2) / factor;
		ih = (sh * aspect_factor) / (3 * factor);

		if (auto_game) {
			if (gw * scale > iw) {
				gw = iw / scale;
			}

			if (gh * scale > ih) {
				gh = ih / scale;
			}
		} else {
			// For an explicit game area (e.g. fixed 320x200), keep the logical
			// size stable and expand the intermediate surface as needed so the
			// view remains visible in centered/aspect-correct modes.
			iw = std::max(iw, gw * scale);
			ih = std::max(ih, gh * scale);
		}
	} else {
		const int fw = fillmode & 0xFFFF;
		const int fh = (fillmode >> 16) & 0xFFFF;

		if (!fw || !fh) {
			return false;
		}

		if (gw == 0 || gh == 0) {
			gw = fw / scale;
			gh = fh / scale;
		}

		if (fw > sw) {
			gw = (gw * sw) / fw;
			iw = gw * scale;
		} else {
			iw = (sw * gw * scale) / fw;
		}

		if (fh > sh) {
			gh = (gh * sh) / fh;
			ih = gh * scale;
		} else {
			ih = (sh * gh * scale) / fh;
		}
	}

	// If there is a rounding error don't scale twice, just centre
	if (iw == (sw / scale) * scale) {
		iw = sw;
	}
	if (ih == (sh / scale) * scale) {
		ih = sh;
	}

	return true;
}

Image_window::FillMode Image_window::string_to_fillmode(const char* str) {
	// If only C++ had reflection capabilities...

	if (!Pentagram::strcasecmp(str, "Fill")) {
		return Fill;
	} else if (!Pentagram::strcasecmp(str, "Fit")) {
		return Fit;
	} else if (!Pentagram::strcasecmp(str, "Aspect Correct Fit")) {
		return AspectCorrectFit;
	} else if (!Pentagram::strcasecmp(str, "Centre") || !Pentagram::strcasecmp(str, "Center")) {
		return Centre;
	} else if (
			!Pentagram::strcasecmp(str, "Aspect Correct Centre") || !Pentagram::strcasecmp(str, "Aspect Correct Center")
			|| !Pentagram::strcasecmp(str, "Centre Aspect Correct") || !Pentagram::strcasecmp(str, "Center Aspect Correct")) {
		return AspectCorrectCentre;
	} else if (!Pentagram::strncasecmp(str, "Centre ", 7) || !Pentagram::strncasecmp(str, "Center ", 7)) {
		str += 7;
		if (*str != 'X' && *str != 'x') {
			return static_cast<FillMode>(0);
		}

		++str;
		if (!std::isdigit(static_cast<unsigned char>(*str))) {
			return static_cast<FillMode>(0);
		}

		char*               end;
		const unsigned long f = std::strtoul(str, &end, 10);
		str += (end - str);

		if (f >= (65536 - Centre) / 2 || *str) {
			return static_cast<FillMode>(0);
		}

		return static_cast<FillMode>(Centre + f * 2);
	} else if (
			!Pentagram::strncasecmp(str, "Aspect Correct Centre ", 22) || !Pentagram::strncasecmp(str, "Aspect Correct Center ", 22)
			|| !Pentagram::strncasecmp(str, "Centre Aspect Correct ", 22)
			|| !Pentagram::strncasecmp(str, "Center Aspect Correct ", 22)) {
		str += 22;
		if (*str != 'X' && *str != 'x') {
			return static_cast<FillMode>(0);
		}

		++str;
		if (!std::isdigit(static_cast<unsigned char>(*str))) {
			return static_cast<FillMode>(0);
		}

		char*               end;
		const unsigned long f = std::strtoul(str, &end, 10);
		str += (end - str);

		if (f >= (65536 - AspectCorrectCentre) / 2 || *str) {
			return static_cast<FillMode>(0);
		}

		return static_cast<FillMode>(AspectCorrectCentre + f * 2);
	} else {
		if (!std::isdigit(static_cast<unsigned char>(*str))) {
			return static_cast<FillMode>(0);
		}

		char*               end;
		const unsigned long fx = std::strtoul(str, &end, 10);
		str += (end - str);

		if (fx > 65535 || (*str != 'X' && *str != 'x')) {
			return static_cast<FillMode>(0);
		}

		++str;
		if (!std::isdigit(static_cast<unsigned char>(*str))) {
			return static_cast<FillMode>(0);
		}

		const unsigned long fy = std::strtoul(str, &end, 10);
		str += (end - str);

		if (fy > 65535 || *str) {
			return static_cast<FillMode>(0);
		}

		return static_cast<FillMode>(fx | (fy << 16));
	}
}

bool Image_window::fillmode_to_string(FillMode fmode, std::string& str) {
	// If only C++ had reflection capabilities...

	if (fmode == Fill) {
		str = "Fill";
		return true;
	} else if (fmode == Fit) {
		str = "Fit";
		return true;
	} else if (fmode == AspectCorrectFit) {
		str = "Aspect Correct Fit";
		return true;
	} else if (fmode >= Centre && fmode < (1 << 16)) {
		const int factor = 2 + ((fmode - Centre) / 2);
		char      factor_str[16];

		if (factor == 2) {
			factor_str[0] = 0;
		} else {
			snprintf(factor_str, sizeof(factor_str), (factor & 1) ? " x%d.5" : " x%d", factor / 2);
		}

		if (fmode & 1) {
			str = std::string("Aspect Correct Centre") + std::string(factor_str);
		} else {
			str = std::string("Centre") + std::string(factor_str);
		}
		return true;
	} else {
		const int fw = fmode & 0xFFFF;
		const int fh = (fmode >> 16) & 0xFFFF;

		if (!fw || !fh) {
			return false;
		}

		char factor_str[16];
		snprintf(factor_str, sizeof(factor_str), "%dx%d", fw, fh);
		str = std::string(factor_str);

		return true;
	}

	return false;
}

/*
 *  Layers.
 */

int Image_window::create_layer(std::string&& name, int w, int h, unsigned char transparent, int fixed_scale, int z) {
	if (w <= 0 || h <= 0) {
		return -1;
	}
	std::unique_ptr<Image_buffer> buf = create_buffer(w, h);
	if (!buf) {
		return -1;
	}
	buf->fill8(transparent);    // Start fully transparent.
	auto layer = std::make_unique<Layer>(std::move(buf), w, h, transparent, fixed_scale, z, std::move(name));
	// Reuse a freed slot if there is one, so handles stay stable.
	for (size_t i = 0; i < layers.size(); i++) {
		if (!layers[i]) {
			layers[i] = std::move(layer);
			return static_cast<int>(i);
		}
	}
	layers.push_back(std::move(layer));
	return static_cast<int>(layers.size() - 1);
}

void Image_window::destroy_layer(int handle) {
	if (handle < 0 || handle >= static_cast<int>(layers.size()) || !layers[handle]) {
		return;
	}
	if (layers[handle]->texture) {
		SDL_DestroyTexture(layers[handle]->texture);
	}
	layers[handle].reset();
}

Image_buffer* Image_window::get_layer_ibuf(int handle) {
	if (handle < 0 || handle >= static_cast<int>(layers.size()) || !layers[handle]) {
		return nullptr;
	}
	return layers[handle]->get_ibuf();
}

void Image_window::layer_set_dirty(int handle) {
	if (handle < 0 || handle >= static_cast<int>(layers.size()) || !layers[handle]) {
		return;
	}
	layers[handle]->set_dirty();
}

void Image_window::layer_set_visible(int handle, bool visible) {
	if (handle < 0 || handle >= static_cast<int>(layers.size()) || !layers[handle]) {
		return;
	}
	layers[handle]->set_visible(visible);
}

void Image_window::layer_set_opaque(int handle, bool opaque) {
	if (handle < 0 || handle >= static_cast<int>(layers.size()) || !layers[handle]) {
		return;
	}
	if (layers[handle]->is_opaque() != opaque) {
		layers[handle]->set_opaque(opaque);
		layers[handle]->set_dirty();    // Re-upload with the new transparency.
	}
}

bool Image_window::layer_is_visible(int handle) {
	if (handle < 0 || handle >= static_cast<int>(layers.size()) || !layers[handle]) {
		return false;
	}
	return layers[handle]->is_visible();
}

void Image_window::layer_set_index_argb(int handle, const uint32* argb256) {
	if (handle < 0 || handle >= static_cast<int>(layers.size()) || !layers[handle]) {
		return;
	}
	Layer& layer = *layers[handle];
	if (argb256 == nullptr) {
		layer.index_argb.clear();
	} else {
		layer.index_argb.assign(argb256, argb256 + 256);
	}
	layer.set_dirty();
}

void Image_window::layer_set_z(int handle, int z) {
	if (handle < 0 || handle >= static_cast<int>(layers.size()) || !layers[handle]) {
		return;
	}
	layers[handle]->z = z;
}

void Image_window::layer_set_alpha(int handle, unsigned char a) {
	if (handle < 0 || handle >= static_cast<int>(layers.size()) || !layers[handle]) {
		return;
	}
	layers[handle]->alpha = a;
}

void Image_window::layer_set_dest(int handle, int x, int y, int w, int h) {
	if (handle < 0 || handle >= static_cast<int>(layers.size()) || !layers[handle]) {
		return;
	}
	Layer& layer   = *layers[handle];
	layer.has_dest = true;
	layer.dest.x   = static_cast<float>(x);
	layer.dest.y   = static_cast<float>(y);
	layer.dest.w   = static_cast<float>(w);
	layer.dest.h   = static_cast<float>(h);
}

void Image_window::layer_clear_dest(int handle) {
	if (handle < 0 || handle >= static_cast<int>(layers.size()) || !layers[handle]) {
		return;
	}
	layers[handle]->has_dest = false;
}

void Image_window::layer_set_ui_kind(int handle, UiLayerKind kind) {
	if (handle < 0 || handle >= static_cast<int>(layers.size()) || !layers[handle]) {
		return;
	}
	if (kind < UiLayerDefault || kind >= NumUiLayerKinds) {
		kind = UiLayerDefault;
	}
	layers[handle]->ui_kind = kind;
}

void Image_window::mark_all_layers_dirty() {
	for (auto& lp : layers) {
		if (lp) {
			lp->set_dirty();
		}
	}
}

int Image_window::layer_render_scale(const Layer& layer) const {
	const UiLayerConfig& cfg     = get_ui_cfg(layer.ui_kind);
	const int            uscaler = eff_ui_scaler(cfg);
	if (uscaler < 0 || static_cast<size_t>(uscaler) >= Scalers.size()) {
		return 1;
	}
	const ScalerInfo& si = Scalers[uscaler];
	// arb scalers (Point/Bilinear) and the SDL scaler are handled by the GPU
	// path (uploaded 1:1 and scaled by SDL), so there is no pre-scaling.
	if (si.arb != nullptr || si.fun8to32 == nullptr) {
		return 1;
	}
	const uint32 mask = si.size_mask;
	for (int f = 2; f <= 4; ++f) {
		if (mask == (1u << (f - 1))) {
			return f;
		}
	}
	const int uscale = eff_ui_scale(cfg);
	if (uscale >= 2 && (mask & (1u << (uscale - 1))) != 0) {
		return uscale;
	}
	return 1;
}

bool Image_window::scale_layer_color(const Layer& layer, SDL_Surface* src8, int logw, int logh, SDL_Surface* dst32) {
	if (src8 == nullptr || dst32 == nullptr || ibuf == nullptr) {
		return false;
	}
	const UiLayerConfig& cfg     = get_ui_cfg(layer.ui_kind);
	const int            uscaler = eff_ui_scaler(cfg);
	if (uscaler < 0 || static_cast<size_t>(uscaler) >= Scalers.size()) {
		return false;
	}
	const ScalerInfo& si = Scalers[uscaler];
	if (si.arb != nullptr || si.fun8to32 == nullptr) {
		return false;
	}
	// The destination is sized by layer_render_scale(); the scaler must run at
	// that same factor so it does not write past the destination.
	const int factor = layer_render_scale(layer);
	// Temporarily repoint the scaling state at the layer's surfaces (and the
	// UI scaler's scale factor), run the scaler, then restore. The source
	// must be guard-banded (guard_band padding on every side) with its content
	// at (guard_band, guard_band).
	SDL_Surface* save_draw   = draw_surface;
	SDL_Surface* save_inter  = inter_surface;
	SDL_Surface* save_pal    = paletted_surface;
	const auto   save_lwidth = ibuf->line_width;
	const auto   save_height = ibuf->height;
	const int    save_scale  = scale;

	draw_surface     = src8;
	paletted_surface = src8;
	inter_surface    = dst32;
	ibuf->line_width = src8->pitch;    // 8-bit: pitch (bytes) == pixels/line.
	ibuf->height     = logh;
	scale            = factor;    // Interlace/point scalers read this.

	(this->*(si.fun8to32))(0, 0, logw, logh);

	draw_surface     = save_draw;
	inter_surface    = save_inter;
	paletted_surface = save_pal;
	ibuf->line_width = save_lwidth;
	ibuf->height     = save_height;
	scale            = save_scale;
	return true;
}

void Image_window::get_layer_dest(const Layer& layer, SDL_FRect& dst) {
	if (layer.has_dest) {    // Explicit placement (e.g. the mouse cursor).
		dst = layer.dest;
		return;
	}
	compute_layer_fill_dest(layer.logw, layer.logh, dst, layer.ui_kind);
}

void Image_window::set_ui_config(int width, int height, int scaler_, FillMode fmode, int fill_scaler_) {
	UiLayerConfig cfg;
	cfg.width       = width;
	cfg.height      = height;
	cfg.scaler      = scaler_;
	cfg.fill_mode   = fmode;
	cfg.fill_scaler = fill_scaler_;
	for (int i = 0; i < NumUiLayerKinds; ++i) {
		ui_cfgs[i] = cfg;
	}
	// Sizes/scale may have changed; force layer textures to rebuild.
	mark_all_layers_dirty();
}

void Image_window::set_ui_layer_config(UiLayerKind kind, int width, int height, int scaler_, FillMode fmode, int fill_scaler_) {
	if (kind < UiLayerDefault || kind >= NumUiLayerKinds) {
		return;
	}
	UiLayerConfig& cfg = ui_cfgs[static_cast<int>(kind)];
	cfg.width          = width;
	cfg.height         = height;
	cfg.scaler         = scaler_;
	cfg.fill_mode      = fmode;
	cfg.fill_scaler    = fill_scaler_;
	mark_all_layers_dirty();
}

void Image_window::set_ui_layer_palette(UiLayerKind kind, int mode) {
	if (kind < UiLayerDefault || kind >= NumUiLayerKinds) {
		return;
	}
	UiLayerConfig& cfg = ui_cfgs[static_cast<int>(kind)];
	if (cfg.ui_palette == mode) {
		return;
	}
	cfg.ui_palette = mode;
	if (mode == UiPaletteDisabled) {
		cfg.ui_palette_colors.clear();    // No override; follow the live palette.
	}
	mark_all_layers_dirty();
}

int Image_window::get_ui_layer_palette_mode(UiLayerKind kind) const {
	if (kind < UiLayerDefault || kind >= NumUiLayerKinds) {
		return UiPaletteDisabled;
	}
	return ui_cfgs[static_cast<int>(kind)].ui_palette;
}

void Image_window::set_ui_layer_palette_colors(UiLayerKind kind, const unsigned char* colors768) {
	if (kind < UiLayerDefault || kind >= NumUiLayerKinds) {
		return;
	}
	UiLayerConfig& cfg = ui_cfgs[static_cast<int>(kind)];
	if (colors768 == nullptr) {
		if (cfg.ui_palette_colors.empty()) {
			return;
		}
		cfg.ui_palette_colors.clear();
	} else {
		cfg.ui_palette_colors.assign(colors768, colors768 + 768);
	}
	mark_all_layers_dirty();
}

void Image_window::set_ui_layer_kind_mask(uint32 mask) {
	ui_layer_kind_mask = mask;
}

uint32 Image_window::get_ui_layer_kind_mask() const {
	return ui_layer_kind_mask;
}

void Image_window::set_ui_layer_kind_enabled(UiLayerKind kind, bool enabled) {
	if (kind < UiLayerDefault || kind >= NumUiLayerKinds) {
		return;
	}
	const uint32 bit = 1u << static_cast<int>(kind);
	if (enabled) {
		ui_layer_kind_mask |= bit;
	} else {
		ui_layer_kind_mask &= ~bit;
	}
}

bool Image_window::is_ui_layer_kind_enabled(UiLayerKind kind) const {
	if (kind < UiLayerDefault || kind >= NumUiLayerKinds) {
		return false;
	}
	const uint32 bit = 1u << static_cast<int>(kind);
	return (ui_layer_kind_mask & bit) != 0;
}

int Image_window::get_ui_width() const {
	return get_ui_width(UiLayerDefault);
}

int Image_window::get_ui_width(UiLayerKind kind) const {
	const UiLayerConfig& cfg = get_ui_cfg(kind);
	if (cfg.width <= 0 || cfg.height <= 0) {
		return game_width;
	}
	return cfg.width;
}

int Image_window::get_ui_height() const {
	return get_ui_height(UiLayerDefault);
}

int Image_window::get_ui_height(UiLayerKind kind) const {
	const UiLayerConfig& cfg = get_ui_cfg(kind);
	if (cfg.width <= 0 || cfg.height <= 0) {
		return game_height;
	}
	return cfg.height;
}

void Image_window::compute_fill_dest(int logw, int logh, FillMode fmode, int escl, SDL_FRect& dst) const {
	const int dw = display_width;
	const int dh = display_height;
	double    cw;
	double    ch;
	if (logw <= 0 || logh <= 0) {
		cw = dw;
		ch = dh;
	} else {
		// Aspect-correct modes give pixels a 1:1.2 aspect (taller).
		const bool aspect = (fmode == AspectCorrectFit) || (fmode == AspectCorrectCentre)
							|| (fmode > Centre && fmode < (1 << 16) && (fmode & 1));
		const double eff_h = logh * (aspect ? 1.2 : 1.0);
		if (fmode == Fill) {
			cw = dw;
			ch = dh;
		} else if (fmode >= Centre && fmode < (1 << 16)) {
			// Centre family: a fixed scale (ui scale, plus .5 increments).
			const int    base = fmode & ~1;    // Even = non-aspect variant.
			const double mult = 1.0 + (base - Centre) / 4.0;
			const double f    = escl * mult;
			cw                = logw * f;
			ch                = eff_h * f;
		} else {
			// Fit / AspectCorrectFit (and any fallback): scale to fit, keeping
			// the pixel aspect, centred.
			const double sx = dw / static_cast<double>(logw);
			const double sy = dh / eff_h;
			const double s  = sx < sy ? sx : sy;
			cw              = logw * s;
			ch              = eff_h * s;
		}
	}
	dst.x = static_cast<float>((dw - cw) / 2.0);
	dst.y = static_cast<float>((dh - ch) / 2.0);
	dst.w = static_cast<float>(cw);
	dst.h = static_cast<float>(ch);
}

void Image_window::compute_layer_fill_dest(int logw, int logh, SDL_FRect& dst) const {
	compute_layer_fill_dest(logw, logh, dst, UiLayerDefault);
}

void Image_window::compute_layer_fill_dest(int logw, int logh, SDL_FRect& dst, UiLayerKind kind) const {
	const UiLayerConfig& cfg = get_ui_cfg(kind);
	compute_fill_dest(logw, logh, eff_ui_fill_mode(cfg), eff_ui_scale(cfg), dst);
}

float Image_window::get_ui_scale_factor() const {
	return get_ui_scale_factor(UiLayerDefault);
}

float Image_window::get_ui_scale_factor(UiLayerKind kind) const {
	const UiLayerConfig& cfg = get_ui_cfg(kind);
	const int            uiw = get_ui_width(kind);
	const int            uih = get_ui_height(kind);
	SDL_FRect            r;
	compute_fill_dest(uiw, uih, eff_ui_fill_mode(cfg), eff_ui_scale(cfg), r);
	const float fw = uiw > 0 ? r.w / static_cast<float>(uiw) : 1.0f;
	const float fh = uih > 0 ? r.h / static_cast<float>(uih) : 1.0f;
	const float f  = fw < fh ? fw : fh;
	return f > 0.0f ? f : 1.0f;
}

float Image_window::get_ui_hud_scale(UiLayerKind kind) const {
	ignore_unused_variable_warning(kind);
	return get_ui_scale_factor(kind);
}

float Image_window::ui_full_pixel_scale(UiLayerKind kind) const {
	const UiLayerConfig& cfg = get_ui_cfg(kind);
	SDL_FRect            r;
	compute_fill_dest(320, 200, eff_ui_fill_mode(cfg), eff_ui_scale(cfg), r);
	const float fw = r.w / 320.0f;
	const float fh = r.h / 200.0f;
	const float s  = fw < fh ? fw : fh;
	return s > 0.0f ? s : 1.0f;
}

void Image_window::compute_ui_layer_dest(int logw, int logh, SDL_FRect& dst) const {
	compute_ui_layer_dest(logw, logh, dst, UiLayerDefault);
}

void Image_window::compute_ui_layer_dest(int logw, int logh, SDL_FRect& dst, UiLayerKind kind) const {
	const UiLayerConfig& cfg = get_ui_cfg(kind);
	const int            uiw = get_ui_width(kind);
	const int            uih = get_ui_height(kind);
	// First compute the configured UI frame (fixed width/height or Auto), then
	// map this layer's logical size into that frame.
	SDL_FRect frame;
	compute_fill_dest(uiw, uih, eff_ui_fill_mode(cfg), eff_ui_scale(cfg), frame);
	if (uiw <= 0 || uih <= 0 || logw <= 0 || logh <= 0) {
		dst = frame;
		return;
	}
	const float sx = frame.w / static_cast<float>(uiw);
	const float sy = frame.h / static_cast<float>(uih);
	dst.w          = static_cast<float>(logw) * sx;
	dst.h          = static_cast<float>(logh) * sy;
	dst.x          = frame.x + (frame.w - dst.w) / 2.0f;
	dst.y          = frame.y + (frame.h - dst.h) / 2.0f;
}

bool Image_window::screen_to_layer(int handle, int sx, int sy, int& lx, int& ly) {
	if (handle < 0 || handle >= static_cast<int>(layers.size()) || !layers[handle]) {
		return false;
	}
	const Layer& layer = *layers[handle];
	SDL_FRect    dst;
	get_layer_dest(layer, dst);
	if (dst.w <= 0 || dst.h <= 0) {
		return false;
	}
	lx = static_cast<int>((static_cast<float>(sx) - dst.x) * layer.logw / dst.w);
	ly = static_cast<int>((static_cast<float>(sy) - dst.y) * layer.logh / dst.h);
	return static_cast<float>(sx) >= dst.x && static_cast<float>(sy) >= dst.y && lx >= 0 && ly >= 0 && lx < layer.logw
		   && ly < layer.logh;
}

bool Image_window::layer_to_screen(int handle, int lx, int ly, int& sx, int& sy) {
	if (handle < 0 || handle >= static_cast<int>(layers.size()) || !layers[handle]) {
		return false;
	}
	const Layer& layer = *layers[handle];
	SDL_FRect    dst;
	get_layer_dest(layer, dst);
	if (dst.w <= 0 || dst.h <= 0 || layer.logw <= 0 || layer.logh <= 0) {
		return false;
	}
	sx = static_cast<int>(dst.x + static_cast<float>(lx) * dst.w / layer.logw);
	sy = static_cast<int>(dst.y + static_cast<float>(ly) * dst.h / layer.logh);
	return true;
}

void Image_window::free_layer_textures() {
	for (auto& lp : layers) {
		if (lp && lp->texture) {
			SDL_DestroyTexture(lp->texture);
			lp->texture = nullptr;
			lp->dirty   = true;
		}
	}
}

void Image_window::composite_layers() {
	if (layers.empty() || screen_renderer == nullptr) {
		return;
	}
	auto perftimer = PerformanceTimer::GetScopedPerfTimer(__func__);
	// Gather the visible layers and composite them from lowest to highest z.
	std::vector<Layer*> ordered;
	ordered.reserve(layers.size());
	for (auto& lp : layers) {
		if (!lp || !lp->visible || !lp->buf) {
			continue;
		}
		const UiLayerKind kind = lp->ui_kind;
		if (kind < UiLayerDefault || kind >= NumUiLayerKinds) {
			continue;
		}
		const uint32 bit = 1u << static_cast<int>(kind);
		if ((ui_layer_kind_mask & bit) == 0) {
			continue;
		}
		ordered.push_back(lp.get());
	}
	if (ordered.empty()) {
		return;
	}
	std::stable_sort(ordered.begin(), ordered.end(), [](const Layer* a, const Layer* b) {
		return a->z < b->z;
	});
	for (Layer* lptr : ordered) {
		Layer& layer     = *lptr;
		auto   perftimer = PerformanceTimer::GetScopedPerfTimer("Composite_Layer:", layer.get_name(), true);

		const UiLayerConfig& cfg = get_ui_cfg(layer.ui_kind);
		// Match filtering to this layer's scaler/fill scaler.
		const bool smooth = (eff_ui_scaler(cfg) == bilinear) || (eff_ui_scaler(cfg) == SDLScaler)
							|| (eff_ui_fill_scaler(cfg) == bilinear) || (eff_ui_fill_scaler(cfg) == SDLScaler);
		const SDL_ScaleMode smode = smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST;
		// If a software (member) scaler is active, layers are pre-scaled by it
		// to this factor; otherwise they are uploaded 1:1 and scaled on the GPU.
		const int render_scale = layer_render_scale(layer);
		// Recreate the texture if the render scale changed (its size depends
		// on it) or it was dropped.
		if (layer.render_scale != render_scale && layer.texture != nullptr) {
			SDL_DestroyTexture(layer.texture);
			layer.texture = nullptr;
		}
		layer.render_scale = render_scale;
		if (layer.texture == nullptr) {    // Lazily (re)create the GPU texture.
			layer.texture = SDL_CreateTexture(
					screen_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, layer.logw * render_scale,
					layer.logh * render_scale);
			if (layer.texture == nullptr) {
				continue;
			}
			SDL_SetTextureBlendMode(layer.texture, SDL_BLENDMODE_BLEND);
			layer.dirty = true;
		}
		SDL_SetTextureScaleMode(layer.texture, smode);
		if (layer.dirty) {
			refresh_layer(layer);
			layer.dirty = false;
		}
		SDL_SetTextureAlphaMod(layer.texture, layer.alpha);
		SDL_FRect dst;
		get_layer_dest(layer, dst);
		SDL_RenderTexture(screen_renderer, layer.texture, nullptr, &dst);
	}
}

void Image_window::UpdateRect(SDL_FRect* dirtyRect, SDL_FRect* fullRect) {
	auto perfcounter = PerformanceTimer::GetScopedPerfTimer(__func__);

	{
		auto perfcounter_rt = PerformanceTimer::GetScopedPerfTimer(__func__, " Rendering Textures");
		if (!SDL_SetRenderTarget(screen_renderer, screen_texture_a)) {
			const char* err = SDL_GetError();
			std::cerr << "SDL_SetRenderTarget(screen_renderer, screen_texture_a) failed: " << (err ? err : "") << std::endl;
			SDL_ClearError();
		}

		if (!SDL_RenderTexture(screen_renderer, screen_texture, dirtyRect, dirtyRect)) {
			const char* err = SDL_GetError();
			std::cerr << "SDL_RenderTexture(screen_renderer, screen_texture, srcRect, srcRect) failed: " << (err ? err : "")
					  << std::endl;
			SDL_ClearError();
		}

		if (!SDL_SetRenderTarget(screen_renderer, nullptr)) {
			const char* err = SDL_GetError();
			std::cerr << "SDL_SetRenderTarget(screen_renderer, screen_texture_a) failed: " << (err ? err : "") << std::endl;
			SDL_ClearError();
		}
		SDL_RenderClear(screen_renderer);
		if (!SDL_RenderTexture(screen_renderer, screen_texture_a, fullRect, nullptr)) {
			const char* err = SDL_GetError();
			std::cerr << "SDL_RenderTexture(screen_renderer, screen_texture_a, srcRect, nullptr) failed: " << (err ? err : "")
					  << std::endl;
			SDL_ClearError();
		}
	}

	// Draw overlay layers on top of the main image, before presenting.
	composite_layers();
	{
		auto perfcounter_srp = PerformanceTimer::GetScopedPerfTimer(__func__, " SDL_RenderPresent");
		if (!SDL_RenderPresent(screen_renderer)) {
			const char* err = SDL_GetError();
			std::cerr << "SDL_RenderPresent failed: " << (err ? err : "") << std::endl;
			SDL_ClearError();
		}
	}
}

int Image_window::VideoModeOK(int width, int height, bool fullscreen, int bpp) {
	if (height > width) {
		// Reject portrait modes.
		return 0;
	}
	if (bpp != 0 && bpp != 8 && bpp != 16 && bpp != 32) {
		// Reject forced bpp out of 8, 16, 32.
		return 0;
	}
	if (!fullscreen) {
		const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
		int                    nbpp;
		Uint32                 Rmask;
		Uint32                 Gmask;
		Uint32                 Bmask;
		Uint32                 Amask;
		if (SDL_GetMasksForPixelFormat(mode->format, &nbpp, &Rmask, &Gmask, &Bmask, &Amask) && mode->w >= width && mode->h >= height
			&& ((bpp == nbpp) || (bpp == 0 && (nbpp == 16 || nbpp == 32)))) {
			return nbpp;
		}
		return 0;
	}

	SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(SDL_GetPrimaryDisplay(), nullptr);
	for (int j = 0; modes[j]; j++) {
		int    nbpp;
		Uint32 Rmask;
		Uint32 Gmask;
		Uint32 Bmask;
		Uint32 Amask;
		if (SDL_GetMasksForPixelFormat(modes[j]->format, &nbpp, &Rmask, &Gmask, &Bmask, &Amask) && modes[j]->w == width
			&& modes[j]->h == height && ((bpp == nbpp) || (bpp == 0 && (nbpp == 16 || nbpp == 32)))) {
			SDL_free(modes);
			return nbpp;
		}
	}
	SDL_free(modes);
	return 0;
}

SDL_DisplayMode Image_window::desktop_displaymode;
