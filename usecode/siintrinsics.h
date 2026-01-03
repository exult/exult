/*
 *  Siintrinsics.h - Intrinsic table for Serpent Isle.
 *
 *  Note:   This is used in the virtual machine and the Usecode compiler.
 *
 *  Copyright (C) 2001-2025  The Exult Team
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

USECODE_INTRINSIC_PTR(get_random),                               // 0x00
		USECODE_INTRINSIC_PTR(execute_usecode_array),            // 0x01
		USECODE_INTRINSIC_PTR(delayed_execute_usecode_array),    // 0x02
		USECODE_INTRINSIC_PTR(show_npc_face),                    // 0x03
		USECODE_INTRINSIC_PTR(remove_npc_face),                  // 0x04
		USECODE_INTRINSIC_PTR(show_npc_face0),                   // 0x05
		USECODE_INTRINSIC_PTR(show_npc_face1),                   // 0x06
		USECODE_INTRINSIC_PTR(remove_npc_face0),                 // 0x07
		USECODE_INTRINSIC_PTR(remove_npc_face1),                 // 0x08
		USECODE_INTRINSIC_PTR(set_conversation_slot),            // 0x09
		USECODE_INTRINSIC_PTR(change_npc_face0),       // 0x0a - UNUSED
		USECODE_INTRINSIC_PTR(change_npc_face1),       // 0x0b - UNUSED
		USECODE_INTRINSIC_PTR(add_answer),             // 0x0c
		USECODE_INTRINSIC_PTR(remove_answer),          // 0x0d
		USECODE_INTRINSIC_PTR(push_answers),           // 0x0e
		USECODE_INTRINSIC_PTR(pop_answers),            // 0x0f
		USECODE_INTRINSIC_PTR(clear_answers),          // 0x10
		USECODE_INTRINSIC_PTR(select_from_menu),       // 0x11
		USECODE_INTRINSIC_PTR(select_from_menu2),      // 0x12
		USECODE_INTRINSIC_PTR(input_numeric_value),    // 0x13
		USECODE_INTRINSIC_PTR(set_item_shape),         // 0x14
		USECODE_INTRINSIC_PTR(find_nearest),           // 0x15
		USECODE_INTRINSIC_PTR(play_sound_effect),      // 0x16
		USECODE_INTRINSIC_PTR(die_roll),               // 0x17
		USECODE_INTRINSIC_PTR(get_item_shape),         // 0x18
		USECODE_INTRINSIC_PTR(get_item_weight),        // 0x19
		USECODE_INTRINSIC_PTR(get_item_frame),         // 0x1a
		USECODE_INTRINSIC_PTR(set_item_frame),         // 0x1b
		USECODE_INTRINSIC_PTR(get_item_quality),       // 0x1c
		USECODE_INTRINSIC_PTR(set_item_quality),       // 0x1d
		USECODE_INTRINSIC_PTR(get_item_quantity),      // 0x1e
		USECODE_INTRINSIC_PTR(set_item_quantity),      // 0x1f
		USECODE_INTRINSIC_PTR(get_object_position),    // 0x20
		USECODE_INTRINSIC_PTR(get_distance),           // 0x21
		USECODE_INTRINSIC_PTR(find_direction),         // 0x22
		USECODE_INTRINSIC_PTR(get_npc_object),         // 0x23
		USECODE_INTRINSIC_PTR(get_schedule_type),      // 0x24
		USECODE_INTRINSIC_PTR(set_schedule_type),      // 0x25
		USECODE_INTRINSIC_PTR(add_to_party),           // 0x26
		USECODE_INTRINSIC_PTR(remove_from_party),      // 0x27
		USECODE_INTRINSIC_PTR(get_npc_prop),           // 0x28
		USECODE_INTRINSIC_PTR(set_npc_prop),           // 0x29
		USECODE_INTRINSIC_PTR(get_avatar_ref),         // 0x2a
		USECODE_INTRINSIC_PTR(get_party_list),         // 0x2b
		USECODE_INTRINSIC_PTR(create_new_object),      // 0x2c
		USECODE_INTRINSIC_PTR(create_new_object2),     // 0x2d
		USECODE_INTRINSIC_PTR(set_last_created),       // 0x2e
		USECODE_INTRINSIC_PTR(update_last_created),    // 0x2f
		USECODE_INTRINSIC_PTR(get_npc_name),           // 0x30
		USECODE_INTRINSIC_PTR(count_objects),          // 0x31
		USECODE_INTRINSIC_PTR(find_object),            // 0x32
		USECODE_INTRINSIC_PTR(get_cont_items),         // 0x33
		USECODE_INTRINSIC_PTR(remove_party_items),     // 0x34
		USECODE_INTRINSIC_PTR(add_party_items),        // 0x35
		USECODE_INTRINSIC_PTR(add_cont_items),         // 0x36
		USECODE_INTRINSIC_PTR(remove_cont_items),      // 0x37
		USECODE_INTRINSIC_PTR(get_music_track),        // 0x38
		USECODE_INTRINSIC_PTR(play_music),             // 0x39
		USECODE_INTRINSIC_PTR(npc_nearby),             // 0x3a
		USECODE_INTRINSIC_PTR(npc_nearby2),            // 0x3b - Guess.
		USECODE_INTRINSIC_PTR(find_nearby_avatar),     // 0x3c
		USECODE_INTRINSIC_PTR(is_npc),                 // 0x3d
		USECODE_INTRINSIC_PTR(display_runes),          // 0x3e
		USECODE_INTRINSIC_PTR(click_on_item),          // 0x3f
		USECODE_INTRINSIC_PTR(error_message),          // 0x40
		USECODE_INTRINSIC_PTR(find_nearby),            // 0x41
		// Yes, these both seem to be 'give_last_created':
		USECODE_INTRINSIC_PTR(
				give_last_created),    // 0x42 - this seems to always succeed?
		USECODE_INTRINSIC_PTR(
				give_last_created),          // 0x43 - this can fail sometimes?
		USECODE_INTRINSIC_PTR(is_dead),      // 0x44
		USECODE_INTRINSIC_PTR(game_hour),    // 0x45
		USECODE_INTRINSIC_PTR(game_minute),        // 0x46
		USECODE_INTRINSIC_PTR(get_npc_number),     // 0x47
		USECODE_INTRINSIC_PTR(part_of_day),        // 0x48
		USECODE_INTRINSIC_PTR(get_alignment),      // 0x49
		USECODE_INTRINSIC_PTR(set_alignment),      // 0x4a
		USECODE_INTRINSIC_PTR(move_object),        // 0x4b
		USECODE_INTRINSIC_PTR(remove_npc),         // 0x4c
		USECODE_INTRINSIC_PTR(item_say),           // 0x4d
		USECODE_INTRINSIC_PTR(clear_item_say),     // 0x4e
		USECODE_INTRINSIC_PTR(set_to_attack),      // 0x4f
		USECODE_INTRINSIC_PTR(get_lift),           // 0x50
		USECODE_INTRINSIC_PTR(set_lift),           // 0x51
		USECODE_INTRINSIC_PTR(get_weather),        // 0x52
		USECODE_INTRINSIC_PTR(set_weather),        // 0x53
		USECODE_INTRINSIC_PTR(sit_down),           // 0x54
		USECODE_INTRINSIC_PTR(summon),             // 0x55
		USECODE_INTRINSIC_PTR(si_display_map),     // 0x56
		USECODE_INTRINSIC_PTR(kill_npc),           // 0x57
		USECODE_INTRINSIC_PTR(roll_to_win),        // 0x58
		USECODE_INTRINSIC_PTR(set_attack_mode),    // 0x59
		USECODE_INTRINSIC_PTR(get_attack_mode),    // 0x5a
		USECODE_INTRINSIC_PTR(set_oppressor),      // 0x5b
		USECODE_INTRINSIC_PTR(get_oppressor),      // 0x5c
		USECODE_INTRINSIC_PTR(get_weapon),         // 0x5d
		USECODE_INTRINSIC_PTR(set_opponent),       // 0x5e
		USECODE_INTRINSIC_PTR(clone),              // 0x5f - UNUSED
		USECODE_INTRINSIC_PTR(
				UNKNOWN),    // 0x60 - UNUSED - Same as BG 0x4e/SIB 0x5f?
		USECODE_INTRINSIC_PTR(display_area),             // 0x61 - UNUSED
		USECODE_INTRINSIC_PTR(wizard_eye),               // 0x62 - UNUSED
		USECODE_INTRINSIC_PTR(resurrect),                // 0x63
		USECODE_INTRINSIC_PTR(resurrect_npc),            // 0x64
		USECODE_INTRINSIC_PTR(get_body_npc),             // 0x65
		USECODE_INTRINSIC_PTR(add_spell),                // 0x66
		USECODE_INTRINSIC_PTR(remove_all_spells),        // 0x67
		USECODE_INTRINSIC_PTR(sprite_effect),            // 0x68
		USECODE_INTRINSIC_PTR(attack_object),            // 0x69
		USECODE_INTRINSIC_PTR(book_mode),                // 0x6a
		USECODE_INTRINSIC_PTR(stop_time),                // 0x6b
		USECODE_INTRINSIC_PTR(cause_light),              // 0x6c
		USECODE_INTRINSIC_PTR(get_barge),                // 0x6d
		USECODE_INTRINSIC_PTR(earthquake),               // 0x6e
		USECODE_INTRINSIC_PTR(is_pc_female),             // 0x6f
		USECODE_INTRINSIC_PTR(armageddon),               // 0x70 - UNUSED
		USECODE_INTRINSIC_PTR(halt_scheduled),           // 0x71
		USECODE_INTRINSIC_PTR(lightning),                // 0x72
		USECODE_INTRINSIC_PTR(get_array_size),           // 0x73
		USECODE_INTRINSIC_PTR(save_pos),                 // 0x74
		USECODE_INTRINSIC_PTR(teleport_to_saved_pos),    // 0x75
		USECODE_INTRINSIC_PTR(apply_damage),             // 0x76
		USECODE_INTRINSIC_PTR(is_pc_inside),             // 0x77
		USECODE_INTRINSIC_PTR(
				UNKNOWN),    // 0x78 - UNUSED - Seems like it could be
							 // set_orrery, but does nothing
		USECODE_INTRINSIC_PTR(
				UNKNOWN),    // 0x79 - UNUSED - Same as BG 0x64/SIB 0x78?
							 // Something with palettes
		USECODE_INTRINSIC_PTR(get_timer),             // 0x7a
		USECODE_INTRINSIC_PTR(set_timer),             // 0x7b
		USECODE_INTRINSIC_PTR(wearing_fellowship),    // 0x7c
		USECODE_INTRINSIC_PTR(mouse_exists),          // 0x7d
		USECODE_INTRINSIC_PTR(get_speech_track),      // 0x7e
		USECODE_INTRINSIC_PTR(flash_mouse),           // 0x7f
		USECODE_INTRINSIC_PTR(get_item_frame_rot),    // 0x80
		USECODE_INTRINSIC_PTR(set_item_frame_rot),    // 0x81
		USECODE_INTRINSIC_PTR(on_barge),              // 0x82
		USECODE_INTRINSIC_PTR(get_container),         // 0x83
		USECODE_INTRINSIC_PTR(remove_item),           // 0x84
		USECODE_INTRINSIC_PTR(init_conversation),     // 0x85
		USECODE_INTRINSIC_PTR(end_conversation),      // 0x86
		USECODE_INTRINSIC_PTR(reduce_health),         // 0x87
		USECODE_INTRINSIC_PTR(is_readied),            // 0x88
		USECODE_INTRINSIC_PTR(restart_game),          // 0x89
		USECODE_INTRINSIC_PTR(start_speech),          // 0x8a
		USECODE_INTRINSIC_PTR(run_endgame),           // 0x8b
		USECODE_INTRINSIC_PTR(fire_projectile),       // 0x8c
		USECODE_INTRINSIC_PTR(nap_time),              // 0x8d
		USECODE_INTRINSIC_PTR(advance_time),          // 0x8e
		USECODE_INTRINSIC_PTR(in_usecode),            // 0x8f
		USECODE_INTRINSIC_PTR(call_guards),           // 0x90
		USECODE_INTRINSIC_PTR(obj_sprite_effect),     // 0x91
		USECODE_INTRINSIC_PTR(attack_avatar),         // 0x92
		USECODE_INTRINSIC_PTR(stop_arresting),        // 0x93
		USECODE_INTRINSIC_PTR(path_run_usecode),      // 0x94
		USECODE_INTRINSIC_PTR(
				UNKNOWN),    // 0x95 - UNUSED - seems like it could be
							 // sib_path_run_usecode, but does nothing
		USECODE_INTRINSIC_PTR(can_avatar_reach_pos),    // 0x96
		USECODE_INTRINSIC_PTR(
				UNKNOWN),    // 0x97 - UNUSED - seems like it could be
							 // sib_is_dest_reachable, but does nothing
		USECODE_INTRINSIC_PTR(close_gumps),     // 0x98
		USECODE_INTRINSIC_PTR(item_say),        // 0x99 - item_say in gump
		USECODE_INTRINSIC_PTR(close_gump),      // 0x9a
		USECODE_INTRINSIC_PTR(in_gump_mode),    // 0x9b
		USECODE_INTRINSIC_PTR(set_light),       // 0x9c
		USECODE_INTRINSIC_PTR(
				UNKNOWN),    // 0x9d - UNUSED - Same as BG 0x83/SIB 0x9c?
							 // Something with palettes
		USECODE_INTRINSIC_PTR(set_time_palette),        // 0x9e
		USECODE_INTRINSIC_PTR(ambient_light),           // 0x9f
		USECODE_INTRINSIC_PTR(is_not_blocked),          // 0xa0
		USECODE_INTRINSIC_PTR(play_sound_effect2),      // 0xa1
		USECODE_INTRINSIC_PTR(direction_from),          // 0xa2
		USECODE_INTRINSIC_PTR(get_item_flag),           // 0xa3
		USECODE_INTRINSIC_PTR(set_item_flag),           // 0xa4
		USECODE_INTRINSIC_PTR(clear_item_flag),         // 0xa5
		USECODE_INTRINSIC_PTR(get_skin_colour),         // 0xa6
		USECODE_INTRINSIC_PTR(set_path_failure),        // 0xa7
		USECODE_INTRINSIC_PTR(fade_palette),            // 0xa8
		USECODE_INTRINSIC_PTR(fade_palette_sleep),      // 0xa9
		USECODE_INTRINSIC_PTR(get_party_list2),         // 0xaa
		USECODE_INTRINSIC_PTR(in_combat),               // 0xab
		USECODE_INTRINSIC_PTR(is_water),                // 0xac
		USECODE_INTRINSIC_PTR(reset_conv_face),         // 0xad - UNUSED
		USECODE_INTRINSIC_PTR(set_camera),              // 0xae
		USECODE_INTRINSIC_PTR(get_dead_party),          // 0xaf - UNUSED
		USECODE_INTRINSIC_PTR(view_tile),               // 0xb0 - UNUSED
		USECODE_INTRINSIC_PTR(telekenesis),             // 0xb1
		USECODE_INTRINSIC_PTR(a_or_an),                 // 0xb2
		USECODE_INTRINSIC_PTR(set_polymorph),           // 0xb3
		USECODE_INTRINSIC_PTR(revert_schedule),         // 0xb4
		USECODE_INTRINSIC_PTR(modify_schedule),         // 0xb5
		USECODE_INTRINSIC_PTR(set_new_schedules),       // 0xb6
		USECODE_INTRINSIC_PTR(run_schedule),            // 0xb7
		USECODE_INTRINSIC_PTR(get_temperature),         // 0xb8
		USECODE_INTRINSIC_PTR(get_temperature_zone),    // 0xb9 - UNUSED
		USECODE_INTRINSIC_PTR(set_temperature),         // 0xba
		USECODE_INTRINSIC_PTR(get_npc_warmth),          // 0xbb - UNUSED
		USECODE_INTRINSIC_PTR(get_npc_id),              // 0xbc
		USECODE_INTRINSIC_PTR(set_npc_id),              // 0xbd
		USECODE_INTRINSIC_PTR(get_readied),             // 0xbe
		USECODE_INTRINSIC_PTR(approach_avatar),         // 0xbf
		USECODE_INTRINSIC_PTR(set_barge_dir),           // 0xc0
		USECODE_INTRINSIC_PTR(si_path_run_usecode),     // 0xc1
		// These are SS-only, but we won't bother with the distinction.
		USECODE_INTRINSIC_PTR(is_on_keyring),       // 0xc2
		USECODE_INTRINSIC_PTR(add_to_keyring),      // 0xc3
		USECODE_INTRINSIC_PTR(remove_from_area),    // 0xc4
		USECODE_INTRINSIC_PTR(infravision),         // 0xc5
		// These are Exult-specific.
		USECODE_INTRINSIC_PTR(set_intercept_item),     // 0xc6 (Exult)
		USECODE_INTRINSIC_PTR(printf),                 // 0xc7 (Exult)
		USECODE_INTRINSIC_PTR(remove_from_keyring),    // 0xc8 (Exult)
		USECODE_INTRINSIC_PTR(begin_casting_mode),     // 0xc9 (Exult)
		USECODE_INTRINSIC_PTR(get_usecode_fun),        // 0xca (Exult)
		USECODE_INTRINSIC_PTR(get_map_num),            // 0xcb (Exult)
		USECODE_INTRINSIC_PTR(display_map_ex),         // 0xcc (Exult)
		USECODE_INTRINSIC_PTR(book_mode_ex),           // 0xcd (Exult)
		// Here, Exult supports BG intrinsics for users of UCC.
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xce
		USECODE_INTRINSIC_PTR(center_view),              // 0xcf
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xd0
		USECODE_INTRINSIC_PTR(is_dest_reachable),        // 0xd1
		USECODE_INTRINSIC_PTR(set_npc_name),             // 0xd2
		USECODE_INTRINSIC_PTR(set_usecode_fun),          // 0xd3
		USECODE_INTRINSIC_PTR(has_spell),                // 0xd4
		USECODE_INTRINSIC_PTR(remove_spell),             // 0xd5
		USECODE_INTRINSIC_PTR(create_barge_object),      // 0xd6
		USECODE_INTRINSIC_PTR(in_usecode_path),          // 0xd7
		USECODE_INTRINSIC_PTR(start_blocking_speech),    // 0xd8
		USECODE_INTRINSIC_PTR(close_gumps2),             // 0xd9
		USECODE_INTRINSIC_PTR(close_gump2),              // 0xda
		USECODE_INTRINSIC_PTR(game_day),                 // 0xdb (Exult)
		USECODE_INTRINSIC_PTR(play_scene),               // 0xdc (Exult)
		USECODE_INTRINSIC_PTR(create_new_egg),           // 0xdd (Exult)
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xde
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xdf
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xe0
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xe1
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xe2
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xe3
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xe4
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xe5
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xe6
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xe7
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xe8
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xe9
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xea
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xeb
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xec
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xed
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xee
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xef
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xf0
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xf1
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xf2
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xf3
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xf4
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xf5
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xf6
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xf7
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xf8
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xf9
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xfa
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xfb
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xfc
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xfd
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xfe
		USECODE_INTRINSIC_PTR(UNKNOWN),                  // 0xff
