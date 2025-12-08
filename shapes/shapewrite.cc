/*
 *  shapewrite.cc - Write out the shape 'info' files.
 *
 *  Note:  Currently only used by ExultStudio.
 *
 *  Copyright (C) 2002-2022  The Exult Team
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
#	include <config.h>
#endif

#include "ammoinf.h"
#include "aniinf.h"
#include "armorinf.h"
#include "bodyinf.h"
#include "continf.h"
#include "data_utils.h"
#include "effhpinf.h"
#include "endianio.h"
#include "exceptions.h"
#include "expinf.h"
#include "frflags.h"
#include "frnameinf.h"
#include "frusefun.h"
#include "gumpinf.h"
#include "ignore_unused_variable_warning.h"
#include "lightinf.h"
#include "monstinf.h"
#include "npcdollinf.h"
#include "objdollinf.h"
#include "ready.h"
#include "sfxinf.h"
#include "shapeinf.h"
#include "shapevga.h"
#include "warminf.h"
#include "weaponinf.h"

#include <iomanip>
#include <map>
#include <vector>

using std::endl;
using std::ifstream;
using std::ios;
using std::ofstream;
using std::ostream;

// A custom writer functor.
class Readytype_writer_functor {
	Flag_check_functor<ready_type_flag, Shape_info> check;

public:
	void operator()(
			ostream& out, int index, Exult_Game game, Shape_info& info) {
		little_endian::Write2(out, index);
		unsigned char data = info.ready_type;
		data               = game == BLACK_GATE ? Ready_spot_to_BG(data)
												: Ready_spot_to_SI(data);
		data               = (data << 3) | info.spell_flag;
		Write1(out, data);
		for (unsigned i = 0; i < 6; i++) {
			out.put(0);
		}
	}

	bool operator()(Shape_info& info) {
		return check(info);
	}
};

/*
 *  Writes text data file according to passed writer functions.
 */
void Write_text_data_file(
		const char* fname,    // Name of file to read, sans extension
		const tcb::span<std::unique_ptr<Base_writer>>&
				writers,    // What to use to write data.
		int version, Exult_Game game) {
	size_t cnt = 0;
	for (auto& writer : writers) {
		cnt += writer->check();
	}
	if (cnt == 0u) {
		// Nothing to do.
		return;
	}
	char buf[50];
	snprintf(buf, sizeof(buf), "<PATCH>/%s.txt", fname);
	auto pOut = U7open_out(buf, true);    // (It's a text file.)
	if (!pOut) {
		return;
	}
	auto& out = *pOut;
	out << "#\tExult " << VERSION << " text message file."
		<< "\tWritten by ExultStudio." << std::endl;
	out << "%%section version" << std::endl
		<< ":" << version << std::endl
		<< "%%endsection" << std::endl;
	for (auto& writer : writers) {
		writer->write_text(out, game);
	}
}

