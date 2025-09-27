/**
**  Imagewin.h - A window to blit images into.
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

#ifndef INCL_IMAGEWIN
#define INCL_IMAGEWIN 1

#include "common_types.h"
#include "ignore_unused_variable_warning.h"
#include "imagebuf.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

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

struct SDL_Surface;
struct SDL_IOStream;

/*
 *   Here's the top-level class to use for image buffers.  Image_window
 *   should be derived from it.
 */

namespace Pentagram {
	class ArbScaler;
}

class Image_window {
public:
	// Firstly just some public scaler stuff
	using scalefun   = void (Image_window::*)(int, int, int, int);
	using ScalerType = int;

	enum FillMode {
		Fill = 1,    ///< Game area fills all of the display surface
		Fit = 2,    ///< Game area is stretched to the closest edge, maintaining
					///< 1:1 pixel aspect
		AspectCorrectFit = 3,    ///< Game area is stretched to the closest
								 ///< edge, with 1:1.2 pixel aspect
		FitAspectCorrect    = 3,
		Centre              = 4,    ///< Game area is centred
		AspectCorrectCentre = 5,    ///< Game area is centred and scaled to have
									///< 1:1.2 pixel aspect
		CentreAspectCorrect = 5,

		// Numbers higher than this incrementally scale by .5 more
		Centre_x1_5              = 6,
		AspectCorrectCentre_x1_5 = 7,
		Centre_x2                = 8,
		AspectCorrectCentre_x2   = 9,
		Centre_x2_5              = 10,
		AspectCorrectCentre_x2_5 = 11,
		Centre_x3                = 12,
		AspectCorrectCentre_x3   = 13,
		// And so on....

		// Arbitrarty scaling support => (x<<16)|y
		Centre_640x480 = (640 << 16)
						 | 480    ///< Scale to specific dimentions and centre
	};

	struct ScalerInfo {
		const char*            name;
		int                    displayname_msg_index;
		uint32                 size_mask;
		Pentagram::ArbScaler*  arb;
		Image_window::scalefun fun8to565;
		Image_window::scalefun fun8to555;
		Image_window::scalefun fun8to16;
		Image_window::scalefun fun8to32;
		Image_window::scalefun fun8to8;
	};

	struct Resolution {
		sint32 width;
		sint32 height;
	};

	struct ScalerVector : public std::vector<Image_window::ScalerInfo> {
		ScalerVector();
		~ScalerVector();
	};

private:
	static ScalerVector                               p_scalers;
	static std::map<uint32, Image_window::Resolution> p_resolutions;
	static bool                                       any_res_allowed;

public:
	static const ScalerVector&                               Scalers;
	static const std::map<uint32, Image_window::Resolution>& Resolutions;
	static const bool&                                       AnyResAllowed;

	static ScalerType get_scaler_for_name(const char* scaler);

	static inline const char* get_name_for_scaler(int num) {
		return Scalers[num].name;
	}

	static const char* get_displayname_for_scaler(int num);

	struct ScalerConst {
		const char* const Name;

		ScalerConst(const char* name) : Name(name) {}

		operator ScalerType() const {
			if (Name == nullptr) {
				return Scalers.size();
			}
			return get_scaler_for_name(Name);
		}
	};

	static const ScalerType  NoScaler;
	static const ScalerConst point;
	static const ScalerConst interlaced;
	static const ScalerConst bilinear;
	static const ScalerConst BilinearPlus;
	static const ScalerConst SaI;
	static const ScalerConst SuperEagle;
	static const ScalerConst Super2xSaI;
	static const ScalerConst Scale2x;
	static const ScalerConst Hq2x;
	static const ScalerConst Hq3x;
	static const ScalerConst Hq4x;
	static const ScalerConst _2xBR;
	static const ScalerConst _3xBR;
	static const ScalerConst _4xBR;
	static const ScalerConst SDLScaler;
	static const ScalerConst NumScalers;

