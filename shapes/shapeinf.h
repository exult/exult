/**
 ** Shapeinf.h: Info. about shapes read from various data files.
 **
 ** Written: 4/29/99 - JSF
 **/

#ifndef INCL_SHAPEINF
#define INCL_SHAPEINF   1

/*
Copyright (C) 1998  Jeffrey S. Freedman

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

#include <iosfwd>
#include <vector>
#include <map>
#include <algorithm>
#include <iostream>
#include "baseinf.h"
#include "utils.h"

class Armor_info;
class Weapon_info;
class Ammo_info;
class Monster_info;
class SFX_info;
class Animation_info;
class Explosion_info;
class Body_info;
class Paperdoll_npc;
class Paperdoll_item;
class Effective_hp_info;
class Frame_name_info;
class Frame_flags_info;
class Frame_usecode_info;
class Warmth_info;
class Content_rules;
class Shapes_vga_file;

template <typename T, class Info, T Info::*data>
class Text_reader_functor;
template <typename T, class Info, T Info::*data1, T Info::*data2>
class Text_pair_reader_functor;
template <typename T, class Info, T Info::*data, int bit>
class Bit_text_reader_functor;
template <typename T, class Info, T Info::*data>
class Bit_field_text_reader_functor;
template <typename T, class Info, T Info::*data, unsigned pad>
class Binary_reader_functor;
template < typename T1, typename T2, class Info,
         T1 Info::*data1, T2 Info::*data2, unsigned pad >
class Binary_pair_reader_functor;
template <typename T, class Info, T *Info::*data>
class Class_reader_functor;
template <typename T, class Info, std::vector<T> Info::*data>
class Vector_reader_functor;
template <class Info>
class Null_functor;
template <int flag, class Info>
class Patch_flags_functor;

class Gump_reader_functor;
class Ready_type_functor;
class Actor_flags_functor;
class Paperdoll_npc_functor;

template <int flag, class Info>
class Flag_check_functor;
template <int flag, typename T, class Info, T Info::*data>
class Text_writer_functor;
template <int flag, typename T, class Info, T Info::*data1, T Info::*data2>
class Text_pair_writer_functor;
template <int flag, typename T, class Info, T Info::*data, int bit>
class Bit_text_writer_functor;
template <int flag, typename T, class Info, T Info::*data>
class Bit_field_text_writer_functor;
template <int flag, typename T, class Info, T Info::*data, int pad>
class Binary_writer_functor;
template < int flag, typename T1, typename T2, class Info,
         T1 Info::*data1, T2 Info::*data2, int pad >
class Binary_pair_writer_functor;
template <typename T, class Info, T *Info::*data>
class Class_writer_functor;
template <typename T, class Info, std::vector<T> Info::*data>
class Vector_writer_functor;

class Readytype_writer_functor;

enum Data_flag_bits {
    tf_ready_type_flag = 0,
    tf_gump_shape_flag,
    tf_monster_food_flag,
    tf_actor_flags_flag,
    tf_mountain_top_flag,
    tf_altready_type_flag,
    tf_barge_type_flag,
    tf_field_type_flag,
    // ++++ Keep shape flags last!
    tf_usecode_events_flag,
    tf_is_body_flag,
    tf_lightweight_flag,
    tf_quantity_frames_flag,
    tf_locked_flag,
    tf_is_volatile_flag,
    tf_jawbone_flag,
    tf_mirror_flag,
	tf_on_fire_flag,
	tf_extradimensional_storage_flag
};
enum Data_flag_names {
    ready_type_flag      = (1U << tf_ready_type_flag),
    gump_shape_flag      = (1U << tf_gump_shape_flag),
    monster_food_flag    = (1U << tf_monster_food_flag),
    actor_flags_flag     = (1U << tf_actor_flags_flag),
    mountain_top_flag    = (1U << tf_mountain_top_flag),
    altready_type_flag   = (1U << tf_altready_type_flag),
    barge_type_flag      = (1U << tf_barge_type_flag),
    field_type_flag      = (1U << tf_field_type_flag),
    usecode_events_flag  = (1U << tf_usecode_events_flag),
    is_body_flag         = (1U << tf_is_body_flag),
    lightweight_flag     = (1U << tf_lightweight_flag),
    quantity_frames_flag = (1U << tf_quantity_frames_flag),
    locked_flag          = (1U << tf_locked_flag),
    is_volatile_flag     = (1U << tf_is_volatile_flag),
    jawbone_flag         = (1U << tf_jawbone_flag),
    mirror_flag          = (1U << tf_mirror_flag),
	on_fire_flag         = (1U << tf_on_fire_flag),
	extradimensional_storage_flag = (1U << tf_extradimensional_storage_flag)
};

/*
 *  This class contains information only about shapes from "shapes.vga".
 */
