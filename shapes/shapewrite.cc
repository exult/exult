/*
 *  shapewrite.cc - Write out the shape 'info' files.
 *
 *  Note:  Currently only used by ExultStudio.
 *
 *  Copyright (C) 2002  The Exult Team
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

#include <map>
#include <vector>
#include <iomanip>
#include "shapevga.h"
#include "shapeinf.h"
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
#include "monstinf.h"
#include "npcdollinf.h"
#include "objdollinf.h"
#include "sfxinf.h"
#include "warminf.h"
#include "weaponinf.h"
#include "utils.h"
#include "exceptions.h"
#include "ready.h"
#include "data_utils.h"
#include "ignore_unused_variable_warning.h"
#include "array_size.h"

using std::ifstream;
using std::ios;
using std::ofstream;
using std::ostream;
using std::endl;

// A custom writer functor.
class Readytype_writer_functor {
	Flag_check_functor<ready_type_flag, Shape_info> check;
public:
	void operator()(ostream &out, int index, Exult_Game game, Shape_info &info) {
		Write2(out, index);
		unsigned char data = info.ready_type;
		data = game == BLACK_GATE ? Ready_spot_to_BG(data)
		       : Ready_spot_to_SI(data);
		data = (data << 3) | info.spell_flag;
		Write1(out, data);
		for (unsigned i = 0; i < 6; i++)
			out.put(0);
	}
	bool operator()(Shape_info &info) {
		return check(info);
	}
};

void Shapes_vga_file::Write_Shapeinf_text_data_file(Exult_Game game) {
	size_t num_shapes = shapes.size();
	Base_writer *writers[] = {
		// For explosions.
		new Functor_multidata_writer < Shape_info,
		Class_writer_functor < Explosion_info, Shape_info,
		&Shape_info::explosion > > ("explosions", info, num_shapes),
		// For sound effects.
		new Functor_multidata_writer < Shape_info,
		Class_writer_functor < SFX_info, Shape_info,
		&Shape_info::sfxinf > > ("shape_sfx", info, num_shapes),
		// For animations.
		new Functor_multidata_writer < Shape_info,
		Class_writer_functor < Animation_info, Shape_info,
		&Shape_info::aniinf > > ("animation", info, num_shapes),
		// For usecode events.
		new Functor_multidata_writer < Shape_info,
		Bit_text_writer_functor < usecode_events_flag, unsigned short,
		Shape_info, &Shape_info::shape_flags,
		Shape_info::usecode_events > > (
		    "usecode_events", info, num_shapes),
		// For mountain tops.
		new Functor_multidata_writer < Shape_info,
		Text_writer_functor < mountain_top_flag, unsigned char,
		Shape_info, &Shape_info::mountain_top > > (
		    "mountain_tops", info, num_shapes),
		// For monster food.
		new Functor_multidata_writer < Shape_info,
		Text_writer_functor < monster_food_flag, short,
		Shape_info, &Shape_info::monster_food > > (
		    "monster_food", info, num_shapes),
		// For actor flags.
		new Functor_multidata_writer < Shape_info,
		Bit_field_text_writer_functor < actor_flags_flag, unsigned char,
		Shape_info, &Shape_info::actor_flags > > (
		    "actor_flags", info, num_shapes),
		// For effective HPs.
		new Functor_multidata_writer < Shape_info,
		Vector_writer_functor < Effective_hp_info, Shape_info,
		&Shape_info::hpinf > > ("effective_hps", info, num_shapes),
		// For lightweight objects.
		new Functor_multidata_writer < Shape_info,
		Bit_text_writer_functor < lightweight_flag, unsigned short,
		Shape_info, &Shape_info::shape_flags,
		Shape_info::lightweight > > (
		    "lightweight_object", info, num_shapes),
		// For warmth data.
		new Functor_multidata_writer < Shape_info,
		Vector_writer_functor < Warmth_info, Shape_info,
		&Shape_info::warminf > > ("warmth_data", info, num_shapes),
		// For quantity frames.
		new Functor_multidata_writer < Shape_info,
		Bit_text_writer_functor < quantity_frames_flag, unsigned short,
		Shape_info, &Shape_info::shape_flags,
		Shape_info::quantity_frames > > (
		    "quantity_frames", info, num_shapes),
		// For locked objects.
		new Functor_multidata_writer < Shape_info,
		Bit_text_writer_functor < locked_flag, unsigned short,
		Shape_info, &Shape_info::shape_flags,
		Shape_info::locked > > (
		    "locked_containers", info, num_shapes),
		// For content rules.
		new Functor_multidata_writer < Shape_info,
		Vector_writer_functor < Content_rules, Shape_info,
		&Shape_info::cntrules > > ("content_rules", info, num_shapes),
		// For highly explosive objects.
		new Functor_multidata_writer < Shape_info,
		Bit_text_writer_functor < is_volatile_flag, unsigned short,
		Shape_info, &Shape_info::shape_flags,
		Shape_info::is_volatile > > (
		    "volatile_explosive", info, num_shapes),
		// For frame names.
		new Functor_multidata_writer < Shape_info,
		Vector_writer_functor < Frame_name_info, Shape_info,
		&Shape_info::nameinf > > ("framenames", info, num_shapes),
		// For alternate ready spots.
		new Functor_multidata_writer < Shape_info,
		Text_pair_writer_functor < altready_type_flag, char, Shape_info,
		&Shape_info::alt_ready1, &Shape_info::alt_ready2 > > (
		    "altready", info, num_shapes),
		// For barge parts.
		new Functor_multidata_writer < Shape_info,
		Text_writer_functor < barge_type_flag, unsigned char, Shape_info,
		&Shape_info::barge_type > > ("barge_type", info, num_shapes),
		// For frame flags.
		new Functor_multidata_writer < Shape_info,
		Vector_writer_functor < Frame_flags_info, Shape_info,
		&Shape_info::frflagsinf > > ("frame_powers", info, num_shapes),
		// For the jawbone.
		new Functor_multidata_writer < Shape_info,
		Bit_text_writer_functor < jawbone_flag, unsigned short,
		Shape_info, &Shape_info::shape_flags,
		Shape_info::jawbone > > (
		    "is_jawbone", info, num_shapes),
		// Mirrors.
		new Functor_multidata_writer < Shape_info,
		Bit_text_writer_functor < mirror_flag, unsigned short,
		Shape_info, &Shape_info::shape_flags,
		Shape_info::mirror > > (
		    "is_mirror", info, num_shapes),
		// Objects on fire.
		new Functor_multidata_writer < Shape_info,
		Bit_text_writer_functor < on_fire_flag, unsigned short,
		Shape_info, &Shape_info::shape_flags,
		Shape_info::on_fire > > (
		    "on_fire", info, num_shapes),
		// Containers with unlimited storage.
		new Functor_multidata_writer < Shape_info,
		Bit_text_writer_functor < extradimensional_storage_flag, unsigned short,
		Shape_info, &Shape_info::shape_flags,
		Shape_info::extradimensional_storage > > (
		    "extradimensional_storage", info, num_shapes),
		// For field types.
		new Functor_multidata_writer < Shape_info,
		Text_writer_functor < field_type_flag, char, Shape_info,
		&Shape_info::field_type > > ("field_type", info, num_shapes),
		// For frame usecode.
		new Functor_multidata_writer < Shape_info,
		Vector_writer_functor < Frame_usecode_info, Shape_info,
		&Shape_info::frucinf > > ("frame_usecode", info, num_shapes)
	};
	int numsections = array_size(writers);
	Write_text_data_file("shape_info", writers, numsections, 7, game);
}

void Shapes_vga_file::Write_Bodies_text_data_file(Exult_Game game) {
	size_t num_shapes = shapes.size();
	Base_writer *writers[] = {
		new Functor_multidata_writer < Shape_info,
		Bit_text_writer_functor < is_body_flag, unsigned short,
		Shape_info, &Shape_info::shape_flags,
		Shape_info::is_body > > (
		    "bodyshapes", info, num_shapes),
		new Functor_multidata_writer < Shape_info,
		Class_writer_functor < Body_info, Shape_info,
		&Shape_info::body > > ("bodylist", info, num_shapes)
	};
	int numsections = array_size(writers);
	Write_text_data_file("bodies", writers, numsections, 2, game);
}

void Shapes_vga_file::Write_Paperdoll_text_data_file(Exult_Game game) {
	size_t num_shapes = shapes.size();
	Base_writer *writers[] = {
		new Functor_multidata_writer < Shape_info,
		Class_writer_functor < Paperdoll_npc, Shape_info,
		&Shape_info::npcpaperdoll > > ("characters", info, num_shapes),
		new Functor_multidata_writer < Shape_info,
		Vector_writer_functor < Paperdoll_item, Shape_info,
		&Shape_info::objpaperdoll > > ("items", info, num_shapes)
	};
	int numsections = array_size(writers);
	Write_text_data_file("paperdol_info", writers, numsections, 3, game);
}


/*
 *  Write out data files about shapes.
 */

