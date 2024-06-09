/*
Copyright (C) 2024 The Exult Team

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

#include "Modal_gump.h"

#include "../conf/Configuration.h"
#include "istring.h"

Modal_gump::Modal_gump(
		Container_game_object* cont, int initx, int inity, int shnum,
		ShapeFile shfile)
		: Gump(cont, initx, inity, shnum, shfile), done(false), pushed(nullptr),
		  drag_mx(INT_MIN), drag_my(INT_MIN), no_dragging(false) {
	GetDragType();
}

// Create centered.

Modal_gump::Modal_gump(Container_game_object* cont, int shnum, ShapeFile shfile)
		: Gump(cont, shnum, shfile), done(false), pushed(nullptr),
		  drag_mx(INT_MIN), drag_my(INT_MIN), no_dragging(false) {
	GetDragType();
}

bool Modal_gump::mouse_down(int mx, int my, MouseButton button) {
	if (!no_dragging && button == MouseButton::Left)    //&&(has_point(mx, my))
	{
		auto dt = GetDragType();
		if (dt == DragType::Always
			|| (dt == DragType::Offscreen && isOffscreen())) {
#ifdef DEBUG
			std::cout << "start draggging modal gump " << mx << " " << my
					  << std::endl;
#endif
			drag_mx    = mx;
			drag_my    = my;
			lastgood_x = get_x();

			lastgood_y = get_y();
		}
	}
	return false;
}

bool Modal_gump::mouse_up(int mx, int my, MouseButton button) {
	if (!no_dragging && button == MouseButton::Left && drag_mx != INT_MIN
		&& drag_my != INT_MIN) {
		int delta_x = mx - drag_mx;
		int delta_y = my - drag_my;
		set_pos(get_x() + delta_x, get_y() + delta_y);
		if (isOffscreen(false)) {
			set_pos(lastgood_x, lastgood_y);

			std::cout << "modal gump dragged off screen returning to "
					  << lastgood_x << " " << lastgood_y << std::endl;
		} else {
			lastgood_x = get_x();
			lastgood_y = get_y();
		}
#ifdef DEBUG
		std::cout << "stop draggging modal gump " << mx << " " << my << " "
				  << delta_x << " " << delta_y << std::endl;
#endif
		drag_mx = INT_MIN;
		drag_my = INT_MIN;
		return true;
	}
	return false;
}

bool Modal_gump::mouse_drag(int mx, int my) {
	if (!no_dragging && drag_mx != INT_MIN && drag_my != INT_MIN) {
		int delta_x = mx - drag_mx;
		int delta_y = my - drag_my;
		set_pos(get_x() + delta_x, get_y() + delta_y);
		if (isOffscreen(false)) {
			set_pos(lastgood_x, lastgood_y);
			std::cout << "modal gump dragged off screen returning to "
					  << lastgood_x << " " << lastgood_y << std::endl;
		} else if (!isOffscreen(true)) {
			lastgood_x = get_x();
			lastgood_y = get_y();
		}
		drag_mx = mx;
		drag_my = my;

#ifdef DEBUG
		std::cout << "draggging modal gump " << mx << " " << my << " "
				  << delta_x << " " << delta_y << std::endl;
#endif
		return true;
	}
	return false;
}

static Modal_gump::DragType dragType = Modal_gump::DragType::Unknown;

Modal_gump::DragType Modal_gump::GetDragType() {
	if (dragType == DragType::Unknown) {
		// Read from config, default to offscreen
		dragType = DragType::Offscreen;
		std::string value;
		config->value(
				"config/gameplay/modal_gump_dragging", value, "offscreen");
		SetDragType(
				!Pentagram::strcasecmp(value.c_str(), "always")
						? DragType::Always
				: !Pentagram::strcasecmp(value.c_str(), "never")
						? DragType::Never
						: DragType::Offscreen);
	}

	return dragType;
}

void Modal_gump::SetDragType(DragType newtype) {
	// set new value and write to config

	dragType = newtype;
	std::string value;

	if (dragType == DragType::Always) {
		value = "Always";
	} else if (dragType == DragType::Never) {
		value = "never";
	} else {
		value = "Offscreen";
	}
	config->set("config/gameplay/modal_gump_dragging", value, true);
}