void Shapes_vga_file::Write_Shapeinf_text_data_file(Exult_Game game) {
	const size_t num_shapes = shapes.size();
	// For explosions.
	using Explosion_writer = Functor_multidata_writer<
			Shape_info,
			Class_writer_functor<
					Explosion_info, Shape_info, &Shape_info::explosion>>;
	// For sound effects.
	using SFX_writer = Functor_multidata_writer<
			Shape_info,
			Class_writer_functor<SFX_info, Shape_info, &Shape_info::sfxinf>>;
	// For animations.
	using Animation_writer = Functor_multidata_writer<
			Shape_info,
			Class_writer_functor<
					Animation_info, Shape_info, &Shape_info::aniinf>>;
	// For usecode events.
	using Usecode_events_writer = Functor_multidata_writer<
			Shape_info,
			Bit_text_writer_functor<
					usecode_events_flag, unsigned short, Shape_info,
					&Shape_info::shape_flags, Shape_info::usecode_events>>;
	// For mountain tops.
	using Mountain_top_writer = Functor_multidata_writer<
			Shape_info, Text_writer_functor<
								mountain_top_flag, unsigned char, Shape_info,
								&Shape_info::mountain_top>>;
	// For monster food.
	using Monster_food_writer = Functor_multidata_writer<
			Shape_info, Text_writer_functor<
								monster_food_flag, short, Shape_info,
								&Shape_info::monster_food>>;
	// For actor flags.
	using Actor_flags_writer = Functor_multidata_writer<
			Shape_info, Bit_field_text_writer_functor<
								actor_flags_flag, unsigned char, Shape_info,
								&Shape_info::actor_flags>>;
	// For effective HPs.
	using Effective_hp_writer = Functor_multidata_writer<
			Shape_info,
			Vector_writer_functor<
					Effective_hp_info, Shape_info, &Shape_info::hpinf>>;
	// For lightweight objects.
	using Lightweight_writer = Functor_multidata_writer<
			Shape_info,
			Bit_text_writer_functor<
					lightweight_flag, unsigned short, Shape_info,
					&Shape_info::shape_flags, Shape_info::lightweight>>;
	// For light data.
	using Light_data_writer = Functor_multidata_writer<
			Shape_info, Vector_writer_functor<
								Light_info, Shape_info, &Shape_info::lightinf>>;
	// For warmth data.
	using Warmth_data_writer = Functor_multidata_writer<
			Shape_info, Vector_writer_functor<
								Warmth_info, Shape_info, &Shape_info::warminf>>;
	// For quantity frames.
	using Quantity_frames_writer = Functor_multidata_writer<
			Shape_info,
			Bit_text_writer_functor<
					quantity_frames_flag, unsigned short, Shape_info,
					&Shape_info::shape_flags, Shape_info::quantity_frames>>;
	// For locked objects.
	using Locked_containers_writer = Functor_multidata_writer<
			Shape_info, Bit_text_writer_functor<
								locked_flag, unsigned short, Shape_info,
								&Shape_info::shape_flags, Shape_info::locked>>;
	// For content rules.
	using Content_rules_writer = Functor_multidata_writer<
			Shape_info,
			Vector_writer_functor<
					Content_rules, Shape_info, &Shape_info::cntrules>>;
	// For highly explosive objects.
	using Explosive_writer = Functor_multidata_writer<
			Shape_info,
			Bit_text_writer_functor<
					is_volatile_flag, unsigned short, Shape_info,
					&Shape_info::shape_flags, Shape_info::is_volatile>>;
	// For frame names.
	using Frame_names_writer = Functor_multidata_writer<
			Shape_info,
			Vector_writer_functor<
					Frame_name_info, Shape_info, &Shape_info::nameinf>>;
	// For alternate ready spots.
	using Altready_writer = Functor_multidata_writer<
			Shape_info,
			Text_pair_writer_functor<
					altready_type_flag, unsigned char, Shape_info,
					&Shape_info::alt_ready1, &Shape_info::alt_ready2>>;
	// For barge parts.
	using Barge_type_writer = Functor_multidata_writer<
			Shape_info, Text_writer_functor<
								barge_type_flag, unsigned char, Shape_info,
								&Shape_info::barge_type>>;
	// For frame flags.
	using Frame_flags_writer = Functor_multidata_writer<
			Shape_info,
			Vector_writer_functor<
					Frame_flags_info, Shape_info, &Shape_info::frflagsinf>>;
	// For the jawbone.
	using Jawbone_writer = Functor_multidata_writer<
			Shape_info, Bit_text_writer_functor<
								jawbone_flag, unsigned short, Shape_info,
								&Shape_info::shape_flags, Shape_info::jawbone>>;
	// Mirrors.
	using Mirror_writer = Functor_multidata_writer<
			Shape_info, Bit_text_writer_functor<
								mirror_flag, unsigned short, Shape_info,
								&Shape_info::shape_flags, Shape_info::mirror>>;
	// Objects on fire.
	using On_fire_writer = Functor_multidata_writer<
			Shape_info, Bit_text_writer_functor<
								on_fire_flag, unsigned short, Shape_info,
								&Shape_info::shape_flags, Shape_info::on_fire>>;
	// Containers with unlimited storage.
	using Extradimensional_storage_writer = Functor_multidata_writer<
			Shape_info, Bit_text_writer_functor<
								extradimensional_storage_flag, unsigned short,
								Shape_info, &Shape_info::shape_flags,
								Shape_info::extradimensional_storage>>;
	// For field types.
	using Field_type_writer = Functor_multidata_writer<
			Shape_info, Text_writer_functor<
								field_type_flag, signed char, Shape_info,
								&Shape_info::field_type>>;
	// For frame usecode.
	using Frame_usecode_writer = Functor_multidata_writer<
			Shape_info,
			Vector_writer_functor<
					Frame_usecode_info, Shape_info, &Shape_info::frucinf>>;
	// For on-hit usecode.
	using On_hit_usecode_writer = Functor_multidata_writer<
			Shape_info, Text_writer_functor<
								on_hit_usecode_flag, int, Shape_info,
								&Shape_info::on_hit_usecode>>;

	std::array writers = make_unique_array<Base_writer>(
			// For explosions.
			std::make_unique<Explosion_writer>("explosions", info, num_shapes),
			// For sound effects.
			std::make_unique<SFX_writer>("shape_sfx", info, num_shapes),
			// For animations.
			std::make_unique<Animation_writer>("animation", info, num_shapes),
			// For usecode events.
			std::make_unique<Usecode_events_writer>(
					"usecode_events", info, num_shapes),
			// For mountain tops.
			std::make_unique<Mountain_top_writer>(
					"mountain_tops", info, num_shapes),
			// For monster food.
			std::make_unique<Monster_food_writer>(
					"monster_food", info, num_shapes),
			// For actor flags.
			std::make_unique<Actor_flags_writer>(
					"actor_flags", info, num_shapes),
			// For effective HPs.
			std::make_unique<Effective_hp_writer>(
					"effective_hps", info, num_shapes),
			// For lightweight objects.
			std::make_unique<Lightweight_writer>(
					"lightweight_object", info, num_shapes),
			// For light data.
			std::make_unique<Light_data_writer>("light_data", info, num_shapes),
			// For warmth data.
			std::make_unique<Warmth_data_writer>(
					"warmth_data", info, num_shapes),
			// For quantity frames.
			std::make_unique<Quantity_frames_writer>(
					"quantity_frames", info, num_shapes),
			// For locked objects.
			std::make_unique<Locked_containers_writer>(
					"locked_containers", info, num_shapes),
			// For content rules.
			std::make_unique<Content_rules_writer>(
					"content_rules", info, num_shapes),
			// For highly explosive objects.
			std::make_unique<Explosive_writer>(
					"volatile_explosive", info, num_shapes),
			// For frame names.
			std::make_unique<Frame_names_writer>(
					"framenames", info, num_shapes),
			// For alternate ready spots.
			std::make_unique<Altready_writer>("altready", info, num_shapes),
			// For barge parts.
			std::make_unique<Barge_type_writer>("barge_type", info, num_shapes),
			// For frame flags.
			std::make_unique<Frame_flags_writer>(
					"frame_powers", info, num_shapes),
			// For the jawbone.
			std::make_unique<Jawbone_writer>("is_jawbone", info, num_shapes),
			// Mirrors.
			std::make_unique<Mirror_writer>("is_mirror", info, num_shapes),
			// Objects on fire.
			std::make_unique<On_fire_writer>("on_fire", info, num_shapes),
			// Containers with unlimited storage.
			std::make_unique<Extradimensional_storage_writer>(
					"extradimensional_storage", info, num_shapes),
			// For field types.
			std::make_unique<Field_type_writer>("field_type", info, num_shapes),
			// For frame usecode.
			std::make_unique<Frame_usecode_writer>(
					"frame_usecode", info, num_shapes),
			// For on hit usecode.
			std::make_unique<On_hit_usecode_writer>(
					"on_hit_usecode", info, num_shapes));
	Write_text_data_file("shape_info", writers, 7, game);
}

