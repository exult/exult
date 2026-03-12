/*
 *  Copyright (C) 2009-2011 Malignant Manor
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
 *  Author: Malignant Manor
 */

void NS_door shape#(162)() // doors that open north or south
{
	var frameoff;
	var distance;
	var otherdoor;
	var Xoff = - 3;
	var Yoff = 3;
	var OPENDOOR = EW_DOOR;
	var FRAME = get_item_frame();
	var pos = get_object_position();

	if (FRAME % 2 == 0)
		frameoff = 1;
	else
		frameoff = -1;
	if (FRAME < 4 || FRAME %16 == 0 || FRAME %17 == 0 || FRAME %18 == 0 || FRAME %19 == 0)
		distance = 5; // closed
	else
		distance = 8; // open
	otherdoor = UI_find_nearby([pos[X], pos[Y], pos[Z], QUALITY_ANY, FRAME + frameoff], get_item_shape(), distance, 0);
	var otherframe = UI_get_item_frame(otherdoor);
	var otherpos = UI_get_object_position(otherdoor);

// CLOSED	
	if (FRAME == 0 || FRAME %16 == 0) // opens north (west side hinge)	a
	{
		if (otherdoor) // opens north (east side hinge)	b
		{
			UI_set_item_frame(otherdoor, otherframe + 12);
			UI_set_item_shape(otherdoor, OPENDOOR);
		}
		move_object([pos[X] + Xoff, pos[Y]]);
		set_item_frame(FRAME + 12);
		set_item_shape(OPENDOOR);
	}
	else if (FRAME == 1 || FRAME %17 == 0) // opens north (east side hinge)	b
	{
		if (otherdoor) // opens north (west side hinge)	a
		{
			UI_move_object(otherdoor, [otherpos[X] + Xoff, otherpos[Y]]);
			UI_set_item_frame(otherdoor, otherframe + 12);
			UI_set_item_shape(otherdoor, OPENDOOR);
		}
		set_item_frame(FRAME + 12);
		set_item_shape(OPENDOOR);
	}
	else if (FRAME == 2 || FRAME %18 == 0) // opens south (west side hinge)	c
	{
		if (otherdoor) //opens south (east side hinge)	d
		{
			UI_move_object(otherdoor, [otherpos[X], otherpos[Y] + Yoff]);
			UI_set_item_frame(otherdoor, otherframe + 12);
			UI_set_item_shape(otherdoor, OPENDOOR);
		}
		move_object([pos[X] + Xoff, pos[Y] + Yoff]);
		set_item_frame(FRAME + 12);
		set_item_shape(OPENDOOR);
	}
	else if (FRAME == 3 || FRAME %19 == 0) // opens south (east side hinge)	d
	{
		if (otherdoor) // opens south (west side hinge)	c
		{
			UI_move_object(otherdoor, [otherpos[X] + Xoff, otherpos[Y] + Yoff]);
			UI_set_item_frame(otherdoor, otherframe + 12);
			UI_set_item_shape(otherdoor, OPENDOOR);
		}
		move_object([pos[X], pos[Y] + Yoff]);
		set_item_frame(FRAME + 12);
		set_item_shape(OPENDOOR);
	}
// locked and magic locked frames are for doors a, b, c, and d
// LOCKED		maybe have a special sfx or message for the locked frames
	else if (FRAME in [4, 5, 6, 7] || FRAME %20 == 0 || FRAME %21 == 0 || FRAME %22 == 0 || FRAME %23 == 0)
		AVATAR.say("The door is locked");
// MAGICALLY LOCKED
	else if (FRAME in [8, 9, 10, 11] || FRAME %24 == 0 || FRAME %25 == 0 || FRAME %26 == 0 || FRAME %27 == 0)
		AVATAR.say("The door is magic locked");
	else if (FRAME == 12 || FRAME %28 == 0) //  closes west (north side hinge)	e
	{
		if (otherdoor) // closes west (south side hinge)	f
		{
			UI_move_object(otherdoor, [otherpos[X] + Xoff, otherpos[Y]]);
			UI_set_item_frame(otherdoor, otherframe - 12);
			UI_set_item_shape(otherdoor, OPENDOOR);
		}
		move_object([pos[X] + Xoff, pos[Y] + Yoff]);
		set_item_frame(FRAME - 12);
		set_item_shape(OPENDOOR);
	}
	else if (FRAME == 13 || FRAME %29 == 0) // closes west (south side hinge)	f
	{
		if (otherdoor) // closes west (north side hinge)	e
		{
			UI_move_object(otherdoor, [otherpos[X] + Xoff, otherpos[Y] + Yoff]);
			UI_set_item_frame(otherdoor, otherframe - 12);
			UI_set_item_shape(otherdoor, OPENDOOR);
		}
		move_object([pos[X] + Xoff, pos[Y]]);
		set_item_frame(FRAME - 12);
		set_item_shape(OPENDOOR);
	}
	else if (FRAME == 14 || FRAME %30 == 0) // closes east (north side hinge)	g
	{
		if (otherdoor) // closes east (south side hinge)	h
		{
			UI_set_item_frame(otherdoor, otherframe - 12);
			UI_set_item_shape(otherdoor, OPENDOOR);
		}
		move_object([pos[X], pos[Y] + Yoff]);
		set_item_frame(FRAME - 12);
		set_item_shape(OPENDOOR);
	}
	else if (FRAME == 15 || FRAME %31 == 0) // closes east (south side hinge)	h
	{
		if (otherdoor) // closes east (north side hinge)	g
		{
			UI_move_object(otherdoor, [otherpos[X], otherpos[Y] + Yoff]);
			UI_set_item_frame(otherdoor, otherframe - 12);
			UI_set_item_shape(otherdoor, OPENDOOR);
		}
		set_item_frame(FRAME - 12);
		set_item_shape(OPENDOOR);
	}
}

