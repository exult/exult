/*
 *  Copyright (C) 2000-2025  The Exult Team
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

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

#include "browser.h"
#include "exult.h"
#include "files/U7file.h"
#include "font.h"
#include "game.h"
#include "gamewin.h"
#include "gump_utils.h"
#include "items.h"
#include "keys.h"
#include "shapeid.h"

ShapeBrowser::ShapeBrowser() {
	num_shapes      = 0;
	current_shape   = 0;
	num_frames      = 0;
	current_frame   = 0;
	num_files       = game->get_resource("files/shapes/count").num;
	current_file    = 0;
	shapes          = nullptr;
	num_palettes    = game->get_resource("palettes/count").num;
	current_palette = 0;
	num_xforms      = game->get_resource("xforms/count").num;
	current_xform   = -1;
}

ShapeBrowser::~ShapeBrowser() {
	delete shapes;
}

static void handle_key(bool shift, int& value, int max, int amt = 1) {
	if (max == 0) {
		return;
	}

	if (shift) {
		value -= amt;
	} else {
		value += amt;
	}

	while (value < 0) {
		value = max + value;
	}
	while (value >= max) {
		value = value - max;
	}
}

void ShapeBrowser::browse_shapes() {
	Game_window*   gwin = Game_window::get_instance();
	Shape_manager* sman = Shape_manager::get_instance();
	Image_buffer8* ibuf = gwin->get_win()->get_ib8();

	// Try to get the Font form Blackgate first because it looks better than the
	// SI one
	auto font = std::make_shared<Font>();
	if (font->load(U7MAINSHP_FLX, 9, 1) != 0) {
		font.reset();
	}

	// Get the font for this game if don't already have it
	if (!font) {
		font = fontManager.get_font("MENU_FONT");
	}
	const int   maxx    = gwin->get_width();
	const int   centerx = maxx / 2;
	const int   maxy    = gwin->get_height();
	const int   centery = maxy / 2;
	Palette     pal;
	char        buf[255];
	const char* fname;
	bool        looping             = true;
	bool        redraw              = true;
	bool        do_palette_rotation = true;
	bool        bounding_box        = false;
	SDL_Event   event;
	// int active;

	auto get_patch_sources = [&](const char* main_file)
			-> std::vector<std::pair<std::string, int>> {
		std::vector<std::pair<std::string, int>> sources;
		sources.emplace_back(main_file, -1);    // Main file

		std::string base_name(main_file);
		if (base_name.find("faces.vga") != std::string::npos) {
			sources.emplace_back(PATCH_FACES, -1);
		} else if (base_name.find("gumps.vga") != std::string::npos) {
			sources.emplace_back(PATCH_GUMPS, -1);
		} else if (base_name.find("sprites.vga") != std::string::npos) {
			sources.emplace_back(PATCH_SPRITES, -1);
		} else if (base_name.find("mainshp.flx") != std::string::npos) {
			sources.emplace_back(PATCH_MAINSHP, -1);
		} else if (base_name.find("endshape.flx") != std::string::npos) {
			sources.emplace_back(PATCH_ENDSHAPE, -1);
		} else if (base_name.find("fonts.vga") != std::string::npos) {
			sources.emplace_back(PATCH_FONTS, -1);
		}

		return sources;
	};

	auto get_shape_frame = [&](int shape, int frame) -> Shape_frame* {
		if (current_file == 0) {
			return sman->get_shapes().get_shape(shape, frame);
		} else {
			// Use the browser's shapes for other files, including patches
			if (!shapes) {
				snprintf(buf, sizeof(buf), "files/shapes/%d", current_file);
				fname = game->get_resource(buf).str;

				std::vector<std::pair<std::string, int>> sources
						= get_patch_sources(fname);
				shapes = new Vga_file();
				shapes->load(sources);
			}
			return shapes->get_shape(shape, frame);
		}
	};

	auto get_num_shapes = [&]() -> int {
		if (current_file == 0) {
			return sman->get_shapes().get_num_shapes();
		} else {
			if (!shapes) {
				snprintf(buf, sizeof(buf), "files/shapes/%d", current_file);
				fname = game->get_resource(buf).str;

				std::vector<std::pair<std::string, int>> sources
						= get_patch_sources(fname);
				shapes = new Vga_file();
				shapes->load(sources);
			}
			return shapes->get_num_shapes();
		}
	};

	auto get_num_frames = [&](int shape) -> int {
		if (current_file == 0) {
			return sman->get_shapes().get_num_frames(shape);
		} else {
			if (!shapes) {
				snprintf(buf, sizeof(buf), "files/shapes/%d", current_file);
				fname = game->get_resource(buf).str;

				std::vector<std::pair<std::string, int>> sources
						= get_patch_sources(fname);
				shapes = new Vga_file();
				shapes->load(sources);
			}
			return shapes->get_num_frames(shape);
		}
	};

	if (!shapes && current_file > 0) {
		snprintf(buf, sizeof(buf), "files/shapes/%d", current_file);
		fname = game->get_resource(buf).str;

		std::vector<std::pair<std::string, int>> sources
				= get_patch_sources(fname);
		shapes = new Vga_file();
		shapes->load(sources);
	} else if (current_file == 0) {
		fname = "shapes.vga";
	}

	do {
		if (redraw) {
			gwin->clear_screen();
			snprintf(buf, sizeof(buf), "palettes/%d", current_palette);
			const str_int_pair& pal_tuple = game->get_resource(buf);
			snprintf(buf, sizeof(buf), "palettes/patch/%d", current_palette);
			const str_int_pair& patch_tuple = game->get_resource(buf);
			if (current_xform > 0) {
				char xfrsc[256];
				snprintf(xfrsc, sizeof(xfrsc), "xforms/%d", current_xform);
				const str_int_pair& xform_tuple = game->get_resource(xfrsc);
				pal.load(
						pal_tuple.str, patch_tuple.str, pal_tuple.num,
						xform_tuple.str, xform_tuple.num);
			} else {
				pal.load(pal_tuple.str, patch_tuple.str, pal_tuple.num);
			}

			pal.apply();
			font->paint_text_fixedwidth(ibuf, "Show [K]eys", 2, maxy - 50, 8);

			snprintf(buf, sizeof(buf), "VGA File: '%s'", fname);
			// font->draw_text(ibuf, 0, 170, buf);
			font->paint_text_fixedwidth(ibuf, buf, 2, maxy - 30, 8);

			num_shapes = get_num_shapes();
			snprintf(
					buf, sizeof(buf), "Shape: %2d/%d", current_shape,
					num_shapes - 1);
			// font->draw_text(ibuf, 0, 180, buf);
			font->paint_text_fixedwidth(ibuf, buf, 2, maxy - 20, 8);

			num_frames = get_num_frames(current_shape);
			snprintf(
					buf, sizeof(buf), "Frame: %2d/%d", current_frame,
					num_frames - 1);
			// font->draw_text(ibuf, 160, 180, buf);
			font->paint_text_fixedwidth(ibuf, buf, 162, maxy - 20, 8);

			snprintf(
					buf, sizeof(buf), "Palette: %s, %d", pal_tuple.str,
					pal_tuple.num);
			// font->draw_text(ibuf, 0, 190, buf);
			font->paint_text_fixedwidth(ibuf, buf, 2, maxy - 10, 8);

			if (num_frames) {
				Shape_frame* frame
						= get_shape_frame(current_shape, current_frame);

				if (frame) {
					snprintf(
							buf, sizeof(buf), "%d x %d", frame->get_width(),
							frame->get_height());
					// font->draw_text(ibuf, 32, 32, buf);
					font->paint_text_fixedwidth(ibuf, buf, 2, 22, 8);

					// Coords for shape to be drawn (centre of the screen)
					const int x = gwin->get_width() / 2;
					const int y = gwin->get_height() / 2;

					// draw outline if not drawing bbox
					if (current_file != 0 || !bounding_box) {
						gwin->get_win()->fill8(
								255, frame->get_width() + 4,
								frame->get_height() + 4,
								x - frame->get_xleft() - 2,
								y - frame->get_yabove() - 2);
						gwin->get_win()->fill8(
								0, frame->get_width() + 2,
								frame->get_height() + 2,
								x - frame->get_xleft() - 1,
								y - frame->get_yabove() - 1);
					}

					// draw shape
					sman->paint_shape(x, y, frame, true);

					// Stuff that should only be drawn for object shapes in
					// shapes.vga
					if (current_file == 0) {
						const Shape_info& info
								= ShapeID::get_info(current_shape);

						snprintf(
								buf, sizeof(buf),
								"class: %2i  ready_type: 0x%02x 3d: %ix%ix%i",
								info.get_shape_class(), info.get_ready_type(),
								info.get_3d_xtiles(current_frame),
								info.get_3d_ytiles(current_frame),
								info.get_3d_height());
						font->paint_text_fixedwidth(ibuf, buf, 2, 12, 8);

						// TODO: do we want to display something other than
						// this for shapes >= 1024?
						if (current_shape < get_num_item_names()
							&& get_item_name(current_shape)) {
							// font->draw_text(ibuf, 32, 16,
							// get_item_name(current_shape));
							font->paint_text_fixedwidth(
									ibuf, get_item_name(current_shape), 2, 2,
									8);
						}
						if (bounding_box) {
							info.paint_bbox(
									x, y, current_frame,
									gwin->get_win()->get_ibuf(), 255);
						}
					}

				} else {
					font->draw_text(
							ibuf, centerx - 20, centery - 5, "No Shape");
				}
			} else {
				font->draw_text(ibuf, centerx - 20, centery - 5, "No Shape");
			}

			pal.apply();
			redraw = false;
		}
		Delay();
		if (SDL_PollEvent(&event) && event.type == SDL_EVENT_KEY_DOWN) {
			redraw           = true;
			const bool shift = event.key.mod & SDL_KMOD_SHIFT;
			switch (event.key.key) {
			case SDLK_ESCAPE:
				looping = false;
				break;
			case SDLK_V:
				handle_key(shift, current_file, num_files);
				current_shape = 0;
				current_frame = 0;
				delete shapes;
				shapes = nullptr;
				if (current_file > 0) {
					snprintf(buf, sizeof(buf), "files/shapes/%d", current_file);
					fname = game->get_resource(buf).str;

					std::vector<std::pair<std::string, int>> sources
							= get_patch_sources(fname);
					shapes = new Vga_file();
					shapes->load(sources);
				} else {
					fname = "shapes.vga";
				}
				break;
			case SDLK_P:
				handle_key(shift, current_palette, num_palettes);
				current_xform = -1;
				break;
			case SDLK_R:
				do_palette_rotation = !do_palette_rotation;
				break;

			case SDLK_B:
				bounding_box = !bounding_box;
				redraw       = true;
				break;

			case SDLK_X:
				handle_key(shift, current_xform, num_xforms);
				break;
				// Shapes
			case SDLK_S:
				if ((event.key.mod & SDL_KMOD_ALT)
					&& (event.key.mod & SDL_KMOD_CTRL)) {
					make_screenshot(true);
				} else {
					handle_key(shift, current_shape, num_shapes);
					current_frame = 0;
				}
				break;
			case SDLK_UP:
				handle_key(true, current_shape, num_shapes);
				current_frame = 0;
				break;
			case SDLK_DOWN:
				handle_key(false, current_shape, num_shapes);
				current_frame = 0;
				break;
			case SDLK_J:    // Jump by 20.
				handle_key(shift, current_shape, num_shapes, 20);
				current_frame = 0;
				break;
			case SDLK_PAGEUP:
				handle_key(true, current_shape, num_shapes, 20);
				current_frame = 0;
				break;
			case SDLK_PAGEDOWN:
				handle_key(false, current_shape, num_shapes, 20);
				current_frame = 0;
				break;
				// Frames
			case SDLK_F:
				handle_key(shift, current_frame, num_frames);
				break;
			case SDLK_LEFT:
				handle_key(true, current_frame, num_frames);
				break;
			case SDLK_RIGHT:
				handle_key(false, current_frame, num_frames);
				break;
			case SDLK_K:
				keybinder->ShowBrowserKeys();
				break;
			default:
				break;
			}
		}
		if (do_palette_rotation && gwin->rotatecolours()) {
			gwin->show();
		}
	} while (looping);
}

bool ShapeBrowser::get_shape(int& shape, int& frame) {
	Shape_manager* sman = Shape_manager::get_instance();
	if (current_shape >= sman->get_shapes().get_num_shapes()
		|| current_file != 0) {
		return false;
	} else {
		shape = current_shape;
		frame = current_frame;
		return true;
	}
}