void Shapes_vga_file::write_info(
    Exult_Game game
) {
	size_t num_shapes = shapes.size();
	bool have_patch_path = is_system_path_defined("<PATCH>");
	assert(have_patch_path);

	// ShapeDims
	// Starts at 0x96'th shape.
	ofstream shpdims;
	U7open(shpdims, PATCH_SHPDIMS);
	for (size_t i = c_first_obj_shape; i < num_shapes; i++) {
		shpdims.put(info[i].shpdims[0]);
		shpdims.put(info[i].shpdims[1]);
	}

	// WGTVOL
	ofstream wgtvol;
	U7open(wgtvol, PATCH_WGTVOL);
	for (size_t i = 0; i < num_shapes; i++) {
		wgtvol.put(info[i].weight);
		wgtvol.put(info[i].volume);
	}

	// TFA
	ofstream tfa;
	U7open(tfa, PATCH_TFA);
	for (size_t i = 0; i < num_shapes; i++)
		tfa.write(reinterpret_cast<char *>(&info[i].tfa[0]), 3);

	// Write data about drawing the weapon in an actor's hand
	ofstream wihh;
	U7open(wihh, PATCH_WIHH);
	size_t cnt = 0;            // Keep track of actual entries.
	for (size_t i = 0; i < num_shapes; i++)
		if (info[i].weapon_offsets == nullptr)
			Write2(wihh, 0);// None for this shape.
		else            // Write where it will go.
			Write2(wihh, 2 * num_shapes + 64 * (cnt++));
	for (size_t i = 0; i < num_shapes; i++)
		if (info[i].weapon_offsets)
			// There are two bytes per frame: 64 total
			wihh.write(reinterpret_cast<char *>(info[i].weapon_offsets), 64);
	wihh.close();

	ofstream occ;          // Write occlude.dat.
	U7open(occ, PATCH_OCCLUDE);
	unsigned char occbits[c_occsize];   // c_max_shapes bit flags.
	// +++++This could be rewritten better!
	memset(&occbits[0], 0, sizeof(occbits));
	for (size_t i = 0; i < sizeof(occbits); i++) {
		unsigned char bits = 0;
		int shnum = i * 8;  // Check each bit.
		for (size_t b = 0; b < 8; b++)
			if (shnum + b >= num_shapes)
				break;
			else if (info[shnum + b].occludes_flag)
				bits |= (1 << b);
		occbits[i] = bits;
	}
	occ.write(reinterpret_cast<char *>(occbits), sizeof(occbits));

	ofstream mfile;         // Now get monster info.
	U7open(mfile, PATCH_EQUIP); // Write 'equip.dat'.
	cnt = Monster_info::get_equip_cnt();
	Write_count(mfile, cnt);    // Exult extension.
	for (size_t i = 0; i < cnt; i++) {
		Equip_record &rec = Monster_info::get_equip(i);
		// 10 elements/record.
		for (int e = 0; e < 10; e++) {
			Equip_element &elem = rec.get(e);
			Write2(mfile, elem.get_shapenum());
			mfile.put(elem.get_probability());
			mfile.put(elem.get_quantity());
			Write2(mfile, 0);
		}
	}
	mfile.close();

	Functor_multidata_writer < Shape_info,
	                         Class_writer_functor < Armor_info, Shape_info,
	                         &Shape_info::armor > > armor(PATCH_ARMOR, info, num_shapes);
	armor.write_binary(game);

	Functor_multidata_writer < Shape_info,
	                         Class_writer_functor < Weapon_info, Shape_info,
	                         &Shape_info::weapon > > weapon(PATCH_WEAPONS, info, num_shapes);
	weapon.write_binary(game);

	Functor_multidata_writer < Shape_info,
	                         Class_writer_functor < Ammo_info, Shape_info,
	                         &Shape_info::ammo > > ammo(PATCH_AMMO, info, num_shapes);
	ammo.write_binary(game);

	Functor_multidata_writer < Shape_info,
	                         Class_writer_functor < Monster_info, Shape_info,
	                         &Shape_info::monstinf > > monstinf(PATCH_MONSTERS, info, num_shapes);
	monstinf.write_binary(game);

	Functor_multidata_writer < Shape_info,
	                         Binary_pair_writer_functor < gump_shape_flag, short, short, Shape_info,
	                         &Shape_info::gump_shape, &Shape_info::gump_font, 0 > > gump(
	                             PATCH_CONTAINER, info, num_shapes, 2);
	gump.write_binary(game);

	Functor_multidata_writer < Shape_info,
	                         Readytype_writer_functor > ready_type(PATCH_READY, info, num_shapes);
	ready_type.write_binary(game);

	Write_Shapeinf_text_data_file(game);
	Write_Bodies_text_data_file(game);
	Write_Paperdoll_text_data_file(game);
}

