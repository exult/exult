/*
 *  miscinf.cc - Information about several previously-hardcoded shape data.
 *
 *  Copyright (C) 2006  The Exult Team
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "exult_constants.h"
#include "miscinf.h"
#include "game.h"
#include "utils.h"
#include "msgfile.h"
#include "U7file.h"
#include "databuf.h"
#include "shapeid.h"
#include "gamewin.h"
#include "ucmachine.h"
#include "actors.h"
#include "ignore_unused_variable_warning.h"

#include <sstream>
#include <fstream>
#include <map>
#include <vector>
#include <iostream>
#include <iomanip>
#include <string>

using std::vector;
using std::ifstream;
using std::map;
using std::multimap;
using std::pair;
using std::string;
using std::stringstream;
using std::skipws;

using namespace std;

static vector<pair<string, int> > *paperdoll_source_table = 0;
static vector<pair<int, int> > *imported_gump_shapes = 0;
static vector<pair<int, int> > *blue_shapes = 0;
static vector<pair<int, int> > *imported_skin_shapes = 0;
static map<string, int> *gumpvars = 0;
static map<string, int> *skinvars = 0;

static map<bool, Base_Avatar_info> *def_av_info = 0;
static Avatar_default_skin *base_av_info = 0;
static vector<Skin_data> *skins_table = 0;
static map<int, bool> *unselectable_skins = 0;
static map<int, int> *petra_table = 0;
static map<int, Usecode_function_data> *usecode_funs = 0;
static int last_skin = 0;

// HACK. NPC Paperdolls need this, but miscinf has too many
// Exult-dependant stuff to be included in ES. Thus, ES
// defines a non-operant version of this.
// Maybe we should do something about this...
int get_skinvar(std::string key) {
	return Shapeinfo_lookup::get_skinvar(key);
}

/*
 *  Base parser class shape data.
 */
class Shapeinfo_entry_parser {
public:
	virtual ~Shapeinfo_entry_parser() { }
	virtual void parse_entry(int index, istream &src,
	                         bool for_patch, int version) = 0;
	int ReadInt(istream &src, int off = 1) {
		src.ignore(off);
		int ret;
		if (src.peek() == '0') {
			src.ignore(1);
			char chr = src.peek();
			if (chr == 'x' || chr == 'X') {
				src.ignore(1);
				src >> hex;
			} else {
				src.unget();
			}
		}
		src >> ret >> skipws >> dec;
		return ret;
	}
	string ReadStr(istream &src, int off = 1) {
		src.ignore(off);
		string ret;
		getline(src, ret, '/');
		src.unget();
		return ret;
	}
};

class Int_pair_parser: public Shapeinfo_entry_parser {
	map<int, int> *table;
public:
	Int_pair_parser(map<int, int> *tbl)
		: table(tbl)
	{  }
	virtual void parse_entry(int index, istream &src,
	                         bool for_patch, int version) {
		ignore_unused_variable_warning(index, for_patch, version);
		int key = ReadInt(src, 0);
		int data = ReadInt(src);
		(*table)[key] = data;
	}
};

class Bool_parser: public Shapeinfo_entry_parser {
	map<int, bool> *table;
public:
	Bool_parser(map<int, bool> *tbl)
		: table(tbl)
	{  }
	virtual void parse_entry(int index, istream &src,
	                         bool for_patch, int version) {
		ignore_unused_variable_warning(index, for_patch, version);
		int key = ReadInt(src, 0);
		(*table)[key] = true;
	}
};

class Shape_imports_parser: public Shapeinfo_entry_parser {
	vector<pair<int, int> > *table;
	map<string, int> *shapevars;
	int shape;
public:
	Shape_imports_parser
	(vector<pair<int, int> > *tbl, map<string, int> *sh)
		: table(tbl), shapevars(sh), shape(c_max_shapes)
	{  }
	virtual void parse_entry(int index, istream &src,
	                         bool for_patch, int version) {
		ignore_unused_variable_warning(index, for_patch, version);
		pair<int, int> data;
		data.second = ReadInt(src, 0); // The real shape.
		for (vector<pair<int, int> >::iterator it = table->begin();
		        it != table->end(); ++it)
			if ((*it).second == data.second)
				return;     // Do nothing for repeated entries.
		src.ignore(1);
		if (src.peek() == '%') {
			data.first = shape;     // The assigned shape.
			string key;
			src >> key;
			(*shapevars)[key] = shape;
			shape++;    // Leave it ready for the next shape.
		} else
			data.first = ReadInt(src, 0);
		table->push_back(data);
	}
};

