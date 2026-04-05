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

#include "Gump.h"
#include "gumpinf.h"

#include <iostream>

Dynamic_shape_widget::Dynamic_shape_widget(Gump* par, const Dynamic_shape_def& def)
		: Gump_widget(par, -1, def.pos_x, def.pos_y, 0, SF_GUMPS_VGA), field_id(def.field_id),
		  displayed(def.shape_num, def.frame_num, def.shapefile == 1 ? SF_GUMPS_VGA : SF_SHAPES_VGA) {}

Gump_widget* Dynamic_shape_widget::clone(Gump* par) {
	Dynamic_shape_def def;
	def.field_id  = field_id;
	def.pos_x     = x;
	def.pos_y     = y;
	def.shape_num = displayed.get_shapenum();
	def.frame_num = displayed.get_framenum();
	def.shapefile = displayed.get_shapefile() == SF_GUMPS_VGA ? 1 : 0;
	return new Dynamic_shape_widget(par, def);
}

void Dynamic_shape_widget::paint() {
	if (displayed.get_shapenum() <= 0) {
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
	displayed.paint_shape(sx, sy);
}
