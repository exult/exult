/*
 *  shapevga.cc - Handle the 'shapes.vga' file and associated info.
 *
 *  Copyright (C) 1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2013  The Exult Team
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

#include <iomanip>          /* For debugging only. */
#include <sstream>
#include "shapevga.h"
#include "ammoinf.h"
#include "aniinf.h"
#include "armorinf.h"
#include "bodyinf.h"
#include "continf.h"
#include "effhpinf.h"
#include "expinf.h"
#include "frnameinf.h"
#include "frflags.h"
#include "frusefun.h"
#include "lightinf.h"
#include "monstinf.h"
#include "npcdollinf.h"
#include "objdollinf.h"
#include "sfxinf.h"
#include "warminf.h"
#include "weaponinf.h"
#include "data/exult_bg_flx.h"
#include "data/exult_si_flx.h"
#include "utils.h"
#include "databuf.h"
#include "msgfile.h"
#include "U7file.h"
#include "exceptions.h"
#include "ready.h"
#include "data_utils.h"
#include "ignore_unused_variable_warning.h"
#include "array_size.h"

using std::ifstream;
using std::ios;
using namespace std;

// For convienience
#define patch_exists(p) (have_patch_path && U7exists(p))
#define patch_name(p) (patch_exists(p) ? (p) : nullptr)

/*
 *  Open, but don't quit if editing.  We first try the patch name if it's
 *  given.
 */

static bool U7open2(
    ifstream &in,           // Stream to open.
    const char *pname,      // Patch name, or null.
    const char *fname,      // File name.
    bool editing
) {
	ignore_unused_variable_warning(editing);
	if (pname) {
		U7open(in, pname);
		return true;
	}
	try {
		U7open(in, fname);
	} catch (const file_exception & /*f*/) {
		return false;
	}
	return true;
}

// Special case ID reader functors.

extern int get_skinvar(const std::string& key);

// Multiracial support in Exult.
class Paperdoll_npc_ID_reader {
public:
	int operator()(std::istream &in, int index, int version, bool binary) {
		ignore_unused_variable_warning(index, version, binary);
		if (in.peek() == '%') {
			std::string key = ReadStr(in);
			// We need these for Exult, but not for ES.
			// For now, a compromise/hack in that ES defines
			// a version of this function which always returns
			// -1, while Exult has another which forwards to
			// Shapeinfo_lookup::get_skinvar
			int id = get_skinvar(key);
			return id < 0 ? -1 : id;
		} else
			return ReadInt(in);
	}
};

// For backward compatibility.
class Body_ID_reader {
public:
	int operator()(std::istream &in, int index, int version, bool binary) {
		ignore_unused_variable_warning(binary);
		return version == 1 ? index : ReadInt(in);
	}
};

// Special case reader functor.
class Gump_reader_functor {
public:
	bool operator()(std::istream &in, int version, bool patch,
	                Exult_Game game, Shape_info &info) {
		ignore_unused_variable_warning(patch, game);
		info.gump_shape = Read2(in);
		if (version >= 2)
			info.gump_font = Read2(in);
		else
			info.gump_font = -1;
		return true;
	}
};

// A few custom post-read functors.
class Ready_type_functor {
	Patch_flags_functor<ready_type_flag, Shape_info> setflags;
public:
	void operator()(std::istream &in, int version, bool patch,
	                Exult_Game game, Shape_info &info) {
		unsigned char ready = info.ready_type;
		info.spell_flag = ready & 1;
		ready >>= 3;
		unsigned char spot = game == BLACK_GATE ? Ready_spot_from_BG(ready)
		            : Ready_spot_from_SI(ready);
		info.ready_type = spot;
		// Init alternate spots.
		switch (spot) {
		case lfinger:
			info.alt_ready1 = rfinger;
			break;
		case lhand:
			info.alt_ready1 = rhand;
			info.alt_ready2 = belt;
			break;
		case both_hands:
			info.alt_ready1 = back_2h;
			break;
		}
		setflags(in, version, patch, game, info);
	}
};