class Shaperef_parser: public Shapeinfo_entry_parser {
	vector<pair<int, int> > *table;
	map<string, int> *shapevars;
public:
	Shaperef_parser
	(vector<pair<int, int> > *tbl, map<string, int> *sh)
		: table(tbl), shapevars(sh)
	{  }
	virtual void parse_entry(int index, istream &src,
	                         bool for_patch, int version) {
		ignore_unused_variable_warning(index, for_patch, version);
		pair<int, int> data;
		data.first = ReadInt(src, 0);  // The spot.
		src.ignore(1);
		if (src.peek() == '%') {
			string key;
			src >> key;
			map<string, int>::iterator it = shapevars->find(key);
			if (it != shapevars->end())
				data.second = (*it).second; // The shape #.
			else
				return; // Invalid reference; bail out.
		} else
			data.second = ReadInt(src, 0);
		table->push_back(data);
	}
};

class Paperdoll_source_parser: public Shapeinfo_entry_parser {
	vector<pair<string, int> > *table;
	bool erased_for_patch;
public:
	Paperdoll_source_parser(vector<pair<string, int> > *tbl)
		: table(tbl), erased_for_patch(false)
	{  }
	virtual void parse_entry(int index, istream &src,
	                         bool for_patch, int version);
};

class Def_av_shape_parser: public Shapeinfo_entry_parser {
	map<bool, Base_Avatar_info> *table;
public:
	Def_av_shape_parser(map<bool, Base_Avatar_info> *tbl)
		: table(tbl)
	{  }
	virtual void parse_entry(int index, istream &src,
	                         bool for_patch, int version) {
		ignore_unused_variable_warning(index, for_patch, version);
		bool fmale = ReadInt(src, 0) != 0;
		Base_Avatar_info entry;
		entry.shape_num = ReadInt(src);
		entry.face_shape = ReadInt(src);
		entry.face_frame = ReadInt(src);
		(*table)[fmale] = entry;
	}
};

class Base_av_race_parser: public Shapeinfo_entry_parser {
	Avatar_default_skin *table;
public:
	Base_av_race_parser(Avatar_default_skin *tbl)
		: table(tbl)
	{  }
	virtual void parse_entry(int index, istream &src,
	                         bool for_patch, int version) {
		ignore_unused_variable_warning(index, for_patch, version);
		table->default_skin = ReadInt(src, 0);
		table->default_female = ReadInt(src) != 0;
	}
};

class Multiracial_parser: public Shapeinfo_entry_parser {
	vector<Skin_data> *table;
	map<string, int> *shapevars;
public:
	Multiracial_parser(vector<Skin_data> *tbl, map<string, int> *sh)
		: table(tbl), shapevars(sh)
	{  }
	int ReadVar(istream &src) {
		src.ignore(1);
		if (src.peek() == '%') {
			string key = ReadStr(src, 0);
			map<string, int>::iterator it = shapevars->find(key);
			if (it != shapevars->end())
				return (*it).second;    // The var value.
			else
				return -1;  // Invalid reference; bail out.
		} else
			return ReadInt(src, 0);
	}
	virtual void parse_entry(int index, istream &src,
	                         bool for_patch, int version) {
		ignore_unused_variable_warning(index);
		Skin_data entry;
		entry.skin_id = ReadInt(src, 0);
		if (entry.skin_id > last_skin)
			last_skin = entry.skin_id;
		entry.is_female = ReadInt(src) != 0;
		if ((entry.shape_num = ReadVar(src)) < 0)
			return;
		if ((entry.naked_shape = ReadVar(src)) < 0)
			return;
		entry.face_shape = ReadInt(src);
		entry.face_frame = ReadInt(src);
		entry.alter_face_shape = ReadInt(src);
		entry.alter_face_frame = ReadInt(src);
		entry.copy_info = !(version == 2 && !src.eof() && ReadInt(src) == 0);
		if (for_patch && !table->empty()) {
			unsigned int i;
			int found = -1;
			for (i = 0; i < table->size(); i++)
				if ((*table)[i].skin_id == entry.skin_id &&
				        (*table)[i].is_female == entry.is_female) {
					found = i;
					break;
				}
			if (found > -1) {
				(*table)[found] = entry;
				return;
			}
		}
		table->push_back(entry);
	}
};