	// Gets the draw surface and intersurface dims.
	// if (inter_surface.wh != (dw*scale,dh*scale))
	//   draw_surface is centred after scaling
	// If (inter_surface.wh == display_surface.wh || strech_scaler == scaler ||
	// scale == 1)
	//   inter_surface wont be used
	static bool get_draw_dims(
			int sw, int sh, int scale, FillMode fillmode, int& gw, int& gh,
			int& iw, int& ih);

	static FillMode string_to_fillmode(const char* str);
	static bool     fillmode_to_string(FillMode fmode, std::string& str);

protected:
	Image_buffer* ibuf;            // Where the data is actually stored.
	int           scale;           // Only 1 or 2 for now.
	int           scaler;          // What scaler do we want to use
	bool          uses_palette;    // Does this window have a palette
	bool          fullscreen;      // Rendering fullscreen.
	int           game_width;
	int           game_height;
	int saved_game_width;     // Normally this is the same as game_width and is
							  // used by the PaintIntoGuardBand code so it can
							  // change and restore the value of game_width
	int saved_game_height;    // Normally this is the same as game_height and is
							  // used by the PaintIntoGuardBand code so it can
							  // change and restore the value of game_height
	int inter_width;
	int inter_height;

	// Guardband around the edge of the draw surface to allow scalers to run
	// without per pixel bounds checking and to allow rounding up to
	// multiples of 4. It should  not be less than 4 and there is no reason for
	// it to be bigger.
	const int guard_band = 4;

	FillMode fill_mode;
	int      fill_scaler;

	static SDL_DisplayMode desktop_displaymode;
	struct SDL_Window*     screen_window;
	struct SDL_Renderer*   screen_renderer;
	struct SDL_Texture*    screen_texture;
	void                   UpdateRect(SDL_Surface* surf);

	SDL_Surface* paletted_surface;    // Surface that palette is set on (Example
									  // res)
	SDL_Surface*
			display_surface;    // Final surface that is displayed  (1024x1024)
	SDL_Surface* inter_surface;    // Post scaled/pre stretch surface  (960x600)
	SDL_Surface* draw_surface;     // Pre scaled surface               (320x200)

	/*
	 *   Scaled blits:
	 */
	void show_scaled8to16_2xSaI(int x, int y, int w, int h);
	void show_scaled8to555_2xSaI(int x, int y, int w, int h);
	void show_scaled8to565_2xSaI(int x, int y, int w, int h);
	void show_scaled8to32_2xSaI(int x, int y, int w, int h);

	void show_scaled8to16_Super2xSaI(int x, int y, int w, int h);
	void show_scaled8to555_Super2xSaI(int x, int y, int w, int h);
	void show_scaled8to565_Super2xSaI(int x, int y, int w, int h);
	void show_scaled8to32_Super2xSaI(int x, int y, int w, int h);

	void show_scaled8to16_bilinear(int x, int y, int w, int h);
	void show_scaled8to555_bilinear(int x, int y, int w, int h);
	void show_scaled8to565_bilinear(int x, int y, int w, int h);
	void show_scaled8to32_bilinear(int x, int y, int w, int h);
	void show_scaled16to16_bilinear(int x, int y, int w, int h);
	void show_scaled32to32_bilinear(int x, int y, int w, int h);
	void show_scaled555to555_bilinear(int x, int y, int w, int h);
	void show_scaled565to565_bilinear(int x, int y, int w, int h);

	void show_scaled8to16_SuperEagle(int x, int y, int w, int h);
	void show_scaled8to555_SuperEagle(int x, int y, int w, int h);
	void show_scaled8to565_SuperEagle(int x, int y, int w, int h);
	void show_scaled8to32_SuperEagle(int x, int y, int w, int h);

	void show_scaled8to16_point(int x, int y, int w, int h);
	void show_scaled8to555_point(int x, int y, int w, int h);
	void show_scaled8to565_point(int x, int y, int w, int h);
	void show_scaled8to32_point(int x, int y, int w, int h);
	void show_scaled8to8_point(int x, int y, int w, int h);
	void show_scaled16to16_point(int x, int y, int w, int h);
	void show_scaled555to555_point(int x, int y, int w, int h);
	void show_scaled565to565_point(int x, int y, int w, int h);
	void show_scaled32to32_point(int x, int y, int w, int h);

