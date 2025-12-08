/*
 *  shapevga.cc - Handle the 'shapes.vga' file and associated info.
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "shapevga.h"

#include "ammoinf.h"
#include "aniinf.h"
#include "armorinf.h"
#include "bodyinf.h"
#include "continf.h"
#include "data/exult_bg_flx.h"
#include "data/exult_si_flx.h"
#include "data_utils.h"
#include "effhpinf.h"
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
#include "utils.h"
#include "warminf.h"
#include "weaponinf.h"

using std::ifstream;
using std::ios;
using namespace std;

// Special case ID reader functors.

extern int get_skinvar(const std::string& key);

// Multiracial support in Exult.
class Paperdoll_npc_ID_reader {
public:
	int operator()(std::istream& in, int index, int version, bool binary) {
		ignore_unused_variable_warning(index, version, binary);
		if (in.peek() == '%') {
			const std::string key = ReadStr(in);
			// We need these for Exult, but not for ES.
			// For now, a compromise/hack in that ES defines
			// a version of this function which always returns
			// -1, while Exult has another which forwards to
			// Shapeinfo_lookup::get_skinvar
			const int id = get_skinvar(key);
			return id < 0 ? -1 : id;
		}
		return ReadInt(in);
	}
};

// For backward compatibility.
class Body_ID_reader {
public:
	int operator()(std::istream& in, int index, int version, bool binary) {
		ignore_unused_variable_warning(binary);
		return version == 1 ? index : ReadInt(in);
	}
};

// Special case reader functor.
class Gump_reader_functor {
public:
	bool operator()(
			std::istream& in, int version, bool patch, Exult_Game game,
			Shape_info& info) {
		ignore_unused_variable_warning(patch, game);
		info.gump_shape = little_endian::Read2(in);
		if (version >= 2) {
			info.gump_font = little_endian::Read2(in);
		} else {
			info.gump_font = -1;
		}
		return true;
	}
};

// A few custom post-read functors.
class Ready_type_functor {
	Patch_flags_functor<ready_type_flag, Shape_info> setflags;

public:
	void operator()(
			std::istream& in, int version, bool patch, Exult_Game game,
			Shape_info& info) {
		unsigned char ready = info.ready_type;
		info.spell_flag     = (ready & 1) != 0;
		ready >>= 3;
		const unsigned char spot = game == BLACK_GATE
										   ? Ready_spot_from_BG(ready)
										   : Ready_spot_from_SI(ready);
		info.ready_type          = spot;
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
		default:
			break;
		}
		setflags(in, version, patch, game, info);
	}
};

class Actor_flags_functor {
	Patch_flags_functor<actor_flags_flag, Shape_info> setflags;

public:
	void operator()(
			std::istream& in, int version, bool patch, Exult_Game game,
			Shape_info& info) {
		setflags(in, version, patch, game, info);
	}
};

class Paperdoll_npc_functor {
public:
	void operator()(
			std::istream& in, int version, bool patch, Exult_Game game,
			Shape_info& info) {
		ignore_unused_variable_warning(patch, game);
		if (version < 3) {
			// We need this for backward compatibility.
			// We use the setter methods so that the info
			// will get saved by ES if that is needed.
			info.set_gump_data(ReadInt(in, -1), -1);
		}
	}
};

/*
 *  Reads text data file and parses it according to passed
 *  parser functions.
 */
