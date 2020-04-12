/*
Copyright (C) 2000-2013 The Exult Team

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
#  include <config.h>
#endif

#include "game.h"
#include "gamewin.h"
#include "Gump_button.h"
#include "Gump.h"
#include "misc_buttons.h"
#include "contain.h"
#include "objiter.h"
#include "Gump_manager.h"
#include "cheat.h"
#include "ignore_unused_variable_warning.h"

/*
 *  Create a gump.
 */

Gump::Gump(
    Container_game_object *cont,    // Container it represents.
    int initx, int inity,       // Coords. on screen.
    int shnum,          // Shape #.
    ShapeFile shfile
) : ShapeID(shnum, 0, shfile), container(cont), x(initx), y(inity),
	handles_kbd(false) {
	if (container){
		if (container->validGumpXY()){
			x = container->getGumpX();
			y = container->getGumpY();
		}
	}
}

/*
 *  Create, centered on screen.
 */

Gump::Gump(
    Container_game_object *cont,    // Container it represents.
    int shnum,          // Shape #.
    ShapeFile shfile
) : ShapeID(shnum, 0, shfile), container(cont),
	handles_kbd(false) {
	Shape_frame *shape = get_shape();
	x = (gwin->get_width() - shape->get_width()) / 2;
	y = (gwin->get_height() - shape->get_height()) / 2;
}

/*
 *  Create for cloning.
 */

Gump::Gump(
    Container_game_object *cont,
    int initx, int inity,
    Gump *from
) : ShapeID(from->get_shapenum(), from->get_framenum(),
	            from->get_shapefile()),
	container(cont), x(initx), y(inity),
	object_area(from->object_area), handles_kbd(false) {
	// Clone widgets.
	for (Gump_elems::iterator it = from->elems.begin();
	        it != from->elems.end(); ++it)
		add_elem((*it)->clone(this));
}

/*
 *  Delete gump.
 */

Gump::~Gump() {
	for (Gump_elems::iterator it = elems.begin(); it != elems.end(); ++it)
		delete *it;
	if (container) // Probabbly dont need to check.. but would crash the game if it was NULL.
		container->setGumpXY(x, y);
}

/*
 *   Set centered.
 */
void Gump::set_pos() {
	Shape_frame *shape = get_shape();
	x = (gwin->get_width() - shape->get_width()) / 2;
	y = (gwin->get_height() - shape->get_height()) / 2;
}

/*
 *  Sets object area and creates checkmark button
 */

void Gump::set_object_area(
    Rectangle const &area,
    int checkx,
    int checky
) {
	object_area = area;
	checkx += 16;
	checky -= 12;
	elems.push_back(new Checkmark_button(this, checkx, checky));
}

/*
 *  Get screen rectangle for one of our objects.
 */

Rectangle Gump::get_shape_rect(
    const Game_object *obj
) const {
	const Shape_frame *s = obj->get_shape();
	if (!s)
		return Rectangle(0, 0, 0, 0);
	return Rectangle(x + object_area.x + obj->get_tx() - s->get_xleft(),
	                 y + object_area.y + obj->get_ty() - s->get_yabove(),
	                 s->get_width(), s->get_height());
}

/*
 *  Get screen location of object within.
 */

void Gump::get_shape_location(
    const Game_object *obj,
    int &ox, int &oy
) const {
	ox = x + object_area.x + obj->get_tx();
	oy = y + object_area.y + obj->get_ty();
}

/*
 *  Find object a screen point is on.
 *
 *  Output: Object found, or null.
 */

Game_object *Gump::find_object(
    int mx, int my          // Mouse pos. on screen.
) {
	int cnt = 0;
	Game_object *list[100];
	if (!container)
		return nullptr;
	Object_iterator next(container->get_objects());
	Game_object *obj;
	Shape_frame *s;

	int ox;
	int oy;

	while ((obj = next.get_next()) != nullptr) {
		Rectangle box = get_shape_rect(obj);
		if (box.has_point(mx, my)) {
			s = obj->get_shape();
			get_shape_location(obj, ox, oy);
			if (s->has_point(mx - ox, my - oy))
				list[cnt++] = obj;
		}
	}
	// ++++++Return top item.
	return cnt ? list[cnt - 1] : nullptr;
}

/*
 *  Get the entire screen rectangle covered by this gump and its contents.
 */