	void show_scaled8to16_interlace(int x, int y, int w, int h);
	void show_scaled8to555_interlace(int x, int y, int w, int h);
	void show_scaled8to565_interlace(int x, int y, int w, int h);
	void show_scaled8to32_interlace(int x, int y, int w, int h);
	void show_scaled8to8_interlace(int x, int y, int w, int h);

	void show_scaled8to16_2x_noblur(int x, int y, int w, int h);
	void show_scaled8to555_2x_noblur(int x, int y, int w, int h);
	void show_scaled8to565_2x_noblur(int x, int y, int w, int h);
	void show_scaled8to32_2x_noblur(int x, int y, int w, int h);
	void show_scaled8to8_2x_noblur(int x, int y, int w, int h);

	void show_scaled8to16_BilinearPlus(int x, int y, int w, int h);
	void show_scaled8to555_BilinearPlus(int x, int y, int w, int h);
	void show_scaled8to565_BilinearPlus(int x, int y, int w, int h);
	void show_scaled8to32_BilinearPlus(int x, int y, int w, int h);

	void show_scaled8to16_Hq2x(int x, int y, int w, int h);
	void show_scaled8to555_Hq2x(int x, int y, int w, int h);
	void show_scaled8to565_Hq2x(int x, int y, int w, int h);
	void show_scaled8to32_Hq2x(int x, int y, int w, int h);

	void show_scaled8to16_Hq3x(int x, int y, int w, int h);
	void show_scaled8to555_Hq3x(int x, int y, int w, int h);
	void show_scaled8to565_Hq3x(int x, int y, int w, int h);
	void show_scaled8to32_Hq3x(int x, int y, int w, int h);

	void show_scaled8to16_Hq4x(int x, int y, int w, int h);
	void show_scaled8to555_Hq4x(int x, int y, int w, int h);
	void show_scaled8to565_Hq4x(int x, int y, int w, int h);
	void show_scaled8to32_Hq4x(int x, int y, int w, int h);

	void show_scaled8to16_2xBR(int x, int y, int w, int h);
	void show_scaled8to555_2xBR(int x, int y, int w, int h);
	void show_scaled8to565_2xBR(int x, int y, int w, int h);
	void show_scaled8to32_2xBR(int x, int y, int w, int h);

	void show_scaled8to16_3xBR(int x, int y, int w, int h);
	void show_scaled8to555_3xBR(int x, int y, int w, int h);
	void show_scaled8to565_3xBR(int x, int y, int w, int h);
	void show_scaled8to32_3xBR(int x, int y, int w, int h);

	void show_scaled8to16_4xBR(int x, int y, int w, int h);
	void show_scaled8to555_4xBR(int x, int y, int w, int h);
	void show_scaled8to565_4xBR(int x, int y, int w, int h);
	void show_scaled8to32_4xBR(int x, int y, int w, int h);

	/*
	 *   Image info.
	 */
	// Create new SDL surface.
	void create_surface(unsigned int w, unsigned int h);
	void free_surface();    // Free it.
	bool create_scale_surfaces(int w, int h, int bpp);
	bool try_scaler(int w, int h);

	static void static_init();

	static int   force_bpp;
	static int   desktop_depth;
	static int   windowed;
	static float nativescale;

public:
	inline struct SDL_Window* get_screen_window() const {
		return screen_window;
	}

	// Looks for the best resolution from the width x height and fullscreen :
	// - A portrait requirement height > width can never be found,
	// - In Windowed, only the Desktop resolution is suitable,
	//      and only if it is larger or equal than width x height,
	// - In Fullscreen, a Fullscreen mode of the display is suitable
	//      if it is equal to width x height.
	// If the required bits per pixel bpp is left to default ( 0 ),
	//      the largest bits per pixel in 8, 16, 32 is returned
	//      from the suitable resolutions, otherwise 0 is returned.
	// If the required bits per pixel bpp is set ( to one of 8, 16, 32 ),
	//      the exact same bits per pixel is returned if a suitable resolution
	//      admits this bpp, otherwise 0 is returned.
	static int VideoModeOK(int width, int height, bool fullscreen, int bpp = 0);
	int        Get_best_bpp(int w, int h, int bpp);