void Read_text_data_file(
		// Name of file to read, sans extension
		const char* fname,
		// What to use to parse data.
		const tcb::span<std::unique_ptr<Base_reader>>& parsers,
		// The names of the sections
		const tcb::span<std::string_view>& sections, bool editing,
		Exult_Game game, int resource) {
	auto value_or = [](std::optional<int> opt) {
		return opt ? *opt : 1;
	};
	Text_msg_file_reader static_reader = [&]() {
		if (game == BLACK_GATE || game == SERPENT_ISLE) {
			/*  ++++ Not because of ES.
			snprintf(buf, sizeof(buf), "config/%s", fname);
			str_int_pair resource = game->get_resource(buf);
			U7object txtobj(resource.str, resource.num);
			*/
			const bool  bg = game == BLACK_GATE;
			const char* flexfile
					= bg ? BUNDLE_CHECK(BUNDLE_EXULT_BG_FLX, EXULT_BG_FLX)
						 : BUNDLE_CHECK(BUNDLE_EXULT_SI_FLX, EXULT_SI_FLX);
			IExultDataSource ds(flexfile, resource);
			return Text_msg_file_reader(ds);
		}
		std::string     file = "<STATIC>/" + std::string(fname) + ".txt";
		IFileDataSource ds(file, false);
		if (!ds.good()) {
			throw file_open_exception(file);
		}
		return Text_msg_file_reader(ds);
	}();
	Text_msg_file_reader patch_reader = [&]() {
		std::string file = "<PATCH>/" + std::string(fname) + ".txt";
		if (!U7exists(file)) {
			return Text_msg_file_reader();
		}
		IFileDataSource ds(file, false);
		if (!ds.good()) {
			if (!editing) {
				throw file_open_exception(file);
			}
			return Text_msg_file_reader();
		}
		return Text_msg_file_reader(ds);
	}();

	int static_version = value_or(static_reader.get_version());
	int patch_version  = value_or(patch_reader.get_version());
	std::vector<std::string> strings;

	for (size_t i = 0; i < sections.size(); i++) {
		static_reader.get_section_strings(sections[i], strings);
		parsers[i]->parse(strings, static_version, false, game);
		patch_reader.get_section_strings(sections[i], strings);
		parsers[i]->parse(strings, patch_version, true, game);
	}
}

