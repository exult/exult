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

#include "gumpinf.h"

#include <map>

// Storage for gump info - static members
std::map<int, Gump_info>                Gump_info::gump_info_map;
std::map<int, Shortcutbar_icon_info>    Gump_info::shortcutbar_icon_map;
std::map<int, Shortcutbar_action_entry> Gump_info::shortcutbar_action_map;
std::map<int, List_layout>              Gump_info::list_layout_map;
bool                                    Gump_info::any_modified = false;

void List_layout::overlay(const List_layout& src) {
	if (src.has_part(LISTLAYOUT_OFF)) {
		off = off || src.off;
	}
	if (src.has_part(LISTLAYOUT_BACKGROUND)) {
		bg_shape  = src.bg_shape;
		bg_width  = src.bg_width;
		bg_height = src.bg_height;
		strip_x0  = src.strip_x0;
		strip_x1  = src.strip_x1;
		extra_w   = src.extra_w;
	}
	if (src.has_part(LISTLAYOUT_LIST)) {
		list = src.list;
	}
	if (src.has_part(LISTLAYOUT_TITLE)) {
		title_x    = src.title_x;
		title_y    = src.title_y;
		title_w    = src.title_w;
		title_font = src.title_font;
	}
	if (src.has_part(LISTLAYOUT_DIVIDER)) {
		divider_x  = src.divider_x;
		divider_y0 = src.divider_y0;
		divider_y1 = src.divider_y1;
	}
	if (src.has_part(LISTLAYOUT_PANEL)) {
		panel_x          = src.panel_x;
		panel_y          = src.panel_y;
		panel_w          = src.panel_w;
		panel_font       = src.panel_font;
		panel_small_font = src.panel_small_font;
	}
	if (src.has_part(LISTLAYOUT_PREVIEW)) {
		preview_x = src.preview_x;
		preview_y = src.preview_y;
		preview_w = src.preview_w;
		preview_h = src.preview_h;
	}
	if (src.has_part(LISTLAYOUT_PICTURE)) {
		pic_right = src.pic_right;
		pic_top   = src.pic_top;
	}
	if (src.has_part(LISTLAYOUT_NAV)) {
		nav_left_x  = src.nav_left_x;
		nav_right_x = src.nav_right_x;
		nav_y       = src.nav_y;
	}
	if (src.has_part(LISTLAYOUT_USECODE)) {
		open_fn = src.open_fn;
	}
	parts |= src.parts;
}

Gump_info::Gump_info()
		: container_from_patch(false), checkmark_from_patch(false), special_from_patch(false), snapzones_from_patch(false),
		  dynbuttons_from_patch(false), dyntexts_from_patch(false), dynshapes_from_patch(false), dynsliders_from_patch(false),
		  dynlists_from_patch(false), gumpname_from_patch(false), container_modified(false), checkmark_modified(false),
		  special_modified(false), snapzones_modified(false), dynbuttons_modified(false), dyntexts_modified(false),
		  dynshapes_modified(false), dynsliders_modified(false), dynlists_modified(false), gumpname_modified(false), container_x(0),
		  container_y(0), container_w(0), container_h(0), debug_flags(0), checkmark_x(0), checkmark_y(0), checkmark_shape(0),
		  has_area(false), has_checkmark(false), is_checkmark(false), is_special(false) {}

const Gump_info* Gump_info::get_gump_info(int shapenum) {
	auto it = gump_info_map.find(shapenum);
	return it != gump_info_map.end() ? &it->second : nullptr;
}

Gump_info& Gump_info::get_or_create_gump_info(int shapenum) {
	return gump_info_map[shapenum];
}

void Gump_info::clear() {
	gump_info_map.clear();
	shortcutbar_icon_map.clear();
	shortcutbar_action_map.clear();
	list_layout_map.clear();
	any_modified = false;
}

List_layout Gump_info::get_list_layout(int gump_shape) {
	List_layout layout;
	auto        it = list_layout_map.find(-1);
	if (it != list_layout_map.end()) {
		layout.overlay(it->second);
	}
	if (gump_shape >= 0) {
		it = list_layout_map.find(gump_shape);
		if (it != list_layout_map.end()) {
			layout.overlay(it->second);
		}
	}
	layout.list.source = 0;    // The list view always lists the contents.
	return layout;
}

List_layout& Gump_info::get_or_create_list_layout(int key) {
	return list_layout_map[key];
}

const Shortcutbar_icon_info* Gump_info::get_shortcutbar_icon(int slot) {
	auto it = shortcutbar_icon_map.find(slot);
	return it != shortcutbar_icon_map.end() ? &it->second : nullptr;
}

const std::map<int, Shortcutbar_icon_info>& Gump_info::get_shortcutbar_icons() {
	return shortcutbar_icon_map;
}

const Shortcutbar_action_entry* Gump_info::get_shortcutbar_action(int slot) {
	auto it = shortcutbar_action_map.find(slot);
	return it != shortcutbar_action_map.end() && it->second.valid ? &it->second : nullptr;
}