class Actor_flags_functor {
	Patch_flags_functor<actor_flags_flag, Shape_info> setflags;
public:
	void operator()(std::istream &in, int version, bool patch,
	                Exult_Game game, Shape_info &info) {
		setflags(in, version, patch, game, info);
	}
};

class Paperdoll_npc_functor {
public:
	void operator()(std::istream &in, int version, bool patch,
	                Exult_Game game, Shape_info &info) {
		ignore_unused_variable_warning(patch, game);
		if (version < 3)
			// We need this for backward compatibility.
			// We use the setter methods so that the info
			// will get saved by ES if that is needed.
			info.set_gump_data(ReadInt(in, -1), -1);
	}
};

void Shapes_vga_file::Read_Shapeinf_text_data_file(
    bool editing,
    Exult_Game game_type
) {
	const char *sections[] = {"explosions", "shape_sfx", "animation",
	                          "usecode_events", "mountain_tops", "monster_food", "actor_flags",
	                          "effective_hps", "lightweight_object", "light_data", "warmth_data",
	                          "quantity_frames", "locked_containers", "content_rules",
	                          "volatile_explosive", "framenames", "altready", "barge_type",
	                          "frame_powers", "is_jawbone", "is_mirror", "on_fire",
	                          "extradimensional_storage", "field_type",
	                          "frame_usecode"
	                         };
	Base_reader *readers[] = {
		// For explosions.
		new Functor_multidata_reader < Shape_info,
		Class_reader_functor < Explosion_info, Shape_info,
		&Shape_info::explosion > > (info),
		// For sound effects.
		new Functor_multidata_reader < Shape_info,
		Class_reader_functor < SFX_info, Shape_info,
		&Shape_info::sfxinf > > (info),
		// For animations.
		new Functor_multidata_reader < Shape_info,
		Class_reader_functor < Animation_info, Shape_info,
		&Shape_info::aniinf > > (info),
		// For usecode events.
		new Functor_multidata_reader < Shape_info,
		Bit_text_reader_functor < unsigned short, Shape_info,
		&Shape_info::shape_flags, Shape_info::usecode_events > ,
		Patch_flags_functor<usecode_events_flag, Shape_info> > (info),
		// For mountain tops.
		new Functor_multidata_reader < Shape_info,
		Text_reader_functor < unsigned char, Shape_info,
		&Shape_info::mountain_top > ,
		Patch_flags_functor<mountain_top_flag, Shape_info> > (info),
		// For monster food.
		new Functor_multidata_reader < Shape_info,
		Text_reader_functor < short, Shape_info,
		&Shape_info::monster_food > ,
		Patch_flags_functor<monster_food_flag, Shape_info> > (info),
		// For actor flags.
		new Functor_multidata_reader < Shape_info,
		Bit_field_text_reader_functor < unsigned char, Shape_info,
		&Shape_info::actor_flags > ,
		Actor_flags_functor > (info),
		// For effective HPs.
		new Functor_multidata_reader < Shape_info,
		Vector_reader_functor < Effective_hp_info, Shape_info,
		&Shape_info::hpinf > > (info),
		// For lightweight objects.
		new Functor_multidata_reader < Shape_info,
		Bit_text_reader_functor < unsigned short, Shape_info,
		&Shape_info::shape_flags, Shape_info::lightweight > ,
		Patch_flags_functor<lightweight_flag, Shape_info> > (info),
		// For light data.
		new Functor_multidata_reader < Shape_info,
		Vector_reader_functor < Light_info, Shape_info,
		&Shape_info::lightinf > > (info),
		// For warmth data.
		new Functor_multidata_reader < Shape_info,
		Vector_reader_functor < Warmth_info, Shape_info,
		&Shape_info::warminf > > (info),
		// For quantity frames.
		new Functor_multidata_reader < Shape_info,
		Bit_text_reader_functor < unsigned short, Shape_info,
		&Shape_info::shape_flags, Shape_info::quantity_frames > ,
		Patch_flags_functor<quantity_frames_flag, Shape_info> > (info),
		// For locked objects.
		new Functor_multidata_reader < Shape_info,
		Bit_text_reader_functor < unsigned short, Shape_info,
		&Shape_info::shape_flags, Shape_info::locked > ,
		Patch_flags_functor<locked_flag, Shape_info> > (info),
		// For content rules.
		new Functor_multidata_reader < Shape_info,
		Vector_reader_functor < Content_rules, Shape_info,
		&Shape_info::cntrules > > (info),
		// For highly explosive objects.
		new Functor_multidata_reader < Shape_info,
		Bit_text_reader_functor < unsigned short, Shape_info,
		&Shape_info::shape_flags, Shape_info::is_volatile > ,
		Patch_flags_functor<is_volatile_flag, Shape_info> > (info),
		// For frame names.
		new Functor_multidata_reader < Shape_info,
		Vector_reader_functor < Frame_name_info, Shape_info,
		&Shape_info::nameinf > > (info),
		// For alternate ready spots.
		new Functor_multidata_reader < Shape_info,
		Text_pair_reader_functor < unsigned char, Shape_info,
		&Shape_info::alt_ready1, &Shape_info::alt_ready2 > ,
		Patch_flags_functor<altready_type_flag, Shape_info> > (info),
		// For barge parts.
		new Functor_multidata_reader < Shape_info,
		Text_reader_functor < unsigned char, Shape_info,
		&Shape_info::barge_type > ,
		Patch_flags_functor<barge_type_flag, Shape_info> > (info),
		// For frame flags.
		new Functor_multidata_reader < Shape_info,
		Vector_reader_functor < Frame_flags_info, Shape_info,
		&Shape_info::frflagsinf > > (info),
		// For the jawbone.
		new Functor_multidata_reader < Shape_info,
		Bit_text_reader_functor < unsigned short, Shape_info,
		&Shape_info::shape_flags, Shape_info::jawbone > ,
		Patch_flags_functor<jawbone_flag, Shape_info> > (info),
		// Mirrors.
		new Functor_multidata_reader < Shape_info,
		Bit_text_reader_functor < unsigned short, Shape_info,
		&Shape_info::shape_flags, Shape_info::mirror > ,
		Patch_flags_functor<mirror_flag, Shape_info> > (info),
		// Objects on fire.
		new Functor_multidata_reader < Shape_info,
		Bit_text_reader_functor < unsigned short, Shape_info,
		&Shape_info::shape_flags, Shape_info::on_fire > ,
		Patch_flags_functor<on_fire_flag, Shape_info> > (info),
		// Containers with unlimited storage.
		new Functor_multidata_reader < Shape_info,
		Bit_text_reader_functor < unsigned short, Shape_info,
		&Shape_info::shape_flags, Shape_info::extradimensional_storage > ,
		Patch_flags_functor<extradimensional_storage_flag, Shape_info> > (info),
		// For field types.
		new Functor_multidata_reader < Shape_info,
		Text_reader_functor < signed char, Shape_info,
		&Shape_info::field_type > ,
		Patch_flags_functor<field_type_flag, Shape_info> > (info),
		// For frame usecode.
		new Functor_multidata_reader < Shape_info,
		Vector_reader_functor < Frame_usecode_info, Shape_info,
		&Shape_info::frucinf > > (info),
	};
	int numsections = array_size(sections);
	int numreaders = array_size(readers);
	assert(numsections == numreaders);
	int flxres = game_type == BLACK_GATE ?
	             EXULT_BG_FLX_SHAPE_INFO_TXT : EXULT_SI_FLX_SHAPE_INFO_TXT;

	Read_text_data_file("shape_info", readers, sections,
	                    numsections, editing, game_type, flxres);
}

