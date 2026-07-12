/*
 *  scene_layer.h - Full-screen "scene" overlay layer helper.
 *
 *  Copyright (C) 2026  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef SCENE_LAYER_H
#define SCENE_LAYER_H

#include "gamewin.h"
#include "ibuf8.h"

/*
 *  Manages a full-screen "scene" overlay layer for takeover screens such as the
 *  cheat screen and the intro. The scene is drawn into a fixed-size 8-bit
 *  buffer (default 320x200; an individual scene may request a different size)
 *  and is composited scaled to fill the display using the main game video
 *  settings. Those come from the UiLayerFullScreenScene layer, which is wired
 *  to config/video/scale_method, fill_mode and fill_scaler (no per-scene
 *  config).
 *
 *  Usage, once per rendered frame:
 *      scene.begin_frame();   // black letterbox + redirect drawing to the scene
 *      ... paint into scene.buffer() (and via the current render target) ...
 *      scene.end_frame();     // restore target, mark the scene visible + dirty
 *      gwin->get_win()->show();   // composite to the display
 */
class Scene_layer {
public:
	explicit Scene_layer(int width = 320, int height = 200) : scene_w(width > 0 ? width : 320), scene_h(height > 0 ? height : 200) {
		Game_window*  gwin = Game_window::get_instance();
		Image_window* iwin = gwin->get_win();
		// Follow the main game video settings so the scene scales exactly like
		// the game area (rather than a fixed scaler). No per-scene config.
		gwin->set_ui_layer_config(
				Image_window::UiLayerFullScreenScene, scene_w, scene_h, iwin->get_scaler(), iwin->get_fill_mode(),
				iwin->get_fill_scaler());
		handle = gwin->create_layer(scene_w, scene_h, transparent_index, 0, scene_layer_z);
		if (handle >= 0) {
			gwin->layer_set_ui_kind(handle, Image_window::UiLayerFullScreenScene);
			// A scene is a full-screen opaque takeover: NO palette index is
			// transparent, so content that legitimately uses the transparent
			// index (e.g. index 255 in an FLI frame) is drawn, not seen through.
			gwin->layer_set_opaque(handle, true);
			// Stay hidden until the first frame has been painted so an early
			// composite (e.g. a palette apply) does not show an uninitialised
			// buffer.
			gwin->layer_set_visible(handle, false);
		}
	}

	~Scene_layer() {
		restore_target();
		if (handle >= 0) {
			Game_window::get_instance()->destroy_layer(handle);
			handle = -1;
		}
	}

	Scene_layer(const Scene_layer&)            = delete;
	Scene_layer& operator=(const Scene_layer&) = delete;

	bool valid() const {
		return handle >= 0;
	}

	int width() const {
		return scene_w;
	}

	int height() const {
		return scene_h;
	}

	int get_handle() const {
		return handle;
	}

	// The 8-bit buffer to paint the scene into.
	Image_buffer8* buffer() const {
		return handle >= 0 ? Game_window::get_instance()->get_layer_ibuf(handle) : nullptr;
	}

	// Begin a frame: clear the game window to black (the letterbox behind the
	// scaled scene), then redirect drawing to the scene buffer and clear it.
	void begin_frame() {
		Game_window* gwin = Game_window::get_instance();
		// Recover if a previous frame's end_frame was skipped (e.g. an early
		// break in the caller's render loop) so the render target is never left
		// pointing at the scene buffer.
		restore_target();
		gwin->clear_screen();    // black background behind the scaled scene
		Image_buffer8* lbuf = buffer();
		if (!lbuf) {
			return;
		}
		prev_target = gwin->push_render_target(lbuf);
		painting    = true;
		lbuf->clear_clip();
		lbuf->fill8(0);    // opaque black scene background
	}

	// End a frame: restore the previous render target and mark the scene layer
	// visible + dirty so the next show() composites it. The scene has no
	// explicit destination, so it fills the display per the configured fill
	// mode.
	void end_frame() {
		restore_target();
		if (handle >= 0) {
			Game_window* gwin = Game_window::get_instance();
			gwin->layer_set_visible(handle, true);
			gwin->layer_set_dirty(handle);
		}
	}

	// Restore the previous render target if a frame is still redirecting
	// drawing to the scene buffer. Safe to call at any time.
	void restore_target() {
		if (painting) {
			Game_window::get_instance()->pop_render_target(prev_target);
			painting    = false;
			prev_target = nullptr;
		}
	}

	// Hide/show the scene, e.g. while temporarily showing the game world.
	void set_visible(bool visible) {
		if (handle >= 0) {
			Game_window::get_instance()->layer_set_visible(handle, visible);
		}
	}

	// Change the scene's composite order.
	void set_z(int z) {
		if (handle >= 0) {
			Game_window::get_instance()->layer_set_z(handle, z);
		}
	}

	// A scene normally sits on top of every other overlay. Lower it beneath the
	// text-gump layers so a text gump (scroll/sign/book) can be shown ON TOP of
	// the scene; call restore_z() afterwards to put the scene back on top.
	void lower_beneath_gumps() {
		set_z(1 << 18);
	}

	void restore_z() {
		set_z(scene_layer_z);
	}

private:
	// Above the gump/HUD/modal layers, below the display-map overlay and mouse.
	constexpr static int           scene_layer_z     = 1 << 19;
	constexpr static unsigned char transparent_index = 255;

	int            handle = -1;
	int            scene_w;
	int            scene_h;
	Image_buffer8* prev_target = nullptr;
	bool           painting    = false;
};

