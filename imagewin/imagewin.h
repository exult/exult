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

	enum UiLayerKind {
		UiLayerDefault = 0,
		UiLayerConversations,
		UiLayerMousePointer,
		UiLayerGumps,
		UiLayerHudGumps,
		UiLayerTextGumps,
		UiLayerModalGumps,
		UiLayerDisplayMap,
		UiLayerTextEffects,
		UiLayerFullScreenScene,
		NumUiLayerKinds
	};

	// Fixed-palette mode for a layer (see UiLayerConfig::ui_palette).
	enum UiPaletteMode {
		UiPaletteDisabled = 0,    // Follow the live (tinted) palette.
		UiPaletteDay,             // Force the day palette.
		UiPaletteDusk,            // Force the dusk palette.
		UiPaletteInvisible,       // Force the invisible palette.
		UiPaletteOvercast,        // Force the overcast palette.
		UiPaletteFog,             // Force the fog palette.
		UiPaletteSpell,           // Force the spell (light) palette.
		UiPaletteCandle,          // Force the candle palette.
		UiPaletteRed,             // Force the red (combat) palette.
		UiPaletteLightning,       // Force the lightning palette.
		UiPaletteSingleLight,     // Force the single-light palette.
		UiPaletteManyLights,      // Force the many-lights palette.
	};

	enum FillMode {
		Fill = 1,                ///< Game area fills all of the display surface
		Fit  = 2,                ///< Game area is stretched to the closest edge, maintaining
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
		Centre_640x480 = (640 << 16) | 480    ///< Scale to specific dimentions and centre
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

	struct UiLayerConfig {
		int      width       = 320;    // 0 with height 0 => Auto (game area size).
		int      height      = 200;
		int      scaler      = 0;
		FillMode fill_mode   = Fit;
		int      fill_scaler = 0;
		// Optional fixed palette for the layer (see UiPaletteMode).
		int                        ui_palette = UiPaletteDisabled;
		std::vector<unsigned char> ui_palette_colors;    // 768 gamma RGB, empty = none.
	};

	struct Resolution {
		sint32 width;
		sint32 height;
	};

	struct ScalerVector : public std::vector<Image_window::ScalerInfo> {
		ScalerVector();
		~ScalerVector();
	};

	// A compositing layer. It owns its own 8-bit paletted buffer
	// that game code paints into using the normal shape/text routines. The
	// window converts the buffer to a texture and draws it on top of the main
	// game image at its own size, decoupled from the world scaling (so its
	// contents keep a constant on-screen size regardless of the game
	// resolution or scaler). A single palette index is treated as
	// transparent so layers can overlap the scene.
	class Layer {
		friend class Image_window;
		friend class Image_window8;
		std::unique_ptr<Image_buffer> buf;                  // 8-bit drawing buffer.
		struct SDL_Texture*           texture = nullptr;    // GPU cache (rebuilt on resize).
		int                           logw;                 // Logical (game-pixel) width.
		int                           logh;                 // Logical (game-pixel) height.
		int                           fixed_scale;          // >0 forces integer scale, 0 = auto-fit.
		unsigned char                 transparent;          // Palette index drawn as transparent.
		bool                          opaque = false;       // If set, NO index is transparent (a
															// full-screen opaque scene: every pixel
															// is drawn, even the 'transparent' index).
		bool        visible  = true;
		bool        dirty    = true;     // Buffer changed => re-upload.
		int         z        = 0;        // Composite order (higher = on top).
		bool        has_dest = false;    // Explicit destination override?
		SDL_FRect   dest{};              // Destination rect (display coords).
		UiLayerKind ui_kind      = UiLayerDefault;
		int         render_scale = 1;    // 1 = 1:1 upload; >1 = pre-scaled by
										 // the game's scaler at this factor.
		unsigned char alpha = 255;       // Whole-layer opacity (255 = opaque).
		// Optional 256-entry ARGB override, one per palette index. A non-zero
		// entry is used verbatim (with its own alpha) instead of the opaque
		// palette colour, letting a layer draw translucent pixels.
		std::vector<uint32> index_argb;

	public:
		Layer(std::unique_ptr<Image_buffer> b, int w, int h, unsigned char transp, int fscale, int zorder)
				: buf(std::move(b)), logw(w), logh(h), fixed_scale(fscale), transparent(transp), z(zorder) {}

		Image_buffer* get_ibuf() const {
			return buf.get();
		}

		int get_width() const {
			return logw;
		}

		int get_height() const {
			return logh;
		}

		unsigned char get_transparent() const {
			return transparent;
		}

		bool is_opaque() const {
			return opaque;
		}

		void set_opaque(bool o) {
			opaque = o;
		}

		void set_dirty() {
			dirty = true;
		}

		void set_visible(bool v) {
			visible = v;
		}

		bool is_visible() const {
			return visible;
		}
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
	static bool get_draw_dims(int sw, int sh, int scale, FillMode fillmode, int& gw, int& gh, int& iw, int& ih);

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
	int           saved_game_width;    // Normally this is the same as game_width and is
									   // used by the PaintIntoGuardBand code so it can
									   // change and restore the value of game_width
	int saved_game_height;             // Normally this is the same as game_height and is
									   // used by the PaintIntoGuardBand code so it can
									   // change and restore the value of game_height
	// While a full-screen "scene" layer drives the display through the normal
	// blit path (intro, endgame, cheat screen), scene_mode is on: the guard band
	// is bypassed (the scene buffer has none) and every show() re-uploads the
	// layers so an animated scene refreshes. get_game_width()/get_game_height()
	// report the scene size so existing centring math lines up with the scene
	// buffer.
	bool scene_mode        = false;
	int  scene_game_width  = 0;
	int  scene_game_height = 0;
	// When scene mode is on, the handle of the full-screen scene layer that owns
	// the display, so screen_to_game()/game_to_screen() can map through it (mouse
	// hit-testing + cursor placement line up with the scaled scene).
	int active_scene_layer = -1;
	int inter_width;
	int inter_height;
	// Guardband around the edge of the draw surface to allow scalers to run
	// without per pixel bounds checking and to allow rounding up to
	// multiples of 4. It should  not be less than 4 and there is no reason for
	// it to be bigger.
	const int guard_band = 4;

	FillMode fill_mode;
	int      fill_scaler;

	// Layer  scaling configuration for composited layers. Layer sizing is
	// explicit (width/height), with 0x0 meaning Auto (use game area size).
	// Scaler/fill settings come from ui_scaler / ui_fill_mode /
	// ui_fill_scaler.
	UiLayerConfig ui_cfgs[NumUiLayerKinds];
	uint32        ui_layer_kind_mask = (1u << NumUiLayerKinds) - 1u;

	const UiLayerConfig& get_ui_cfg(UiLayerKind kind) const {
		return ui_cfgs[static_cast<int>(kind)];
	}

	// Effective UI scaling values.
	int eff_ui_scaler(const UiLayerConfig& cfg) const {
		return cfg.scaler;
	}

	int eff_ui_scale(const UiLayerConfig&) const {
		return 1;
	}

	FillMode eff_ui_fill_mode(const UiLayerConfig& cfg) const {
		return cfg.fill_mode;
	}

	int eff_ui_fill_scaler(const UiLayerConfig& cfg) const {
		return cfg.fill_scaler;
	}

	static SDL_DisplayMode desktop_displaymode;
	struct SDL_Window*     screen_window;
	struct SDL_Renderer*   screen_renderer;
	struct SDL_Texture*    screen_texture;
	void                   UpdateRect(SDL_Surface* surf);

	SDL_Surface* paletted_surface;    // Surface that palette is set on (Example
									  // res)
	SDL_Surface* display_surface;     // Final surface that is displayed  (1024x1024)
	SDL_Surface* inter_surface;       // Post scaled/pre stretch surface  (960x600)
	SDL_Surface* draw_surface;        // Pre scaled surface               (320x200)

	// Layers composited on top of the main image (see class Layer).
	std::vector<std::unique_ptr<Layer>> layers;

	// Compute a layer's on-screen destination rect (in display coords, which
	// match the renderer's logical presentation).
	void get_layer_dest(const Layer& layer, struct SDL_FRect& dst);
	// Place a logw x logh layer on the display using the UI fill mode / scale.
	void compute_layer_fill_dest(int logw, int logh, struct SDL_FRect& dst) const;
	void compute_layer_fill_dest(int logw, int logh, struct SDL_FRect& dst, UiLayerKind kind) const;
	// Place a logw x logh area on the display using an explicit fill mode /
	// scale (used to size a hypothetical game area, e.g. 320x200 for "Full").
	void compute_fill_dest(int logw, int logh, FillMode fmode, int escl, struct SDL_FRect& dst) const;
	// Per-pixel scale a 320x200 game area would get on this display (the "Full"
	// layer scale), used to turn get_ui_scale_factor() into a size ratio.
	float ui_full_pixel_scale(UiLayerKind kind) const;
	// Composite all visible layers onto the renderer (after the main image,
	// before presenting).
	void composite_layers();

	// (Re)upload a layer's 8-bit pixels into its texture, using the palette.
	// Overridden by the palettized 8-bit window.
	virtual void refresh_layer(Layer& layer) {
		ignore_unused_variable_warning(layer);
	}

	// Scale factor the current game scaler would apply to layer, or
	// 1 if the layer should just be uploaded 1:1 (arb / GPU scalers).
	int layer_render_scale(const Layer& layer) const;
	// Run the current (member) scaler on a guard-banded 8-bit source surface
	// into a 32-bit destination surface, by temporarily repointing the
	// scaling state.  Returns false if the current scaler can't be used this
	// way (arb or SDL scaler).  Used to apply the game scaler to layers.
	bool scale_layer_color(const Layer& layer, struct SDL_Surface* src8, int logw, int logh, struct SDL_Surface* dst32);
	// Free any GPU textures owned by layers (e.g. before the renderer is
	// destroyed).  The layers and their buffers are kept.
	void free_layer_textures();

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
			Image_buffer* ib, int w, int h, int gamew, int gameh, int scl = 1, bool fs = false, int sclr = point,
			FillMode fmode = AspectCorrectCentre, int fillsclr = point)
			: ibuf(ib), scale(scl), scaler(sclr), uses_palette(true), fullscreen(fs), game_width(gamew), game_height(gameh),
			  saved_game_width(gamew), saved_game_height(gameh), fill_mode(fmode), fill_scaler(fillsclr), screen_window(nullptr),
			  screen_renderer(nullptr), screen_texture(nullptr), paletted_surface(nullptr), display_surface(nullptr),
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
		return scene_mode ? scene_game_width : game_width;
	}

	int get_game_height() {
		return scene_mode ? scene_game_height : game_height;
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
			unsigned int neww, unsigned int newh, bool newfs, unsigned int newgw, unsigned int newgh, int newsc,
			int newscaler = point, FillMode fmode = AspectCorrectCentre, int fillsclr = point);

	void show() {    // Repaint entire window.
		show(get_start_x(), get_start_y(), get_full_width(), get_full_height());
	}

	// Repaint rectangle.
	void show(int x, int y, int w, int h);

	void toggle_fullscreen();

	// -------- Layers --------
	// Create a layer with a logical (game-pixel) size of w x h.
	// Returns a non-negative handle, or -1 on failure.  Paint into the
	// buffer returned by get_layer_ibuf(handle) using the normal routines.
	// 'transparent' is the palette index treated as see-through; when
	// 'fixed_scale' is 0 the layer is scaled to fit and centred (keeping a
	// constant on-screen size independent of the world scaling), otherwise it
	// is drawn at the given integer scale.  'z' orders layers: higher values
	// are composited on top.
	int           create_layer(int w, int h, unsigned char transparent = 255, int fixed_scale = 0, int z = 0);
	void          destroy_layer(int handle);
	Image_buffer* get_layer_ibuf(int handle);
	// Mark a layer's buffer as changed so it is re-uploaded before the next
	// composite.  Call after painting into it.
	void layer_set_dirty(int handle);
	void layer_set_visible(int handle, bool visible);
	bool layer_is_visible(int handle);
	// Set a layer's composite order (higher is on top).
	void layer_set_z(int handle, int z);
	// Give a layer an explicit destination rectangle (in display coords),
	// overriding the centred auto-fit placement.  Used to position a layer
	// (e.g. the mouse cursor) freely. layer_clear_dest() restores auto-fit.
	void layer_set_dest(int handle, int x, int y, int w, int h);
	void layer_clear_dest(int handle);
	void layer_set_ui_kind(int handle, UiLayerKind kind);
	// Set (or clear, with nullptr) a layer's 256-entry ARGB override table.
	// Non-zero entries replace the opaque palette colour for that index,
	// carrying their own alpha (used for translucent pixels).
	void layer_set_index_argb(int handle, const uint32* argb256);
	// Whole-layer opacity (255 = opaque). Lets an opaque-painted layer be
	// composited semi-transparently (e.g. the translucent shortcut bar).
	void layer_set_alpha(int handle, unsigned char a);

	// -------- Layer scaling config --------
	// Configure how layers (conversation, mouse cursor) are scaled and
	// placed. width/height set a fixed UI layout size in game pixels. A value
	// of 0,0 means Auto (use game area size).
	void set_ui_config(int width, int height, int scaler, FillMode fmode, int fill_scaler);
	void set_ui_layer_config(UiLayerKind kind, int width, int height, int scaler, FillMode fmode, int fill_scaler);
	// -------- Layer fixed palette --------
	// Which fixed palette (if any) a layer uses when the game's live
	// palette is not the day palette. Values are UiPaletteMode; the mapping to
	// an actual game palette number is done by the caller (palette code).
	void set_ui_layer_palette(UiLayerKind kind, int mode);
	int  get_ui_layer_palette_mode(UiLayerKind kind) const;
	// Push the (gamma-corrected, 768-byte RGB) palette a layer should draw with,
	// or nullptr to clear the override so the layer follows the live palette.
	void set_ui_layer_palette_colors(UiLayerKind kind, const unsigned char* colors768);
	// Bitmask of composited layer kinds. Bit i controls UiLayerKind i.
	// Default enables every kind.
	void   set_ui_layer_kind_mask(uint32 mask);
	uint32 get_ui_layer_kind_mask() const;
	void   set_ui_layer_kind_enabled(UiLayerKind kind, bool enabled);
	bool   is_ui_layer_kind_enabled(UiLayerKind kind) const;

	int get_ui_width() const;
	int get_ui_height() const;
	int get_ui_width(UiLayerKind kind) const;
	int get_ui_height(UiLayerKind kind) const;

	// Uniform scale factor from UI logical pixels to display pixels for the
	// configured UI width/height (or game area when Auto 0x0).
	float get_ui_scale_factor() const;
	float get_ui_scale_factor(UiLayerKind kind) const;
	float get_ui_hud_scale(UiLayerKind kind) const;
	// Compute a layer's on-screen destination for a logw x logh
	// layout: shaped by the UI fill mode (Fill stretches, Fit keeps 1:1 pixels,
	// AspectCorrect* uses 1:1.2, Centre a fixed scale) and scaled by the UI
	// size, centred in display coords.
	void compute_ui_layer_dest(int logw, int logh, struct SDL_FRect& dst) const;
	void compute_ui_layer_dest(int logw, int logh, struct SDL_FRect& dst, UiLayerKind kind) const;
	// Mark every layer's texture stale so it is re-converted (e.g. after a
	// palette change, since a layer texture is a snapshot of the palette).
	void mark_all_layers_dirty();
	// Map a point in display/window coords to a layer's logical coords.
	// Returns false if the layer is invalid or the point falls outside it.
	bool screen_to_layer(int handle, int sx, int sy, int& lx, int& ly);
	// Inverse of screen_to_layer: map a layer's logical coords to display coords.
	bool layer_to_screen(int handle, int lx, int ly, int& sx, int& sy);
	// Mark a layer as fully opaque: no palette index is treated as transparent
	// (for a full-screen scene whose content may legitimately use index 255).
	void layer_set_opaque(int handle, bool opaque);

	// Set palette.
	virtual void set_palette(const unsigned char* rgbs, int maxval, int brightness = 100) {
		ignore_unused_variable_warning(rgbs, maxval, brightness);
	}

	// Rotate palette colors.
	virtual void rotate_colors(int first, int num, int upd) {
		ignore_unused_variable_warning(first, num, upd);
	}

	// This method adjusts buffer dimensions so gamewin can draw into the
	// guard band on the right and bottom in order to prevent fine black lines
	// when scalers read from the guardband. Must call EndPaintIntoGuardBand
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
		// A full-screen scene layer buffer has no guard band, so use the plain
		// (point-scaler style) path with no guard band expansion.
		if (scene_mode) {
			return false;
		}
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

	// Enter/leave "scene mode" while a full-screen scene layer is driving the
	// display through the normal blit path. In scene mode the guard band fill is
	// skipped (the scene buffer has no guard band) and every show() re-uploads
	// the layers so an animated scene refreshes. get_game_width()/
	// get_game_height() report the scene size so existing centring math lines up
	// with the scene buffer.
	void set_scene_mode(bool on, int scene_w = 0, int scene_h = 0, int layer_handle = -1) {
		scene_mode         = on;
		scene_game_width   = scene_w;
		scene_game_height  = scene_h;
		active_scene_layer = on ? layer_handle : -1;
	}

	bool in_scene_mode() const {
		return scene_mode;
	}

	// The scene size reported by get_game_width()/get_game_height() while scene
	// mode is on. Used to save/restore scene mode so scenes can nest.
	int get_scene_game_width() const {
		return scene_game_width;
	}

	int get_scene_game_height() const {
		return scene_game_height;
	}

	int get_active_scene_layer() const {
		return active_scene_layer;
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
	void copy8(const unsigned char* src_pixels, int srcw, int srch, int destx, int desty) {
		ibuf->copy8(src_pixels, srcw, srch, destx, desty);
	}

	// Copy line to here.
	void copy_hline8(const unsigned char* src_pixels, int srcw, int destx, int desty) {
		ibuf->copy_hline8(src_pixels, srcw, destx, desty);
	}

	// Copy with translucency table.
	void copy_hline_translucent8(
			const unsigned char* src_pixels, int srcw, int destx, int desty, int first_translucent, int last_translucent,
			const Xform_palette* xforms) {
		ibuf->copy_hline_translucent8(src_pixels, srcw, destx, desty, first_translucent, last_translucent, xforms);
	}

	// Apply translucency to a line.
	void fill_hline_translucent8(unsigned char val, int srcw, int destx, int desty, const Xform_palette& xform) {
		ibuf->fill_hline_translucent8(val, srcw, destx, desty, xform);
	}

	// Apply translucency to a rectangle
	virtual void fill_translucent8(unsigned char val, int srcw, int srch, int destx, int desty, const Xform_palette& xform) {
		ibuf->fill_translucent8(val, srcw, srch, destx, desty, xform);
	}

	// Copy rect. with transp. color.
	void copy_transparent8(const unsigned char* src_pixels, int srcw, int srch, int destx, int desty) {
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