void Shapes_vga_file::Read_Bodies_text_data_file(
    bool editing,
    Exult_Game game_type
) {
	const char *sections[] = {"bodyshapes", "bodylist"};
	Base_reader *readers[] = {
		new Functor_multidata_reader < Shape_info,
		Bit_text_reader_functor < unsigned short, Shape_info,
		&Shape_info::shape_flags, Shape_info::is_body > ,
		Patch_flags_functor<is_body_flag, Shape_info>,
		Body_ID_reader > (info),
		new Functor_multidata_reader < Shape_info,
		Class_reader_functor < Body_info, Shape_info,
		&Shape_info::body > > (info)
	};
	int numsections = array_size(sections);
	int numreaders = array_size(readers);
	assert(numsections == numreaders);
	int flxres = game_type == BLACK_GATE ?
	             EXULT_BG_FLX_BODIES_TXT : EXULT_SI_FLX_BODIES_TXT;

	Read_text_data_file("bodies", readers, sections,
	                    numsections, editing, game_type, flxres);
}

void Shapes_vga_file::Read_Paperdoll_text_data_file(
    bool editing,
    Exult_Game game_type
) {
	const char *sections[] = {"characters", "items"};
	Base_reader *readers[] = {
		new Functor_multidata_reader < Shape_info,
		Class_reader_functor < Paperdoll_npc, Shape_info,
		&Shape_info::npcpaperdoll > ,
		Paperdoll_npc_functor,
		Paperdoll_npc_ID_reader > (info),
		new Functor_multidata_reader < Shape_info,
		Vector_reader_functor < Paperdoll_item, Shape_info,
		&Shape_info::objpaperdoll > > (info),
	};
	int numsections = array_size(sections);
	int numreaders = array_size(readers);
	assert(numsections == numreaders);
	int flxres = game_type == BLACK_GATE ?
	             EXULT_BG_FLX_PAPERDOL_INFO_TXT : EXULT_SI_FLX_PAPERDOL_INFO_TXT;

	Read_text_data_file("paperdol_info", readers, sections,
	                    numsections, editing, game_type, flxres);
}