class Shape_info {
protected:
	// For some non-class data (see Data_flag_names enum).
	unsigned int modified_flags = 0;
	unsigned int frompatch_flags = 0;
	// For class data (to indicate an invalid entry should
	// be written by ES).
	unsigned int have_static_flags = 0;
	unsigned char tfa[3] = {0};       // From "tfa.dat".+++++Keep for
	//   debugging, for now.
	// 3D dimensions in tiles:
	unsigned char dims[3] = {0};      //   (x, y, z)
	unsigned char weight = 0, volume = 0;   // From "wgtvol.dat".
	unsigned char shpdims[2] = {0};   // From "shpdims.dat".
	unsigned char *weapon_offsets = nullptr;  // From "wihh.dat": pixel offsets
	//   for drawing weapon in hand
	Armor_info *armor = nullptr;          // From armor.dat.
	Weapon_info *weapon = nullptr;        // From weapon.dat, if a weapon.
	Ammo_info *ammo = nullptr;            // From ammo.dat, if ammo.
	Monster_info *monstinf = nullptr;     // From monster.dat.
	SFX_info *sfxinf = nullptr;
	Animation_info *aniinf = nullptr;
	Explosion_info *explosion = nullptr;
	Body_info *body = nullptr;
	Paperdoll_npc *npcpaperdoll = nullptr;
	// These vectors should be totally ordered by the strict-weak
	// order operator defined for the classes.
	std::vector<Paperdoll_item> objpaperdoll;
	std::vector<Effective_hp_info> hpinf;
	std::vector<Frame_name_info> nameinf;
	std::vector<Frame_flags_info> frflagsinf;
	std::vector<Frame_usecode_info> frucinf;
	std::vector<Warmth_info> warminf;
	std::vector<Content_rules> cntrules;
	short gump_shape = -1;       // From container.dat.
	short gump_font = -1;        // From container.dat v2+.
	short monster_food = -1;
	unsigned short shape_flags = 0;
	unsigned char mountain_top = 0;
	unsigned char barge_type = 0;
	unsigned char actor_flags = 0;
	char field_type = -1;
	char ready_type = -1;    // From "ready.dat": where item can be worn.
	char alt_ready1 = -1;    // Alternate spot where item can be worn.
	char alt_ready2 = -1;    // Second alternate spot where item can be worn.
	bool spell_flag = false;        // Flagged as spell in 'ready.dat'.
	bool occludes_flag = false;     // Flagged in 'occlude.dat'.  Roof.
	void set_tfa_data() {   // Set fields from tfa.
		dims[0] = static_cast<unsigned char>(1 + (tfa[2] & 7));
		dims[1] = static_cast<unsigned char>(1 + ((tfa[2] >> 3) & 7));
		dims[2] = static_cast<unsigned char>(tfa[0] >> 5);
	}
	// Set/clear tfa bit.
	void set_tfa(int i, int bit, bool tf) {
		tfa[i] = static_cast<unsigned char>(tf ?
		                                    (tfa[i] | (1 << bit)) : (tfa[i]&~(1 << bit)));
	}
public:
	enum Actor_flags {
	    cold_immune = 0,
	    doesnt_eat,
	    teleports,
	    summons,
	    turns_invisible,
	    armageddon_safe,
	    quake_walk,
	    goblin
	};
	enum Shape_flags {
	    usecode_events = 0,
	    is_body,
	    lightweight,
	    quantity_frames,
	    locked,
	    is_volatile,
	    jawbone,
	    mirror,
		on_fire,
		extradimensional_storage
	};
	enum Mountain_tops {
	    not_mountain_top = 0,
	    normal_mountain_top,
	    snow_mountain_top
	};
	enum Barge_types {
	    barge_generic = 0,
	    barge_raft,
	    barge_seat,
	    barge_sails,
	    barge_wheel,
	    barge_draftanimal,
	    barge_turtle
	};
	enum Field_types {
	    no_field = -1,
	    fire_field = 0,
	    sleep_field,
	    poison_field,
	    caltrops
	};
	friend class Shapes_vga_file;   // Class that reads in data.

