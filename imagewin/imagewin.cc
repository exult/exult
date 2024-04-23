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
#include "istring.h"
#include "manip.h"
#include "mouse.h"

#include <cstdlib>
#include <iostream>
#include <string>

// Simulate HighDPI mode without OS or Display Support for it
// uncomment the define and set to a value greater than 1.0 to multiply the
// fullscreen render surface resolution This should only be used for testing and
// development and will likely degrade performance and quality 
//#define SIMULATE_HIDPI 2.0f

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

bool SaveIMG_RW(
		SDL_Surface* saveme, SDL_IOStream* dst, bool freedst, int guardband);

using std::cerr;
using std::cout;
using std::endl;
using std::exit;

#define SCALE_BIT(factor) (1 << ((factor)-1))

const Image_window::ScalerType  Image_window::NoScaler(-1);
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
const Image_window::ScalerConst Image_window::NumScalers(nullptr);

Image_window::ScalerVector        Image_window::p_scalers;
const Image_window::ScalerVector& Image_window::Scalers
		= Image_window::p_scalers;

std::map<uint32, Image_window::Resolution>        Image_window::p_resolutions;
const std::map<uint32, Image_window::Resolution>& Image_window::Resolutions
		= Image_window::p_resolutions;
bool        Image_window::any_res_allowed;
const bool& Image_window::AnyResAllowed = Image_window::any_res_allowed;

int Image_window::force_bpp     = 0;
int Image_window::desktop_depth = 0;
int Image_window::windowed      = 0;

const int Image_window::guard_band = 4;

SDL_PixelFormat*
		  ManipBase::fmt;    // Format of dest. pixels (and src for rgb src).
SDL_Color ManipBase::colors[256];    // Palette for source window.

