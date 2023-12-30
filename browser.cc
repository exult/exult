/*
 *  Copyright (C) 2000-2022  The Exult Team
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
#  include <config.h>
#endif

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "files/U7file.h"
#include "gamewin.h"
#include "game.h"
#include "browser.h"
#include "exult.h"
#include "font.h"
#include "items.h"
#include "shapeid.h"
#include "keys.h"

ShapeBrowser::ShapeBrowser() {
	num_shapes = 0;
	current_shape = 0;
	num_frames = 0;
	current_frame = 0;
	num_files = game->get_resource("files/shapes/count").num;
	current_file = 0;
	shapes = nullptr;
	num_palettes = game->get_resource("palettes/count").num;
	current_palette = 0;
	num_xforms = game->get_resource("xforms/count").num;
	current_xform = -1;
}

ShapeBrowser::~ShapeBrowser() {
	delete shapes;
}

static void handle_key(bool shift, int &value, int max, int amt = 1) {
	if (max == 0) return;

	if (shift)
		value -= amt;
	else
		value += amt;

	while (value < 0)
		value = max + value;
	while (value >= max)
		value = value - max;
}

void ShapeBrowser::browse_shapes() {

	Game_window *gwin = Game_window::get_instance();
	Shape_manager *sman = Shape_manager::get_instance();
	Image_buffer8 *ibuf = gwin->get_win()->get_ib8();
	Font *font = fontManager.get_font("MENU_FONT");

	const int maxx = gwin->get_width();
	const int centerx = maxx / 2;
	const int maxy = gwin->get_height();
	const int centery = maxy / 2;
	Palette pal;
	char buf[255];
	const char *fname;

	snprintf(buf, 255, "files/shapes/%d", current_file);
	fname = game->get_resource(buf).str;
	if (!shapes)
		shapes = new Vga_file(fname);
	bool looping = true;
	bool redraw = true;
	SDL_Event event;
	//int active;

	do {
		if (redraw) {
			gwin->clear_screen();
			snprintf(buf, 255, "palettes/%d", current_palette);
			const str_int_pair &pal_tuple = game->get_resource(buf);
			snprintf(buf, 255, "palettes/patch/%d", current_palette);
			const str_int_pair &patch_tuple = game->get_resource(buf);
			if (current_xform > 0) {
				char xfrsc[256];
				snprintf(xfrsc, 255, "xforms/%d",
				         current_xform);
				const str_int_pair &xform_tuple = game->get_resource(xfrsc);
				pal.load(pal_tuple.str, patch_tuple.str,
				         pal_tuple.num, xform_tuple.str, xform_tuple.num);
			} else
				pal.load(pal_tuple.str, patch_tuple.str, pal_tuple.num);

			pal.apply();
			font->paint_text_fixedwidth(ibuf, "Show [K]eys", 2, maxy - 50, 8);

			snprintf(buf, 255, "VGA File: '%s'", fname);
			//font->draw_text(ibuf, 0, 170, buf);
			font->paint_text_fixedwidth(ibuf, buf, 2, maxy - 30, 8);

			num_shapes = shapes->get_num_shapes();
			snprintf(buf, 255, "Shape: %2d/%d", current_shape, num_shapes - 1);
			//font->draw_text(ibuf, 0, 180, buf);
			font->paint_text_fixedwidth(ibuf, buf, 2, maxy - 20, 8);

			num_frames = shapes->get_num_frames(current_shape);
			snprintf(buf, 255, "Frame: %2d/%d", current_frame, num_frames - 1);
			//font->draw_text(ibuf, 160, 180, buf);
			font->paint_text_fixedwidth(ibuf, buf, 162, maxy - 20, 8);

			snprintf(buf, 255, "Palette: %s, %d", pal_tuple.str, pal_tuple.num);
			//font->draw_text(ibuf, 0, 190, buf);
			font->paint_text_fixedwidth(ibuf, buf, 2, maxy - 10, 8);

			if (num_frames) {
				Shape_frame *frame = shapes->get_shape(
				                         current_shape, current_frame);

				if (frame) {
					snprintf(buf, 255, "%d x %d", frame->get_width(), frame->get_height());
					//font->draw_text(ibuf, 32, 32, buf);
					font->paint_text_fixedwidth(ibuf, buf, 2, 22, 8);

					const Shape_info &info =
					    ShapeID::get_info(current_shape);

					snprintf(buf, 255, "class: %2i  ready_type: 0x%02x", info.get_shape_class(), info.get_ready_type());
					font->paint_text_fixedwidth(ibuf, buf, 2, 12, 8);

					// TODO: do we want to display something other than
					// this for shapes >= 1024?
					if (current_shape < get_num_item_names() && get_item_name(current_shape)) {
						//font->draw_text(ibuf, 32, 16, get_item_name(current_shape));
						font->paint_text_fixedwidth(ibuf, get_item_name(current_shape), 2, 2, 8);
					}

					//draw outline
					gwin->get_win()->fill8(255,
					                       frame->get_width() + 4, frame->get_height() + 4,
					                       gwin->get_width() / 2 - frame->get_xleft() - 2,
					                       gwin->get_height() / 2 - frame->get_yabove() - 2);
					gwin->get_win()->fill8(0,
					                       frame->get_width() + 2, frame->get_height() + 2,
					                       gwin->get_width() / 2 - frame->get_xleft() - 1,
					                       gwin->get_height() / 2 - frame->get_yabove() - 1);

					//draw shape
					sman->paint_shape(gwin->get_width() / 2, gwin->get_height() / 2, frame, true);

				} else
					font->draw_text(ibuf, centerx - 20, centery - 5, "No Shape");
			} else
				font->draw_text(ibuf, centerx - 20, centery - 5, "No Shape");

			pal.apply();
			redraw = false;
		}
		SDL_WaitEvent(&event);
		if (event.type == SDL_KEYDOWN) {
			redraw = true;
			const bool shift = event.key.keysym.mod & KMOD_SHIFT;
			//int ctrl = event.key.keysym.mod & KMOD_CTRL;
			switch (event.key.keysym.sym) {
			case SDLK_ESCAPE:
				looping = false;
				break;
			case SDLK_v:
				handle_key(shift, current_file, num_files);
				current_shape = 0;
				current_frame = 0;
				delete shapes;
				snprintf(buf, 255, "files/shapes/%d", current_file);
				fname = game->get_resource(buf).str;
				shapes = new Vga_file(fname);
				break;
			case SDLK_p:
				handle_key(shift, current_palette, num_palettes);
				current_xform = -1;
				break;
			case SDLK_x:
				handle_key(shift, current_xform,
				           num_xforms);
				break;
				// Shapes
			case SDLK_s:
				if ((event.key.keysym.mod & KMOD_ALT) && (event.key.keysym.mod & KMOD_CTRL))
					make_screenshot(true);
				else {
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
			case SDLK_j:    // Jump by 20.
				handle_key(shift, current_shape,
				           num_shapes, 20);
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
			case SDLK_f:
				handle_key(shift, current_frame, num_frames);
				break;
			case SDLK_LEFT:
				handle_key(true, current_frame, num_frames);
				break;
			case SDLK_RIGHT:
				handle_key(false, current_frame, num_frames);
				break;
			case SDLK_k:
				keybinder->ShowBrowserKeys();
				break;
			default:

				break;
			}
		}
	} while (looping);
}

bool ShapeBrowser::get_shape(int &shape, int &frame) {
	if (!shapes || current_file != 0)
		return false;
	else {
		shape = current_shape;
		frame = current_frame;
		return true;
	}
}