class Avatar_usecode_parser: public Shapeinfo_entry_parser {
	map<int, Usecode_function_data> *table;
	Usecode_machine *usecode;
public:
	Avatar_usecode_parser(map<int, Usecode_function_data> *tbl)
		: table(tbl), usecode(Game_window::get_instance()->get_usecode())
	{  }
	virtual void parse_entry(int index, istream &src,
	                         bool for_patch, int version) {
		ignore_unused_variable_warning(index, for_patch, version);
		Usecode_function_data entry;
		int type = ReadInt(src, 0);
		if (src.peek() == ':') {
			string name = ReadStr(src);
			entry.fun_id = usecode->find_function(name.c_str(), true);
		} else
			entry.fun_id = ReadInt(src);
		entry.event_id = ReadInt(src);
		(*table)[type] = entry;
	}
};


void Paperdoll_source_parser::parse_entry(
    int index,
    istream &src,
    bool for_patch,
    int version
) {
	ignore_unused_variable_warning(index, version);
	if (!erased_for_patch && for_patch)
		table->clear();
	string line;
	src >> line;
	if (line == "static" ||
	        (GAME_BG && line == "bg") ||
	        (GAME_SI && line == "si"))
		table->push_back(pair<string, int>(string(PAPERDOL), -1));
	else if (line == "si")
		table->push_back(pair<string, int>(string("<SERPENT_STATIC>/paperdol.vga"), -1));
	else if (GAME_SI && line == "flx")
		// ++++ FIMXME: Implement in the future for SI paperdoll patches.
		CERR("Paperdoll source file '" << line << "' is not implemented yet.");
	else if (GAME_BG && line == "flx") {
		const str_int_pair &resource = game->get_resource("files/paperdolvga");
		table->push_back(pair<string, int>(string(resource.str), resource.num));
	} else
		CERR("Unknown paperdoll source file '" << line << "' was specified.");
}

/*
 *  Parses a shape data file.
 */
void Shapeinfo_lookup::Read_data_file(
    const char *fname,                  // Name of file to read, sans extension
    const char *sections[],             // The names of the sections
    Shapeinfo_entry_parser *parsers[],  // Parsers to use for each section
    int numsections                     // Number of sections
) {
	vector<Readstrings> static_strings;
	vector<Readstrings> patch_strings;
	static_strings.resize(numsections);
	int static_version = 1;
	int patch_version = 1;
	char buf[50];
	if (GAME_BG || GAME_SI) {
		snprintf(buf, 50, "config/%s", fname);
		const str_int_pair &resource = game->get_resource(buf);
		U7object txtobj(resource.str, resource.num);
		std::size_t len;
		char *txt = txtobj.retrieve(len);
		BufferDataSource ds(txt, len);
		static_version = Read_text_msg_file_sections(&ds,
		                 static_strings, sections, numsections);
		delete [] txt;
	} else {
		try {
			snprintf(buf, 50, "<STATIC>/%s.txt", fname);
			ifstream in;
			U7open(in, buf, false);
			static_version = Read_text_msg_file_sections(in,
			                 static_strings, sections, numsections);
			in.close();
		} catch (std::exception const &e) {
			if (!Game::is_editing())
				throw;
			static_strings.resize(numsections);
		}
	}
	patch_strings.resize(numsections);
	snprintf(buf, 50, "<PATCH>/%s.txt", fname);
	if (U7exists(buf)) {
		ifstream in;
		U7open(in, buf, false);
		patch_version = Read_text_msg_file_sections(in, patch_strings,
		                sections, numsections);
		in.close();
	}

	for (unsigned int i = 0; i < static_strings.size(); i++) {
		Readstrings &section = static_strings[i];
		for (unsigned int j = 0; j < section.size(); j++) {
			if (section[j].size()) {
				stringstream src(section[j]);
				parsers[i]->parse_entry(j, src, false, static_version);
			}
		}
		section.clear();
	}
	static_strings.clear();
	for (unsigned int i = 0; i < patch_strings.size(); i++) {
		Readstrings &section = patch_strings[i];
		for (unsigned int j = 0; j < section.size(); j++) {
			if (section[j].size()) {
				stringstream src(section[j]);
				parsers[i]->parse_entry(j, src, true, patch_version);
			}
		}
		section.clear();
	}
	patch_strings.clear();
	for (int i = 0; i < numsections; i++)
		delete parsers[i];
}

/*
 *  Setup shape file tables.
 */