void Animation_info::write(
    ostream &out,       // Output stream.
    int shapenum,       // Shape number.
    Exult_Game game     // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, type);
	WriteInt(out, frame_count, type == FA_HOURLY);
	if (type != FA_HOURLY) {
		// We still have things to write.
		WriteInt(out, frame_delay);
		WriteInt(out, sfx_delay, type != FA_LOOPING);
		if (type == FA_LOOPING) {
			// We *still* have things to write.
			WriteInt(out, freeze_first);
			WriteInt(out, recycle, true);
		}
	}
}

void Body_info::write(
    ostream &out,       // Output stream.
    int shapenum,       // Shape number.
    Exult_Game game     // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, bshape);
	WriteInt(out, bframe, true);
}

void Frame_name_info::write(
    ostream &out,       // Output stream.
    int shapenum,       // Shape number.
    Exult_Game game     // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, frame < 0 ? -1 : (frame & 0xff));
	WriteInt(out, quality < 0 ? -1 : (quality & 0xff));
	int mtype = is_invalid() ? -255 : type;
	WriteInt(out, mtype, mtype < 0);
	if (mtype < 0)
		return;
	WriteInt(out, msgid, type == 0);
	if (type == 0)
		return;
	WriteInt(out, othermsg, true);
}

void Frame_flags_info::write(
    ostream &out,       // Output stream.
    int shapenum,       // Shape number.
    Exult_Game game     // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, frame < 0 ? -1 : (frame & 0xff));
	WriteInt(out, quality < 0 ? -1 : (quality & 0xff));
	unsigned int flags = is_invalid() ? 0 : m_flags;
	int size = 8 * sizeof(m_flags) - 1; // Bit count.
	int bit = 0;
	while (bit < size) {
		out << ((flags & (1 << bit)) != 0) << '/';
		bit++;
	}
	out << ((flags & (1 << size)) != 0) << endl;
}

