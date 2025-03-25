/*
 *  miscinf.h - Information about several previously-hardcoded shape data.
 *
 *  Copyright (C) 2006-2022  The Exult Team
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

#ifndef MISCINF_H
#define MISCINF_H

#include "span.h"

#include <memory>
#include <string>
#include <vector>

class Actor;

struct Base_Avatar_info {
	int shape_num;
	int face_shape;    // Shape and frame for face during the
	int face_frame;    // normal game.
};

struct Avatar_default_skin {
	int  default_skin;      // The starting skin color.
	bool default_female;    // True if the default sex if female.
};

struct Skin_data {
	int  skin_id;
	int  shape_num;
	int  naked_shape;
	int  face_shape;          // Shape and frame for face during the
	int  face_frame;          // normal game.
	int  alter_face_shape;    // Shape and frame for face to be used
	int  alter_face_frame;    // when flag 33 is set.
	bool is_female;
	bool copy_info;    // Whether or not Exult should overwrite shape info
					   // with info from the default avatar shape
};

struct Usecode_function_data {
	int fun_id;
	int event_id;
};

class Shapeinfo_entry_parser;
struct Shapeinfo_data;
struct Avatar_data;

/*
 *  A class to get the extra information for a given shape.
 */
class Shapeinfo_lookup {
	using Readstrings = std::vector<std::string>;
	static Skin_data* ScrollSkins(
			int skin, bool sex, bool sishapes, bool ignoresex, bool prev,
			bool sel);

public:
	static void reset();

	static std::vector<std::pair<std::string, int>>* GetFacesSources();
	static std::vector<std::pair<std::string, int>>* GetPaperdollSources();
	static std::vector<std::pair<int, int>>*         GetImportedSkins();
	static std::vector<std::pair<int, int>>*         GetImportedGumpShapes();
	static int                                       GetBlueShapeData(int spot);
	static bool                                      IsSkinImported(int shape);

	static Base_Avatar_info* GetBaseAvInfo(bool sex);
	static int               GetMaleAvShape();
	static int               GetFemaleAvShape();

	static int GetNextSkin(
			int skin, bool sex, bool sishapes, bool ignoresex = false) {
		return (ScrollSkins(skin, sex, sishapes, ignoresex, false, false))
				->skin_id;
	}

	static int GetPrevSkin(
			int skin, bool sex, bool sishapes, bool ignoresex = false) {
		return (ScrollSkins(skin, sex, sishapes, ignoresex, true, false))
				->skin_id;
	}

	static Skin_data* GetNextSelSkin(
			Skin_data* sk, bool sishapes, bool ignoresex = false) {
		return ScrollSkins(
				sk->skin_id, sk->is_female, sishapes, ignoresex, false, true);
	}

	static Skin_data* GetPrevSelSkin(
			Skin_data* sk, bool sishapes, bool ignoresex = false) {
		return ScrollSkins(
				sk->skin_id, sk->is_female, sishapes, ignoresex, true, true);
	}

	static int                     GetNumSkins(bool sex);
	static Avatar_default_skin*    GetDefaultAvSkin();
	static std::vector<Skin_data>* GetSkinList();
	static Skin_data*              GetSkinInfo(int skin, bool sex);
	static Skin_data* GetSkinInfoSafe(int skin, bool sex, bool sishapes);
	static Skin_data* GetSkinInfoSafe(Actor* npc);
	static bool       IsSkinSelectable(int skin);
	static bool       HasFaceReplacement(int npcid);
	static int        GetFaceReplacement(int facenum);
	static Usecode_function_data* GetAvUsecode(int type);

	static int get_skinvar(const std::string& key);

private:
	static void Read_data_file(
			const char* fname, const tcb::span<std::string_view>& sections,
			const tcb::span<std::unique_ptr<Shapeinfo_entry_parser>>& parsers);
	static void                            setup_shape_files();
	static void                            setup_avatar_data();
	static std::unique_ptr<Shapeinfo_data> data;
	static std::unique_ptr<Avatar_data>    avdata;
};

#endif