	template <typename T, class Info, T Info::*data>
	friend class Text_reader_functor;
	template <typename T, class Info, T Info::*data1, T Info::*data2>
	friend class Text_pair_reader_functor;
	template <typename T, class Info, T Info::*data, int bit>
	friend class Bit_text_reader_functor;
	template <typename T, class Info, T Info::*data>
	friend class Bit_field_text_reader_functor;
	template <typename T, class Info, T Info::*data, unsigned pad>
	friend class Binary_reader_functor;
	template < typename T1, typename T2, class Info,
	         T1 Info::*data1, T2 Info::*data2, unsigned pad >
	friend class Binary_pair_reader_functor;
	template <typename T, class Info, T *Info::*data>
	friend class Class_reader_functor;
	template <typename T, class Info, std::vector<T> Info::*data>
	friend class Vector_reader_functor;
	template <class Info>
	friend class Null_functor;
	template <int flag, class Info>
	friend class Patch_flags_functor;

	friend class Gump_reader_functor;
	friend class Ready_type_functor;
	friend class Actor_flags_functor;
	friend class Paperdoll_npc_functor;

	template <int flag, class Info>
	friend class Flag_check_functor;
	template <int flag, typename T, class Info, T Info::*data>
	friend class Text_writer_functor;
	template <int flag, typename T, class Info, T Info::*data1, T Info::*data2>
	friend class Text_pair_writer_functor;
	template <int flag, typename T, class Info, T Info::*data, int bit>
	friend class Bit_text_writer_functor;
	template <int flag, typename T, class Info, T Info::*data>
	friend class Bit_field_text_writer_functor;
	template <int flag, typename T, class Info, T Info::*data, int pad>
	friend class Binary_writer_functor;
	template < int flag, typename T1, typename T2, class Info,
	         T1 Info::*data1, T2 Info::*data2, int pad >
	friend class Binary_pair_writer_functor;
	template <typename T, class Info, T *Info::*data>
	friend class Class_writer_functor;
	template <typename T, class Info, std::vector<T> Info::*data>
	friend class Vector_writer_functor;

	friend class Readytype_writer_functor;

	Shape_info();
	// This copy constructor and assignment operator intentionally cause
	// errors.
	Shape_info(const Shape_info &other);
	Shape_info &operator = (const Shape_info &other);
	~Shape_info();
	void copy(const Shape_info &inf2, bool skip_dolls = false);

	int get_weight() const {    // Get weight, volume.
		return weight;
	}
	int get_volume() const {
		return volume;
	}
	void set_weight_volume(int w, int v) {
		weight = static_cast<unsigned char>(w);
		volume = static_cast<unsigned char>(v);
	}

	int get_armor() const;
	int get_armor_immunity() const;

	int get_explosion_sprite() const;
	int get_explosion_sfx() const;

	int get_body_shape() const;
	int get_body_frame() const;

	bool has_weapon_info() const {
		return weapon != nullptr;
	}
	const Weapon_info *get_weapon_info_safe() const;
	const Weapon_info *get_weapon_info() const {
		return weapon;
	}
	Weapon_info *set_weapon_info(bool tf);