void Frame_usecode_info::write(
    ostream &out,       // Output stream.
    int shapenum,       // Shape number.
    Exult_Game game     // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, frame < 0 ? -1 : (frame & 0xff));
	WriteInt(out, quality < 0 ? -1 : (quality & 0xff));
	if (is_invalid()) {
		WriteInt(out, 0);
		WriteInt(out, -1, true);
		return;
	}
	bool type = usecode_name.length() != 0;
	WriteInt(out, type);
	if (type)
		WriteStr(out, usecode_name, true);
	else
		WriteInt(out, usecode, true);
}

void Effective_hp_info::write(
    ostream &out,       // Output stream.
    int shapenum,       // Shape number.
    Exult_Game game     // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, frame < 0 ? -1 : (frame & 0xff));
	WriteInt(out, quality < 0 ? -1 : (quality & 0xff));
	WriteInt(out, is_invalid() ? 0 : hps, true);
}

void Warmth_info::write(
    ostream &out,       // Output stream.
    int shapenum,       // Shape number.
    Exult_Game game     // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, frame < 0 ? -1 : (frame & 0xff));
	WriteInt(out, is_invalid() ? 0 : warmth, true);
}

void Content_rules::write(
    ostream &out,       // Output stream.
    int shapenum,       // Shape number.
    Exult_Game game     // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, shape < 0 ? -1 : shape);
	WriteInt(out, is_invalid() ? true : accept, true);
}

