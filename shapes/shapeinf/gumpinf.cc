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

// Storage for gump info - static member
std::map<int, Gump_info> Gump_info::gump_info_map;
bool                     Gump_info::any_modified = false;

Gump_info::Gump_info()
		: container_from_patch(false), checkmark_from_patch(false),
		  special_from_patch(false), container_modified(false),
		  checkmark_modified(false), special_modified(false), container_x(0),
		  container_y(0), container_w(0), container_h(0), checkmark_x(0),
		  checkmark_y(0), checkmark_shape(0), has_area(false),
		  has_checkmark(false), is_checkmark(false), is_special(false) {}

const Gump_info* Gump_info::get_gump_info(int shapenum) {
	auto it = gump_info_map.find(shapenum);
	return it != gump_info_map.end() ? &it->second : nullptr;
}

Gump_info& Gump_info::get_or_create_gump_info(int shapenum) {
	return gump_info_map[shapenum];
}

void Gump_info::clear() {
	gump_info_map.clear();
	any_modified = false;
}