void EW_door shape#(163)() // Doors that open east or west
{
	var frameoff;
	var distance;
	var Xoff = 3;
	var Yoff = - 3;
	var OPENDOOR = NS_DOOR;
	var FRAME = get_item_frame();
	var pos = get_object_position();

	if (FRAME % 2 == 0)
		frameoff = 1;
	else
		frameoff = -1;
	if (FRAME < 4 || FRAME %16 == 0 || FRAME %17 == 0 || FRAME %18 == 0 || FRAME %19 == 0)
		distance = 4; //closed
	else
		distance = 7; // open
	var otherdoor = UI_find_nearby([pos[X], pos[Y], pos[Z], QUALITY_ANY, FRAME + frameoff], get_item_shape(), distance, 0);
	var otherframe = UI_get_item_frame(otherdoor);
	var otherpos = UI_get_object_position(otherdoor);

// CLOSED	
	if (FRAME == 0 || FRAME %16 == 0) // opens east (north side hinge)	e
	{
		if (otherdoor) // opens east (south side hinge)	f
		{
			UI_move_object(otherdoor, [otherpos[X] + Xoff, otherpos[Y]]);
			UI_set_item_frame(otherdoor, otherframe + 12);
			UI_set_item_shape(otherdoor, OPENDOOR);
		}
		move_object([pos[X] + Xoff, pos[Y] + Yoff]);
		set_item_frame(FRAME + 12);
		set_item_shape(OPENDOOR);
	}
	else if (FRAME == 1 || FRAME %17 == 0) // opens east (south side hinge)	f
	{
		if (otherdoor) // opens north (west side hinge)
		{
			UI_move_object(otherdoor, [otherpos[X] + Xoff, otherpos[Y] + Yoff]);
			UI_set_item_frame(otherdoor, otherframe + 12);
			UI_set_item_shape(otherdoor, OPENDOOR);
		}
		move_object([pos[X] + Xoff, pos[Y]]);
		set_item_frame(FRAME + 12);
		set_item_shape(OPENDOOR);
	}
	else if (FRAME == 2 || FRAME %18 == 0) // opens west (north side hinge)	g
	{
		if (otherdoor) // opens west (south side hinge)	h
		{
			UI_set_item_frame(otherdoor, otherframe + 12);
			UI_set_item_shape(otherdoor, OPENDOOR);
		}
		move_object([pos[X], pos[Y] + Yoff]);
		set_item_frame(FRAME + 12);
		set_item_shape(OPENDOOR);
	}
	else if (FRAME == 3 || FRAME %19 == 0) // opens west (south side hinge)	h
	{
		if (otherdoor) // opens west (north side hinge)	g
		{
			UI_move_object(otherdoor, [otherpos[X], otherpos[Y] + Yoff]);
			UI_set_item_frame(otherdoor, otherframe + 12);
			UI_set_item_shape(otherdoor, OPENDOOR);
		}
		set_item_frame(FRAME + 12);
		set_item_shape(OPENDOOR);
	}
// locked and magic locked frames are for doors are for e, f, g, and h
// LOCKED		maybe have a special sfx or message for the locked frames
	else if (FRAME in [4, 5, 6, 7] || FRAME %20 == 0 || FRAME %21 == 0 || FRAME %22 == 0 || FRAME %23 == 0)
		AVATAR.say("The door is locked");
// MAGICALLY LOCKED
	else if (FRAME in [8, 9, 10, 11] || FRAME %24 == 0 || FRAME %25 == 0 || FRAME %26 == 0 || FRAME %27 == 0)
		AVATAR.say("The door is magic locked");
	else if (FRAME == 12 || FRAME %28 == 0) // closes south (south west hinge)	a
	{
		if (otherdoor) //  closes south (south east hinge)	b
		{
			move_object([pos[X] + Xoff, pos[Y]]);
			UI_set_item_frame(otherdoor, otherframe - 12);
			UI_set_item_shape(otherdoor, OPENDOOR);
		}
		move_object([pos[X] + Xoff, pos[Y]]);
		set_item_frame(FRAME - 12);
		set_item_shape(OPENDOOR);
	}
	else if (FRAME == 13 || FRAME %29 == 0) //  closes south (south east hinge)	b
	{
		if (otherdoor) // closes south (south west hinge)		a
		{
			UI_move_object(otherdoor, [otherpos[X] + Xoff, otherpos[Y]]);
			UI_set_item_frame(otherdoor, otherframe - 12);
			UI_set_item_shape(otherdoor, OPENDOOR);
		}
		set_item_frame(FRAME - 12);
		set_item_shape(OPENDOOR);
	}
	else if (FRAME == 14 || FRAME %30 == 0) // closes north (north west side hinge)	c
	{
		if (otherdoor) // closes north (north east side hinge)	d
		{
			UI_move_object(otherdoor, [otherpos[X], otherpos[Y] + Yoff]);
			UI_set_item_frame(otherdoor, otherframe - 12);
			UI_set_item_shape(otherdoor, OPENDOOR);
		}
		move_object([pos[X] + Xoff, pos[Y] + Yoff]);
		set_item_frame(FRAME - 12);
		set_item_shape(OPENDOOR);
	}
	else if (FRAME == 15 || FRAME %31 == 0) // closes north (north east side hinge)	d
	{
		if (otherdoor) // closes north (north west side hinge)	c
		{
			UI_move_object(otherdoor, [otherpos[X] + Xoff, otherpos[Y] + Yoff]);
			UI_set_item_frame(otherdoor, otherframe - 12);
			UI_set_item_shape(otherdoor, OPENDOOR);
		}
		move_object([pos[X], pos[Y] + Yoff]);
		set_item_frame(FRAME - 12);
		set_item_shape(OPENDOOR);
	}
}
