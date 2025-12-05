/*
 *  shapevga.h - Handle the 'shapes.vga' file and associated info.
 *
 *  Copyright (C) 1999  Jeffrey S. Freedman
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

#ifndef INCL_SHAPEVGA
#define INCL_SHAPEVGA 1

#include <fstream>
#include <iostream>
#include <map>
#include <vector>
// #include "autoarray.h"   +++++GOING AWAY.
#include "exult_constants.h"
#include "fnames.h"
#include "imagebuf.h"
#include "shapeinf.h"
#include "vgafile.h"

#define c_first_obj_shape 0x96    // 0-0x95 are 8x8 flat shapes.

/*
 *  The "shapes.vga" file:
 */
class Shapes_vga_file : public Vga_file {
	std::map<int, Shape_info> info;     // Extra info. about each shape.
	Shape_info                zinfo;    // A fake one (all 0's).
	bool                      info_read = false;    // True when info is set.
	void Read_Shapeinf_text_data_file(bool editing, Exult_Game game_type);
	void Read_Bodies_text_data_file(bool editing, Exult_Game game_type);
	void Read_Paperdoll_text_data_file(bool editing, Exult_Game game_type);
	void Read_Gumpinf_text_data_file(bool editing, Exult_Game game_type);
	void Write_Shapeinf_text_data_file(Exult_Game game_type);
	void Write_Bodies_text_data_file(Exult_Game game_type);
	void Write_Paperdoll_text_data_file(Exult_Game game_type);
	void Write_Gumpinf_text_data_file(Exult_Game game_type);

	friend class ExultStudio;

public:
	Shapes_vga_file() = default;
	Shapes_vga_file(const char* nm, int u7drag = -1, const char* nm2 = nullptr);
	void init();
	// Read additional data files.
	void reload_info(Exult_Game game);
	void fix_old_shape_info(Exult_Game game);
	// Returns true when some Shape data have been migrated.
	bool   read_info(Exult_Game game, bool editing = false);
	void   write_info(Exult_Game game);    // Write them back out.
	Shape* new_shape(int shapenum) override;

	Shape_info& get_info(int shapenum) {
		auto it = info.find(shapenum);
		if (it != info.end()) {
			return it->second;
		}
		return zinfo;
	}

	bool has_info(int shapenum) {
		auto it = info.find(shapenum);
		return it != info.end();
	}

	void set_info(int shapenum, const Shape_info& inf) {
		info[shapenum] = inf;
	}

	void copy_info(int shapenum, const Shape_info& inf) {
		info[shapenum].copy(inf, true);
	}
};

#endif