void Shapes_vga_file::Write_Bodies_text_data_file(Exult_Game game) {
	const size_t num_shapes = shapes.size();
	using Body_shape_writer = Functor_multidata_writer<
			Shape_info, Bit_text_writer_functor<
								is_body_flag, unsigned short, Shape_info,
								&Shape_info::shape_flags, Shape_info::is_body>>;
	using Body_list_writer = Functor_multidata_writer<
			Shape_info,
			Class_writer_functor<Body_info, Shape_info, &Shape_info::body>>;
	std::array writers = make_unique_array<Base_writer>(
			std::make_unique<Body_shape_writer>("bodyshapes", info, num_shapes),
			std::make_unique<Body_list_writer>("bodylist", info, num_shapes));
	Write_text_data_file("bodies", writers, 2, game);
}

void Shapes_vga_file::Write_Paperdoll_text_data_file(Exult_Game game) {
	const size_t num_shapes   = shapes.size();
	using NpcPaperdoll_writer = Functor_multidata_writer<
			Shape_info,
			Class_writer_functor<
					Paperdoll_npc, Shape_info, &Shape_info::npcpaperdoll>>;
	using Paperdoll_writer = Functor_multidata_writer<
			Shape_info,
			Vector_writer_functor<
					Paperdoll_item, Shape_info, &Shape_info::objpaperdoll>>;
	std::array writers = make_unique_array<Base_writer>(
			std::make_unique<NpcPaperdoll_writer>(
					"characters", info, num_shapes),
			std::make_unique<Paperdoll_writer>("items", info, num_shapes));
	Write_text_data_file("paperdol_info", writers, 3, game);
}