void Paperdoll_npc::write(
    ostream &out,       // Output stream.
    int shapenum,       // Shape number.
    Exult_Game game     // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, is_female);
	WriteInt(out, translucent);
	WriteInt(out, body_shape);
	WriteInt(out, body_frame);
	WriteInt(out, head_shape);
	WriteInt(out, head_frame);
	WriteInt(out, head_frame_helm);
	WriteInt(out, arms_shape);
	WriteInt(out, arms_frame[0]);
	WriteInt(out, arms_frame[1]);
	WriteInt(out, arms_frame[2], true);
}

void Paperdoll_item::write(
    ostream &out,       // Output stream.
    int shapenum,       // Shape number.
    Exult_Game game     // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, world_frame < 0 ? -1 : world_frame);
	WriteInt(out, translucent);
	WriteInt(out, spot);
	int mtype = is_invalid() ? -255 : type;
	WriteInt(out, mtype, mtype < 0);
	if (mtype < 0)  // 'Invalid' entry; we are done.
		return;
	WriteInt(out, gender);
	WriteInt(out, shape);
	int i = 0;
	for (; i < 3; i++) {
		if (frame[i] == -1)
			return;     // Done writing.
		WriteInt(out, frame[i], frame[i + 1] == -1);
	}
	if (frame[3] != -1)
		WriteInt(out, frame[3], true);
}

void Explosion_info::write(
    ostream &out,       // Output stream.
    int shapenum,       // Shape number.
    Exult_Game game     // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, sprite, sfxnum < 0);
	if (sfxnum >= 0)
		WriteInt(out, sfxnum, true);
}

void SFX_info::write(
    ostream &out,       // Output stream.
    int shapenum,       // Shape number.
    Exult_Game game     // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, sfxnum);
	WriteInt(out, chance);
	WriteInt(out, range);
	WriteInt(out, random, extra < 0);
	if (extra >= 0)
		WriteInt(out, extra, true);
}

/*
 *  Write out a weapon-info entry to 'weapons.dat'.
 */

void Weapon_info::write(
    ostream &out,       // Output stream.
    int shapenum,       // Shape number.
    Exult_Game game     // Writing BG file.
) {
	uint8 buf[21];          // Entry length.
	uint8 *ptr = buf;
	Write2(ptr, shapenum);      // Bytes 0-1.
	Write2(ptr, ammo);
	Write2(ptr, projectile);
	*ptr++ = damage;
	unsigned char flags0 = (damage_type << 4) | (m_delete_depleted ? (1 << 3) : 0) |
	                       (m_no_blocking ? (1 << 2) : 0) | (m_explodes ? (1 << 1) : 0) | (m_lucky ? 1 : 0);
	*ptr++ = flags0;
	*ptr++ = (range << 3) | (uses << 1) | (m_autohit ? 1 : 0);
	unsigned char flags1 = (m_returns ? 1 : 0) | (m_need_target ? (1 << 1) : 0) | (rotation_speed << 4) |
	                       ((missile_speed == 4 ? 1 : 0) << 2);
	*ptr++ = flags1;
	int delay = missile_speed >= 3 ? 0 : (missile_speed == 2 ? 2 : 3);
	unsigned char flags2 = actor_frames | (delay << 5);
	*ptr++ = flags2;
	*ptr++ = powers;
	*ptr++ = 0;         // ??
	Write2(ptr, usecode);
	// BG:  Subtracted 1 from each sfx.
	int sfx_delta = game == BLACK_GATE ? -1 : 0;
	Write2(ptr, sfx - sfx_delta);
	Write2(ptr, hitsfx - sfx_delta);
	// Last 2 bytes unknown/unused.
	Write2(ptr, 0);
	out.write(reinterpret_cast<char *>(buf), sizeof(buf));
}