void Shapes_vga_file::Read_Shapeinf_text_data_file(
		bool editing, Exult_Game game_type) {
	std::array sections{
			"explosions"sv,
			"shape_sfx"sv,
			"animation"sv,
			"usecode_events"sv,
			"mountain_tops"sv,
			"monster_food"sv,
			"actor_flags"sv,
			"effective_hps"sv,
			"lightweight_object"sv,
			"light_data"sv,
			"warmth_data"sv,
			"quantity_frames"sv,
			"locked_containers"sv,
			"content_rules"sv,
			"volatile_explosive"sv,
			"framenames"sv,
			"altready"sv,
			"barge_type"sv,
			"frame_powers"sv,
			"is_jawbone"sv,
			"is_mirror"sv,
			"on_fire"sv,
			"extradimensional_storage"sv,
			"field_type"sv,
			"frame_usecode"sv,
			"on_hit_usecode"sv};
	// For explosions.
	using Explosion_reader = Functor_multidata_reader<
			Shape_info,
			Class_reader_functor<
					Explosion_info, Shape_info, &Shape_info::explosion>>;
	// For sound effects.
	using SFX_reader = Functor_multidata_reader<
			Shape_info,
			Class_reader_functor<SFX_info, Shape_info, &Shape_info::sfxinf>>;
	// For animations.
	using Animation_reader = Functor_multidata_reader<
			Shape_info,
			Class_reader_functor<
					Animation_info, Shape_info, &Shape_info::aniinf>>;
	// For usecode events.
	using Usecode_events_reader = Functor_multidata_reader<
			Shape_info,
			Bit_text_reader_functor<
					unsigned short, Shape_info, &Shape_info::shape_flags,
					Shape_info::usecode_events>,
			Patch_flags_functor<usecode_events_flag, Shape_info>>;
	// For mountain tops.
	using Mountain_top_reader = Functor_multidata_reader<
			Shape_info,
			Text_reader_functor<
					unsigned char, Shape_info, &Shape_info::mountain_top>,
			Patch_flags_functor<mountain_top_flag, Shape_info>>;
	// For monster food.
	using Monster_food_reader = Functor_multidata_reader<
			Shape_info,
			Text_reader_functor<short, Shape_info, &Shape_info::monster_food>,
			Patch_flags_functor<monster_food_flag, Shape_info>>;
	// For actor flags.
	using Actor_flags_reader = Functor_multidata_reader<
			Shape_info,
			Bit_field_text_reader_functor<
					unsigned char, Shape_info, &Shape_info::actor_flags>,
			Actor_flags_functor>;
	// For effective HPs.
	using Effective_hp_reader = Functor_multidata_reader<
			Shape_info,
			Vector_reader_functor<
					Effective_hp_info, Shape_info, &Shape_info::hpinf>>;
	// For lightweight objects.
	using Lightweight_reader = Functor_multidata_reader<
			Shape_info,
			Bit_text_reader_functor<
					unsigned short, Shape_info, &Shape_info::shape_flags,
					Shape_info::lightweight>,
			Patch_flags_functor<lightweight_flag, Shape_info>>;
	// For light data.
	using Light_data_reader = Functor_multidata_reader<
			Shape_info, Vector_reader_functor<
								Light_info, Shape_info, &Shape_info::lightinf>>;
	// For warmth data.
	using Warmth_data_reader = Functor_multidata_reader<
			Shape_info, Vector_reader_functor<
								Warmth_info, Shape_info, &Shape_info::warminf>>;
	// For quantity frames.
	using Quantity_frames_reader = Functor_multidata_reader<
			Shape_info,
			Bit_text_reader_functor<
					unsigned short, Shape_info, &Shape_info::shape_flags,
					Shape_info::quantity_frames>,
			Patch_flags_functor<quantity_frames_flag, Shape_info>>;
	// For locked objects.
	using Locked_containers_reader = Functor_multidata_reader<
			Shape_info,
			Bit_text_reader_functor<
					unsigned short, Shape_info, &Shape_info::shape_flags,
					Shape_info::locked>,
			Patch_flags_functor<locked_flag, Shape_info>>;
	// For content rules.
	using Content_rules_reader = Functor_multidata_reader<
			Shape_info,
			Vector_reader_functor<
					Content_rules, Shape_info, &Shape_info::cntrules>>;
	// For highly explosive objects.
	using Explosive_reader = Functor_multidata_reader<
			Shape_info,
			Bit_text_reader_functor<
					unsigned short, Shape_info, &Shape_info::shape_flags,
					Shape_info::is_volatile>,
			Patch_flags_functor<is_volatile_flag, Shape_info>>;
	// For frame names.
	using Frame_names_reader = Functor_multidata_reader<
			Shape_info,
			Vector_reader_functor<
					Frame_name_info, Shape_info, &Shape_info::nameinf>>;
	// For alternate ready spots.
	using Altready_reader = Functor_multidata_reader<
			Shape_info,
			Text_pair_reader_functor<
					unsigned char, Shape_info, &Shape_info::alt_ready1,
					&Shape_info::alt_ready2>,
			Patch_flags_functor<altready_type_flag, Shape_info>>;
	// For barge parts.
	using Barge_type_reader = Functor_multidata_reader<
			Shape_info,
			Text_reader_functor<
					unsigned char, Shape_info, &Shape_info::barge_type>,
			Patch_flags_functor<barge_type_flag, Shape_info>>;
	// For frame flags.
	using Frame_flags_reader = Functor_multidata_reader<
			Shape_info,
			Vector_reader_functor<
					Frame_flags_info, Shape_info, &Shape_info::frflagsinf>>;
	// For the jawbone.
	using Jawbone_reader = Functor_multidata_reader<
			Shape_info,
			Bit_text_reader_functor<
					unsigned short, Shape_info, &Shape_info::shape_flags,
					Shape_info::jawbone>,
			Patch_flags_functor<jawbone_flag, Shape_info>>;
	// Mirrors.
	using Mirror_reader = Functor_multidata_reader<
			Shape_info,
			Bit_text_reader_functor<
					unsigned short, Shape_info, &Shape_info::shape_flags,
					Shape_info::mirror>,
			Patch_flags_functor<mirror_flag, Shape_info>>;
	// Objects on fire.
	using On_fire_reader = Functor_multidata_reader<
			Shape_info,
			Bit_text_reader_functor<
					unsigned short, Shape_info, &Shape_info::shape_flags,
					Shape_info::on_fire>,
			Patch_flags_functor<on_fire_flag, Shape_info>>;
	// Containers with unlimited storage.
	using Extradimensional_storage_reader = Functor_multidata_reader<
			Shape_info,
			Bit_text_reader_functor<
					unsigned short, Shape_info, &Shape_info::shape_flags,
					Shape_info::extradimensional_storage>,
			Patch_flags_functor<extradimensional_storage_flag, Shape_info>>;
	// For field types.
	using Field_type_reader = Functor_multidata_reader<
			Shape_info,
			Text_reader_functor<
					signed char, Shape_info, &Shape_info::field_type>,
			Patch_flags_functor<field_type_flag, Shape_info>>;
	// For frame usecode.
	using Frame_usecode_reader = Functor_multidata_reader<
			Shape_info,
			Vector_reader_functor<
					Frame_usecode_info, Shape_info, &Shape_info::frucinf>>;
	// For on-hit usecode.
	using On_hit_usecode_reader = Functor_multidata_reader<
			Shape_info,
			Text_reader_functor<int, Shape_info, &Shape_info::on_hit_usecode>,
			Patch_flags_functor<on_hit_usecode_flag, Shape_info>>;

	std::array readers = make_unique_array<Base_reader>(
			std::make_unique<Explosion_reader>(info),
			std::make_unique<SFX_reader>(info),
			std::make_unique<Animation_reader>(info),
			std::make_unique<Usecode_events_reader>(info),
			std::make_unique<Mountain_top_reader>(info),
			std::make_unique<Monster_food_reader>(info),
			std::make_unique<Actor_flags_reader>(info),
			std::make_unique<Effective_hp_reader>(info),
			std::make_unique<Lightweight_reader>(info),
			std::make_unique<Light_data_reader>(info),
			std::make_unique<Warmth_data_reader>(info),
			std::make_unique<Quantity_frames_reader>(info),
			std::make_unique<Locked_containers_reader>(info),
			std::make_unique<Content_rules_reader>(info),
			std::make_unique<Explosive_reader>(info),
			std::make_unique<Frame_names_reader>(info),
			std::make_unique<Altready_reader>(info),
			std::make_unique<Barge_type_reader>(info),
			std::make_unique<Frame_flags_reader>(info),
			std::make_unique<Jawbone_reader>(info),
			std::make_unique<Mirror_reader>(info),
			std::make_unique<On_fire_reader>(info),
			std::make_unique<Extradimensional_storage_reader>(info),
			std::make_unique<Field_type_reader>(info),
			std::make_unique<Frame_usecode_reader>(info),
			std::make_unique<On_hit_usecode_reader>(info));
	static_assert(sections.size() == readers.size());
	const int flxres = game_type == BLACK_GATE ? EXULT_BG_FLX_SHAPE_INFO_TXT
											   : EXULT_SI_FLX_SHAPE_INFO_TXT;

	Read_text_data_file(
			"shape_info", readers, sections, editing, game_type, flxres);
}