void Shapes_vga_file::Write_Gumpinf_text_data_file(Exult_Game game) {
	ignore_unused_variable_warning(game);

	// Collect entries that should be in the patch file
	std::map<int, const Gump_info*> container_data;
	std::map<int, const Gump_info*> checkmark_data;
	std::map<int, const Gump_info*> special_data;

	for (const auto& [shapenum, gumpinf] : Gump_info::gump_info_map) {
		if (gumpinf.has_area && gumpinf.is_container_dirty()) {
			container_data[shapenum] = &gumpinf;
		}
		if (gumpinf.has_checkmark && gumpinf.is_checkmark_dirty()) {
			checkmark_data[shapenum] = &gumpinf;
		}
		if (gumpinf.is_special && gumpinf.is_special_dirty()) {
			special_data[shapenum] = &gumpinf;
		}
	}

	if (container_data.empty() && checkmark_data.empty()
		&& special_data.empty()) {
		return;
	}

	auto pOut = U7open_out("<PATCH>/gump_info.txt", true);
	if (!pOut) {
		return;
	}
	auto& out = *pOut;

	out << "#\tExult " << VERSION << " gump info file."
		<< "\tWritten by ExultStudio." << std::endl;
	out << "%%section version" << std::endl
		<< ":1" << std::endl
		<< "%%endsection" << std::endl;

	// container_area
	if (!container_data.empty()) {
		out << "%%section container_area" << std::endl;
		for (const auto& [shapenum, info] : container_data) {
			out << ":" << shapenum << "/" << info->container_x << "/"
				<< info->container_y << "/" << info->container_w << "/"
				<< info->container_h << std::endl;
		}
		out << "%%endsection" << std::endl;
	}

	// checkmark_pos
	if (!checkmark_data.empty()) {
		out << "%%section checkmark_pos" << std::endl;
		for (const auto& [shapenum, info] : checkmark_data) {
			out << ":" << shapenum << "/" << info->checkmark_x << "/"
				<< info->checkmark_y << "/" << info->checkmark_shape
				<< std::endl;
		}
		out << "%%endsection" << std::endl;
	}

	// special
	if (!special_data.empty()) {
		out << "%%section special" << std::endl;
		for (const auto& [shapenum, info] : special_data) {
			out << ":" << shapenum << std::endl;
		}
		out << "%%endsection" << std::endl;
	}
}

/*
 *  Write out data files about shapes.
 */