// Constructor for the ScalerVector, setup the list
Image_window::ScalerVector::ScalerVector() {
	reserve(13);

	// This is all the names of the scalers. It needs to match the ScalerType
	// enum
	const ScalerInfo point = {"Point", 0xFFFFFFFF, new Pentagram::PointScaler(),
							  nullptr, nullptr,    nullptr,
							  nullptr, nullptr};
	push_back(point);

	const ScalerInfo Interlaced
			= {"Interlaced",
			   0xFFFFFFFE,
			   nullptr,
			   &Image_window::show_scaled8to565_interlace,
			   &Image_window::show_scaled8to555_interlace,
			   &Image_window::show_scaled8to16_interlace,
			   &Image_window::show_scaled8to32_interlace,
			   &Image_window::show_scaled8to8_interlace};
	push_back(Interlaced);

	const ScalerInfo Bilinear
			= {"Bilinear", 0xFFFFFFFF, new Pentagram::BilinearScaler(),
			   nullptr,    nullptr,    nullptr,
			   nullptr,    nullptr};
	push_back(Bilinear);

	const ScalerInfo BilinearPlus
			= {"BilinearPlus",
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
			   SCALE_BIT(4),
			   nullptr,
			   &Image_window::show_scaled8to565_4xBR,
			   &Image_window::show_scaled8to555_4xBR,
			   &Image_window::show_scaled8to16_4xBR,
			   &Image_window::show_scaled8to32_4xBR,
			   nullptr};
	push_back(_4xbr);
#endif
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
		if (!(fullscreen)) {
			if (windowed != 0) {
				return 16;
			}
		}

		if (best_bpp != 0) {
			return best_bpp;
		}

		cerr << "SDL Reports " << w << "x" << h << " " << bpp << " bpp "
			 << ((fullscreen) ? "fullscreen" : "windowed")
			 << " surface is not OK. Attempting to use " << bpp
			 << " bpp anyway." << endl;
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
			return 16;
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
		return 16;
	}

	cerr << "SDL Reports " << w << "x" << h << " "
		 << ((fullscreen) ? "fullscreen" : "windowed")
		 << " surfaces are not OK. Attempting to use 16 bpp. anyway" << endl;
	return 16;
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

	const SDL_DisplayMode* dispmode
			= SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
	int    bpp;
	Uint32 Rmask;
	Uint32 Gmask;
	Uint32 Bmask;
	Uint32 Amask;
	if (SDL_GetMasksForPixelFormat(
				dispmode->format, &bpp, &Rmask, &Gmask, &Bmask, &Amask)
		== SDL_TRUE) {
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
	const SDL_DisplayMode** modes
			= SDL_GetFullscreenDisplayModes(SDL_GetPrimaryDisplay(), nullptr);
	for (int j = 0; modes[j]; j++) {
		const Resolution res = {modes[j]->w, modes[j]->h};
		p_resolutions[(res.width << 16) | res.height] = res;
	}
	SDL_free(static_cast<void*>(modes));

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

	config->value("config/video/force_bpp", force_bpp, 0);

	if (force_bpp != 0 && force_bpp != 16 && force_bpp != 8
		&& force_bpp != 32) {
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

	if (!Scalers[fill_scaler].arb) {
		if (Scalers[scaler].arb) {
			fill_scaler = scaler;
		} else {
			fill_scaler = point;
		}
	}

	get_draw_dims(
			w, h, scale, fill_mode, game_width, game_height, inter_width,
			inter_height);

	if (!try_scaler(w, h)) {
		// Try fallback to point scaler if it failed, if it doesn't work, we
		// probably can't run
		scaler = point;
		try_scaler(w, h);
	}

	if (!paletted_surface && !force_bpp) {    // No scaling, or failed?
		uint32 flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
		if (screen_window != nullptr) {
			SDL_SetWindowSize(screen_window, w / scale, h / scale);
			SDL_SetWindowFullscreen(
					screen_window, (fullscreen ? SDL_TRUE : SDL_FALSE));
		} else {
			screen_window = SDL_CreateWindow("", w / scale, h / scale, flags);
			SDL_SetWindowFullscreen(
					screen_window, (fullscreen ? SDL_TRUE : SDL_FALSE));
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
		SDL_SetRenderVSync(screen_renderer, 1);
		// Do an initial draw/fill
		SDL_SetRenderDrawColor(screen_renderer, 0, 0, 0, 255);
		SDL_RenderClear(screen_renderer);
		SDL_RenderPresent(screen_renderer);

		int    sbpp;
		Uint32 sRmask;
		Uint32 sGmask;
		Uint32 sBmask;
		Uint32 sAmask;
		SDL_GetMasksForPixelFormat(
				desktop_displaymode.format, &sbpp, &sRmask, &sGmask, &sBmask,
				&sAmask);
		display_surface = SDL_CreateSurface(
				(w / scale), (h / scale),
				SDL_GetPixelFormatEnumForMasks(
						sbpp, sRmask, sGmask, sBmask, sAmask));
		if (display_surface == nullptr) {
			cout << "Couldn't create display surface: " << SDL_GetError()
				 << std::endl;
		}
		if (screen_texture == nullptr) {
			screen_texture = SDL_CreateTexture(
					screen_renderer, desktop_displaymode.format,
					SDL_TEXTUREACCESS_STREAMING, (w / scale), (h / scale));
		}
		if (screen_texture == nullptr) {
			cout << "Couldn't create texture: " << SDL_GetError() << std::endl;
		}

		inter_surface = draw_surface = paletted_surface = display_surface;
		inter_width                                     = w / scale;
		inter_height                                    = h / scale;
		scale                                           = 1;
	}
	if (!paletted_surface) {
		cerr << "Couldn't set video mode (" << w << ", " << h << ") at "
			 << ibuf->depth
			 << " bpp depth: " << (force_bpp ? "" : SDL_GetError()) << endl;
		if (w == 640 && h == 480) {
			exit(-1);
		} else {
			cerr << "Attempting fallback to 640x480. Good luck..." << endl;
			scale = 2;
			create_surface(640, 480);
			return;
		}
	}

	ibuf->width  = draw_surface->w;
	ibuf->height = draw_surface->h;

	if (draw_surface != display_surface) {
		ibuf->width -= guard_band * 2;
		ibuf->height -= guard_band * 2;
	}

	// Update line size in words.
	ibuf->line_width = draw_surface->pitch / ibuf->pixel_size;
	// Offset it set to the top left pixel if the game window
	ibuf->offset_x = (get_full_width() - get_game_width()) / 2;
	ibuf->offset_y = (get_full_height() - get_game_height()) / 2;
	ibuf->bits     = static_cast<unsigned char*>(draw_surface->pixels)
				 - get_start_x() - get_start_y() * ibuf->line_width;
	// Scaler guardband is in effect
	if (draw_surface != display_surface) {
		ibuf->bits += guard_band + ibuf->line_width * guard_band;
	}
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
		SDL_SetWindowFullscreen(
				screen_window, (fullscreen ? SDL_TRUE : SDL_FALSE));
	} else {
		screen_window = SDL_CreateWindow("", w, h, flags);
		SDL_SetWindowFullscreen(
				screen_window, (fullscreen ? SDL_TRUE : SDL_FALSE));
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
	SDL_SetRenderVSync(screen_renderer, 1);

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
		// FIXME: Not used in SDL 3
		// int sw;
		// SDL_GetWindowSize(screen_window, &sw, nullptr);
		// nativescale = float(dw) / sw;
		// high resolution fullscreen needs this to make the whole screen
		// available
		SDL_SetRenderLogicalPresentation(
				screen_renderer, w, h, SDL_LOGICAL_PRESENTATION_LETTERBOX,
				SDL_SCALEMODE_LINEAR);
	} else {
		// make sure the window has the right dimensions
		SDL_SetWindowSize(screen_window, w, h);
		// center the window on the screen
		SDL_SetWindowPosition(
				screen_window,
				SDL_WINDOWPOS_CENTERED_DISPLAY(original_displayID),
				SDL_WINDOWPOS_CENTERED_DISPLAY(original_displayID));
	}

	// Do an initial draw/fill
	SDL_SetRenderDrawColor(screen_renderer, 0, 0, 0, 255);
	SDL_RenderClear(screen_renderer);
	SDL_RenderPresent(screen_renderer);

	int    sbpp;
	Uint32 sRmask;
	Uint32 sGmask;
	Uint32 sBmask;
	Uint32 sAmask;
	SDL_GetMasksForPixelFormat(
			desktop_displaymode.format, &sbpp, &sRmask, &sGmask, &sBmask,
			&sAmask);

	display_surface = SDL_CreateSurface(
			w, h,
			SDL_GetPixelFormatEnumForMasks(
					sbpp, sRmask, sGmask, sBmask, sAmask));
	if (display_surface == nullptr) {
		cout << "Couldn't create display surface: " << SDL_GetError()
			 << std::endl;
	}
	if (screen_texture == nullptr) {
		screen_texture = SDL_CreateTexture(
				screen_renderer, desktop_displaymode.format,
				SDL_TEXTUREACCESS_STREAMING, w, h);
	}
	if (screen_texture == nullptr) {
		cout << "Couldn't create texture: " << SDL_GetError() << std::endl;
	}
	if (!display_surface) {
		cerr << "Unable to set video mode to" << w << "x" << h << " " << hwdepth
			 << " bpp" << endl;
		free_surface();
		return false;
	}

	if (!(draw_surface = SDL_CreateSurface(
				  inter_width / scale + 2 * guard_band,
				  inter_height / scale + 2 * guard_band,
				  SDL_GetPixelFormatEnumForMasks(ibuf->depth, 0, 0, 0, 0)))) {
		cerr << "Couldn't create draw surface" << endl;
		free_surface();
		return false;
	}

	// Scale using 'fill_scaler' only
	if (scaler == fill_scaler || scale == 1) {
		inter_surface = draw_surface;
	} else if (inter_width != w || inter_height != h) {
		if (!(inter_surface = SDL_CreateSurface(
					  inter_width + 2 * scale * guard_band,
					  inter_height + 2 * scale * guard_band,
					  SDL_GetPixelFormatEnumForMasks(
							  hwdepth, display_surface->format->Rmask,
							  display_surface->format->Gmask,
							  display_surface->format->Bmask,
							  display_surface->format->Amask)))) {
			cerr << "Couldn't create inter surface: " << SDL_GetError() << endl;
			free_surface();
			return false;
		}
	}
	// Scale using 'scaler' only
	else {
		inter_surface = display_surface;
	}

	if ((uses_palette = (bpp == 8))) {
		paletted_surface = display_surface;
	} else {
		paletted_surface = draw_surface;
	}

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

	bool has8 = ibuf->depth == 8 && info->fun8to8
				&& (force_bpp == 0 || force_bpp == 8);
	bool has16 = ibuf->depth == 8 && info->fun8to16
				 && (force_bpp == 0 || force_bpp == 16);
	bool has32 = ibuf->depth == 8 && info->fun8to32
				 && (force_bpp == 0 || force_bpp == 32);

	if (info->arb) {
		has8 |= (force_bpp == 0 || force_bpp == 8)
				&& info->arb->Support8bpp(ibuf->depth);
		has16 |= (force_bpp == 0 || force_bpp == 16)
				 && info->arb->Support16bpp(ibuf->depth);
		has32 |= (force_bpp == 0 || force_bpp == 32)
				 && info->arb->Support32bpp(ibuf->depth);
	}

	// First try best of 16 bit/32 bit scaler
	if (has16 && has32 && create_scale_surfaces(w, h, 0)) {
		return true;
	}

	if (has16 && create_scale_surfaces(w, h, 16)) {
		return true;
	}

	if (has32 && create_scale_surfaces(w, h, 32)) {
		return true;
	}

	// 8bit display output is mostly deprecated!
	if (has8 && create_scale_surfaces(w, h, 8)) {
		return true;
	}

	return false;
}

/*
 *   Free the surface.
 */

void Image_window::free_surface() {
	if (inter_surface != nullptr && inter_surface != display_surface
		&& inter_surface != draw_surface) {
		SDL_DestroySurface(inter_surface);
	}
	if (display_surface != nullptr && display_surface != draw_surface) {
		SDL_DestroySurface(display_surface);
	}
	if (draw_surface != nullptr) {
		SDL_DestroySurface(draw_surface);
	}
	if (screen_texture != nullptr) {
		SDL_DestroyTexture(screen_texture);
	}
	screen_texture   = nullptr;
	paletted_surface = nullptr;
	inter_surface    = nullptr;
	draw_surface     = nullptr;
	display_surface  = nullptr;
	ibuf->bits       = nullptr;
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
		unsigned int neww, unsigned int newh, bool newfs, unsigned int newgw,
		unsigned int newgh, int newsc, int newscaler, FillMode fmode,
		int fillsclr) {
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
	if (!ready()) {
		return;
	}

	int srcx = 0;
	int srcy = 0;
	if (!ibuf->clip(srcx, srcy, w, h, x, y)) {
		return;
	}
	x -= get_start_x();
	y -= get_start_y();

	// Increase the area by 4 pixels
	increase_area(x, y, w, h, 4, 4, 4, 4, get_full_width(), get_full_height());

	// Make it 4 pixel aligned too
	const int dx = x & 3;
	const int dy = y & 3;
	x -= dx;
	w += dx;
	y -= dy;
	h += dy;

	if (w & 3) {
		w += 4 - (w & 3);
	}
	if (h & 3) {
		h += 4 - (h & 3);
	}

	if (w + x > get_full_width()) {
		w = get_full_width() - x;
	}
	if (h + y > get_full_height()) {
		h = get_full_height() - y;
	}

	w &= ~3;
	h &= ~3;

	// Phase 1 blit from draw_surface to inter_surface
	if (draw_surface != inter_surface) {
		const ScalerInfo& sel_scaler = Scalers[scaler];

		// Need to apply an offset to compensate for the guard_band
		if (inter_surface == display_surface) {
			inter_surface->pixels = static_cast<uint8*>(inter_surface->pixels)
									- inter_surface->pitch * guard_band * scale
									- inter_surface->format->bytes_per_pixel
											  * guard_band * scale;
		}

		if (sel_scaler.arb) {
			if (!sel_scaler.arb->Scale(
						draw_surface, x + guard_band, y + guard_band, w, h,
						inter_surface, scale * (x + guard_band),
						scale * (y + guard_band), scale * w, scale * h,
						false)) {
				Scalers[point].arb->Scale(
						draw_surface, x + guard_band, y + guard_band, w, h,
						inter_surface, scale * (x + guard_band),
						scale * (y + guard_band), scale * w, scale * h, false);
			}
		} else {
			scalefun show_scaled;
			if (inter_surface->format->bits_per_pixel == 16
				|| inter_surface->format->bits_per_pixel == 15) {
				const int r = inter_surface->format->Rmask;
				const int g = inter_surface->format->Gmask;
				const int b = inter_surface->format->Bmask;

				show_scaled = (r == 0xf800 && g == 0x7e0 && b == 0x1f)
											  || (b == 0xf800 && g == 0x7e0
												  && r == 0x1f)
									  ? (sel_scaler.fun8to565 != nullptr
												 ? sel_scaler.fun8to565
												 : sel_scaler.fun8to16)
							  : (r == 0x7c00 && g == 0x3e0 && b == 0x1f)
											  || (b == 0x7c00 && g == 0x3e0
												  && r == 0x1f)
									  ? (sel_scaler.fun8to555 != nullptr
												 ? sel_scaler.fun8to555
												 : sel_scaler.fun8to16)
									  : sel_scaler.fun8to16;
			} else if (inter_surface->format->bits_per_pixel == 32) {
				show_scaled = sel_scaler.fun8to32;
			} else {
				show_scaled = sel_scaler.fun8to8;
			}

			(this->*show_scaled)(x, y, w, h);
		}

		// Undo guard_band offset
		if (inter_surface == display_surface) {
			inter_surface->pixels = static_cast<uint8*>(inter_surface->pixels)
									+ inter_surface->pitch * guard_band * scale
									+ inter_surface->format->bytes_per_pixel
											  * guard_band * scale;
		}

		x *= scale;
		y *= scale;
		w *= scale;
		h *= scale;
	}

	// Phase 2 blit from inter_surface to display_surface
	if (inter_surface != display_surface) {
		const ScalerInfo& sel_scaler = Scalers[fill_scaler];

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

		if (!sel_scaler.arb
			|| !sel_scaler.arb->Scale(
					inter_surface, x, y, w, h, display_surface, 0, 0,
					display_surface->w, display_surface->h, false)) {
			Scalers[point].arb->Scale(
					inter_surface, x, y, w, h, display_surface, 0, 0,
					display_surface->w, display_surface->h, false);
		}

		x = 0;
		y = 0;
		w = display_surface->w;
		h = display_surface->h;
	}
	// Phase 3 blit high res draw surface on top of display_surface
	// Phase 4 notify SDL
	UpdateRect(display_surface);
}

/*
 *   Toggle fullscreen.
 */
void Image_window::toggle_fullscreen() {
	int w;
	int h;

	w = display_surface->w;
	h = display_surface->h;

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

bool Image_window::screenshot(SDL_IOStream* dst) {
	if (!paletted_surface) {
		return false;
	}
	return SaveIMG_RW(draw_surface, dst, true, guard_band);
}

void Image_window::set_title(const char* title) {
	SDL_SetWindowTitle(screen_window, title);
}

int Image_window::get_display_width() {
	return display_surface->w;
}

int Image_window::get_display_height() {
	return display_surface->h;
}

void Image_window::screen_to_game(int sx, int sy, bool fast, int& gx, int& gy) {
	if (fast) {
		gx = sx + get_start_x();
		gy = sy + get_start_y();
		if (Mouse::mouse) {
			Mouse::mouse->apply_fast_offset(gx, gy);
		}
	} else {
		gx = (sx * inter_width) / (scale * get_display_width()) + get_start_x();
		gy = (sy * inter_height) / (scale * get_display_height())
			 + get_start_y();
	}
}

void Image_window::game_to_screen(int gx, int gy, bool fast, int& sx, int& sy) {
	if (fast) {
		if (Mouse::mouse) {
			Mouse::mouse->unapply_fast_offset(gx, gy);
		}
		sx = gx - get_start_x();
		sy = gy - get_start_y();
	} else {
		sx = ((gx - get_start_x()) * scale * get_display_width()) / inter_width;
		sy = ((gy - get_start_y()) * scale * get_display_height())
			 / inter_height;
	}
}

bool Image_window::get_draw_dims(
		int sw, int sh, int scale, FillMode fillmode, int& gw, int& gh, int& iw,
		int& ih) {
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
		const int factor        = 2 + ((fillmode - Centre) / 2);
		const int aspect_factor = (fillmode & 1) ? 5 : 6;

		if (gw == 0 || gh == 0) {
			gw = (sw * 2) / (factor * scale);
			gh = (sh * aspect_factor) / (3 * factor * scale);
		}

		iw = (sw * 2) / factor;
		ih = (sh * aspect_factor) / (3 * factor);

		if (gw * scale > iw) {
			gw = iw / scale;
		}

		if (gh * scale > ih) {
			gh = ih / scale;
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
	} else if (
			!Pentagram::strcasecmp(str, "Centre")
			|| !Pentagram::strcasecmp(str, "Center")) {
		return Centre;
	} else if (
			!Pentagram::strcasecmp(str, "Aspect Correct Centre")
			|| !Pentagram::strcasecmp(str, "Aspect Correct Center")
			|| !Pentagram::strcasecmp(str, "Centre Aspect Correct")
			|| !Pentagram::strcasecmp(str, "Center Aspect Correct")) {
		return AspectCorrectCentre;
	} else if (
			!Pentagram::strncasecmp(str, "Centre ", 7)
			|| !Pentagram::strncasecmp(str, "Center ", 7)) {
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
			!Pentagram::strncasecmp(str, "Aspect Correct Centre ", 22)
			|| !Pentagram::strncasecmp(str, "Aspect Correct Center ", 22)
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
			snprintf(
					factor_str, sizeof(factor_str),
					(factor & 1) ? " x%d.5" : " x%d", factor / 2);
		}

		if (fmode & 1) {
			str = std::string("Aspect Correct Centre")
				  + std::string(factor_str);
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

void Image_window::UpdateRect(SDL_Surface* surf) {
	// TODO: Only update the necessary portion of the screen.
	// Seem to get flicker like crazy or some other ill effect no matter
	// what I try. -Lanica 08/28/2013
	SDL_UpdateTexture(screen_texture, nullptr, surf->pixels, surf->pitch);
	SDL_RenderTexture(screen_renderer, screen_texture, nullptr, nullptr);
	SDL_RenderPresent(screen_renderer);
}

int Image_window::VideoModeOK(int width, int height, bool fullscreen) {
	if (height > width) {
		// Reject portrait modes.
		return 0;
	}
	if (!fullscreen) {
		const SDL_DisplayMode* mode
				= SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
		int    nbpp;
		Uint32 Rmask;
		Uint32 Gmask;
		Uint32 Bmask;
		Uint32 Amask;
		if (SDL_GetMasksForPixelFormat(
					mode->format, &nbpp, &Rmask, &Gmask, &Bmask, &Amask)
					== SDL_TRUE
			&& mode->w >= width && mode->h >= height) {
			return nbpp;
		} else {
			return 0;
		}
	}

	const SDL_DisplayMode** modes
			= SDL_GetFullscreenDisplayModes(SDL_GetPrimaryDisplay(), nullptr);
	for (int j = 0; modes[j]; j++) {
		int    nbpp;
		Uint32 Rmask;
		Uint32 Gmask;
		Uint32 Bmask;
		Uint32 Amask;
		if (SDL_GetMasksForPixelFormat(
					modes[j]->format, &nbpp, &Rmask, &Gmask, &Bmask, &Amask)
					== SDL_TRUE
			&& modes[j]->w == width && modes[j]->h == height) {
			SDL_free(static_cast<void*>(modes));
			return nbpp;
		}
	}
	SDL_free(static_cast<void*>(modes));
	return 0;
}

SDL_DisplayMode Image_window::desktop_displaymode;