void Shapes_vga_file::Read_Bodies_text_data_file(
		bool editing, Exult_Game game_type) {
	std::array sections{"bodyshapes"sv, "bodylist"sv};
	using Body_shape_reader = Functor_multidata_reader<
			Shape_info,
			Bit_text_reader_functor<
					unsigned short, Shape_info, &Shape_info::shape_flags,
					Shape_info::is_body>,
			Patch_flags_functor<is_body_flag, Shape_info>, Body_ID_reader>;
	using Body_list_reader = Functor_multidata_reader<
			Shape_info,
			Class_reader_functor<Body_info, Shape_info, &Shape_info::body>>;
	std::array readers = make_unique_array<Base_reader>(
			std::make_unique<Body_shape_reader>(info),
			std::make_unique<Body_list_reader>(info));
	static_assert(sections.size() == readers.size());
	const int flxres = game_type == BLACK_GATE ? EXULT_BG_FLX_BODIES_TXT
											   : EXULT_SI_FLX_BODIES_TXT;

	Read_text_data_file(
			"bodies", readers, sections, editing, game_type, flxres);
}

void Shapes_vga_file::Read_Paperdoll_text_data_file(
		bool editing, Exult_Game game_type) {
	std::array sections{"characters"sv, "items"sv};
	using NpcPaperdoll_reader_functor = Functor_multidata_reader<
			Shape_info,
			Class_reader_functor<
					Paperdoll_npc, Shape_info, &Shape_info::npcpaperdoll>,
			Paperdoll_npc_functor, Paperdoll_npc_ID_reader>;
	using ObjPaperdoll_reader_functor = Functor_multidata_reader<
			Shape_info,
			Vector_reader_functor<
					Paperdoll_item, Shape_info, &Shape_info::objpaperdoll>>;
	std::array readers = make_unique_array<Base_reader>(
			std::make_unique<NpcPaperdoll_reader_functor>(info),
			std::make_unique<ObjPaperdoll_reader_functor>(info));
	static_assert(sections.size() == readers.size());
	const int flxres = game_type == BLACK_GATE ? EXULT_BG_FLX_PAPERDOL_INFO_TXT
											   : EXULT_SI_FLX_PAPERDOL_INFO_TXT;

	Read_text_data_file(
			"paperdol_info", readers, sections, editing, game_type, flxres);
}