void Shapes_vga_file::write_info(Exult_Game game) {
	const size_t num_shapes      = shapes.size();
	const bool   have_patch_path = is_system_path_defined("<PATCH>");
	assert(have_patch_path);

	// ShapeDims
	// Starts at 0x96'th shape.
	auto pShpdims = U7open_out(PATCH_SHPDIMS);
	if (!pShpdims) {
		return;
	}
	auto& shpdims = *pShpdims;
	for (size_t i = c_first_obj_shape; i < num_shapes; i++) {
		shpdims.put(info[i].shpdims[0]);
		shpdims.put(info[i].shpdims[1]);
	}

	// WGTVOL
	auto pWgtvol = U7open_out(PATCH_WGTVOL);
	if (!pWgtvol) {
		return;
	}
	auto& wgtvol = *pWgtvol;
	for (size_t i = 0; i < num_shapes; i++) {
		wgtvol.put(info[i].weight);
		wgtvol.put(info[i].volume);
	}

	// TFA
	auto pTfa = U7open_out(PATCH_TFA);
	if (!pTfa) {
		return;
	}
	auto& tfa = *pTfa;
	for (size_t i = 0; i < num_shapes; i++) {
		tfa.write(reinterpret_cast<char*>(&info[i].tfa[0]), 3);
	}

	// Write data about drawing the weapon in an actor's hand
	auto pWihh = U7open_out(PATCH_WIHH);
	if (!pWihh) {
		return;
	}
	auto&  wihh = *pWihh;
	size_t cnt  = 0;    // Keep track of actual entries.
	for (size_t i = 0; i < num_shapes; i++) {
		if (info[i].weapon_offsets == nullptr) {
			little_endian::Write2(wihh, 0);    // None for this shape.
		} else {                               // Write where it will go.
			little_endian::Write2(wihh, 2 * num_shapes + 64 * (cnt++));
		}
	}
	for (size_t i = 0; i < num_shapes; i++) {
		if (info[i].weapon_offsets) {
			// There are two bytes per frame: 64 total
			wihh.write(reinterpret_cast<char*>(info[i].weapon_offsets), 64);
		}
	}

	// Write occlude.dat.
	auto pOcc = U7open_out(PATCH_OCCLUDE);
	if (!pOcc) {
		return;
	}
	auto&         occ = *pOcc;
	unsigned char occbits[c_occsize];    // c_max_shapes bit flags.
	// +++++This could be rewritten better!
	memset(&occbits[0], 0, sizeof(occbits));
	for (size_t i = 0; i < sizeof(occbits); i++) {
		unsigned char bits  = 0;
		const int     shnum = i * 8;    // Check each bit.
		for (size_t b = 0; b < 8; b++) {
			if (shnum + b >= num_shapes) {
				break;
			} else if (info[shnum + b].occludes_flag) {
				bits |= (1 << b);
			}
		}
		occbits[i] = bits;
	}
	occ.write(reinterpret_cast<char*>(occbits), sizeof(occbits));

	// Now get monster info.
	auto pMfile = U7open_out(PATCH_EQUIP);    // Write 'equip.dat'.
	if (!pMfile) {
		return;
	}
	auto& mfile = *pMfile;
	cnt         = Monster_info::get_equip_cnt();
	Write_count(mfile, cnt);    // Exult extension.
	for (size_t i = 0; i < cnt; i++) {
		Equip_record& rec = Monster_info::get_equip(i);
		// 10 elements/record.
		for (int e = 0; e < 10; e++) {
			const Equip_element& elem = rec.get(e);
			little_endian::Write2(mfile, elem.get_shapenum());
			mfile.put(elem.get_probability());
			mfile.put(elem.get_quantity());
			little_endian::Write2(mfile, 0);
		}
	}

	Functor_multidata_writer<
			Shape_info,
			Class_writer_functor<Armor_info, Shape_info, &Shape_info::armor>>
			armor(PATCH_ARMOR, info, num_shapes);
	armor.write_binary(game);

	Functor_multidata_writer<
			Shape_info,
			Class_writer_functor<Weapon_info, Shape_info, &Shape_info::weapon>>
			weapon(PATCH_WEAPONS, info, num_shapes);
	weapon.write_binary(game);

	Functor_multidata_writer<
			Shape_info,
			Class_writer_functor<Ammo_info, Shape_info, &Shape_info::ammo>>
			ammo(PATCH_AMMO, info, num_shapes);
	ammo.write_binary(game);

	Functor_multidata_writer<
			Shape_info,
			Class_writer_functor<
					Monster_info, Shape_info, &Shape_info::monstinf>>
			monstinf(PATCH_MONSTERS, info, num_shapes);
	monstinf.write_binary(game);

	Functor_multidata_writer<
			Shape_info,
			Binary_pair_writer_functor<
					gump_shape_flag, short, short, Shape_info,
					&Shape_info::gump_shape, &Shape_info::gump_font, 0>>
			gump(PATCH_CONTAINER, info, num_shapes, 2);
	gump.write_binary(game);

	Functor_multidata_writer<Shape_info, Readytype_writer_functor> ready_type(
			PATCH_READY, info, num_shapes);
	ready_type.write_binary(game);

	Write_Shapeinf_text_data_file(game);
	Write_Bodies_text_data_file(game);
	Write_Paperdoll_text_data_file(game);
	Write_Gumpinf_text_data_file(game);    // Add this line
}