/*
 *  Reload static data for weapons, ammo and mosters to
 *  fix data that was lost by earlier versions of ES.
 */
void Shapes_vga_file::fix_old_shape_info(
    Exult_Game game     // Which game.
) {
	if (!info_read) // Read info first.
		read_info(game, true);
	Functor_multidata_reader < Shape_info,
	                         Class_reader_functor < Weapon_info, Shape_info,
	                         &Shape_info::weapon > > weapon(info);
	weapon.read(WEAPONS, false, game);
	Functor_multidata_reader < Shape_info,
	                         Class_reader_functor < Ammo_info, Shape_info,
	                         &Shape_info::ammo > > ammo(info);
	ammo.read(AMMO, false, game);
	Functor_multidata_reader < Shape_info,
	                         Class_reader_functor < Monster_info, Shape_info,
	                         &Shape_info::monstinf > > monstinf(info);
	monstinf.read(MONSTERS, false, game);
}

/*
 *  Reload info.
 */

void Shapes_vga_file::reload_info(
    Exult_Game game     // Which game.
) {
	info_read = false;
	info.clear();
	read_info(game);
}

/*
 *  Read in data files about shapes.
 *
 *  Output: 0 if error.
 */

bool Shapes_vga_file::read_info(
    Exult_Game game,        // Which game.
    bool editing            // True to allow files to not exist.
) {
	if (info_read)
		return false;
	info_read = true;
	bool have_patch_path = is_system_path_defined("<PATCH>");

	// ShapeDims

	// Starts at 0x96'th shape.
	ifstream shpdims;
	if (U7open2(shpdims, patch_name(PATCH_SHPDIMS), SHPDIMS, editing))
		for (size_t i = c_first_obj_shape;
		        i < shapes.size() && !shpdims.eof(); i++) {
			info[i].shpdims[0] = shpdims.get();
			info[i].shpdims[1] = shpdims.get();
		}

	// WGTVOL
	ifstream wgtvol;
	if (U7open2(wgtvol, patch_name(PATCH_WGTVOL), WGTVOL, editing))
		for (size_t i = 0; i < shapes.size() && !wgtvol.eof(); i++) {
			info[i].weight = wgtvol.get();
			info[i].volume = wgtvol.get();
		}

	// TFA
	ifstream tfa;
	if (U7open2(tfa, patch_name(PATCH_TFA), TFA, editing))
		for (size_t i = 0; i < shapes.size() && !tfa.eof(); i++) {
			tfa.read(reinterpret_cast<char *>(&info[i].tfa[0]), 3);
			info[i].set_tfa_data();
		}

	if (game == BLACK_GATE || game == SERPENT_ISLE) {
		// Animation data at the end of BG and SI TFA.DAT
		ifstream stfa;
		// We *should* blow up if TFA not there.
		U7open(stfa, TFA);
		stfa.seekg(3 * 1024);
		unsigned char buf[512];
		stfa.read(reinterpret_cast<char *>(buf), 512);
		stfa.close();
		unsigned char *ptr = buf;
		for (int i = 0; i < 512; i++, ptr++) {
			int val = *ptr;
			int shape = 2 * i;
			while (val) {
				if (val & 0xf) {
					delete info[shape].aniinf;
					info[shape].aniinf = Animation_info::create_from_tfa(
					                         val & 0xf, get_num_frames(shape));
				}
				val >>= 4;
				shape++;
			}
		}
	}

	// Load data about drawing the weapon in an actor's hand
	ifstream wihh;
	if (U7open2(wihh, patch_name(PATCH_WIHH), WIHH, editing)) {
		size_t cnt = shapes.size();
		unsigned short offsets[c_max_shapes];
		for (size_t i = 0; i < cnt; i++)
			offsets[i] = Read2(wihh);
		for (size_t i = 0; i < cnt; i++)
			// A zero offset means there is no record
			if (offsets[i] == 0)
				info[i].weapon_offsets = nullptr;
			else {
				wihh.seekg(offsets[i]);
				// There are two bytes per frame: 64 total
				info[i].weapon_offsets = new unsigned char[64];
				for (int j = 0; j < 32; j++) {
					unsigned char x = Read1(wihh);
					unsigned char y = Read1(wihh);
					// Set x/y to 255 if weapon is not to be drawn
					// In the file x/y are either 64 or 255:
					// I am assuming that they mean the same
					if (x > 63 || y > 63)
						x = y = 255;
					info[i].weapon_offsets[j * 2] = x;
					info[i].weapon_offsets[j * 2 + 1] = y;
				}
			}
		wihh.close();
	}

	ifstream occ;          // Read flags from occlude.dat.
	if (U7open2(occ, patch_name(PATCH_OCCLUDE), OCCLUDE, editing)) {
		unsigned char occbits[c_occsize];   // c_max_shapes bit flags.
		// Ensure sensible defaults.
		memset(&occbits[0], 0, sizeof(occbits));
		occ.read(reinterpret_cast<char *>(occbits), sizeof(occbits));
		for (int i = 0; i < occ.gcount(); i++) {
			unsigned char bits = occbits[i];
			int shnum = i * 8;  // Check each bit.
			for (int b = 0; bits; b++, bits = bits >> 1)
				if (bits & 1)
					info[shnum + b].occludes_flag = true;
		}
	}

	// Get 'equip.dat'.
	ifstream mfile;
	if (U7open2(mfile, patch_name(PATCH_EQUIP), EQUIP, editing)) {
		// Get # entries (with Exult extension).
		int num_recs = Read_count(mfile);
		Monster_info::reserve_equip(num_recs);
		for (int i = 0; i < num_recs; i++) {
			Equip_record equip;
			// 10 elements/record.
			for (int elem = 0; elem < 10; elem++) {
				int shnum = Read2(mfile);
				unsigned prob = Read1(mfile);
				unsigned quant = Read1(mfile);
				Read2(mfile);
				equip.set(elem, shnum, prob, quant);
			}
			Monster_info::add_equip(equip);
		}
		mfile.close();
	}

	Functor_multidata_reader < Shape_info,
	                         Class_reader_functor < Armor_info, Shape_info,
	                         &Shape_info::armor > > armor(info);
	armor.read(ARMOR, false, game);
	armor.read(PATCH_ARMOR, true, game);

	Functor_multidata_reader < Shape_info,
	                         Class_reader_functor < Weapon_info, Shape_info,
	                         &Shape_info::weapon > > weapon(info);
	weapon.read(WEAPONS, false, game);
	weapon.read(PATCH_WEAPONS, true, game);

	Functor_multidata_reader < Shape_info,
	                         Class_reader_functor < Ammo_info, Shape_info,
	                         &Shape_info::ammo > > ammo(info);
	ammo.read(AMMO, false, game);
	ammo.read(PATCH_AMMO, true, game);

	Functor_multidata_reader < Shape_info,
	                         Class_reader_functor < Monster_info, Shape_info,
	                         &Shape_info::monstinf > > monstinf(info);
	monstinf.read(MONSTERS, false, game);
	monstinf.read(PATCH_MONSTERS, true, game);

	Functor_multidata_reader < Shape_info, Gump_reader_functor,
	                         Patch_flags_functor<gump_shape_flag, Shape_info> > gump(info, true);
	if (game == BLACK_GATE || game == SERPENT_ISLE)
		gump.read(game, game == BLACK_GATE ?
		          EXULT_BG_FLX_CONTAINER_DAT : EXULT_SI_FLX_CONTAINER_DAT);
	else
		gump.read(CONTAINER, false, game);
	gump.read(PATCH_CONTAINER, true, game);

	Functor_multidata_reader < Shape_info,
	                         Binary_reader_functor < unsigned char, Shape_info,
	                         &Shape_info::ready_type, 6 > ,
	                         Ready_type_functor > ready(info);
	ready.read(READY, false, game);
	ready.read(PATCH_READY, true, game);

	Read_Shapeinf_text_data_file(editing, game);
	Read_Bodies_text_data_file(editing, game);
	Read_Paperdoll_text_data_file(editing, game);

	// Ensure valid ready spots for all shapes.
	unsigned char defready = game == BLACK_GATE ? backpack : rhand;
	zinfo.ready_type = defready;
	for (auto& it : info) {
		Shape_info &inf = it.second;
		if (inf.ready_type == invalid_spot)
			inf.ready_type = defready;
	}
	bool auto_modified = false;
	for (auto &it : info) {
		int shnum = it.first;
		Shape_info &inf = it.second;
		if (inf.has_monster_info()) {
			Monster_info *minf = inf.monstinf;
			if (minf->can_teleport()) {
				std::cerr << "Shape " << shnum << " is a monster that can teleport, teleport flag moved from Monster info ( monster.dat ) to Actor info ( shape_info.txt ) as NPC flags." << std::endl;
				inf.set_actor_flag(Shape_info::teleports);
				minf->set_can_teleport(false);
				auto_modified = true;
			}
			if (minf->can_summon()) {
				std::cerr << "Shape " << shnum << " is a monster that can summon, summon flag moved from Monster info ( monster.dat ) to Actor info ( shape_info.txt ) as NPC flags." << std::endl;
				inf.set_actor_flag(Shape_info::summons);
				minf->set_can_summon(false);
				auto_modified = true;
			}
			if (minf->can_be_invisible()) {
				std::cerr << "Shape " << shnum << " is a monster that can be invisible, invisible flag moved from Monster info ( monster.dat ) to Actor info ( shape_info.txt ) as NPC flags." << std::endl;
				inf.set_actor_flag(Shape_info::turns_invisible);
				minf->set_can_be_invisible(false);
				auto_modified = true;
			}
		}
	}
	return auto_modified;
}

/*
 *  Open/close file.
 */

Shapes_vga_file::Shapes_vga_file(
    const char *nm,         // Path to file.
    int u7drag,         // # from u7drag.h, or -1.
    const char *nm2         // Path to patch version, or 0.
) : Vga_file(nm, u7drag, nm2) {
}

void Shapes_vga_file::init() {
	if (is_system_path_defined("<PATCH>") && U7exists(PATCH_SHAPES))
		load(SHAPES_VGA, PATCH_SHAPES);
	else
		load(SHAPES_VGA);
	info_read = false;
}

/*
 *  Make a spot for a new shape, and delete frames in existing shape.
 *
 *  Output: ->shape, or 0 if invalid shapenum.
 */

Shape *Shapes_vga_file::new_shape(
    int shapenum
) {
	Shape *newshape = Vga_file::new_shape(shapenum);
	return newshape;
}