void Shapes_vga_file::Read_Gumpinf_text_data_file(
		bool editing, Exult_Game game_type) {
	if (Gump_info::was_any_modified()) {
		return;
	}
	std::array sections{"container_area"sv, "checkmark_pos"sv, "special"sv};

	// Functor for reading container area
	struct Container_area_functor {
		bool operator()(
				std::istream& in, int version, bool patch, Exult_Game game,
				Gump_info& ginfo) const {
			ignore_unused_variable_warning(version, patch, game);
			ginfo.container_x = ReadInt(in);
			ginfo.container_y = ReadInt(in);
			ginfo.container_w = ReadInt(in);
			ginfo.container_h = ReadInt(in);
			ginfo.has_area    = true;
			if (patch) {
				ginfo.set_container_from_patch(true);
			}
			return true;
		}
	};

	// Functor for reading checkmark position
	struct Checkmark_pos_functor {
		bool operator()(
				std::istream& in, int version, bool patch, Exult_Game game,
				Gump_info& ginfo) const {
			ignore_unused_variable_warning(version, game);
			ginfo.checkmark_x     = ReadInt(in);
			ginfo.checkmark_y     = ReadInt(in);
			ginfo.checkmark_shape = ReadInt(in);
			ginfo.has_checkmark   = true;
			if (patch) {
				ginfo.set_checkmark_from_patch(true);
			}
			return true;
		}
	};

	// Functor for reading special shapes
	struct Special_functor {
		bool operator()(
				std::istream& in, int version, bool patch, Exult_Game game,
				Gump_info& ginfo) const {
			ignore_unused_variable_warning(in, version, game);
			ginfo.is_special = true;
			if (patch) {
				ginfo.set_special_from_patch(true);
			}
			return true;
		}
	};

	// The template parameters are: <Info, Functor>  (Info comes FIRST!)
	// The constructor takes: std::map<int, Info>&
	using Container_area_reader
			= Functor_multidata_reader<Gump_info, Container_area_functor>;
	using Checkmark_pos_reader
			= Functor_multidata_reader<Gump_info, Checkmark_pos_functor>;
	using Special_reader = Functor_multidata_reader<Gump_info, Special_functor>;

	std::array readers = make_unique_array<Base_reader>(
			std::make_unique<Container_area_reader>(Gump_info::gump_info_map),
			std::make_unique<Checkmark_pos_reader>(Gump_info::gump_info_map),
			std::make_unique<Special_reader>(Gump_info::gump_info_map));
	static_assert(sections.size() == readers.size());

	const int flxres = game_type == BLACK_GATE ? EXULT_BG_FLX_GUMP_INFO_TXT
											   : EXULT_SI_FLX_GUMP_INFO_TXT;

	Read_text_data_file(
			"gump_info", readers, sections, editing, game_type, flxres);

	for (auto& [shnum, ginfo] : Gump_info::gump_info_map) {
		if (ginfo.has_checkmark && ginfo.checkmark_shape > 0) {
			// Mark the checkmark shape itself
			auto& checkmark_info
					= Gump_info::gump_info_map[ginfo.checkmark_shape];
			checkmark_info.is_checkmark = true;
		}
	}
}

