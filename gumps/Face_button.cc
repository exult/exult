/*
Copyright (C) 2001-2022 The Exult Team

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

#include "Face_button.h"

#include "Paperdoll_gump.h"
#include "actors.h"
#include "gamewin.h"
#include "npcdollinf.h"

Face_button::Face_button(Gump* par, int px, int py, Actor* a)
		: Gump_button(par, 0, px, py), actor(a) {
	const Paperdoll_npc* npcinfo = a->get_info().get_npc_paperdoll();

	if (!npcinfo) {
		const Shape_info& inf
				= ShapeID::get_info(a->get_sexed_coloured_shape());
		npcinfo = inf.get_npc_paperdoll();
	}
	if (!npcinfo) {
		const Shape_info& inf = ShapeID::get_info(a->get_shape_real());
		npcinfo = inf.get_npc_paperdoll_safe(a->get_type_flag(Actor::tf_sex));
	}

	set_shape(npcinfo->get_head_shape());
	set_frame(npcinfo->get_head_frame());
	translucent = npcinfo->is_translucent();
	set_file(SF_PAPERDOL_VGA);
}

void Face_button::double_clicked(int x, int y) {
	ignore_unused_variable_warning(x, y);
	actor->show_inventory();
}

void Face_button::paint() {
	int px = 0;
	int py = 0;

	if (parent) {
		px = parent->get_x();
		py = parent->get_y();
	}
	paint_shape(x + px, y + py, translucent);
}

void Face_button::update_widget() {
	const Paperdoll_npc* npcinfo = actor->get_info().get_npc_paperdoll();

	if (!npcinfo) {
		const Shape_info& inf
				= ShapeID::get_info(actor->get_sexed_coloured_shape());
		npcinfo = inf.get_npc_paperdoll();
	}
	if (!npcinfo) {
		const Shape_info& inf = ShapeID::get_info(actor->get_shape_real());
		npcinfo               = inf.get_npc_paperdoll_safe(
                actor->get_type_flag(Actor::tf_sex));
	}

	if (get_shapenum() != npcinfo->get_head_shape()
		|| get_framenum() != npcinfo->get_head_frame()) {
		gwin->add_dirty(get_rect());
		set_shape(npcinfo->get_head_shape());
		set_frame(npcinfo->get_head_frame());
		gwin->add_dirty(get_rect());
	}
}