void Shapeinfo_lookup::setup_shape_files() {
	paperdoll_source_table = new vector<pair<string, int> >;
	imported_gump_shapes = new vector<pair<int, int> >;
	gumpvars = new map<string, int>;
	blue_shapes = new vector<pair<int, int> >;
	imported_skin_shapes = new vector<pair<int, int> >;
	skinvars = new map<string, int>;
	const int size = 4;
	const char *sections[size] = {
		"paperdoll_source",
		"gump_imports",
		"blue_shapes",
		"multiracial_imports"
	};
	Shapeinfo_entry_parser *parsers[size] = {
		new Paperdoll_source_parser(paperdoll_source_table),
		new Shape_imports_parser(imported_gump_shapes, gumpvars),
		new Shaperef_parser(blue_shapes, gumpvars),
		new Shape_imports_parser(imported_skin_shapes, skinvars)
	};
	Read_data_file("shape_files", sections, parsers, size);
	// For safety.
	if (paperdoll_source_table->empty())
		paperdoll_source_table->push_back(pair<string, int>(string(PAPERDOL), -1));
	// Add in patch paperdolls too.
	paperdoll_source_table->push_back(pair<string, int>(string(PATCH_PAPERDOL), -1));
}

/*
 *  Setup avatar data tables.
 */
void Shapeinfo_lookup::setup_avatar_data() {
	if (!skinvars)
		setup_shape_files();
	def_av_info = new map<bool, Base_Avatar_info>;
	base_av_info = new Avatar_default_skin;
	skins_table = new vector<Skin_data>;
	unselectable_skins = new map<int, bool>;
	petra_table = new map<int, int>;
	usecode_funs = new map<int, Usecode_function_data>;
	const int size = 6;
	const char *sections[size] = {
		"defaultshape",
		"baseracesex",
		"multiracial_table",
		"unselectable_races_table",
		"petra_face_table",
		"usecode_info"
	};
	Shapeinfo_entry_parser *parsers[size] = {
		new Def_av_shape_parser(def_av_info),
		new Base_av_race_parser(base_av_info),
		new Multiracial_parser(skins_table, skinvars),
		new Bool_parser(unselectable_skins),
		new Int_pair_parser(petra_table),
		new Avatar_usecode_parser(usecode_funs)
	};
	Read_data_file("avatar_data", sections, parsers, size);
}


vector<pair<string, int> > *Shapeinfo_lookup::GetPaperdollSources() {
	if (!paperdoll_source_table)
		setup_shape_files();
	return paperdoll_source_table;
}

vector<pair<int, int> > *Shapeinfo_lookup::GetImportedSkins() {
	if (!imported_skin_shapes)
		setup_shape_files();
	return imported_skin_shapes;
}

bool Shapeinfo_lookup::IsSkinImported(int shape) {
	if (!imported_skin_shapes)
		setup_shape_files();
	assert(imported_skin_shapes);
	for (vector<pair<int, int> >::iterator it = imported_skin_shapes->begin();
	        it != imported_skin_shapes->end(); ++it) {
		if ((*it).first == shape)
			return true;
	}
	return false;
}

vector<pair<int, int> > *Shapeinfo_lookup::GetImportedGumpShapes() {
	if (!imported_gump_shapes)
		setup_shape_files();
	return imported_gump_shapes;
}

int Shapeinfo_lookup::GetBlueShapeData(int spot) {
	if (!blue_shapes)
		setup_shape_files();

	for (vector<pair<int, int> >::iterator it = blue_shapes->begin();
	        it != blue_shapes->end(); ++it)
		if (it->first == -1 || it->first == spot)
			return it->second;
	return 54;
}

Base_Avatar_info *Shapeinfo_lookup::GetBaseAvInfo(bool sex) {
	if (!def_av_info)
		setup_avatar_data();
	map<bool, Base_Avatar_info>::iterator it = def_av_info->find(sex);
	if (it != def_av_info->end())
		return &((*it).second);
	else
		return NULL;
}

int Shapeinfo_lookup::get_skinvar(string key) {
	if (!skinvars)
		setup_shape_files();
	map<string, int>::iterator it = skinvars->find(key);
	if (it != skinvars->end())
		return (*it).second;    // The shape #.
	else
		return -1;  // Invalid reference; bail out.
}

int Shapeinfo_lookup::GetMaleAvShape() {
	if (!def_av_info)
		setup_avatar_data();
	return (*def_av_info)[false].shape_num;
}

int Shapeinfo_lookup::GetFemaleAvShape() {
	if (!def_av_info)
		setup_avatar_data();
	return (*def_av_info)[true].shape_num;
}

Avatar_default_skin *Shapeinfo_lookup::GetDefaultAvSkin() {
	if (!base_av_info)
		setup_avatar_data();
	return base_av_info;
}

vector<Skin_data> *Shapeinfo_lookup::GetSkinList() {
	if (!skins_table)
		setup_avatar_data();
	return skins_table;
}

