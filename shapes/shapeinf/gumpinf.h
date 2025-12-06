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

#ifndef GUMPINFO_H
#define GUMPINFO_H

#include <map>

class Gump_info {
	static std::map<int, Gump_info> gump_info_map;

	friend class Shapes_vga_file;

public:
	int  container_x;
	int  container_y;
	int  container_w;
	int  container_h;
	int  checkmark_x;
	int  checkmark_y;
	int  checkmark_shape;
	bool has_area;
	bool has_checkmark;
	bool is_checkmark;
	bool is_special;

	Gump_info();

	static const Gump_info* get_gump_info(int shapenum);
	static Gump_info&       get_or_create_gump_info(int shapenum);
	static void             clear();
};

#endif