Rectangle Gump::get_dirty(
) {
	Rectangle rect = get_rect();
	if (!container)
		return rect;
	Object_iterator next(container->get_objects());
	Game_object *obj;
	while ((obj = next.get_next()) != nullptr) {
		Rectangle orect = get_shape_rect(obj);
		rect = rect.add(orect);
	}
	return rect;
}

/*
 *  Get object this belongs to.
 */

Game_object *Gump::get_owner() {
	return container;
}

/*
 *  Is a given screen point on the checkmark?
 *
 *  Output: ->button if so.
 */

Gump_button *Gump::on_button(
    int mx, int my          // Point in window.
) {
	for (Gump_elems::iterator it = elems.begin(); it != elems.end(); ++it) {
		Gump_widget *w = *it;
		if (w->on_button(mx, my))
			return dynamic_cast<Gump_button *>(w);
	}
	return nullptr;
}

/*
 *  Add an object.  If mx, my, sx, sy are all -1, the object's position
 *  is calculated by 'paint()'.  If they're all -2, it's assumed that
 *  obj->tx, obj->ty are already correct.
 *
 *  Output: false if cannot add it.
 */

bool Gump::add(
    Game_object *obj,
    int mx, int my,         // Mouse location.
    int sx, int sy,         // Screen location of obj's hotspot.
    bool dont_check,        // Skip volume check.
    bool combine            // True to try to combine obj.  MAY
    //   cause obj to be deleted.
) {
	ignore_unused_variable_warning(combine);
	if (!container || (!cheat.in_hack_mover() &&
	                   !dont_check && !container->has_room(obj)))
		return false;     // Full.
	// Dropping on same thing?
	Game_object *onobj = find_object(mx, my);
	// If possible, combine.

	if (onobj && onobj != obj && onobj->drop(obj))
		return true;

	if (!container->add(obj, dont_check))   // DON'T combine here.
		return false;

	// Not a valid spot?
	if (sx == -1 && sy == -1 && mx == -1 && my == -1)
		// Let paint() set spot.
		obj->set_shape_pos(255, 255);
	// -2's mean tx, ty are already set.
	else if (sx != -2 && sy != -2 && mx != -2 && my != -2) {
		// Put it where desired.
		sx -= x + object_area.x;// Get point rel. to object_area.
		sy -= y + object_area.y;
		Shape_frame *shape = obj->get_shape();
		// But shift within range.
		if (sx - shape->get_xleft() < 0)
			sx = shape->get_xleft();
		else if (sx + shape->get_xright() > object_area.w)
			sx = object_area.w - shape->get_xright();
		if (sy - shape->get_yabove() < 0)
			sy = shape->get_yabove();
		else if (sy + shape->get_ybelow() > object_area.h)
			sy = object_area.h - shape->get_ybelow();
		obj->set_shape_pos(sx, sy);
	}
	return true;
}

/*
 *  Remove object.
 */

void Gump::remove(
    Game_object *obj
) {
	container->remove(obj);

	// Paint Objects
	Rectangle box = object_area;    // Paint objects inside.
	box.shift(x, y);        // Set box to screen location.


	gwin->set_all_dirty();
	gwin->paint_dirty();
}

/*
 *  Paint all elems.
 */

void Gump::paint_elems(
) {
	for (Gump_elems::iterator it = elems.begin(); it != elems.end(); ++it)
		(*it)->paint();
}

void check_elem_positions(Object_list &objects)
{
	int prevx = -1;
	int prevy = -1;
	Game_object *obj;
	Object_iterator next(objects);
	// See if all have the same position, indicating from a new game.
	while ((obj = next.get_next()) != nullptr) {
		int tx = obj->get_tx();
		int ty = obj->get_ty();
	    if (prevx == -1) {
		    prevx = tx;
			prevy = ty;
		} else if (tx != prevx || ty != prevy) {
		    return;
		}
	}
	next.reset();
	while ((obj = next.get_next()) != nullptr) {
	    obj->set_shape_pos(255, 255);
	}
}

/*
 *  Paint on screen.
 */