Skin_data *Shapeinfo_lookup::GetSkinInfo(int skin, bool sex) {
	if (!skins_table)
		setup_avatar_data();
	for (vector<Skin_data>::iterator it = skins_table->begin();
	        it != skins_table->end(); ++it)
		if ((*it).skin_id == skin && (*it).is_female == sex)
			return &(*it);
	return NULL;
}

Skin_data *Shapeinfo_lookup::GetSkinInfoSafe(int skin, bool sex, bool sishapes) {
	Skin_data *sk = GetSkinInfo(skin, sex);
	if (sk && (sishapes ||
	           (!IsSkinImported(sk->shape_num) &&
	            !IsSkinImported(sk->naked_shape))))
		return sk;
	sk = GetSkinInfo(GetDefaultAvSkin()->default_skin, sex);
	// Prevent unavoidable problems. *Should* never be needed.
	assert(sk && (sishapes ||
	              (!IsSkinImported(sk->shape_num) &&
	               !IsSkinImported(sk->naked_shape))));
	return sk;
}

Skin_data *Shapeinfo_lookup::GetSkinInfoSafe(Actor *npc) {
	int skin = npc->get_skin_color();
	bool sex = npc->get_type_flag(Actor::tf_sex) != 0;
	return GetSkinInfoSafe(skin, sex, Shape_manager::get_instance()->have_si_shapes());
}

Skin_data *Shapeinfo_lookup::ScrollSkins(
    int skin, bool sex, bool sishapes, bool ignoresex, bool prev, bool sel
) {
	if (!base_av_info)
		setup_avatar_data();
	bool nsex = sex;
	int nskin = skin;
	bool chskin = true;
	while (true) {
		if (ignoresex) {
			nsex = !nsex;
			chskin = (nsex == base_av_info->default_female);
		}
		nskin = (nskin + (prev ? last_skin : 0) + chskin) % (last_skin + 1);
		if (sel && !IsSkinSelectable(nskin))
			continue;
		Skin_data *newskin = GetSkinInfo(nskin, nsex);
		if (newskin && (sishapes ||
		                (!IsSkinImported(newskin->shape_num) &&
		                 !IsSkinImported(newskin->naked_shape))))
			return newskin;
	}
}

int Shapeinfo_lookup::GetNumSkins(bool sex) {
	if (!skins_table)
		setup_avatar_data();
	int cnt = 0;
	for (vector<Skin_data>::iterator it = skins_table->begin();
	        it != skins_table->end(); ++it)
		if ((*it).is_female == sex)
			cnt++;
	return cnt;
}

Usecode_function_data *Shapeinfo_lookup::GetAvUsecode(int type) {
	if (!usecode_funs)
		setup_avatar_data();
	map<int, Usecode_function_data>::iterator it = usecode_funs->find(type);
	if (it != usecode_funs->end())
		return &(*it).second;
	else
		return NULL;
}

bool Shapeinfo_lookup::IsSkinSelectable(int skin) {
	if (!unselectable_skins)
		setup_avatar_data();
	map<int, bool>::iterator it = unselectable_skins->find(skin);
	if (it != unselectable_skins->end())
		return false;
	else
		return true;
}

bool Shapeinfo_lookup::HasFaceReplacement(int npcid) {
	if (!petra_table)
		setup_avatar_data();
	map<int, int>::iterator it = petra_table->find(npcid);
	if (it != petra_table->end())
		return (*it).second != 0;
	else
		return npcid != 0;
}

int Shapeinfo_lookup::GetFaceReplacement(int facenum) {
	if (!petra_table)
		setup_avatar_data();
	Game_window *gwin = Game_window::get_instance();
	if (gwin->get_main_actor()->get_flag(Obj_flags::petra)) {
		map<int, int>::iterator it = petra_table->find(facenum);
		if (it != petra_table->end())
			return (*it).second;
	}
	return facenum;
}

void Shapeinfo_lookup::reset() {
	FORGET_OBJECT(paperdoll_source_table);
	FORGET_OBJECT(imported_gump_shapes);
	FORGET_OBJECT(blue_shapes);
	FORGET_OBJECT(imported_skin_shapes);
	FORGET_OBJECT(gumpvars);
	FORGET_OBJECT(skinvars);
	FORGET_OBJECT(def_av_info);
	FORGET_OBJECT(base_av_info);
	FORGET_OBJECT(skins_table);
	FORGET_OBJECT(unselectable_skins);
	FORGET_OBJECT(petra_table);
	FORGET_OBJECT(usecode_funs);
	last_skin = 0;
}