	// Create with given buffer.
	Image_window(
			Image_buffer* ib, int w, int h, int gamew, int gameh, int scl = 1,
			bool fs = false, int sclr = point,
			FillMode fmode = AspectCorrectCentre, int fillsclr = point)
			: ibuf(ib), scale(scl), scaler(sclr), uses_palette(true),
			  fullscreen(fs), game_width(gamew), game_height(gameh),
			  saved_game_width(gamew), saved_game_height(gameh),
			  fill_mode(fmode), fill_scaler(fillsclr), screen_window(nullptr),
			  screen_renderer(nullptr), screen_texture(nullptr),
			  paletted_surface(nullptr), display_surface(nullptr),
			  inter_surface(nullptr), draw_surface(nullptr) {
		static_init();
		create_surface(w, h);
	}

	virtual ~Image_window();

	// int get_scale()           // Returns 1 or 2.
	//{ return scale; }
	int get_scale_factor() {
		return scale;
	}

	int get_display_width();
	int get_display_height();

	void screen_to_game(int sx, int sy, bool fast, int& gx, int& gy);

	void game_to_screen(int gx, int gy, bool fast, int& sx, int& sy);

	int get_scaler() {    // Returns 1 or 2.
		return scaler;
	}

	bool is_palettized() {    // Does the window have a palette?
		return uses_palette;
	}

	bool fast_palette_rotate() {
		return uses_palette || scale == 1;
	}

	// Is rect. visible within clip?
	bool is_visible(int x, int y, int w, int h) {
		return ibuf->is_visible(x, y, w, h);
	}

	// Set title.
	void set_title(const char* title);

	Image_buffer* get_ibuf() {
		return ibuf;
	}

	int get_start_x() {
		return -ibuf->offset_x;
	}

	int get_start_y() {
		return -ibuf->offset_y;
	}

	int get_full_width() {
		return ibuf->width;
	}

	int get_full_height() {
		return ibuf->height;
	}

	int get_game_width() {
		return game_width;
	}

	int get_game_height() {
		return game_height;
	}

	int get_end_x() {
		return get_full_width() + get_start_x();
	}

	int get_end_y() {
		return get_full_height() + get_start_y();
	}

	FillMode get_fill_mode() {
		return fill_mode;
	}

	int get_fill_scaler() {
		return fill_scaler;
	}

	SDL_Surface* get_draw_surface() {
		return draw_surface;
	}

	bool ready() {    // Ready to draw?
		return ibuf->bits != nullptr;
	}

	bool is_fullscreen() {
		return fullscreen;
	}

	// Create a compatible image buffer.
	std::unique_ptr<Image_buffer> create_buffer(int w, int h);
	// Resize event occurred.
	void resized(
			unsigned int neww, unsigned int newh, bool newfs,
			unsigned int newgw, unsigned int newgh, int newsc,
			int newscaler = point, FillMode fmode = AspectCorrectCentre,
			int fillsclr = point);

	void show() {    // Repaint entire window.
		show(get_start_x(), get_start_y(), get_full_width(), get_full_height());
	}

	// Repaint rectangle.
	void show(int x, int y, int w, int h);

	void toggle_fullscreen();

	// Set palette.
	virtual void set_palette(
			const unsigned char* rgbs, int maxval, int brightness = 100) {
		ignore_unused_variable_warning(rgbs, maxval, brightness);
	}

	// Rotate palette colors.
	virtual void rotate_colors(int first, int num, int upd) {
		ignore_unused_variable_warning(first, num, upd);
	}

	// This method adjusts buffer dimensions so gamewin can draw into the
	// guard band on the right and bottom in order to prevent fine black lines
	// when scalers read from the guardband Must call EndPaintIntoGuardBand
	// after calling this Parmeters are the region to be painted. This will be
	// clipped against buffer dimension and enlarged into the guardband. If
	// Parameters are nullptr they are treated as if they are zero when
	// clipping the other coordinates.
	// This method does nothing if ShouldPaintIntoGuardband() returns false
	void BeginPaintIntoGuardBand(int* x, int* y, int* w, int* h);

