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
#include "exult_flx.h"
#include "game.h"
#include "gamewin.h"
#include "istring.h"
#include "misc_buttons.h"
#include "palette.h"

#include <algorithm>

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
	if (is_draggable()
		&& button == MouseButton::Left)    //&&(has_point(mx, my))
	{
		auto dt = GetDragType();
		if (dt == DragType::Always
			|| (dt == DragType::Offscreen && isOffscreen())) {
#ifdef EXTRA_DEBUG
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
	if (is_draggable() && button == MouseButton::Left && drag_mx != INT_MIN
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
#ifdef EXTRA_DEBUG
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
	if (is_draggable() && drag_mx != INT_MIN && drag_my != INT_MIN) {
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

#ifdef EXTRA_DEBUG
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

void Modal_gump::SetProceduralBackground(
		TileRect backsize, int Checkbg_paletteramp, bool centre_gump_on_screen) {
	checkmark_background
			= ShapeID(EXULT_FLX_CHECKMARK_BACKGROUND_SHP, 0, SF_EXULT_FLX);

	if (Checkbg_paletteramp >= 0 && Checkbg_paletteramp <= 16)
	checkmark_background.set_palette_transform(
			PT_RampRemapAllFrom | Checkbg_paletteramp);
	

	procedural_background = backsize;
	// Enlarge by 2 for the 3d edge and black border
	procedural_background.enlarge(2);

	// create checkmark button
	// if don't already have one
	if (std::none_of(elems.begin(), elems.end(), [](auto elem) -> bool {
			return dynamic_cast<Checkmark_button*>(elem) != nullptr;
		})) {
		int checkx = backsize.x, checky = backsize.y;

		auto cmbshape = checkmark_background.get_shape();

		int cmbleft   = backsize.x - 2;
		int cmbbottom = backsize.y + backsize.h + 2;
		int cmbtop    = cmbbottom;
		int cmbright  = backsize.x;

		if (cmbshape) {
			cmbleft -= cmbshape->get_xleft();
			cmbbottom += cmbshape->get_ybelow();
			cmbtop -= cmbshape->get_yabove();
		} else {
			cmbleft -= 27;
			cmbbottom -= 6;
			cmbtop = cmbbottom - 40;
		}
		// Create button here so we ccan get it's shape for positioning
		auto checkmarkbutton = new Checkmark_button(this, 0, 0);
		auto checkshape      = checkmarkbutton->get_shape();
		int  csleft          = -20;
		int  cswidth         = 20;
		int  csheight        = 20;
		int  cstop           = -20;

		if (checkshape) {
			csleft   = -checkshape->get_xleft();
			cswidth  = checkshape->get_width();
			csheight = checkshape->get_height();
			cstop    = checkshape->get_yabove();
		}

		// Want to centre check on the cmbackground
		checkx = (cmbright + cmbleft - cswidth) / 2 - csleft;
		checky = (cmbbottom + cmbtop - csheight) / 2 + cstop;

		checkmarkbutton->set_pos(checkx, checky);
		elems.push_back(checkmarkbutton);
	}
	// Set colours
	procedural_colours = ProceduralColours();

	if (centre_gump_on_screen) {
		set_pos();
	}
}

void Modal_gump::paint() {
	if (procedural_background) {
		auto ib = gwin->get_win()->get_ibuf();

		TileRect rect = procedural_background;
		local_to_screen(rect.x, rect.y);

		checkmark_background.paint_shape(rect.x, rect.y + rect.h);
		checkmark_background.paint_shape(rect.x + rect.w, rect.y + rect.h);

		ib->draw_box(
				rect.x, rect.y, rect.w, rect.h, 1, 0xFF,
				procedural_colours.border);
		ib->draw_beveled_box(
				rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2, 1,
				procedural_colours.Background, procedural_colours.Highlight,
				procedural_colours.Highlight2, procedural_colours.Shadow,
				procedural_colours.Shadow);
	}

	Gump::paint();
}

TileRect Modal_gump::get_rect() const {
	if (!procedural_background) {
		return Gump::get_rect();
	} else {
		auto     cmbshape = checkmark_background.get_shape();
		TileRect ret      = procedural_background;
		if (cmbshape) {
			int cmbleft   = procedural_background.x;
			int cmbbottom = procedural_background.y + procedural_background.h;
			int cmbtop    = cmbbottom;
			int cmbright  = procedural_background.x;
			cmbleft -= cmbshape->get_xleft();
			cmbbottom += cmbshape->get_ybelow();
			cmbtop -= cmbshape->get_yabove();
			int      cmbwidth  = cmbright - cmbleft;
			int      cmbheight = cmbbottom - cmbtop;
			TileRect cmbounds(cmbleft, cmbtop, cmbwidth, cmbheight);

			ret = cmbounds.add(procedural_background);
		}
		// put it into sceenspace
		local_to_screen(ret.x, ret.y);
		return ret;
	}
}

void Modal_gump::ProceduralColours::RemapColours(int newramp) {
	if (newramp < 0) {
		return;
	}
	auto pal   = gwin->get_pal();
	Background = pal->remap_colour_to_ramp(Background, newramp);
	Highlight  = pal->remap_colour_to_ramp(Highlight, newramp);
	Highlight2 = pal->remap_colour_to_ramp(Highlight2, newramp);
	Shadow     = pal->remap_colour_to_ramp(Shadow, newramp);
}
