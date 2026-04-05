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

#ifndef DYNAMIC_SHAPE_WIDGET_H
#define DYNAMIC_SHAPE_WIDGET_H

#include "Gump_widget.h"
#include "shapeid.h"

struct Dynamic_shape_def;

/*
 *  A data-driven shape display on a gump, configured via gump_info.txt.
 *  Shape/frame can be updated at runtime via usecode intrinsic UI_set_gump_shape.
 */
class Dynamic_shape_widget : public Gump_widget {
	int     field_id;     // Unique ID for usecode to address this field
	ShapeID displayed;    // Current shape/frame to display (per-instance)

public:
	Dynamic_shape_widget(Gump* par, const Dynamic_shape_def& def);

	Gump_widget* clone(Gump* par) override;

	void paint() override;

	int get_field_id() const {
		return field_id;
	}

	int get_shape_num() const {
		return displayed.get_shapenum();
	}

	int get_frame_num() const {
		return displayed.get_framenum();
	}

	ShapeFile get_shape_file() const {
		return displayed.get_shapefile();
	}

	void set_shape(int shnum, int frnum) {
		displayed.set_shape(shnum, frnum);
	}
};

#endif
