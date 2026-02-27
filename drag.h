/*
 *  drag.cc - Dragging objects in Game_window.
 *
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

#ifndef INCL_DRAG_H
#define INCL_DRAG_H

#include "mouse.h"
#include "objs.h"
#include "rect.h"
#include "singles.h"
#include "tiles.h"

class Gump;
class Gump_button;
class Image_buffer;

/*
 *  Data needed when dragging an object.
 */
class Dragging_info : public Game_singletons {
	Game_object_shared obj;       // What's being dragged.
	bool               is_new;    // Object was newly created.
	Gump*              gump;
	Gump_button*       button;
	Tile_coord         old_pos;    // Original pos. of object if it wasn't
	//   in a container.
	TileRect old_foot;         // Original footprint.
	int      old_lift;         // Lift of obj OR its owner.
	int      quantity;         // Amount of object being moved.
	int      readied_index;    // If it was a 'readied' item.
	// Last mouse, paint positions:
	int                 mousex, mousey, paintx, painty;
	Mouse::Mouse_shapes mouse_shape;       // Save starting mouse shape.
	TileRect            rect;              // Rectangle to repaint.
	bool                okay;              // True if drag constructed okay.
	bool                possible_theft;    // Moved enough to be 'theft'.

	bool start(int x, int y);    // First motion.
	void put_back();             // Put back object.
	bool drop_on_gump(int x, int y, Game_object* to_drop, Gump* gump);
	bool drop_on_map(int x, int y, Game_object* to_drop);

public:
	friend class Game_window;
	// Create for dropping new object.
	Dragging_info(Game_object_shared newobj);
	Dragging_info(int x, int y);    // Create for given mouse position.
	bool moved(int x, int y);       // Mouse moved.
	void paint();                   // Paint object being dragged.
	bool drop(int x, int y);        // Drop obj. at given position.
	// Mouse button released.
	bool drop(int x, int y, bool moved);
};

#endif /* INCL_DRAG_H */