/*
 *  Write out an ammo-info entry to 'ammo.dat'.
 */

void Ammo_info::write(
    ostream &out,       // Output stream.
    int shapenum,       // Shape number.
    Exult_Game game     // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	uint8 buf[13];          // Entry length.
	uint8 *ptr = buf;
	Write2(ptr, shapenum);
	Write2(ptr, family_shape);
	Write2(ptr, sprite);
	*ptr++ = damage;
	unsigned char flags0;
	flags0 = (m_explodes ? (1 << 6) : 0) | ((homing ? 3 : drop_type) << 4) | (m_lucky ? 1 : 0) |
	         (m_autohit ? (1<< 1) : 0) | (m_returns ? (1 << 2) : 0) | (m_no_blocking ? (1 << 3) : 0);
	*ptr++ = flags0;
	*ptr++ = 0;         // Unknown.
	*ptr++ = damage_type << 4;
	*ptr++ = powers;
	Write2(ptr, 0);         // Unknown.
	out.write(reinterpret_cast<char *>(buf), sizeof(buf));
}

/*
 *  Write out an armor-info entry to 'armor.dat'.
 */

void Armor_info::write(
    ostream &out,       // Output stream.
    int shapenum,       // Shape number.
    Exult_Game game     // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	uint8 buf[10];          // Entry length.
	uint8 *ptr = buf;
	Write2(ptr, shapenum);
	*ptr++ = prot;          // Protection value.
	*ptr++ = 0;         // Unknown.
	*ptr++ = immune;        // Immunity flags.
	Write4(ptr, 0);         // Last 5 are unknown/unused.
	*ptr = 0;
	out.write(reinterpret_cast<char *>(buf), sizeof(buf));
}

/*
 *  Write out monster info. to 'monsters.dat'.
 */

void Monster_info::write(
    ostream &out,       // Output stream.
    int shapenum,       // Shape number.
    Exult_Game game     // Writing BG file.
) {
	uint8 buf[25];      // Entry length.
	memset(&buf[0], 0, sizeof(buf));
	uint8 *ptr = buf;
	Write2(ptr, shapenum);
	*ptr++ = (strength << 2) | (m_charm_safe ? 2 : 0) |
	         (m_sleep_safe ? 1 : 0);
	*ptr++ = (dexterity << 2) | (m_paralysis_safe ? 2 : 0) |
	         (m_curse_safe ? 1 : 0);
	*ptr++ = (intelligence << 2) | (m_int_b1 ? 2 : 0) |
	         (m_poison_safe ? 1 : 0);
	*ptr++ = (combat << 2) | alignment;
	*ptr++ = (armor << 4) | (m_splits ? 1 : 0) | (m_cant_die ? 2 : 0) |
	         (m_power_safe ? 4 : 0) | (m_death_safe ? 8 : 0);
	*ptr++ = 0;         // Unknown.
	*ptr++ = (weapon << 4) | reach;
	*ptr++ = flags;         // Byte 9.
	*ptr++ = vulnerable;
	*ptr++ = immune;
	*ptr++ = (m_cant_yell ? (1 << 5) : 0) |
	         (m_cant_bleed ? (1 << 6) : 0);
	*ptr++ = m_byte13 | (m_attackmode + 1);
	*ptr++ = equip_offset;
	*ptr++ = (m_can_teleport ? (1 << 0) : 0) |
	         (m_can_summon ? (1 << 1) : 0) |
	         (m_can_be_invisible ? (1 << 2) : 0);
	*ptr++ = 0;         // Unknown.
	int sfx_delta = game == BLACK_GATE ? -1 : 0;
	*ptr++ = static_cast<unsigned char>((sfx & 0xff) - sfx_delta);
	out.write(reinterpret_cast<char *>(buf), sizeof(buf));
}