	// Reset iwin and buffers back to normal after calling
	// BeginPaintIntoGuardBand
	void EndPaintIntoGuardBand();

	// whether or not we should do guardband painting
	// Criteria is using a scaler other than point and there is a guardband
	bool ShouldPaintIntoGuardband() {
		// Only if actually scaling
		if (draw_surface == display_surface) {
			return false;
		}

		// If rendering to inter_surface, the scaling is only being done uing
		// the fill_scaler and if is point we don't need to do this
		if (draw_surface == inter_surface && point == fill_scaler) {
			return false;
		}

		// if inter is the same as display the scaling is only being done by
		// scaler and if is point we don't need to do this
		if (display_surface == inter_surface && point == scaler) {
			return false;
		}

		// Is there even a guardband
		return guard_band > 0;
	}

	// Fill the right and bottom guardband by duplicating edge pixels across it
	// Used during cinematic sequences
	void FillGuardband();

	// Show but first fill guardband
	void ShowFillGuardBand() {
		FillGuardband();
		show();
	}

	/*
	 *   8-bit color methods:
	 */
	// Fill with given (8-bit) value.
	void fill8(unsigned char val) {
		ibuf->fill8(val);
	}

	// Fill rect. wth pixel.
	void fill8(unsigned char val, int srcw, int srch, int destx, int desty) {
		ibuf->fill8(val, srcw, srch, destx, desty);
	}

	// Fill line with pixel.
	void fill_hline8(unsigned char val, int srcw, int destx, int desty) {
		ibuf->fill_hline8(val, srcw, destx, desty);
	}

	// Copy rectangle into here.
	void copy8(
			const unsigned char* src_pixels, int srcw, int srch, int destx,
			int desty) {
		ibuf->copy8(src_pixels, srcw, srch, destx, desty);
	}

	// Copy line to here.
	void copy_hline8(
			const unsigned char* src_pixels, int srcw, int destx, int desty) {
		ibuf->copy_hline8(src_pixels, srcw, destx, desty);
	}

	// Copy with translucency table.
	void copy_hline_translucent8(
			const unsigned char* src_pixels, int srcw, int destx, int desty,
			int first_translucent, int last_translucent,
			const Xform_palette* xforms) {
		ibuf->copy_hline_translucent8(
				src_pixels, srcw, destx, desty, first_translucent,
				last_translucent, xforms);
	}

	// Apply translucency to a line.
	void fill_hline_translucent8(
			unsigned char val, int srcw, int destx, int desty,
			const Xform_palette& xform) {
		ibuf->fill_hline_translucent8(val, srcw, destx, desty, xform);
	}

	// Apply translucency to a rectangle
	virtual void fill_translucent8(
			unsigned char val, int srcw, int srch, int destx, int desty,
			const Xform_palette& xform) {
		ibuf->fill_translucent8(val, srcw, srch, destx, desty, xform);
	}

	// Copy rect. with transp. color.
	void copy_transparent8(
			const unsigned char* src_pixels, int srcw, int srch, int destx,
			int desty) {
		ibuf->copy_transparent8(src_pixels, srcw, srch, destx, desty);
	}

	// Put a single pixel.
	void put_pixel8(unsigned char pix, int x, int y) {
		ibuf->put_pixel8(pix, x, y);
	}

	/*
	 *   Depth-independent methods:
	 */
	void clear_clip() {    // Reset clip to whole window.
		ibuf->clear_clip();
	}

	// Set clip.
	void set_clip(int x, int y, int w, int h) {
		ibuf->set_clip(x, y, w, h);
	}

	// Copy within itself.
	void copy(int srcx, int srcy, int srcw, int srch, int destx, int desty) {
		ibuf->copy(srcx, srcy, srcw, srch, destx, desty);
	}

	// Get rect. into another buf.
	void get(Image_buffer* dest, int srcx, int srcy) {
		ibuf->get(dest, srcx, srcy);
	}

	// Put rect. back.
	void put(Image_buffer* src, int destx, int desty) {
		ibuf->put(src, destx, desty);
	}

	bool screenshot(SDL_IOStream* dst);
};
#endif /* INCL_IMAGEWIN    */