	bool has_ammo_info() const {
		return ammo != nullptr;
	}
	const Ammo_info *get_ammo_info_safe() const;
	const Ammo_info *get_ammo_info() const {
		return ammo;
	}
	Ammo_info *set_ammo_info(bool tf);

	bool has_armor_info() const {
		return armor != nullptr;
	}
	const Armor_info *get_armor_info() const {
		return armor;
	}
	Armor_info *set_armor_info(bool tf);

	bool has_monster_info() const {
		return monstinf != nullptr;
	}
	const Monster_info *get_monster_info_safe() const;
	const Monster_info *get_monster_info() const {
		return monstinf;
	}
	Monster_info *set_monster_info(bool tf);

	bool has_npc_paperdoll_info() const {
		return npcpaperdoll != nullptr;
	}
	const Paperdoll_npc *get_npc_paperdoll() const {
		return npcpaperdoll;
	}
	Paperdoll_npc *set_npc_paperdoll_info(bool tf);
	const Paperdoll_npc *get_npc_paperdoll_safe(bool sex) const;

	bool has_sfx_info() const {
		return sfxinf != nullptr;
	}
	const SFX_info *get_sfx_info() const {
		return sfxinf;
	}
	SFX_info *set_sfx_info(bool tf);

	bool has_explosion_info() const {
		return explosion != nullptr;
	}
	const Explosion_info *get_explosion_info() const {
		return explosion;
	}
	Explosion_info *set_explosion_info(bool tf);

	bool has_animation_info() const {
		return aniinf != nullptr;
	}
	const Animation_info *get_animation_info() const {
		return aniinf;
	}
	const Animation_info *get_animation_info_safe(int shnum, int nframes);
	Animation_info *set_animation_info(bool tf);

	bool has_body_info() const {
		return body != nullptr;
	}
	const Body_info *get_body_info() const {
		return body;
	}
	Body_info *set_body_info(bool tf);

	bool has_paperdoll_info() const;
	const std::vector<Paperdoll_item> &get_paperdoll_info() const {
		return objpaperdoll;
	}
	std::vector<Paperdoll_item> &set_paperdoll_info(bool tf);
	void clean_invalid_paperdolls();
	void clear_paperdoll_info();
	void add_paperdoll_info(Paperdoll_item &add);
	const Paperdoll_item *get_item_paperdoll(int frame, int spot) const;
	bool is_object_allowed(int frame, int spot) const {
		return get_item_paperdoll(frame, spot) != nullptr;
	}

	bool has_content_rules() const;
	const std::vector<Content_rules> &get_content_rules() const {
		return cntrules;
	}
	std::vector<Content_rules> &set_content_rules(bool tf);
	void clean_invalid_content_rules();
	void clear_content_rules();
	void add_content_rule(Content_rules &add);
	bool is_shape_accepted(int shape) const;

	bool has_effective_hp_info() const;
	const std::vector<Effective_hp_info> &get_effective_hp_info() const {
		return hpinf;
	}
	std::vector<Effective_hp_info> &set_effective_hp_info(bool tf);
	void clean_invalid_hp_info();
	void clear_effective_hp_info();
	void add_effective_hp_info(Effective_hp_info &add);
	int get_effective_hps(int frame, int quality) const;

	bool has_frame_name_info() const;
	const std::vector<Frame_name_info> &get_frame_name_info() const {
		return nameinf;
	}
	std::vector<Frame_name_info> &set_frame_name_info(bool tf);
	void clean_invalid_name_info();
	void clear_frame_name_info();
	void add_frame_name_info(Frame_name_info &add);
	const Frame_name_info *get_frame_name(int frame, int quality) const;