void Animation_info::write(
		ostream&   out,         // Output stream.
		int        shapenum,    // Shape number.
		Exult_Game game         // Writing BG file.
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
		ostream&   out,         // Output stream.
		int        shapenum,    // Shape number.
		Exult_Game game         // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, bshape);
	WriteInt(out, bframe, true);
}

void Frame_name_info::write(
		ostream&   out,         // Output stream.
		int        shapenum,    // Shape number.
		Exult_Game game         // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, frame < 0 ? -1 : (frame & 0xff));
	WriteInt(out, quality < 0 ? -1 : (quality & 0xff));
	const int mtype = is_invalid() ? -255 : type;
	WriteInt(out, mtype, mtype < 0);
	if (mtype < 0) {
		return;
	}
	WriteInt(out, msgid, type == 0);
	if (type == 0) {
		return;
	}
	WriteInt(out, othermsg, true);
}

void Frame_flags_info::write(
		ostream&   out,         // Output stream.
		int        shapenum,    // Shape number.
		Exult_Game game         // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, frame < 0 ? -1 : (frame & 0xff));
	WriteInt(out, quality < 0 ? -1 : (quality & 0xff));
	const unsigned int flags = is_invalid() ? 0 : m_flags;
	const int          size  = 8 * sizeof(m_flags) - 1;    // Bit count.
	int                bit   = 0;
	while (bit < size) {
		out << ((flags & (1U << bit)) != 0) << '/';
		bit++;
	}
	out << ((flags & (1U << size)) != 0) << endl;
}

void Frame_usecode_info::write(
		ostream&   out,         // Output stream.
		int        shapenum,    // Shape number.
		Exult_Game game         // Writing BG file.
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
	const bool type = usecode_name.length() != 0;
	WriteInt(out, type);
	if (type) {
		WriteStr(out, usecode_name, true);
	} else {
		WriteInt(out, usecode, true);
	}
}

void Effective_hp_info::write(
		ostream&   out,         // Output stream.
		int        shapenum,    // Shape number.
		Exult_Game game         // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, frame < 0 ? -1 : (frame & 0xff));
	WriteInt(out, quality < 0 ? -1 : (quality & 0xff));
	WriteInt(out, is_invalid() ? 0 : hps, true);
}

void Light_info::write(
		ostream&   out,         // Output stream.
		int        shapenum,    // Shape number.
		Exult_Game game         // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, frame < 0 ? -1 : (frame & 0xff));
	WriteInt(out, is_invalid() ? 0 : light, true);
}