/*
 *  Reload static data for weapons, ammo and mosters to
 *  fix data that was lost by earlier versions of ES.
 */
void Shapes_vga_file::fix_old_shape_info(Exult_Game game    // Which game.
) {
	if (!info_read) {    // Read info first.
		read_info(game, true);
	}
	Functor_multidata_reader<
			Shape_info,
			Class_reader_functor<Weapon_info, Shape_info, &Shape_info::weapon>>
			weapon(info);
	weapon.read(WEAPONS, false, game);
	Functor_multidata_reader<
			Shape_info,
			Class_reader_functor<Ammo_info, Shape_info, &Shape_info::ammo>>
			ammo(info);
	ammo.read(AMMO, false, game);
	Functor_multidata_reader<
			Shape_info,
			Class_reader_functor<
					Monster_info, Shape_info, &Shape_info::monstinf>>
			monstinf(info);
	monstinf.read(MONSTERS, false, game);
}

/*
 *  Reload info.
 */

void Shapes_vga_file::reload_info(Exult_Game game    // Which game.
) {
	info_read = false;
	info.clear();
	Gump_info::clear();
	read_info(game);
}

/*
 *  Read in data files about shapes.
 *
 *  Output: 0 if error.
 */

bool Shapes_vga_file::read_info(
		Exult_Game game,      // Which game.
		bool       editing    // True to allow files to not exist.
) {
	if (info_read) {
		return false;
	}
	info_read = true;

	// ShapeDims

	// Starts at 0x96'th shape.
	if (IExultDataSource shpdims(SHPDIMS, PATCH_SHPDIMS, 0); shpdims.good()) {
		for (size_t i = c_first_obj_shape; i < shapes.size() && !shpdims.eof();
			 i++) {
			info[i].shpdims[0] = shpdims.read1();
			info[i].shpdims[1] = shpdims.read1();
		}
	}

	// WGTVOL
	if (IExultDataSource wgtvol(WGTVOL, PATCH_WGTVOL, 0); wgtvol.good()) {
		for (size_t i = 0; i < shapes.size() && !wgtvol.eof(); i++) {
			info[i].weight = wgtvol.read1();
			info[i].volume = wgtvol.read1();
		}
	}

	// TFA
	if (IExultDataSource tfa(TFA, PATCH_TFA, 0); tfa.good()) {
		for (size_t i = 0; i < shapes.size() && !tfa.eof(); i++) {
			tfa.read(info[i].tfa, sizeof(info[i].tfa));
			info[i].set_tfa_data();
		}
	}

	if (game == BLACK_GATE || game == SERPENT_ISLE) {
		// Animation data at the end of BG and SI TFA.DAT
		// We *should* blow up if TFA not there.
		IExultDataSource stfa(TFA, 0);
		if (!stfa.good()) {
			throw file_open_exception(TFA);
		}
		stfa.seek(static_cast<size_t>(3 * 1024));
		unsigned char buf[512];
		stfa.read(buf, sizeof(buf));
		unsigned char* ptr = buf;
		for (size_t i = 0; i < sizeof(buf); i++, ptr++) {
			int    val   = *ptr;
			size_t shape = 2 * i;
			while (val != 0) {
				if ((val & 0xf) != 0) {
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
	if (IExultDataSource wihh(WIHH, PATCH_WIHH, 0); wihh.good()) {
		const size_t   cnt = shapes.size();
		unsigned short offsets[c_max_shapes];
		for (size_t i = 0; i < cnt; i++) {
			offsets[i] = wihh.read2();
		}
		for (size_t i = 0; i < cnt; i++) {
			// A zero offset means there is no record also ignore invalid
			// offsets past the end of the file or point into the offset table
			if (offsets[i] == 0 || offsets[i] > (wihh.getSize() - 64)
				|| offsets[i] < 2048) {
				info[i].weapon_offsets = nullptr;
			} else {
				wihh.seek(offsets[i]);
				// There are two bytes per frame: 64 total
				info[i].weapon_offsets = new unsigned char[64];
				for (size_t j = 0; j < 32; j++) {
					unsigned char x   = wihh.read1();
					unsigned char y   = wihh.read1();
					size_t        off = 2 * j;
					// Set x/y to 255 if weapon is not to be drawn
					// In the file x/y are either 64 or 255:
					// I am assuming that they mean the same
					if (x > 63 || y > 63) {
						x = y = 255;
					}
					info[i].weapon_offsets[off]     = x;
					info[i].weapon_offsets[off + 1] = y;
				}
			}
		}
	}

	// Read flags from occlude.dat.
	if (IExultDataSource occ(OCCLUDE, PATCH_OCCLUDE, 0); occ.good()) {
		unsigned char occbits[c_occsize];    // c_max_shapes bit flags.
		// Ensure sensible defaults.
		std::fill(std::begin(occbits), std::end(occbits), 0);
		occ.read(occbits, occ.getSize());
		for (size_t i = 0; i < occ.getSize(); i++) {
			unsigned char bits  = occbits[i];
			const int     shnum = i * 8;    // Check each bit.
			for (int b = 0; bits != 0u; b++, bits = bits >> 1) {
				if ((bits & 1) != 0) {
					info[shnum + b].occludes_flag = true;
				}
			}
		}
	}

	// Get 'equip.dat'.
	if (IExultDataSource mfile(EQUIP, PATCH_EQUIP, 0); mfile.good()) {
		// Get # entries (with Exult extension).
		const int num_recs = Read_count(mfile);
		Monster_info::reserve_equip(num_recs);
		for (int i = 0; i < num_recs; i++) {
			Equip_record equip;
			// 10 elements/record.
			for (int elem = 0; elem < 10; elem++) {
				const int      shnum = mfile.read2();
				const unsigned prob  = mfile.read1();
				const unsigned quant = mfile.read1();
				mfile.skip(2);
				equip.set(elem, shnum, prob, quant);
			}
			Monster_info::add_equip(equip);
		}
	}

	Functor_multidata_reader<
			Shape_info,
			Class_reader_functor<Armor_info, Shape_info, &Shape_info::armor>>
			armor(info);
	armor.read(ARMOR, false, game);
	armor.read(PATCH_ARMOR, true, game);

	Functor_multidata_reader<
			Shape_info,
			Class_reader_functor<Weapon_info, Shape_info, &Shape_info::weapon>>
			weapon(info);
	weapon.read(WEAPONS, false, game);
	weapon.read(PATCH_WEAPONS, true, game);

	Functor_multidata_reader<
			Shape_info,
			Class_reader_functor<Ammo_info, Shape_info, &Shape_info::ammo>>
			ammo(info);
	ammo.read(AMMO, false, game);
	ammo.read(PATCH_AMMO, true, game);

	Functor_multidata_reader<
			Shape_info,
			Class_reader_functor<
					Monster_info, Shape_info, &Shape_info::monstinf>>
			monstinf(info);
	monstinf.read(MONSTERS, false, game);
	monstinf.read(PATCH_MONSTERS, true, game);

	Functor_multidata_reader<
			Shape_info, Gump_reader_functor,
			Patch_flags_functor<gump_shape_flag, Shape_info>>
			gump(info, true);
	if (game == BLACK_GATE || game == SERPENT_ISLE) {
		gump.read(
				game, game == BLACK_GATE ? EXULT_BG_FLX_CONTAINER_DAT
										 : EXULT_SI_FLX_CONTAINER_DAT);
	} else {
		gump.read(CONTAINER, false, game);
	}
	gump.read(PATCH_CONTAINER, true, game);

	Functor_multidata_reader<
			Shape_info,
			Binary_reader_functor<
					unsigned char, Shape_info, &Shape_info::ready_type, 6>,
			Ready_type_functor>
			ready(info);
	ready.read(READY, false, game);
	ready.read(PATCH_READY, true, game);

	Read_Shapeinf_text_data_file(editing, game);
	Read_Bodies_text_data_file(editing, game);
	Read_Paperdoll_text_data_file(editing, game);
	Read_Gumpinf_text_data_file(editing, game);

	// Ensure valid ready spots for all shapes.
	const unsigned char defready = game == BLACK_GATE ? backpack : rhand;
	zinfo.ready_type             = defready;
	for (auto& it : info) {
		Shape_info& inf = it.second;
		if (inf.ready_type == invalid_spot) {
			inf.ready_type = defready;
		}
	}
	bool auto_modified = false;
	for (auto& it : info) {
		const int   shnum = it.first;
		Shape_info& inf   = it.second;
		if (inf.has_monster_info()) {
			Monster_info* minf = inf.monstinf;
			if (minf->can_teleport()) {
				std::cerr << "Shape " << shnum
						  << " is a monster that can teleport, teleport flag "
							 "moved from Monster info ( monster.dat ) to Actor "
							 "info ( shape_info.txt ) as NPC flags."
						  << std::endl;
				inf.set_actor_flag(Shape_info::teleports);
				minf->set_can_teleport(false);
				auto_modified = true;
			}
			if (minf->can_summon()) {
				std::cerr << "Shape " << shnum
						  << " is a monster that can summon, summon flag moved "
							 "from Monster info ( monster.dat ) to Actor info "
							 "( shape_info.txt ) as NPC flags."
						  << std::endl;
				inf.set_actor_flag(Shape_info::summons);
				minf->set_can_summon(false);
				auto_modified = true;
			}
			if (minf->can_be_invisible()) {
				std::cerr << "Shape " << shnum
						  << " is a monster that can be invisible, invisible "
							 "flag moved from Monster info ( monster.dat ) to "
							 "Actor info ( shape_info.txt ) as NPC flags."
						  << std::endl;
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
		const char* nm,        // Path to file.
		int         u7drag,    // # from u7drag.h, or -1.
		const char* nm2        // Path to patch version, or 0.
		)
		: Vga_file(nm, u7drag, nm2) {}

void Shapes_vga_file::init() {
	if (is_system_path_defined("<PATCH>") && U7exists(PATCH_SHAPES)) {
		load(SHAPES_VGA, PATCH_SHAPES);
	} else {
		load(SHAPES_VGA);
	}
	info_read = false;
}

/*
 *  Make a spot for a new shape, and delete frames in existing shape.
 *
 *  Output: ->shape, or 0 if invalid shapenum.
 */

Shape* Shapes_vga_file::new_shape(int shapenum) {
	Shape* newshape = Vga_file::new_shape(shapenum);
	return newshape;
}