/*
 *  RAII driver for a full-screen "takeover" screen (intro, endgame, character
 *  creation, the top menu, ...) rendered through a Scene_layer. On construction
 *  it:
 *    - creates the fixed-size scene layer,
 *    - masks every UI layer except the scene (plus the mouse pointer when
 *      'interactive'),
 *    - clears the game window to black (the letterbox) and redirects all drawing
 *      into the scene buffer,
 *    - puts the window in "scene mode" (guard band bypassed; every show()
 *      re-uploads the scene so animation refreshes) and makes the scene visible,
 *    - rewrites the caller's game-coordinate anchors so 320x200-relative drawing
 *      lands in the scene (topx=topy=0, centerx=w/2, centery=h/2).
 *  Every piece of that state (render target, scene mode, coordinate anchors and
 *  the layer mask) is saved and restored on destruction, so scenes may nest
 *  (e.g. the intro launched from the layered top menu) and the whole thing is
 *  exception-safe.
 *
 *  Typical use:
 *      Scene_view scene(topx, topy, centerx, centery, ibuf, interactive);
 *      ... draw the screen using the (now scene-relative) anchors ...
 *  and the scene is torn down when 'scene' goes out of scope.
 */
class Scene_view {
public:
	Scene_view(
			int& topx, int& topy, int& centerx, int& centery, Image_buffer8*& ibuf, bool interactive = false, int width = 320,
			int height = 200)
			: scene(width, height), r_topx(topx), r_topy(topy), r_centerx(centerx), r_centery(centery), r_ibuf(ibuf), s_topx(topx),
			  s_topy(topy), s_centerx(centerx), s_centery(centery), s_ibuf(ibuf) {
		Game_window*  gwin = Game_window::get_instance();
		Image_window* iwin = gwin->get_win();
		// Save everything we are about to change so it can be restored (this is
		// also what makes scenes nestable).
		s_mask         = gwin->get_ui_layer_kind_mask();
		s_scene_mode   = iwin->in_scene_mode();
		s_scene_w      = iwin->get_scene_game_width();
		s_scene_h      = iwin->get_scene_game_height();
		s_scene_handle = iwin->get_active_scene_layer();

		uint32 mask = 1u << static_cast<int>(Image_window::UiLayerFullScreenScene);
		if (interactive) {
			mask |= 1u << static_cast<int>(Image_window::UiLayerMousePointer);
		}
		gwin->set_ui_layer_kind_mask(mask);

		if (scene.valid()) {
			scene.begin_frame();    // letterbox + redirect drawing to the scene
			iwin->set_scene_mode(true, scene.width(), scene.height(), scene.get_handle());
			gwin->layer_set_visible(scene.get_handle(), true);
			gwin->layer_set_dirty(scene.get_handle());
			r_ibuf = scene.buffer();
			// In scene mode get_width()/get_height() report the scene size, so
			// these evaluate to topx=topy=0, centerx=w/2, centery=h/2.
			r_topx    = (gwin->get_width() - scene.width()) / 2;
			r_topy    = (gwin->get_height() - scene.height()) / 2;
			r_centerx = gwin->get_width() / 2;
			r_centery = gwin->get_height() / 2;
		}
	}

	~Scene_view() {
		Game_window*  gwin = Game_window::get_instance();
		Image_window* iwin = gwin->get_win();
		scene.restore_target();    // pop the render target back
		if (scene.valid()) {
			gwin->layer_set_visible(scene.get_handle(), false);
		}
		// Restore scene mode (to the parent scene when nested, or off).
		iwin->set_scene_mode(s_scene_mode, s_scene_w, s_scene_h, s_scene_handle);
		r_topx    = s_topx;
		r_topy    = s_topy;
		r_centerx = s_centerx;
		r_centery = s_centery;
		r_ibuf    = s_ibuf;
		gwin->set_ui_layer_kind_mask(s_mask);
		// The scene layer itself is destroyed by the Scene_layer member dtor.
	}

	Scene_view(const Scene_view&)            = delete;
	Scene_view& operator=(const Scene_view&) = delete;

	bool valid() const {
		return scene.valid();
	}

	Scene_layer& layer() {
		return scene;
	}

	int get_handle() const {
		return scene.get_handle();
	}

	// Map a display/screen point to scene-buffer (game-relative) coordinates for
	// hit-testing widgets drawn into the scene. The scene is scaled to fill the
	// display, so raw screen_to_game() coordinates do not line up with widgets
	// laid out in the 320x200 scene. Falls back to screen_to_game() when the
	// scene is unavailable.
	bool screen_to_scene(int sx, int sy, int& lx, int& ly) const {
		Game_window* gwin = Game_window::get_instance();
		if (scene.valid()) {
			return gwin->get_win()->screen_to_layer(scene.get_handle(), sx, sy, lx, ly);
		}
		gwin->get_win()->screen_to_game(sx, sy, gwin->get_fastmouse(), lx, ly);
		return true;
	}

private:
	Scene_layer     scene;
	int&            r_topx;
	int&            r_topy;
	int&            r_centerx;
	int&            r_centery;
	Image_buffer8*& r_ibuf;
	int             s_topx;
	int             s_topy;
	int             s_centerx;
	int             s_centery;
	Image_buffer8*  s_ibuf;
	uint32          s_mask         = 0;
	bool            s_scene_mode   = false;
	int             s_scene_w      = 0;
	int             s_scene_h      = 0;
	int             s_scene_handle = -1;
};

#endif