void Gump::paint(
) {
	// Paint the gump itself.
	paint_shape(x, y);
	gwin->set_painted();

	// Paint red "checkmark".
	paint_elems();

	if (!container)
		return;         // Empty.
	Object_list &objects = container->get_objects();
	if (objects.is_empty())
		return;         // Empty.
	Rectangle box = object_area;    // Paint objects inside.
	box.shift(x, y);        // Set box to screen location.
	int cury = 0;
	int curx = 0;
	int endy = box.h;
	int endx = box.w;
	int loop = 0;           // # of times covering container.
	check_elem_positions(objects);	// Set to place if new game.
	Game_object *obj;
	Object_iterator next(objects);
	while ((obj = next.get_next()) != nullptr) {
		Shape_frame *shape = obj->get_shape();
		if (!shape)
			continue;
		int objx = obj->get_tx() - shape->get_xleft() +
		           1 + object_area.x;
		int objy = obj->get_ty() - shape->get_yabove() +
		           1 + object_area.y;
		// Does obj. appear to be placed?
		if (!object_area.has_point(objx, objy) ||
		        !object_area.has_point(objx + shape->get_xright() - 1,
		                               objy + shape->get_ybelow() - 1)) {
			// No.
			int px = curx + shape->get_width();
			int py = cury + shape->get_height();
			if (px > endx)
				px = endx;
			if (py > endy)
				py = endy;
			obj->set_shape_pos(px - shape->get_xright(),
			                   py - shape->get_ybelow());
			// Mostly avoid overlap.
			curx += shape->get_width() - 1;
			if (curx >= endx) {
				cury += 8;
				curx = 0;
				if (cury >= endy)
					cury = 2 * (++loop);
			}
		}
		obj->paint_shape(box.x + obj->get_tx(), box.y + obj->get_ty());
		obj = obj->get_next();
	}
	// Outline selections in this gump.
	const Game_object_shared_vector &sel = cheat.get_selected();
	for (Game_object_shared_vector::const_iterator it = sel.begin();
	        it != sel.end(); ++it) {
		Game_object *obj = (*it).get();
		if (container == obj->get_owner()) {
			int x;
			int y;
			get_shape_location(obj, x, y);
			obj->ShapeID::paint_outline(x, y, HIT_PIXEL);
		}
	}
}

/*
 *  Close and delete.
 */

void Gump::close(
) {
	gumpman->close_gump(this);
}

/*
 *  Does the gump have this spot
 */
bool Gump::has_point(int sx, int sy) const {
	Shape_frame *s = get_shape();

	return s && s->has_point(sx - x, sy - y);
}

/*
 *  Get screen area used by a gump.
 */

Rectangle Gump::get_rect() const {
	Shape_frame *s = get_shape();

	if (!s) return Rectangle(0, 0, 0, 0);

	return Rectangle(x - s->get_xleft(),    y - s->get_yabove(),
	                 s->get_width(), s->get_height());
}


/*
 *  Container_gump Initialize
 */

void Container_gump::initialize(
    int shnum
) {
	if (shnum == game->get_shape("gumps/box")) {
		set_object_area(Rectangle(46, 28, 74, 32), 8, 56);
	} else if (shnum == game->get_shape("gumps/crate")) {
		set_object_area(Rectangle(50, 20, 80, 24), 8, 64);
	} else if (shnum == game->get_shape("gumps/barrel")) {
		set_object_area(Rectangle(32, 32, 40, 40), 12, 124);
	} else if (shnum == game->get_shape("gumps/bag")) {
		set_object_area(Rectangle(48, 20, 66, 44), 8, 66);
	} else if (shnum == game->get_shape("gumps/backpack")) {
		set_object_area(Rectangle(36, 36, 85, 40), 8, 62);
	} else if (shnum == game->get_shape("gumps/basket")) {
		set_object_area(Rectangle(42, 32, 70, 26), 8, 56);
	} else if (shnum == game->get_shape("gumps/chest")) {
		set_object_area(Rectangle(40, 18, 60, 37), 8, 46);
	} else if (shnum == game->get_shape("gumps/shipshold")) {
		set_object_area(Rectangle(38, 10, 82, 80), 8, 92);
	} else if (shnum == game->get_shape("gumps/drawer")) {
		set_object_area(Rectangle(36, 12, 70, 26), 8, 46);
	} else if (shnum == game->get_shape("gumps/tree")) {
		set_object_area(Rectangle(62, 22, 36, 44), 9, 100);
	} else if (shnum == game->get_shape("gumps/body")) {
		set_object_area(Rectangle(36, 46, 84, 40), 8, 70);
	} else
		set_object_area(Rectangle(52, 22, 60, 40), 8, 64);
}