void Warmth_info::write(
		ostream&   out,         // Output stream.
		int        shapenum,    // Shape number.
		Exult_Game game         // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, frame < 0 ? -1 : (frame & 0xff));
	WriteInt(out, is_invalid() ? 0 : warmth, true);
}

void Content_rules::write(
		ostream&   out,         // Output stream.
		int        shapenum,    // Shape number.
		Exult_Game game         // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, shape < 0 ? -1 : shape);
	WriteInt(out, is_invalid() ? true : accept, true);
}

void Paperdoll_npc::write(
		ostream&   out,         // Output stream.
		int        shapenum,    // Shape number.
		Exult_Game game         // Writing BG file.
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
		ostream&   out,         // Output stream.
		int        shapenum,    // Shape number.
		Exult_Game game         // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, world_frame < 0 ? -1 : world_frame);
	WriteInt(out, translucent);
	WriteInt(out, spot);
	const int mtype = is_invalid() ? -255 : type;
	WriteInt(out, mtype, mtype < 0);
	if (mtype < 0) {    // 'Invalid' entry; we are done.
		return;
	}
	WriteInt(out, gender);
	WriteInt(out, shape);
	for (int i = 0; i < 3; i++) {
		if (frame[i] == -1) {
			return;    // Done writing.
		}
		WriteInt(out, frame[i], frame[i + 1] == -1);
	}
	if (frame[3] != -1) {
		WriteInt(out, frame[3], true);
	}
}

void Explosion_info::write(
		ostream&   out,         // Output stream.
		int        shapenum,    // Shape number.
		Exult_Game game         // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, sprite, sfxnum < 0);
	if (sfxnum >= 0) {
		WriteInt(out, sfxnum, true);
	}
}

void SFX_info::write(
		ostream&   out,         // Output stream.
		int        shapenum,    // Shape number.
		Exult_Game game         // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	out << ":";
	WriteInt(out, shapenum);
	WriteInt(out, sfxnum);
	WriteInt(out, chance);
	WriteInt(out, range);
	WriteInt(out, random, false);
	WriteInt(out, extra, false);
	WriteInt(out, volume, frame < 0);
	if (frame >= 0) {
		WriteInt(out, frame, true);
	}
}

/*
 *  Write out a weapon-info entry to 'weapons.dat'.
 */

void Weapon_info::write(
		ostream&   out,         // Output stream.
		int        shapenum,    // Shape number.
		Exult_Game game         // Writing BG file.
) {
	uint8  buf[21];    // Entry length.
	uint8* ptr = buf;
	little_endian::Write2(ptr, shapenum);    // Bytes 0-1.
	little_endian::Write2(ptr, ammo);
	little_endian::Write2(ptr, projectile);
	Write1(ptr, damage);
	const unsigned char flags0
			= (damage_type << 4) | (m_delete_depleted ? (1 << 3) : 0)
			  | (m_no_blocking ? (1 << 2) : 0) | (m_explodes ? (1 << 1) : 0)
			  | (m_lucky ? 1 : 0);
	Write1(ptr, flags0);
	Write1(ptr, (range << 3) | (uses << 1) | (m_autohit ? 1 : 0));
	const unsigned char flags1
			= (m_returns ? 1 : 0) | (m_need_target ? (1 << 1) : 0)
			  | (rotation_speed << 4) | ((missile_speed == 4 ? 1 : 0) << 2);
	Write1(ptr, flags1);
	const int delay = missile_speed >= 3 ? 0 : (missile_speed == 2 ? 2 : 3);
	const unsigned char flags2 = actor_frames | (delay << 5);
	Write1(ptr, flags2);
	Write1(ptr, powers);
	Write1(ptr, 0);    // ??
	little_endian::Write2(ptr, usecode);
	// BG:  Subtracted 1 from each sfx.
	const int sfx_delta = game == BLACK_GATE ? -1 : 0;
	little_endian::Write2(ptr, sfx - sfx_delta);
	little_endian::Write2(ptr, hitsfx - sfx_delta);
	// Last 2 bytes unknown/unused.
	little_endian::Write2(ptr, 0);
	out.write(reinterpret_cast<char*>(buf), sizeof(buf));
}