	bool has_frame_usecode_info() const;
	const std::vector<Frame_usecode_info> &get_frame_usecode_info() const {
		return frucinf;
	}
	std::vector<Frame_usecode_info> &set_frame_usecode_info(bool tf);
	void clean_invalid_usecode_info();
	void clear_frame_usecode_info();
	void add_frame_usecode_info(Frame_usecode_info &add);
	const Frame_usecode_info *get_frame_usecode(int frame, int quality) const;

	bool has_frame_flags() const;
	const std::vector<Frame_flags_info> &get_frame_flags() const {
		return frflagsinf;
	}
	std::vector<Frame_flags_info> &set_frame_flags(bool tf);
	void clean_invalid_frame_flags();
	void clear_frame_flags();
	void add_frame_flags(Frame_flags_info &add);
	int get_object_flags(int frame, int qual) const;
	bool has_object_flag(int frame, int qual, int p) const {
		return (get_object_flags(frame, qual) & (1 << p)) != 0;
	}

	bool has_warmth_info() const;
	const std::vector<Warmth_info> &get_warmth_info() const {
		return warminf;
	}
	std::vector<Warmth_info> &set_warmth_info(bool tf);
	void clean_invalid_warmth_info();
	void clear_warmth_info();
	void add_warmth_info(Warmth_info &add);
	int get_object_warmth(int frame) const;

	int get_monster_food() const {
		return monster_food;
	}
	void set_monster_food(int sh) {
		if (monster_food != static_cast<short>(sh)) {
			modified_flags |= monster_food_flag;
			monster_food = static_cast<short>(sh);
		}
	}

	int get_mountain_top_type() const {
		return mountain_top;
	}
	void set_mountain_top(int sh) {
		if (mountain_top != static_cast<unsigned char>(sh)) {
			modified_flags |= mountain_top_flag;
			mountain_top = static_cast<unsigned char>(sh);
		}
	}

	int get_barge_type() const {
		return barge_type;
	}
	void set_barge_type(int sh) {
		if (barge_type != static_cast<unsigned char>(sh)) {
			modified_flags |= barge_type_flag;
			barge_type = static_cast<unsigned char>(sh);
		}
	}

	int get_field_type() const {
		return field_type;
	}
	void set_field_type(int sh) {
		if (field_type != static_cast<char>(sh)) {
			modified_flags |= field_type_flag;
			field_type = static_cast<char>(sh);
		}
	}

	int get_gump_shape() const {
		return gump_shape;
	}
	int get_gump_font() const {
		return gump_font;
	}
	void set_gump_data(int sh, int fnt) {
		if (gump_shape != static_cast<short>(sh)
		        || gump_font != static_cast<short>(sh)) {
			modified_flags |= gump_shape_flag;
			gump_shape = static_cast<short>(sh);
			gump_font = static_cast<short>(fnt);
		}
	}

	unsigned short get_shape_flags() const {
		return shape_flags;
	}
	void set_shape_flags(unsigned short flags) {
		if (shape_flags != flags) {
			int diff = (shape_flags ^ flags) * usecode_events_flag;
			modified_flags |= diff;
			shape_flags = flags;
		}
	}
	bool get_shape_flag(int tf) const {
		return (shape_flags & (1U << tf)) != 0;
	}
	void set_shape_flag(int tf, int mod) {
		if (!(shape_flags & (1U << tf))) {
			modified_flags |= (1U << mod);
			shape_flags |= (1U << tf);
		}
	}
	void clear_shape_flag(int tf, int mod) {
		if (shape_flags & (1U << tf)) {
			modified_flags |= (1U << mod);
			shape_flags &= ~(1U << tf);
		}
	}

	bool has_usecode_events() const {
		return get_shape_flag(usecode_events);
	}
	bool is_body_shape() const {
		return get_shape_flag(is_body);
	}
	bool is_lightweight() const {
		return get_shape_flag(lightweight);
	}
	bool has_quantity_frames() const {
		return get_shape_flag(quantity_frames);
	}
	bool is_container_locked() const {
		return get_shape_flag(locked);
	}
	bool is_explosive() const {
		return get_shape_flag(is_volatile);
	}
	bool is_jawbone() const {
		return get_shape_flag(jawbone);
	}
	bool is_mirror() const {
		return get_shape_flag(mirror);
	}
	bool is_on_fire() const {
		return get_shape_flag(on_fire);
	}
	bool has_extradimensional_storage() const {
		return get_shape_flag(extradimensional_storage);
	}

