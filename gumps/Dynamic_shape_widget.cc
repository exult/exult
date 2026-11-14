/*
Copyright (C) 2025 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "Dynamic_shape_widget.h"

#include "Dynamic_list_layout.h"
#include "Dynamic_list_widget.h"
#include "Gump.h"
#include "gamewin.h"
#include "gumpinf.h"
#include "ibuf8.h"

#include <iostream>

Dynamic_shape_widget::Dynamic_shape_widget(Gump* par, const Dynamic_shape_def& def)
		: Gump_widget(par, -1, def.pos_x, def.pos_y, 0, SF_GUMPS_VGA), field_id(def.field_id),
		  displayed(def.shape_num, def.frame_num, def.shapefile == 1 ? SF_GUMPS_VGA : SF_SHAPES_VGA), list_id(def.list_id),
		  box_w(def.box_w), box_h(def.box_h) {}

Gump_widget* Dynamic_shape_widget::clone(Gump* par) {
	Dynamic_shape_def def;
	def.field_id  = field_id;
	def.pos_x     = x;
	def.pos_y     = y;
	def.shape_num = displayed.get_shapenum();
	def.frame_num = displayed.get_framenum();
	def.shapefile = displayed.get_shapefile() == SF_GUMPS_VGA ? 1 : 0;
	def.list_id   = list_id;
	def.box_w     = box_w;
	def.box_h     = box_h;
	return new Dynamic_shape_widget(par, def);
}

// The list this display is bound to, on the same gump, or null.
Dynamic_list_widget* Dynamic_shape_widget::find_list() const {
	auto* gump = dynamic_cast<Gump*>(parent);
	if (list_id < 0 || !gump) {
		return nullptr;
	}
	for (auto* w : gump->get_elems()) {
		auto* list = dynamic_cast<Dynamic_list_widget*>(w);
		if (list && list->get_field_id() == list_id) {
			return list;
		}
	}
	return nullptr;
}

void Dynamic_shape_widget::paint() {
	// A bound display shows the list's selection, else its own shape.
	const ShapeID*       shown = &displayed;
	Dynamic_list_widget* list  = find_list();
	if (list) {
		const ShapeID* sel = list->get_preview();
		if (sel) {
			shown = sel;
		}
	}
	if (shown == &displayed && displayed.get_shapenum() <= 0) {
		return;    // No shape to display
	}
	int sx = x;
	int sy = y;
	if (parent) {
		parent->local_to_screen(sx, sy);
		if (parent->get_debug_flags() & GUMP_DEBUG_CONSOLE) {
			std::cerr << "[DynShape] field_id=" << field_id << " local=(" << x << "," << y << ")"
					  << " screen=(" << sx << "," << sy << ")"
					  << " shape=" << displayed.get_shapenum() << std::endl
					  << std::flush;
		}
	}
	if (box_w > 0 && box_h > 0) {
		paint_enlarged(*shown, sx, sy, box_w, box_h);
	} else {
		shown->paint_shape(sx, sy);
	}
}

/*
 *  Paint a shape k times its size, centred in a box.  It is painted
 *  first into two scratch buffers, one filled with the colour under the
 *  box and one with 0xff: a pixel is transparent only where they still
 *  read those.  Translucent pixels come out blended with the background.
 */

void Dynamic_shape_widget::paint_enlarged(const ShapeID& sid, int bx, int by, int bw, int bh) {
	const Shape_frame* frame = sid.get_shape();
	if (!frame) {
		return;
	}
	const int w = frame->get_width();
	const int h = frame->get_height();
	if (w <= 0 || h <= 0) {
		return;
	}
	Image_buffer8* ib = gwin->get_win()->get_ib8();
	// The background colour under the box.
	unsigned char back = 0;
	{
		const auto save = ib->SaveClip();
		const int  px   = bx + 4;
		const int  py   = by + bh / 2;
		if (save.Rect().has_point(px, py)) {
			back = ib->get_pixel8(px, py);
		}
		if (back == 0xff) {
			back = 0;
		}
	}
	Image_buffer8 lo(w, h);
	Image_buffer8 hi(w, h);
	lo.fill8(back);
	hi.fill8(0xff);
	for (Image_buffer8* buf : {&lo, &hi}) {
		Image_buffer8* prev = gwin->push_render_target(buf);
		sid.paint_shape(frame->get_xleft(), frame->get_yabove());
		gwin->pop_render_target(prev);
	}
	const int k  = Dynamic_list_layout::preview_scale(w, h, bw, bh);
	const int ox = bx + (bw - w * k) / 2;
	const int oy = by + (bh - h * k) / 2;
	for (int py = 0; py < h; py++) {
		for (int px = 0; px < w; px++) {
			const unsigned char pix = lo.get_pixel8(px, py);
			if (pix == back && hi.get_pixel8(px, py) == 0xff) {
				continue;    // Transparent.
			}
			for (int dy = 0; dy < k; dy++) {
				for (int dx = 0; dx < k; dx++) {
					ib->put_pixel8(pix, ox + px * k + dx, oy + py * k + dy);
				}
			}
		}
	}
}