/*
 *  Write out an ammo-info entry to 'ammo.dat'.
 */

void Ammo_info::write(
		ostream&   out,         // Output stream.
		int        shapenum,    // Shape number.
		Exult_Game game         // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	uint8  buf[13];    // Entry length.
	uint8* ptr = buf;
	little_endian::Write2(ptr, shapenum);
	little_endian::Write2(ptr, family_shape);
	little_endian::Write2(ptr, sprite);
	Write1(ptr, damage);
	unsigned char flags0;
	flags0 = (m_explodes ? (1 << 6) : 0) | ((homing ? 3 : drop_type) << 4)
			 | (m_lucky ? 1 : 0) | (m_autohit ? (1 << 1) : 0)
			 | (m_returns ? (1 << 2) : 0) | (m_no_blocking ? (1 << 3) : 0);
	Write1(ptr, flags0);
	Write1(ptr, 0);    // Unknown.
	Write1(ptr, damage_type << 4);
	Write1(ptr, powers);
	little_endian::Write2(ptr, 0);    // Unknown.
	out.write(reinterpret_cast<char*>(buf), sizeof(buf));
}

/*
 *  Write out an armor-info entry to 'armor.dat'.
 */

void Armor_info::write(
		ostream&   out,         // Output stream.
		int        shapenum,    // Shape number.
		Exult_Game game         // Writing BG file.
) {
	ignore_unused_variable_warning(game);
	uint8  buf[10];    // Entry length.
	uint8* ptr = buf;
	little_endian::Write2(ptr, shapenum);
	Write1(ptr, prot);                // Protection value.
	Write1(ptr, 0);                   // Unknown.
	Write1(ptr, immune);              // Immunity flags.
	little_endian::Write4(ptr, 0);    // Last 5 are unknown/unused.
	*ptr = 0;
	out.write(reinterpret_cast<char*>(buf), sizeof(buf));
}

/*
 *  Write out monster info. to 'monsters.dat'.
 */

void Monster_info::write(
		ostream&   out,         // Output stream.
		int        shapenum,    // Shape number.
		Exult_Game game         // Writing BG file.
) {
	uint8 buf[25];    // Entry length.
	memset(&buf[0], 0, sizeof(buf));
	uint8* ptr = buf;
	little_endian::Write2(ptr, shapenum);
	Write1(ptr,
		   (strength << 2) | (m_charm_safe ? 2 : 0) | (m_sleep_safe ? 1 : 0));
	Write1(ptr, (dexterity << 2) | (m_paralysis_safe ? 2 : 0)
						| (m_curse_safe ? 1 : 0));
	Write1(ptr,
		   (intelligence << 2) | (m_int_b1 ? 2 : 0) | (m_poison_safe ? 1 : 0));
	Write1(ptr, (combat << 2) | alignment);
	Write1(ptr, (armor << 4) | (m_splits ? 1 : 0) | (m_cant_die ? 2 : 0)
						| (m_power_safe ? 4 : 0) | (m_death_safe ? 8 : 0));
	Write1(ptr, 0);    // Unknown).
	Write1(ptr, (weapon << 4) | reach);
	Write1(ptr, flags);    // Byte 9).
	Write1(ptr, vulnerable);
	Write1(ptr, immune);
	Write1(ptr, (m_cant_yell ? (1 << 5) : 0) | (m_cant_bleed ? (1 << 6) : 0));
	Write1(ptr, m_byte13 | (m_attackmode + 1));
	Write1(ptr, equip_offset);
	Write1(ptr, (m_can_teleport ? (1 << 0) : 0) | (m_can_summon ? (1 << 1) : 0)
						| (m_can_be_invisible ? (1 << 2) : 0));
	Write1(ptr, 0);    // Unknown.
	const int sfx_delta = game == BLACK_GATE ? -1 : 0;
	Write1(ptr, static_cast<unsigned char>((sfx & 0xff) - sfx_delta));
	out.write(reinterpret_cast<char*>(buf), sizeof(buf));
}