	unsigned char get_actor_flags() const {
		return actor_flags;
	}
	void set_actor_flags(char flags) {
		if (actor_flags != flags) {
			modified_flags |= actor_flags_flag;
			actor_flags = flags;
		}
	}
	bool get_actor_flag(int tf) const {
		return (actor_flags & (1 << tf)) != 0;
	}
	void set_actor_flag(int tf) {
		if (!(actor_flags & (1 << tf))) {
			modified_flags |= actor_flags_flag;
			actor_flags |= (1U << tf);
		}
	}
	void clear_actor_flag(int tf) {
		if (actor_flags & (1 << tf)) {
			modified_flags |= actor_flags_flag;
			actor_flags &= ~(1U << tf);
		}
	}

	bool is_cold_immune() const {
		return get_actor_flag(cold_immune);
	}
	bool does_not_eat() const {
		return get_actor_flag(doesnt_eat);
	}
	bool can_teleport() const {
		return get_actor_flag(teleports);
	}
	bool can_summon() const {
		return get_actor_flag(summons);
	}
	bool can_be_invisible() const {
		return get_actor_flag(turns_invisible);
	}
	bool survives_armageddon() const {
		return get_actor_flag(armageddon_safe);
	}
	bool quake_on_walk() const {
		return get_actor_flag(quake_walk);
	}
	bool is_goblin() const {
		return get_actor_flag(goblin);
	}
	// Get tile dims., flipped for
	//   reflected (bit 5) frames.
	int get_3d_xtiles(unsigned int framenum) const {
		return dims[(framenum >> 5) & 1];
	}
	int get_3d_ytiles(unsigned int framenum) const {
		return dims[1 ^ ((framenum >> 5) & 1)];
	}
	int get_3d_height() const {   // Height (in lifts?).
		return dims[2];
	}
	void set_3d(int xt, int yt, int zt);
	unsigned char get_tfa(int i) const {  // For debugging:
		return tfa[i];
	}
	bool has_sfx() const {         // Has a sound effect (guessing).
		return (tfa[0] & (1 << 0)) != 0;
	}
	void set_sfx(bool tf) {
		set_tfa(0, 0, tf);
	}
	bool has_strange_movement() const { // Slimes, sea monsters.
		return (tfa[0] & (1 << 1)) != 0;
	}
	void set_strange_movement(bool tf) {
		set_tfa(0, 1, tf);
	}
	bool is_animated() const {
		return (tfa[0] & (1 << 2)) != 0;
	}
	void set_animated(bool tf) {
		set_tfa(0, 2, tf);
	}
	bool is_solid() const {        // Guessing.  Means can't walk through.
		return (tfa[0] & (1 << 3)) != 0;
	}
	void set_solid(bool tf) {
		set_tfa(0, 3, tf);
	}
	bool is_water() const {        // Guessing.
		return (tfa[0] & (1 << 4)) != 0;
	}
	void set_water(bool tf) {
		set_tfa(0, 4, tf);
	}
	bool is_poisonous() const {    // Swamps.  Applies to tiles.
		return (tfa[1] & (1 << 4)) != 0;
	}
	bool is_field() const {        // Applies to Game_objects??
		return (tfa[1] & (1 << 4)) != 0;
	}
	void set_field(bool tf) {
		set_tfa(1, 4, tf);
	}
	bool is_door() const {
		return (tfa[1] & (1 << 5)) != 0;
	}
	void set_door(bool tf) {
		set_tfa(1, 5, tf);
	}
	bool is_barge_part() const {
		return (tfa[1] & (1 << 6)) != 0;
	}
	void set_barge_part(bool tf) {
		set_tfa(1, 6, tf);
	}
	bool is_transparent() const {      // ??
		return (tfa[1] & (1 << 7)) != 0;
	}
	void set_transparent(bool tf) {
		set_tfa(1, 7, tf);
	}
	bool is_light_source() const {
		return (tfa[2] & (1 << 6)) != 0;
	}
	void set_light_source(bool tf) {
		set_tfa(2, 6, tf);
	}
	bool has_translucency() const {
		return (tfa[2] & (1 << 7)) != 0;
	}
	void set_translucency(bool tf) {
		set_tfa(2, 7, tf);
	}
	bool is_xobstacle() const {    // Obstacle in x-dir.???
		return (shpdims[1] & 1) != 0;
	}
	bool is_yobstacle() const {    // Obstacle in y-dir.???
		return (shpdims[0] & 1) != 0;
	}
	void set_obstacle(bool x, bool y) {
		shpdims[1] = x ? (shpdims[1] | 1) : (shpdims[1]&~1);
		shpdims[0] = y ? (shpdims[0] | 1) : (shpdims[0]&~1);
	}
	/*
	 *  TFA[1][b0-b3] seems to indicate object types:
	 */
	enum Shape_class {
	    unusable = 0,       // Trees.
	    quality = 2,
	    quantity = 3,       // Can have more than 1:  coins, arrs.
	    has_hp = 4,     // Breakable items (if hp != 0, that is)
	    quality_flags = 5,  // Item quality is set of flags:
	    // Bit 3 = okay-to-take.
	    container = 6,
	    hatchable = 7,      // Eggs, traps, moongates.
	    spellbook = 8,
	    barge = 9,
	    virtue_stone = 11,
	    monster = 12,       // Non-human's.
	    human = 13,     // Human NPC's.
	    building = 14       // Roof, window, mountain.
	};
	Shape_class get_shape_class() const {
		return static_cast<Shape_class>(tfa[1] & 15);
	}
	void set_shape_class(Shape_class c) {
		tfa[1] = (tfa[1]&~15) | static_cast<int>(c);
	}
	bool is_npc() const {
		Shape_class c = get_shape_class();
		return c == human || c == monster;
	}
	bool has_quantity() const {
		return get_shape_class() == quantity;
	}
	bool has_quality_flags() const {  // Might be more...
		return get_shape_class() == quality_flags;
	}
	bool has_quality() const {
		Shape_class c = get_shape_class();
		return c == 2 || c == 6 || c == 7 || c == 11 || c == 12 || c == 13;
		//      return qual[(int) c];
	}
	bool occludes() const {
		return occludes_flag;
	}
	void set_occludes(bool tf) {
		occludes_flag = tf;
	}
	char get_ready_type() const {
		return ready_type;
	}
	void set_ready_type(char t) {
		if (ready_type != t) {
			modified_flags |= ready_type_flag;
			ready_type = t;
		}
	}
	bool is_spell() const {
		return spell_flag;
	}
	void set_is_spell(bool tf) {
		if (spell_flag != tf) {
			modified_flags |= ready_type_flag;
			spell_flag = tf;
		}
	}
	char get_alt_ready1() const {
		return alt_ready1;
	}
	char get_alt_ready2() const {
		return alt_ready2;
	}
	void set_alt_ready(char t1, char t2) {
		if (alt_ready1 != t1 || alt_ready2 != t2) {
			modified_flags |= altready_type_flag;
			alt_ready1 = t1;
			alt_ready2 = t2;
		}
	}
	// Sets x to 255 if there is no weapon offset
	void get_weapon_offset(int frame, unsigned char &x, unsigned char &y) const {
		if (!weapon_offsets) {
			x = 255;
			y = 255;
		} else {
			// x could be 255 (see read_info())
			x = weapon_offsets[frame * 2];
			y = weapon_offsets[frame * 2 + 1];
		}
	}
	void set_weapon_offset(int frame, unsigned char x, unsigned char y);
	int get_rotated_frame(int curframe, int quads) const;
};

#endif
