# Usecode Intrinsic Reference

## General Definitions

For Usecode C purposes, the avatar is an NPC just like any other. Constants referred to below (usually in ALL CAPS) are defined in 'src/headers/constants.uc' unless otherwise stated.

Unlike Usecode C-defined functions, intrinsics do not have compiler-enforced prototypes. The signatures below describe the values expected by the engine, rather than declarations to copy into your source. Supply the documented parameters and use the named intrinsic so UCC selects the correct game-specific ID.

There is no fixed 12-argument buffer limit in the current engine. This does not make arbitrary argument lists meaningful: the implementation determines which values it reads, how it interprets them, and whether omitted arguments have defaults. Omit an argument only when the intrinsic description explicitly makes it optional. The runtime checks each intrinsic's declared minimum argument count before calling its implementation; calls with too few arguments print a diagnostic and are skipped. Optional-argument defaults are handled by the implementation, which receives exactly the arguments supplied by the caller.

The descriptions and ID table have been checked against `usecode/intrinsics.cc`, `usecode/bgintrinsics.h`, `usecode/siintrinsics.h`, and `usecode/sibetaintrinsics.h`. UCC portrait statements are also checked against `tools/compiler/ucparse.yy`.

To the right of many intrinsic 'prototypes', you can see an indicator between square brackets. This indicator is a quick way to see if that intrinsic is available for the specific game you are interested in. The possible values of these indicators are:

| Indicator | Meaning |
| --- | --- |
| [BG] | BG-specific intrinsic |
| [SI] | SI-specific intrinsic |
| [Exult] | Exult-specific intrinsic, available to both BG and SI |
| [BG, Exult] | Originally BG-only, Exult allows it in SI too |
| [SI, Exult] | Originally SI-only, Exult allows it in BG too |
| [Exult: BG] | Exult-specific intrinsic, available only to BG |
| [Exult: SI] | Exult-specific intrinsic, available only to SI |
| (not present) | Intrinsic available for BG and SI |

The '[Exult]', '[BG, Exult]' and '[SI, Exult]' indicators are essentially equivalent to no indicator at all; they are used in this document only for completeness.


The intrinsic prototypes below are in Usecode C-esque form. It is actually more like C than Usecode C. Specifically, all parameters have a type instead of the Usecode C-'`var`'. The parameter type indicates what Exult will be expecting; the Usecode C compiler won't care in the slightest what you pass as a parameter. The parameter types I use are as follows:

| Parameter / value | Description |
| --- | --- |
| `int` | This means any integer, positive or negative. |
| `object` | Any object or NPC. It can be an object reference, '`item`' or a number (that **must** be negative, which Exult will convert to an NPC if it is in the correct range). |
| `actor` | Any NPC. It is evaluated the same way as '`object`' is, but Exult will reject anything that can't be converted to an NPC. Monsters are also accepted. |
| `bool` | A '`true`'/'`false`' parameter. Can be a number (nonzero/zero), an array (non-empty/empty) or a comparison expression (e.g., `a == b`). |
| `string` | A string (text). Strings in usecode are any set of characters surrounded by double-quotes; for example, `"This is a string."`. If you need a double-quote inside a string you can 'escape' it by prefixing them with a backslash: `"\""`; alternatively, Exult converts '`@`' into a double-quote. |
| `function` | A usecode function, which **must** be declared with either, the `shape#` *or* the `object#` specifiers. In Usecode C, this can be the function name or its usecode number. For example: '`extern Bolt shape#(0x2D3) ();`' can be passed as '`Bolt`' (the function name), '`0x2D3`' (its function number since '`shape#(shnum) == shnum`' if `shnum < 0x400`) or as '`&Bolt`' (using the 'address of' operator '`&`' to get the function number). |

If the parameter name is followed by a '`[]`', it indicates an array with elements of the specified type. Example: '`actor npc[]`' means an array of NPCs. If there is a number between the brackets, it means an array of that length; for example, '`int num[3]`' means an array with 3 '`int`' elements. In some cases, an array can be of type '`mixed`' to indicate that not all of the array's elements are of the same type.

Some intrinsics are 'overloaded'; i.e., they accept more than one parameter type. In those cases, all of the possibilities are listed as prototypes.

If the parameter list is simply '`[special]`', see the description for details.

If the intrinsic is preceded by a parameter type, it means that the intrinsic returns something of that type; the description explains the return value. In some cases, the return value can be '`mixed[]`' to indicate that the intrinsic returns an array, not all of which are of the same type.

All intrinsics can be called as presented in the list. If an intrinsic has at least one parameter, it can be called in 'calle' form, with the first parameter as its '`object`'. Example:


```usecode
int UI_execute_usecode_array(object obj, int script[]);
int obj->execute_usecode_array(int script[]);
```

The first parameter and the '<code>-&gt;</code>' can both be omitted if '`obj`' is '`item`', and if you omit either of them you **must** omit both or the Usecode will fail to compile. For stylistic purposes, the 'calle' form should be used only when the first parameter is an object or NPC (or '`item`'). For all intrinsics with 'appropriate' parameter types, I have listed both forms in the list below.

Also, you may have noticed that I did not include the parameter type of '`obj`' when I gave the 'calle' form of execute_usecode_array; since I will only give the 'calle' form when the parameter type is '`object`' or '`actor`', I feel that it is unneeded.

Usecode arrays are one-based: the first element is `array[1]`. Array sizes in signatures describe the number of elements, not the last zero-based index. Flat coordinate lists such as `[x1, y1, x2, y2]` are single arrays, not arrays of arrays.


### Intrinsic IDs and game availability

The following IDs come from Exult's `usecode/bgintrinsics.h`, `usecode/siintrinsics.h`, and `usecode/sibetaintrinsics.h`. Select the appropriate table with `#game "blackgate"`, `#game "serpentisle"`, or `#game "serpentbeta"`; use the named intrinsic in source rather than copying an ID from another game. Multiple IDs in a cell are aliases for the same intrinsic. An em dash means that the intrinsic is absent from that table. `UNKNOWN` and `NOP` entries are omitted.

The badges on individual descriptions distinguish original game support from Exult extensions for BG and SI. This table also records SI beta availability, which those badges do not cover. In particular, SI beta has `sib_path_run_usecode` and `sib_is_dest_reachable` with different signatures from their counterparts, supports the virtue-stone intrinsics, and lacks `save_pos` and `teleport_to_saved_pos`.

| Intrinsic | BG | SI | SI beta |
| --- | --- | --- | --- |
| [`UI_a_or_an`](#a_or_an) | `0x96` | `0xb2` | `0xb1` |
| [`UI_add_answer`](#add_answer) | `0x05` | `0x0c` | `0x0c` |
| [`UI_add_cont_items`](#add_cont_items) | `0xb8` | `0x36` | `0x36` |
| [`UI_add_party_items`](#add_party_items) | `0x2c` | `0x35` | `0x35` |
| [`UI_add_spell`](#add_spell) | `0x52` | `0x66` | `0x65` |
| [`UI_add_to_keyring`](#add_to_keyring) | — | `0xc3` | `0xc2` |
| [`UI_add_to_party`](#add_to_party) | `0x1e` | `0x26` | `0x26` |
| [`UI_advance_time`](#advance_time) | `0x78` | `0x8e` | `0x8d` |
| [`UI_ambient_light`](#ambient_light) | `0xd4` | `0x9f` | `0x9e` |
| [`UI_apply_damage`](#apply_damage) | `0x61` | `0x76` | `0x75` |
| [`UI_approach_avatar`](#approach_avatar) | `0xa0` | `0xbf` | `0xbe` |
| [`UI_armageddon`](#armageddon) | `0x5b` | `0x70` | `0x6f` |
| [`UI_attack_avatar`](#attack_avatar) | `0x7c` | `0x92` | `0x91` |
| [`UI_attack_object`](#attack_object) | `0x54` | `0x69` | `0x68` |
| [`UI_begin_casting_mode`](#begin_casting_mode) | `0xb4` | `0xc9` | `0xc8` |
| [`UI_book_mode`](#book_mode) | `0x55` | `0x6a` | `0x69` |
| [`UI_book_mode_ex`](#book_mode_ex) | `0xc0` | `0xcd` | `0xcc` |
| [`UI_call_guards`](#call_guards) | `0x7a` | `0x90` | `0x8f` |
| [`UI_can_avatar_reach_pos`](#can_avatar_reach_pos) | — | `0x96` | `0x95` |
| [`UI_cause_light`](#cause_light) | `0x57` | `0x6c` | `0x6b` |
| [`UI_center_view`](#center_view) | `0xa2` | `0xcf` | `0xce` |
| [`UI_change_npc_face0`](#change_npc_face0) | `0xcc` | `0x0a` | `0x0a` |
| [`UI_change_npc_face1`](#change_npc_face1) | `0xcd` | `0x0b` | `0x0b` |
| [`UI_clear_answers`](#clear_answers) | `0x09` | `0x10` | `0x10` |
| [`UI_clear_item_flag`](#clear_item_flag) | `0x8a` | `0xa5` | `0xa4` |
| [`UI_clear_item_say`](#clear_item_say) | `0xb9` | `0x4e` | `0x4d` |
| [`UI_click_on_item`](#click_on_item) | `0x33` | `0x3f` | `0x3f` |
| [`UI_clone`](#clone) | `0x4d` | `0x5f` | `0x5e` |
| [`UI_close_gump`](#close_gump) | `0x80` | `0x9a` | `0x99` |
| [`UI_close_gump2`](#close_gump2) | `0xd9` | `0xda` | `0xd9` |
| [`UI_close_gumps`](#close_gumps) | `0x7e` | `0x98` | `0x97` |
| [`UI_close_gumps2`](#close_gumps2) | `0xd8` | `0xd9` | `0xd8` |
| [`UI_count_objects`](#count_objects) | `0x28` | `0x31` | `0x31` |
| [`UI_create_barge_object`](#create_barge_object) | `0xd5` | `0xd6` | `0xd5` |
| [`UI_create_new_egg`](#create_new_egg) | `0xdc` | `0xdd` | `0xdc` |
| [`UI_create_new_object`](#create_new_object) | `0x24` | `0x2c` | `0x2c` |
| [`UI_create_new_object2`](#create_new_object2) | `0xb7` | `0x2d` | `0x2d` |
| [`UI_delayed_execute_usecode_array`](#delayed_execute_usecode_array) | `0x02` | `0x02` | `0x02` |
| [`UI_die_roll`](#die_roll) | `0x10` | `0x17` | `0x17` |
| [`UI_direction_from`](#direction_from) | `0x87` | `0xa2` | `0xa1` |
| [`UI_display_area`](#display_area) | `0x4f` | `0x61` | `0x60` |
| [`UI_display_map`](#display_map) | `0x48` | — | — |
| [`UI_display_map_ex`](#display_map_ex) | `0xbf` | `0xcc` | `0xcb` |
| [`UI_display_runes`](#display_runes) | `0x32` | `0x3e` | `0x3e` |
| [`UI_earthquake`](#earthquake) | `0x59` | `0x6e` | `0x6d` |
| [`UI_end_conversation`](#end_conversation) | — | `0x86` | `0x85` |
| [`UI_error_message`](#error_message) | `0x34` | `0x40` | `0x40` |
| [`UI_execute_usecode_array`](#execute_usecode_array) | `0x01` | `0x01` | `0x01` |
| [`UI_fade_palette`](#fade_palette) | `0x8c` | `0xa8` | `0xa7` |
| [`UI_fade_palette_sleep`](#fade_palette_sleep) | `0xd7` | `0xa9` | `0xa8` |
| [`UI_find_direction`](#find_direction) | `0x1a` | `0x22` | `0x22` |
| [`UI_find_nearby`](#find_nearby) | `0x35` | `0x41` | `0x41` |
| [`UI_find_nearby_avatar`](#find_nearby_avatar) | `0x30` | `0x3c` | `0x3c` |
| [`UI_find_nearest`](#find_nearest) | `0x0e` | `0x15` | `0x15` |
| [`UI_find_object`](#find_object) | `0x29` | `0x32` | `0x32` |
| [`UI_fire_projectile`](#fire_projectile) | `0x76` | `0x8c` | `0x8b` |
| [`UI_flash_mouse`](#flash_mouse) | `0x6a` | `0x7f` | `0x7e` |
| [`UI_game_day`](#game_day) | `0xda` | `0xdb` | `0xda` |
| [`UI_game_hour`](#game_hour) | `0x38` | `0x45` | `0x44` |
| [`UI_game_minute`](#game_minute) | `0x39` | `0x46` | `0x45` |
| [`UI_get_alignment`](#get_alignment) | `0x3c` | `0x49` | `0x48` |
| [`UI_get_array_size`](#get_array_size) | `0x5e` | `0x73` | `0x72` |
| [`UI_get_attack_mode`](#get_attack_mode) | `0xa1` | `0x5a` | `0x59` |
| [`UI_get_avatar_ref`](#get_avatar_ref) | `0x22` | `0x2a` | `0x2a` |
| [`UI_get_barge`](#get_barge) | `0x58` | `0x6d` | `0x6c` |
| [`UI_get_body_npc`](#get_body_npc) | `0xa3` | `0x65` | `0x64` |
| [`UI_get_cont_items`](#get_cont_items) | `0x2a` | `0x33` | `0x33` |
| [`UI_get_container`](#get_container) | `0x6e` | `0x83` | `0x82` |
| [`UI_get_dead_party`](#get_dead_party) | `0x93` | `0xaf` | `0xae` |
| [`UI_get_distance`](#get_distance) | `0x19` | `0x21` | `0x21` |
| [`UI_get_item_flag`](#get_item_flag) | `0x88` | `0xa3` | `0xa2` |
| [`UI_get_item_frame`](#get_item_frame) | `0x12` | `0x1a` | `0x1a` |
| [`UI_get_item_frame_rot`](#get_item_frame_rot) | `0x6b` | `0x80` | `0x7f` |
| [`UI_get_item_gump_position`](#get_item_gump_position) | `0xdd` | `0xde` | `0xdd` |
| [`UI_get_item_quality`](#get_item_quality) | `0x14` | `0x1c` | `0x1c` |
| [`UI_get_item_quantity`](#get_item_quantity) | `0x16` | `0x1e` | `0x1e` |
| [`UI_get_item_shape`](#get_item_shape) | `0x11` | `0x18` | `0x18` |
| [`UI_get_item_weight`](#get_item_weight) | `0xc9` | `0x19` | `0x19` |
| [`UI_get_lift`](#get_lift) | `0x42` | `0x50` | `0x4f` |
| [`UI_get_map_num`](#get_map_num) | `0xb6` | `0xcb` | `0xca` |
| [`UI_get_music_track`](#get_music_track) | `0x2d` | `0x38` | `0x38` |
| [`UI_get_npc_id`](#get_npc_id) | `0xa4` | `0xbc` | `0xbb` |
| [`UI_get_npc_name`](#get_npc_name) | `0x27` | `0x30` | `0x30` |
| [`UI_get_npc_number`](#get_npc_number) | `0x3a` | `0x47` | `0x46` |
| [`UI_get_npc_object`](#get_npc_object) | `0x1b` | `0x23` | `0x23` |
| [`UI_get_npc_prop`](#get_npc_prop) | `0x20` | `0x28` | `0x28` |
| [`UI_get_npc_warmth`](#get_npc_warmth) | `0xd1` | `0xbb` | `0xba` |
| [`UI_get_object_position`](#get_object_position) | `0x18` | `0x20` | `0x20` |
| [`UI_get_oppressor`](#get_oppressor) | `0xa6` | `0x5c` | `0x5b` |
| [`UI_get_party_list`](#get_party_list) | `0x23` | `0x2b` | `0x2b` |
| [`UI_get_party_list2`](#get_party_list2) | `0x8d` | `0xaa` | `0xa9` |
| [`UI_get_random`](#get_random) | `0x00` | `0x00` | `0x00` |
| [`UI_get_readied`](#get_readied) | `0xa7` | `0xbe` | `0xbd` |
| [`UI_get_schedule_type`](#get_schedule_type) | `0x1c` | `0x24` | `0x24` |
| [`UI_get_shape_flag`](#get_shape_flag) | `0xe4` | `0xe5` | `0xe4` |
| [`UI_get_skin_colour`](#get_skin_colour) | `0xcb` | `0xa6` | `0xa5` |
| [`UI_get_slider_value`](#get_slider_value) | `0xe2` | `0xe3` | `0xe2` |
| [`UI_get_speech_track`](#get_speech_track) | `0x69` | `0x7e` | `0x7d` |
| [`UI_get_temperature`](#get_temperature) | `0xce` | `0xb8` | `0xb7` |
| [`UI_get_temperature_zone`](#get_temperature_zone) | `0xd0` | `0xb9` | `0xb8` |
| [`UI_get_timer`](#get_timer) | `0x65` | `0x7a` | `0x79` |
| [`UI_get_usecode_fun`](#get_usecode_fun) | `0xb5` | `0xca` | `0xc9` |
| [`UI_get_weapon`](#get_weapon) | `0xa8` | `0x5d` | `0x5c` |
| [`UI_get_weather`](#get_weather) | `0x44` | `0x52` | `0x51` |
| [`UI_give_last_created`](#give_last_created) | `0x36` | `0x42`, `0x43` | `0x42` |
| [`UI_halt_scheduled`](#halt_scheduled) | `0x5c` | `0x71` | `0x70` |
| [`UI_has_spell`](#has_spell) | `0xd2` | `0xd4` | `0xd3` |
| [`UI_in_combat`](#in_combat) | `0x8e` | `0xab` | `0xaa` |
| [`UI_in_gump_mode`](#in_gump_mode) | `0x81` | `0x9b` | `0x9a` |
| [`UI_in_usecode`](#in_usecode) | `0x79` | `0x8f` | `0x8e` |
| [`UI_in_usecode_path`](#in_usecode_path) | `0xd6` | `0xd7` | `0xd6` |
| [`UI_infravision`](#infravision) | `0xbe` | `0xc5` | `0xc4` |
| [`UI_init_conversation`](#init_conversation) | — | `0x85` | `0x84` |
| [`UI_input_numeric_value`](#input_numeric_value) | `0x0c` | `0x13` | `0x13` |
| [`UI_is_dead`](#is_dead) | `0x37` | `0x44` | `0x43` |
| [`UI_is_dest_reachable`](#is_dest_reachable) | `0xc6` | `0xd1` | `0xd0` |
| [`UI_is_not_blocked`](#is_not_blocked) | `0x85` | `0xa0` | `0x9f` |
| [`UI_is_npc`](#is_npc) | `0x31` | `0x3d` | `0x3d` |
| [`UI_is_on_keyring`](#is_on_keyring) | — | `0xc2` | `0xc1` |
| [`UI_is_pc_female`](#is_pc_female) | `0x5a` | `0x6f` | `0x6e` |
| [`UI_is_pc_inside`](#is_pc_inside) | `0x62` | `0x77` | `0x76` |
| [`UI_is_readied`](#is_readied) | `0x72` | `0x88` | `0x87` |
| [`UI_is_water`](#is_water) | `0x90` | `0xac` | `0xab` |
| [`UI_item_say`](#item_say) | `0x40`, `0x7f` | `0x4d`, `0x99` | `0x4c`, `0x98` |
| [`UI_kill_npc`](#kill_npc) | `0x49` | `0x57` | `0x56` |
| [`UI_lightning`](#lightning) | `0x5d` | `0x72` | `0x71` |
| [`UI_mark_virtue_stone`](#mark_virtue_stone) | `0x5f` | — | `0x73` |
| [`UI_modify_schedule`](#modify_schedule) | `0xa9` | `0xb5` | `0xb4` |
| [`UI_mouse_exists`](#mouse_exists) | `0x68` | `0x7d` | `0x7c` |
| [`UI_move_object`](#move_object) | `0x3e` | `0x4b` | `0x4a` |
| [`UI_nap_time`](#nap_time) | `0x77` | `0x8d` | `0x8c` |
| [`UI_npc_nearby`](#npc_nearby) | `0x2f` | `0x3a` | `0x3a` |
| [`UI_npc_nearby2`](#npc_nearby2) | — | `0x3b` | `0x3b` |
| [`UI_obj_sprite_effect`](#obj_sprite_effect) | `0x7b` | `0x91` | `0x90` |
| [`UI_on_barge`](#on_barge) | `0x6d` | `0x82` | `0x81` |
| [`UI_part_of_day`](#part_of_day) | `0x3b` | `0x48` | `0x47` |
| [`UI_path_run_usecode`](#path_run_usecode) | `0x7d` | `0x94` | `0x93` |
| [`UI_play_music`](#play_music) | `0x2e` | `0x39` | `0x39` |
| [`UI_play_scene`](#play_scene) | `0xdb` | `0xdc` | `0xdb` |
| [`UI_play_sound_effect`](#play_sound_effect) | `0x0f` | `0x16` | `0x16` |
| [`UI_play_sound_effect2`](#play_sound_effect2) | `0x86` | `0xa1` | `0xa0` |
| [`UI_pop_answers`](#pop_answers) | `0x08` | `0x0f` | `0x0f` |
| [`UI_printf`](#printf) | `0xaa` | `0xc7` | `0xc6` |
| [`UI_push_answers`](#push_answers) | `0x07` | `0x0e` | `0x0e` |
| [`UI_recall_virtue_stone`](#recall_virtue_stone) | `0x60` | — | `0x74` |
| [`UI_reduce_health`](#reduce_health) | `0x71` | `0x87` | `0x86` |
| [`UI_remove_all_spells`](#remove_all_spells) | `0xca` | `0x67` | `0x66` |
| [`UI_remove_answer`](#remove_answer) | `0x06` | `0x0d` | `0x0d` |
| [`UI_remove_cont_items`](#remove_cont_items) | `0xab` | `0x37` | `0x37` |
| [`UI_remove_from_area`](#remove_from_area) | `0xac` | `0xc4` | `0xc3` |
| [`UI_remove_from_keyring`](#remove_from_keyring) | — | `0xc8` | `0xc7` |
| [`UI_remove_from_party`](#remove_from_party) | `0x1f` | `0x27` | `0x27` |
| [`UI_remove_item`](#remove_item) | `0x6f` | `0x84` | `0x83` |
| [`UI_remove_npc`](#remove_npc) | `0x3f` | `0x4c` | `0x4b` |
| [`UI_remove_npc_face`](#remove_npc_face) | `0x04` | `0x04` | `0x04` |
| [`UI_remove_npc_face0`](#remove_npc_face0) | `0xc3` | `0x07` | `0x07` |
| [`UI_remove_npc_face1`](#remove_npc_face1) | `0xc4` | `0x08` | `0x08` |
| [`UI_remove_party_items`](#remove_party_items) | `0x2b` | `0x34` | `0x34` |
| [`UI_remove_spell`](#remove_spell) | `0xd3` | `0xd5` | `0xd4` |
| [`UI_reset_conv_face`](#reset_conv_face) | `0x91` | `0xad` | `0xac` |
| [`UI_restart_game`](#restart_game) | `0x73` | `0x89` | `0x88` |
| [`UI_resurrect`](#resurrect) | `0x51` | `0x63` | `0x62` |
| [`UI_resurrect_npc`](#resurrect_npc) | — | `0x64` | `0x63` |
| [`UI_revert_schedule`](#revert_schedule) | `0xad` | `0xb4` | `0xb3` |
| [`UI_roll_to_win`](#roll_to_win) | `0x4a` | `0x58` | `0x57` |
| [`UI_run_endgame`](#run_endgame) | `0x75` | `0x8b` | `0x8a` |
| [`UI_run_schedule`](#run_schedule) | `0xae` | `0xb7` | `0xb6` |
| [`UI_save_pos`](#save_pos) | `0xba` | `0x74` | — |
| [`UI_select_from_menu`](#select_from_menu) | `0x0a` | `0x11` | `0x11` |
| [`UI_select_from_menu2`](#select_from_menu2) | `0x0b` | `0x12` | `0x12` |
| [`UI_set_alignment`](#set_alignment) | `0x3d` | `0x4a` | `0x49` |
| [`UI_set_attack_mode`](#set_attack_mode) | `0x4b` | `0x59` | `0x58` |
| [`UI_set_barge_dir`](#set_barge_dir) | `0xaf` | `0xc0` | `0xbf` |
| [`UI_set_camera`](#set_camera) | `0x92`, `0xbc` | `0xae` | `0xad` |
| [`UI_set_conversation_slot`](#set_conversation_slot) | `0xc5` | `0x09` | `0x09` |
| [`UI_set_gump_shape`](#set_gump_shape) | `0xe0` | `0xe1` | `0xe0` |
| [`UI_set_gump_text`](#set_gump_text) | `0xdf` | `0xe0` | `0xdf` |
| [`UI_set_gump_text_font`](#set_gump_text_font) | `0xe3` | `0xe4` | `0xe3` |
| [`UI_set_intercept_item`](#set_intercept_item) | `0xb0` | `0xc6` | `0xc5` |
| [`UI_set_item_flag`](#set_item_flag) | `0x89` | `0xa4` | `0xa3` |
| [`UI_set_item_frame`](#set_item_frame) | `0x13` | `0x1b` | `0x1b` |
| [`UI_set_item_frame_rot`](#set_item_frame_rot) | `0x6c` | `0x81` | `0x80` |
| [`UI_set_item_gump_position`](#set_item_gump_position) | `0xde` | `0xdf` | `0xde` |
| [`UI_set_item_quality`](#set_item_quality) | `0x15` | `0x1d` | `0x1d` |
| [`UI_set_item_quantity`](#set_item_quantity) | `0x17` | `0x1f` | `0x1f` |
| [`UI_set_item_shape`](#set_item_shape) | `0x0d` | `0x14` | `0x14` |
| [`UI_set_last_created`](#set_last_created) | `0x25` | `0x2e` | `0x2e` |
| [`UI_set_lift`](#set_lift) | `0x43` | `0x51` | `0x50` |
| [`UI_set_light`](#set_light) | `0x82` | `0x9c` | `0x9b` |
| [`UI_set_new_schedules`](#set_new_schedules) | `0xb1` | `0xb6` | `0xb5` |
| [`UI_set_npc_id`](#set_npc_id) | `0xa5` | `0xbd` | `0xbc` |
| [`UI_set_npc_name`](#set_npc_name) | `0xc7` | `0xd2` | `0xd1` |
| [`UI_set_npc_prop`](#set_npc_prop) | `0x21` | `0x29` | `0x29` |
| [`UI_set_opponent`](#set_opponent) | `0xb2` | `0x5e` | `0x5d` |
| [`UI_set_oppressor`](#set_oppressor) | `0x4c` | `0x5b` | `0x5a` |
| [`UI_set_orrery`](#set_orrery) | `0x63` | — | — |
| [`UI_set_path_failure`](#set_path_failure) | `0x8b` | `0xa7` | `0xa6` |
| [`UI_set_polymorph`](#set_polymorph) | `0xbd` | `0xb3` | `0xb2` |
| [`UI_set_schedule_type`](#set_schedule_type) | `0x1d` | `0x25` | `0x25` |
| [`UI_set_slider_value`](#set_slider_value) | `0xe1` | `0xe2` | `0xe1` |
| [`UI_set_temperature`](#set_temperature) | `0xcf` | `0xba` | `0xb9` |
| [`UI_set_time_palette`](#set_time_palette) | `0x84` | `0x9e` | `0x9d` |
| [`UI_set_timer`](#set_timer) | `0x66` | `0x7b` | `0x7a` |
| [`UI_set_to_attack`](#set_to_attack) | `0x41` | `0x4f` | `0x4e` |
| [`UI_set_usecode_fun`](#set_usecode_fun) | `0xc8` | `0xd3` | `0xd2` |
| [`UI_set_weather`](#set_weather) | `0x45` | `0x53` | `0x52` |
| [`UI_show_npc_face`](#show_npc_face) | `0x03` | `0x03` | `0x03` |
| [`UI_show_npc_face0`](#show_npc_face0) | `0xc1` | `0x05` | `0x05` |
| [`UI_show_npc_face1`](#show_npc_face1) | `0xc2` | `0x06` | `0x06` |
| [`UI_si_display_map`](#si_display_map) | — | `0x56` | `0x55` |
| [`UI_si_path_run_usecode`](#si_path_run_usecode) | `0xb3` | `0xc1` | `0xc0` |
| [`UI_sib_is_dest_reachable`](#sib_is_dest_reachable) | — | — | `0x96` |
| [`UI_sib_path_run_usecode`](#sib_path_run_usecode) | — | — | `0x94` |
| [`UI_sit_down`](#sit_down) | `0x46` | `0x54` | `0x53` |
| [`UI_sprite_effect`](#sprite_effect) | `0x53` | `0x68` | `0x67` |
| [`UI_start_blocking_speech`](#start_blocking_speech) | `0x8f` | `0xd8` | `0xd7` |
| [`UI_start_speech`](#start_speech) | `0x74` | `0x8a` | `0x89` |
| [`UI_stop_arresting`](#stop_arresting) | — | `0x93` | `0x92` |
| [`UI_stop_time`](#stop_time) | `0x56` | `0x6b` | `0x6a` |
| [`UI_summon`](#summon) | `0x47` | `0x55` | `0x54` |
| [`UI_telekenesis`](#telekenesis) | `0x95` | `0xb1` | `0xb0` |
| [`UI_teleport_to_saved_pos`](#teleport_to_saved_pos) | `0xbb` | `0x75` | — |
| [`UI_update_last_created`](#update_last_created) | `0x26` | `0x2f` | `0x2f` |
| [`UI_view_tile`](#view_tile) | `0x94` | `0xb0` | `0xaf` |
| [`UI_wearing_fellowship`](#wearing_fellowship) | `0x67` | `0x7c` | `0x7b` |
| [`UI_wizard_eye`](#wizard_eye) | `0x50` | `0x62` | `0x61` |



## Random Number Intrinsics

These intrinsics are all related to random numbers and their generation.

<a id="get_random"></a>

### UI_get_random

```usecode
int UI_get_random(int max)
```


Generates a random number, similar to rolling a dice of the given side.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `max` | The maximum value of the random number. Should be positive. |

#### Return

Zero if `max` is zero; otherwise, a random number ranging from 1 to `max`.


<a id="die_roll"></a>

### UI_die_roll

```usecode
int UI_die_roll(int min, int max)
```


Generates a random number in the specified range.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `min` | The minimum value of the random number. Should be positive. |
| `max` | The maximum value of the random number. Should be positive. |

#### Return

A random number ranging from `min` to `max`. If `min > max`, the two parameters are reversed by Exult.


<a id="roll_to_win"></a>

### UI_roll_to_win

```usecode
bool UI_roll_to_win(int att, int def)
```


Performs a contest between two supplied values.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `att` | What should be added to the die roll, improving the chances of success. |
| `def` | What should be subtracted from the die roll, reducing the chances of success. |

#### Return

The contest is a roll of a 30-sided die: if the die comes up 30, the return is `true`; if it comes up 1, the return is `false`. For all other values, the attack value is added to the die roll and the defense value is subtracted from the same roll. If the result is 15 or more, the attacker wins and the return is `true`; otherwise, the attacker loses and the return is `false`.


## Scripting Intrinsics

These intrinsics are used to animate characters from usecode. Some of these are deprecated by script blocks, but all others are still useful at times.

<a id="execute_usecode_array"></a>

### UI_execute_usecode_array

```usecode
int UI_execute_usecode_array(object obj, int script[])
int obj->execute_usecode_array(int script[])
```


Deprecated; use Usecode C script blocks instead. Animates an object according to the supplied script.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object to be animated. |
| `script` | The sequence of actions to be executed. |

#### Return

Always returns '1'.


<a id="delayed_execute_usecode_array"></a>

### UI_delayed_execute_usecode_array

```usecode
int UI_delayed_execute_usecode_array(object obj, int script[], int delay)
int obj->delayed_execute_usecode_array(int script[], int delay)
```


Deprecated; use Usecode C script blocks instead. Animates an object according to the supplied script after a delay. Works exactly like `execute_usecode_array`, but starts only after the specified delay.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object to be animated. |
| `script` | The sequence of actions to be executed. |
| `delay` | The number of ticks that must elapse before the animation starts. |

#### Return

Always returns '1'.


<a id="in_usecode"></a>

### UI_in_usecode

```usecode
bool UI_in_usecode(object obj)
bool obj->in_usecode()
```


Checks if an object is currently executing any usecode scripts.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object to be inspected. |

#### Return

`true` if there are any scripts for the specified object, `false` otherwise.


<a id="halt_scheduled"></a>

### UI_halt_scheduled

```usecode
UI_halt_scheduled(object obj)
obj->halt_scheduled()
```


Terminates usecode scripts that animate the supplied object. Scripts that include the '`nohalt`' command are not affected by this intrinsic.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose scripts should be terminated. |


<a id="begin_casting_mode"></a>

### UI_begin_casting_mode

[Exult]


```usecode
UI_begin_casting_mode(actor npc)
UI_begin_casting_mode(actor npc, int val)
npc->begin_casting_mode()
npc->begin_casting_mode(int val)
```


Makes an NPC display casting frames until the end of its next script.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC that should display casting frames. |
| `val` | The shape in 'SHAPES.VGA' to use as casting frames. If omitted, this is assumed to be shape 859. |


<a id="set_to_attack"></a>

### UI_set_to_attack

```usecode
int UI_set_to_attack(actor attacker, object target, int weapon)
int attacker->set_to_attack(object target, int weapon)
int UI_set_to_attack(actor attacker, mixed[4] target, int weapon)
int attacker->set_to_attack(mixed[4] target, int weapon)
```


Schedules a NPC to attack the object with a given weapon. This intrinsic does not animate the NPC in any way; the scheduled attack only emulates the actual hit with the selected weapon. The actual attack is triggered by a script; in an Usecode C script, use the '`attack`' command to actually perform the attack.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `attacker` | Who is attacking. |
| `target` | What is being attacked. Can be an object, NPC, or the return of a `click_on_item` intrinsic call, even if a tile has been clicked. |
| `weapon` | What shape in 'SHAPES.VGA' to use as the source of the weapon information in the attack. It **must** specify a shape that has a 'weapon' tab in Exult Studio, or nothing will happen. |

If the NPC is targeting a tile, the attack will only be meaningful if the weapon causes an explosion.

#### Return

`true` if the attacker, weapon, and object or tile target were accepted and stored, `false` otherwise. This does not mean that an attack has already occurred or hit the target.


<a id="path_run_usecode"></a>

### UI_path_run_usecode

```usecode
bool UI_path_run_usecode(int pos[], function fun, object obj, int event)
bool UI_path_run_usecode(int pos[], function fun, object obj, int event, bool simode)
```


Causes the avatar to walk to a location and execute the specified usecode function when he/she arrives at the destination. If a path cannot be found for the destination, no usecode will be executed unless a call to `set_path_failure` intrinsic is made before returning from usecode.

The avatar can be prevented from reaching his/her destination simply by moving away from the path; this will cancel the effects of the intrinsic, and will cause the usecode function set by `set_path_failure` to be called. If you want the path to be uninterruptible, the avatar's `DONT_MOVE` flag must be set (this is flag 22 in BG and 16 in SI).

The other party members will follow the avatar towards the destination.

When `simode` is true, Exult looks for a free spot within 3 tiles of `pos` and allows a rise/drop of 3 in the z direction. If no spot is found, it tries again at the NPC's current height.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `pos` | The location that the avatar should walk to. Can be a (x, y) position or a (x, y, z) position. If the z coordinate is missing, or if it is negative, it is assumed to be zero. |
| `fun` | The usecode function to be called if the destination is reached. |
| `obj` | The object that should be used as `item` if the usecode function is called. If this parameter is zero, usecode will **not be called** at all. |
| `event` | The `event` that should be used if the usecode function is called. |
| `simode` | This optional parameter can be used to control whether or not to use the SI-specific behavior described above. Defaults to `true` in SI (including SI beta) and `false` in BG. Pass an explicit value to choose the behavior in either game. |

#### Return

If no path can be found to the destination, returns `false`; otherwise, returns `true`.

A successful return means a path has been started, not that the destination has been reached. Use the arrival or failure callback to detect the outcome. `UI_sib_path_run_usecode` is a separate SI beta variant that takes an explicit NPC before `pos`.


<a id="si_path_run_usecode"></a>

### UI_si_path_run_usecode

[SI, Exult]


```usecode
UI_si_path_run_usecode(actor npc, int pos[], int event, object obj, function fun)
UI_si_path_run_usecode(actor npc, int pos[], int event, object obj, function fun, bool flag)
npc->si_path_run_usecode(int pos[], int event, object obj, function fun)
npc->si_path_run_usecode(int pos[], int event, object obj, function fun, bool flag)
```


Causes an NPC to walk to a location and execute the specified usecode function when he/she arrives at the destination. If a path cannot be found for the destination, no usecode will be executed unless a call to `set_path_failure` intrinsic is made before returning from usecode.

The avatar can be prevented from reaching his/her destination simply by moving away from the path; similarly, a scheduled event or scripted event may prevent an NPC from reaching his/her destination. In these cases, the effects of the intrinsic will be cancelled, and the usecode function set by `set_path_failure` will be called. If you want the path to be uninterruptible, the NPC's `DONT_MOVE` flag must be set (this is flag 22 in BG and 16 in SI).

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC that will be affected by this intrinsic. If this isn't a valid NPC, nothing will happen. |
| `pos` | The location that the NPC should walk to. Can be a (x, y) position or a (x, y, z) position. If the z coordinate is missing, or if it is negative, it is assumed to be zero. |
| `event` | The `event` that should be used if the usecode function is called. |
| `obj` | The object that should be used as `item` if the usecode function is called. If this parameter is zero, usecode will **not be called** at all. |
| `fun` | The usecode function to be called if the destination is reached. |
| `flag` | Optional, default `false`. Flag specifying whether or not the usecode function should always be called. If `false`, it will be called only if the NPC can reach its intended destination; if `true`, the specified usecode function will be called even if the destination cannot be reached. The automatic failure callback uses event `SI_PATH_FAILURE` (14). A later `UI_set_path_failure` call can replace the failure function, item, and event. |

This intrinsic uses the nearby-free-spot search described for `UI_path_run_usecode` with `simode = true`. An NPC on the `WAIT` schedule has its `ASLEEP` and `PARALYZED` flags cleared before pathfinding. The call has no meaningful return value.

For SI beta, `UI_sib_path_run_usecode(npc, pos, fun, obj, event)` is a different intrinsic: it has five parameters, places `fun` before `obj` and `event`, and returns a success boolean. `UI_si_path_run_usecode`, with its optional sixth argument, remains available in that table as well.


<a id="set_path_failure"></a>

### UI_set_path_failure

```usecode
UI_set_path_failure(function fun, object obj, int event)
```


Sets usecode to be called if an NPC fails to reach its destination. This intrinsic must be used **after** `UI_path_run_usecode`, `UI_si_path_run_usecode`, or `UI_sib_path_run_usecode`, and affects the NPC from the most recent path call. If, for any reason, the NPC in question cannot reach the destination, the usecode function assigned by this intrinsic will be run.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `fun` | The usecode function to be called if the path fails. |
| `obj` | The object that should be used as `item` if the usecode function is called. This must resolve to a valid object. An invalid or zero value leaves any existing failure callback unchanged. |
| `event` | The `event` that should be used if the usecode function is called. |

> **SI SPECIFIC**
>
> In the original SI, this could never affect the failure `event` for `si_path_run_usecode`, which would always be 14. Exult does not suffer from this limitation.

The NPC must still have a usecode path action for the callback to be installed. A path requested with no callback object may use a plain walking action, in which case this intrinsic has no effect.


<a id="in_usecode_path"></a>

### UI_in_usecode_path

```usecode
bool UI_in_usecode_path(actor npc)
bool npc->in_usecode_path()
```


Checks whether or not the actor is traversing a path due to `path_run_usecode` or `si_path_run_usecode` intrinsics.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC to be checked. |

#### Return

`true` if the NPC is still walking a usecode path, `false` otherwise.


<a id="telekenesis"></a>

### UI_telekenesis

```usecode
UI_telekenesis(function fun)
```


Deprecated; stores a usecode function for special script behavior. The next **script** call to the stored function (e.g., '`script item call fun;`') will have an event ID of 'DOUBLECLICK' instead of the standard 'SCRIPTED' event ID used for scripts. Since Usecode C scripts allow calling a function with an event ID of your choice (e.g., '`script item call fun, DOUBLECLICK;`') it is best to simply ignore this intrinsic.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `fun` | The function to be stored. |


<a id="sib_path_run_usecode"></a>

### UI_sib_path_run_usecode

```usecode
bool UI_sib_path_run_usecode(actor npc, int pos[], function fun, object obj, int event)
bool npc->sib_path_run_usecode(int pos[], function fun, object obj, int event)
```


Causes an NPC to walk to a location and execute a usecode function on arrival. This SI beta intrinsic uses the same argument order as `UI_path_run_usecode`, rather than the order used by `UI_si_path_run_usecode`.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC to move. Invalid NPCs cause the call to fail. |
| `pos` | Destination `[x, y]` or `[x, y, z]`. Missing or negative `z` is treated as zero. |
| `fun` | The usecode function to call on arrival. |
| `obj` | The object to use as `item` for the callback. Zero disables the callback. |
| `event` | The callback's event ID. |

#### Return

`true` if a path was successfully started, `false` otherwise. Starting a path does not guarantee that the NPC will reach its destination.

This intrinsic is available only with `#game "serpentbeta"`. It does not take the `flag` argument of `UI_si_path_run_usecode` and does not request that the callback run when the destination cannot be reached. Use `UI_set_path_failure` to supply a failure callback.


## Conversation Intrinsics

These intrinsics are used to display faces for conversations, storing and recalling answers, etc.

<a id="show_npc_face"></a>

### UI_show_npc_face

```usecode
UI_show_npc_face(int face, int frnum)
UI_show_npc_face(actor face, int frnum)
face->show_npc_face(int frnum)
face.show(frnum)
face.show()
```


Displays a face onscreen for conversations. The face is a shape from 'FACES.VGA'. This intrinsic finds a free slot to display the face in or uses the second slot if there are no free slots.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `face` | The face to be displayed. May be an integer or an NPC. |
| `frnum` | The frame number of the face to be displayed. |

If '`face`' is an integer, the shape to be displayed is the absolute value of this number. If this value is `0` or `356`, the avatar's face will be displayed, using the correct face for the avatar's skin and `PETRA` status flag.

If '`face`' is an NPC, the Exult Studio-set face for that NPC is displayed; the "default" face is equal to the NPC's number.

Use Usecode C's `face.show(frnum)` statement in place of this intrinsic. The frame argument is optional: `face.show()` is equivalent to `face.show(0)`. The receiver may be either a face number or an NPC.

When `frnum` is zero, `npc.say("@Text.@")` displays the NPC's face and the supplied text. Use `npc.say()` to display text previously set with `message`.


<a id="show_npc_face0"></a>

### UI_show_npc_face0

[SI, Exult]


```usecode
UI_show_npc_face0(int face, int frnum)
UI_show_npc_face0(actor face, int frnum)
face->show_npc_face0(int frnum)
face.show@0(frnum)
face.show@0()
```


Displays a face onscreen for conversations. The face is a shape from 'FACES.VGA'. This intrinsic always uses slot 0 for the new face.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `face` | The face to be displayed. May be an integer or an NPC. |
| `frnum` | The frame number of the face to be displayed. |

If '`face`' is an integer, the shape to be displayed is the absolute value of this number. If this value is `0` or `356`, the avatar's face will be displayed, using the correct face for the avatar's skin and `PETRA` status flag.

If '`face`' is an NPC, the Exult Studio-set face for that NPC is displayed; the "default" face is equal to the NPC's number.

Use Usecode C's `face.show@0(frnum)` statement in place of this intrinsic. The frame argument is optional: `face.show@0()` is equivalent to `face.show@0(0)`. The receiver may be either a face number or an NPC.

When `frnum` is zero, `npc.say@0("@Text.@")` displays the NPC's face in slot 0 and the supplied text. Use `npc.say@0()` to display text previously set with `message`.


<a id="show_npc_face1"></a>

### UI_show_npc_face1

[SI, Exult]


```usecode
UI_show_npc_face1(int face, int frnum)
UI_show_npc_face1(actor face, int frnum)
face->show_npc_face1(int frnum)
face.show@1(frnum)
face.show@1()
```


Displays a face onscreen for conversations. The face is a shape from 'FACES.VGA'. This intrinsic always uses slot 1 for the new face.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `face` | The face to be displayed. May be an integer or an NPC. |
| `frnum` | The frame number of the face to be displayed. |

If '`face`' is an integer, the shape to be displayed is the absolute value of this number. If this value is `0` or `356`, the avatar's face will be displayed, using the correct face for the avatar's skin and `PETRA` status flag.

If '`face`' is an NPC, the Exult Studio-set face for that NPC is displayed; the "default" face is equal to the NPC's number.

Use Usecode C's `face.show@1(frnum)` statement in place of this intrinsic. The frame argument is optional: `face.show@1()` is equivalent to `face.show@1(0)`. The receiver may be either a face number or an NPC.

When `frnum` is zero, `npc.say@1("@Text.@")` displays the NPC's face in slot 1 and the supplied text. Use `npc.say@1()` to display text previously set with `message`.


<a id="remove_npc_face"></a>

### UI_remove_npc_face

```usecode
UI_remove_npc_face(int face)
UI_remove_npc_face(actor face)
face->remove_npc_face()
```


Deprecated; use Usecode C's '`npc.hide()`' statement instead. Hides the specified face if it is being shown.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `face` | The face to be hidden. May be an integer or an NPC. |

If '`face`' is an integer, the shape to be hidden is the absolute value of this number. If this value is `0` or `356`, the avatar's face will be hidden.

If '`face`' is an NPC, the Exult Studio-set face for that NPC is hidden; the "default" face is equal to the NPC's number.


<a id="remove_npc_face0"></a>

### UI_remove_npc_face0

[SI, Exult]


```usecode
UI_remove_npc_face0()
hide@0()
```


Removes whatever face is being shown in slot 0. Use Usecode C's `hide@0()` statement in place of this intrinsic.


<a id="remove_npc_face1"></a>

### UI_remove_npc_face1

[SI, Exult]


```usecode
UI_remove_npc_face1()
hide@1()
```


Removes whatever face is being shown in slot 1. Use Usecode C's `hide@1()` statement in place of this intrinsic.


<a id="reset_conv_face"></a>

### UI_reset_conv_face

```usecode
UI_reset_conv_face()
```


Resets the portrait frame in conversation slot 0 to frame 0 after displaying any pending text. This does not change the active conversation slot; use `UI_set_conversation_slot(0)` to select that slot for subsequent speech.


<a id="set_conversation_slot"></a>

### UI_set_conversation_slot

[SI, Exult]


```usecode
UI_set_conversation_slot(int slot)
npc->set_conversation_slot()
```


Changes the active conversation slot.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `slot` | The new active slot. Must be zero or one. |


<a id="change_npc_face0"></a>

### UI_change_npc_face0

[SI, Exult]


```usecode
UI_change_npc_face0(int frame)
```


Changes the frame of the face being displayed in slot 0.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `frame` | The new frame to be used. |


<a id="change_npc_face1"></a>

### UI_change_npc_face1

[SI, Exult]


```usecode
UI_change_npc_face1(int frame)
```


Changes the frame of the face being displayed in slot 1.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `frame` | The new frame to be used. |


<a id="init_conversation"></a>

### UI_init_conversation

[SI]


```usecode
UI_init_conversation()
```


Initializes the face information for a conversation. In Exult, it is usually safe to skip it.


<a id="end_conversation"></a>

### UI_end_conversation

[SI]


```usecode
UI_end_conversation()
```


Shows any pending text and removes all faces from the screen. Usually safe to skip this in Exult.


<a id="add_answer"></a>

### UI_add_answer

```usecode
UI_add_answer(string answer)
UI_add_answer(string answer[])
```


Deprecated; use Usecode C's '`add(string answer)`' or '`add(string answer[])`' statements instead. Adds the specified strings as dialog options that the player can select.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `answer` | A string or array of strings containing the dialog options to be added. |


<a id="remove_answer"></a>

### UI_remove_answer

```usecode
UI_remove_answer(string answer)
UI_remove_answer(string answer[])
```


Deprecated; use Usecode C's '`(remove)`' construct instead. Removes the supplied strings from the dialog options available to the player.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `answer` | A string or array of strings containing the dialog options to be removed. |


<a id="push_answers"></a>

### UI_push_answers

```usecode
UI_push_answers()
```


Stores the current conversation options and clears all choices. The dialog options are stored in a LIFO stack that persists until the dialog ends; before that point, the stored options can be retrieved with a call to `UI_pop_answers` intrinsic.


<a id="pop_answers"></a>

### UI_pop_answers

```usecode
UI_pop_answers()
```


Clears all current conversation options and restores the stored conversation options, if any. The restored dialog options are those stored by the *last* call to `UI_push_answers` intrinsic.


<a id="clear_answers"></a>

### UI_clear_answers

```usecode
UI_clear_answers()
```


Clears all current dialog options.


<a id="select_from_menu"></a>

### UI_select_from_menu

```usecode
string UI_select_from_menu()
```


Makes the player choose one answer from all dialog options currently being displayed.

#### Return

The chosen dialog option.


<a id="select_from_menu2"></a>

### UI_select_from_menu2

```usecode
int UI_select_from_menu2()
```


Makes the player choose one answer from all dialog options currently being displayed.

#### Return

The index (ranging from 1 to n) of the chosen dialog option.


## Sound Intrinsics

All intrinsics in this section relate to playing sound effects, music or speech.

<a id="play_sound_effect"></a>

### UI_play_sound_effect

```usecode
UI_play_sound_effect(int sfx)
```


Plays a sound effect.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `sfx` | A numerical value indicating the sound effect that should be played. |


<a id="play_sound_effect2"></a>

### UI_play_sound_effect2

```usecode
UI_play_sound_effect2(int sfx, object obj)
```


Causes an object to generate a sound effect.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `sfx` | A numerical value indicating the sound effect that should be played. |
| `obj` | The object generating the sound effect. The sound intensity decrease as the avatar gets farther away from this object. |


<a id="play_music"></a>

### UI_play_music

```usecode
UI_play_music(int track, object obj)
```


Plays the specified song.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `track` | The track number to be played or 255 (0xFF) to **stop** any music that is playing. |
| `obj` | The source of the music, or zero for 'unattached' music. Positive numbers are **not** interpreted as NPCs! |

If supplied with a valid object or NPC that is on the game world, this intrinsic will also display the sprite effect # 24 (musical notes).


<a id="get_music_track"></a>

### UI_get_music_track

```usecode
int UI_get_music_track()
```


Gets the music being played.

#### Return

If music is enabled and a song is playing, returns the track number of the current song. Otherwise, return is -1.

> **NOTES ABOUT ORIGINAL BG**
>
> The return value in the original Black Gate was more complicated; the current song was the return value divided by 256, while the next song (the one that would be resumed after the current one ends) was the remainder of that same division. None of this behavior is true for Exult, which works as described above.


<a id="start_speech"></a>

### UI_start_speech

```usecode
bool UI_start_speech(int track)
```


Plays the specified speech track.

> **SI SPECIFIC**
>
> In SI, tracks 0-20 show the Balance Serpent's portrait (solid when wearing the serpent ring, translucent otherwise), tracks 21-22 show the Guardian, tracks 23-24 show Arcadion, and tracks 25-26 show the Chaos and Order Serpents respectively. These portraits wait for a click before being removed; other tracks do not select a portrait here.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `track` | Number indicating what speech track is to be played. |

#### Return

Returns `true` for a non-negative track number and `false` for a negative track number. This does not report whether audio was actually available or played. The requested number is stored for `UI_get_speech_track`; a negative number also reinitializes the conversation.


<a id="start_blocking_speech"></a>

### UI_start_blocking_speech

[BG, Exult]


```usecode
bool UI_start_blocking_speech(int track)
```


Plays the specified speech track. Blocks until the speech finishes.

> **SI SPECIFIC**
>
> In SI, tracks 0-20 show the Balance Serpent's portrait (solid when wearing the serpent ring, translucent otherwise), tracks 21-22 show the Guardian, tracks 23-24 show Arcadion, and tracks 25-26 show the Chaos and Order Serpents respectively. These portraits wait for a click before being removed; other tracks do not select a portrait here.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `track` | Number indicating what speech track is to be played. |

#### Return

Returns `true` for a non-negative track number and `false` for a negative track number. This does not report whether audio was actually available or played. The requested number is stored for `UI_get_speech_track`; a negative number also reinitializes the conversation.


<a id="get_speech_track"></a>

### UI_get_speech_track

```usecode
int UI_get_speech_track()
```


Determines the currently playing speech track.

#### Return

A number indicating what voice track is being played.


## Searching and Counting Intrinsics

These intrinsics allow you to find and count objects, whether they are in a container or in the game world.

<a id="find_nearest"></a>

### UI_find_nearest

```usecode
object UI_find_nearest(object obj, int shape, int dist)
object obj->find_nearest(int shape, int dist)
```


Searches the area around a given object looking for other objects of a specified type.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object around which the search will happen. |
| `shape` | The shape we are looking for. Can be `SHAPE_ANY = -359` to match any shape. |
| `dist` | The maximum distance from 'obj' that will be considered in the search. |

#### Return

An object reference to the nearest object found given the search criteria. You cannot find eggs, barges, invisible or transparent objects with this intrinsic.


<a id="find_object"></a>

### UI_find_object

```usecode
object UI_find_object(object cont, int shape, int qual, int frame)
object cont->find_object(int shape, int qual, int frame)
object UI_find_object(int cont, int shape, int qual, int frame)
object UI_find_object(int cont[3], int shape, int qual, int frame)
```


Finds the first object matching the search criteria.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `cont` | The object or location to be searched. Can be a container or NPC, a (x, y, z) location, `PARTY = -357` to search the whole party or `FIND_ON_SCREEN = -359` to search a square region around the screen location with size given by the screen's height. |
| `shape` | The shape we are looking for. |
| `qual` | The quality that is being sought. Can be `QUALITY_ANY = -359` to match any quality. |
| `frame` | What frame is desired. Can be `FRAME_ANY = -359` to match any frame. |

> **IMPORTANT NOTES**
>
> When 'cont' is a position, the search is made in a 1-tile-wide region centered on the given position.
>
> Also, if the search is **not** being done in a container or NPC, 'shape' can be `SHAPE_ANY = -359` to match any shape.

#### Return

An object reference to the first found object, or zero if no objects match the given criteria.


<a id="find_nearby"></a>

### UI_find_nearby

```usecode
object[] UI_find_nearby(object loc, int shape, int dist, int mask)
object[] loc->find_nearby(int shape, int dist, int mask)
object[] UI_find_nearby(int loc[3], int shape, int dist, int mask)
object[] UI_find_nearby(int loc[5], int shape, int dist, int mask)
object[] UI_find_nearby(mixed loc[4], int shape, int dist, int mask)
```


Searches a specified area looking for objects matching a given criteria.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `loc` | The center of the area to be searched. Can be the return of a '`UI_click_on_item`' intrinsic call, an object reference, a (x, y, z) position vector or a (x, y, z, quality, frame) vector. For the last form, quality can be `QUALITY_ANY = -359` and frame can be `FRAME_ANY = -359` (but there is little point in both being wildcards, as this is the default behavior). |
| `shape` | The shape we are looking for. Can be `SHAPE_ANY = -359` to match any shape. |
| `dist` | Radius (in tiles) of the area to be searched. |
| `mask` | Parameter specifying the classes of objects that can be found. |

The mask value controls what can be found; using the proper mask, you can find eggs, barges, invisible or transparent objects with this intrinsic. The mask can be the combination (sum) of one or more of the following values:

| Parameter / value | Description |
| --- | --- |
| MASK_NONE = 0x00 | Cannot find eggs, barges, invisible or transparent objects. |
| MASK_NPC = 0x04 | Restricts the search to NPCs, alive or dead. Ignored if a non-wildcard shape is given. |
| MASK_NPC2 = 0x08 | Restricts the search to living NPCs. |
| MASK_EGG = 0x10 | Allows eggs and barges to be found. |
| MASK_INVISIBLE = 0x20 | Allows invisible objects that are not in the avatar's party to be found. |
| MASK_PARTY_INVISIBLE = 0x40 | Allows invisible party members to be found |
| MASK_TRANSLUCENT = 0x80 | Allows translucent objects to be found |

#### Return

Array containing all objects of matching the given criteria that are located in the specified area. This array is sorted from right to left, closest first.


<a id="find_nearby_avatar"></a>

### UI_find_nearby_avatar

```usecode
object[] UI_find_nearby_avatar(int shape)
```


Searches the area around the avatar looking for other objects of the specified shape.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `shape` | The shape we are looking for. Can be `SHAPE_ANY = -359` to match any shape. |

#### Return

Array containing all objects of the given shape that are located within 192 tiles of the avatar. This array is sorted from right to left, closest first. You cannot find eggs, barges, invisible or transparent objects with this intrinsic.


<a id="npc_nearby"></a>

### UI_npc_nearby

```usecode
bool UI_npc_nearby(object obj)
bool obj->npc_nearby()
```


Finds out if the desired object is on screen and can act. An NPC is considered able to act if it is not asleep, paralyzed or dead; other objects are always considered able to act. Note that, despite the name, this intrinsic can also be used for objects.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object or NPC to be checked. |

#### Return

`true` if the object is in the screen and can act, `false` otherwise or if the supplied parameter is not an object or NPC.


<a id="npc_nearby2"></a>

### UI_npc_nearby2

[SI]


```usecode
bool UI_npc_nearby2(object obj)
bool obj->npc_nearby2()
```


Finds out if the desired object is nearby. In this context, 'nearby' means fewer than 40 tiles from the avatar. Note that, despite the name, this intrinsic is valid for all objects, not just NPCs.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object to be checked. |

#### Return

`true` if the object is valid and fewer than 40 tiles from the avatar, `false` otherwise. Sleeping, paralysis, or death do not affect this test.


<a id="count_objects"></a>

### UI_count_objects

```usecode
int UI_count_objects(object obj, int shape, int qual, int frame)
int obj->count_objects(int shape, int qual, int frame)
```


Counts all objects matching a given search criteria that are contained in the supplied object.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose contents are to be searched. Can be a container, an NPC or `PARTY = -357` to search the whole party. |
| `shape` | The shape we are looking for. Can be `SHAPE_ANY = -359` to match any shape. |
| `qual` | The quality that is being sought. Can be `QUALITY_ANY = -359` to match any quality. |
| `frame` | What frame is desired. Can be `FRAME_ANY = -359` to match any frame. |

#### Return

The number of objects that match the given criteria.


<a id="get_cont_items"></a>

### UI_get_cont_items

```usecode
object[] UI_get_cont_items(object obj, int shape, int qual, int frame)
object[] obj->get_cont_items(int shape, int qual, int frame)
```


Looks for all objects matching a given search criteria that are contained in the supplied object.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose contents are to be searched. Can be a container or an NPC. |
| `shape` | The shape we are looking for. Can be `SHAPE_ANY = -359` to match any shape. |
| `qual` | The quality that is being sought. Can be `QUALITY_ANY = -359` to match any quality. |
| `frame` | What frame is desired. Can be `FRAME_ANY = -359` to match any frame. |

#### Return

Array containing object references to all objects that match the given criteria.


## Placement Intrinsics

With these intrinsics, you can create, move or delete objects or NPCs, whether they are in the game world or in a container.

<a id="is_not_blocked"></a>

### UI_is_not_blocked

```usecode
bool UI_is_not_blocked(int pos[3], int shape, int frame)
```


Checks if there is enough room for a given shape in the specified location.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `pos` | The location to be checked. |
| `shape` | The shape in 'SHAPES.VGA' that is to be checked. |
| `frame` | The frame of the given shape that should be checked. |

#### Return

If there is enough room in the given location for an object of the given shape and frame, returns `true`; otherwise, returns `false`.


<a id="add_party_items"></a>

### UI_add_party_items

```usecode
actor[] UI_add_party_items(int count, int shape, int qual, int frame)
actor[] UI_add_party_items(int count, int shape, int qual, int frame, bool temp)
```


Creates a specified amount of objects matching the given parameters and gives them all to the party. Not all objects requested may be created; in particular, this intrinsic is guaranteed to create only as many of the requested objects as the party can hold.

> **SI SPECIFIC**
>
> If the party cannot hold all requested objects, the remaining objects are created and then placed in the ground nearby.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `count` | The amount of objects to be created. |
| `shape` | The shape in 'SHAPES.VGA' that should be used for the created objects. |
| `qual` | The quality of the objects being created. Can be `QUALITY_ANY = -359` to create items of any quality. |
| `frame` | What frame will be used for the created objects. Can be `FRAME_ANY = -359` to create items of any frame. |
| `temp` | Optional flag, default `false`, indicating whether the created items are temporary or not. |

#### Return

Returns an array of object references for all party members that received any of the created objects.

> **BG SPECIFIC**
>
> The 'temp' flag applies to all objects created by this function.

> **SI SPECIFIC**
>
> If any objects were added to the ground, the number of objects on the ground is appended to the end of the return array. The 'temp' flag applies only to those objects that were added to the floor.


<a id="remove_party_items"></a>

### UI_remove_party_items

```usecode
bool UI_remove_party_items(int count, int shape, int qual, int frame)
bool UI_remove_party_items(int count, int shape, int qual, int frame, bool unk)
```


Looks through the party's possessions and deletes a specified amount of objects matching a given search criteria. This function is an all-or-nothing function: it will delete all requested objects or none at all if it found less objects than the amount that was requested for deletion.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `count` | The amount of objects to be deleted. Can be `QUANTITY_ANY = -359` to delete all objects that match the criteria. |
| `shape` | The shape we are looking for. |
| `qual` | The quality that is being sought. Can be `QUALITY_ANY = -359` to match any quality. |
| `frame` | What frame is desired. Can be `FRAME_ANY = -359` to match any frame. |
| `unk` | Flag of unknown meaning. This parameter is ignored by Exult, and can be omitted. |

#### Return

`true` if 'count' objects were found and deleted, `false` if no objects were deleted.


<a id="summon"></a>

### UI_summon

```usecode
object UI_summon(int shape)
object UI_summon(int shape, bool unk)
```


Creates a monster of the supplied shape. This monster has equipment as defined in its equipment record, starts with an `IN_COMBAT` schedule, and inherits `item`'s effective alignment when `item` is an NPC outside the party. If `item` is a party member or is not an NPC, the summoned monster's alignment is `GOOD`. The monster will be placed within five tiles of the avatar.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `shape` | The shape in 'SHAPES.VGA' to be used for the summoned monster. It **must** have a 'monster' tab in Exult Studio. |
| `unk` | A flag of unknown meaning. Exult ignores this flag, and it is safe to omit it. |

#### Return

Zero if there is no free tile near the avatar or the specified shape is not a valid monster shape; an object reference to the summoned monster otherwise.


<a id="clone"></a>

### UI_clone

```usecode
actor UI_clone(actor npc)
actor npc->clone()
```


Creates a duplicate of the supplied NPC. This duplicate is always of `GOOD` alignment and starts with an `IN_COMBAT` schedule. The clone is placed in a spot near to the original NPC.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC that should be cloned. |

#### Return

An object reference to the clone, or zero if the intrinsic did not receive a valid NPC to clone or if a suitable spot for the clone could not be found.


<a id="create_new_object"></a>

### UI_create_new_object

```usecode
object UI_create_new_object(int shape)
```


Creates a new object with the specified shape.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `shape` | The shape in 'SHAPES.VGA' of the new object. If this is a monster shape, the object is created without any equipment whatsoever, even if the shape has an associated equipment entry. |

#### Return

An object reference to the newly created object. This object is pushed into the 'last created' LIFO stack, and starts out in 'limbo'; it will remain there until placed either, in the game world through the '`UI_update_last_created`' intrinsic or inside a container through the '`UI_give_last_created`' intrinsic.


<a id="create_new_object2"></a>

### UI_create_new_object2

[SI, Exult]


```usecode
object UI_create_new_object2(int shape, int pos[])
object UI_create_new_object2(int shape, int pos[], [int frame])
```


Creates a new object with the specified shape and places it in the game world at the supplied location.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `shape` | The shape in 'SHAPES.VGA' of the new object. If this is a monster shape, the object is created with all equipment specified by the shape's equipment entry. |
| `pos` | The location where the new object is to be placed. |
| `frame` | Optional frame number for the object. If omitted or negative, preserves the frame selected during creation. |

The location can be specified by arrays ranging from 1 to 4 components. The meaning of these components is as follows:

- 1 element: the object will be deleted (equivalent to '`remove_item`' intrinsic);
- 2 elements: mean a (x, y) location. The object is placed at z = 0 in the current game map;
- 3 elements: mean a (x, y, z) location. The object is placed in the current game map;
- 4 elements: mean a (x, y, z, map) location. The object is placed in the specified map unless it is -1, in which case it will be placed in the current game map.

#### Return

An object reference to the newly created object. This object is placed in the game world exactly as if `UI_update_last_created(pos)` had been called, and hence will **not** be in the 'last created' stack.


<a id="create_barge_object"></a>

### UI_create_barge_object

[Exult]


```usecode
UI_create_barge_object(int width, int height)
UI_create_barge_object(int width, int height, int dir)
```


Creates a barge object with the specified width, height and direction.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `width` | The width (x), in tiles, of the barge object. |
| `height` | The height (y), in tiles, of the barge object. |
| `dir` | This optional parameter sets the barge's initial direction. If missing, it is assumed to be north (0). |

#### Return

An object reference to the newly created barge object. This object is pushed into the 'last created' LIFO stack, and starts out in 'limbo'; it will remain there until placed either, in the game world through the '`UI_update_last_created`' intrinsic or inside a container through the '`UI_give_last_created`' intrinsic.


<a id="set_last_created"></a>

### UI_set_last_created

```usecode
object UI_set_last_created(object obj)
object obj->set_last_created()
```


Moves the object from the game map or container where it was and places it in the 'last created' stack. This intrinsic does nothing if the specified object is already in the aforementioned stack.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object to be moved to the 'last created' stack. |

#### Return

Returns zero if the object was already in the 'last created' stack, an object reference to the object otherwise.


<a id="update_last_created"></a>

### UI_update_last_created

```usecode
bool UI_update_last_created(int pos[])
```


Pops the last object pushed in the 'last created' stack and places it in the game map at the supplied position.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `pos` | The location where the new object is to be placed. |

The location can be specified by arrays ranging from 1 to 4 components. The meaning of these components is as follows:

- 1 element: the object will be deleted (equivalent to '`remove_item`' intrinsic). The original usecode generally passed '<code>-358</code>' for this parameter, but this is likely just by convention as any value works.
- 2 elements: mean a (x, y) location. In Exult, the object is placed at `z = 0` in the current game map; in the original games, nothing is done and the update fails.
- 3 elements: mean a (x, y, z) location. The object is placed in the current game map. In Exult, this always succeeds; it could fail in the original games.
- 4 elements: mean a (x, y, z, map) location. Exult-only. The object is placed in the specified map unless it is -1, in which case it will be placed in the current game map.

#### Return

For the 1-element version, `false` if the 'last created' stack is empty or `true` if the object was deleted. Otherwise, returns `true` if object was successfully placed on the map, `false` otherwise.

> **NOTES ABOUT ORIGINAL GAMES**
>
> In the original games, the function could fail to place the item in the map. In this case, it would try to delete an unrelated on-screen object flagged with the TEMPORARY flag to make room for the updated object and retry. The retry could fail again, in which case the update fails; otherwise, it would succeed. This means that, on occasion, using this function could cause objects to be deleted in the original.


<a id="give_last_created"></a>

### UI_give_last_created

```usecode
bool UI_give_last_created(object obj)
bool obj->give_last_created()
```


Gives the last item in the 'last created' stack to the specified container or NPC. This intrinsic ignores volume, and is quite likely to succeed; the given object is popped from the 'last created' stack only if the intrinsic succeeds.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | Container or NPC that will gain the object. |

#### Return

`true` if successful, `false` otherwise.


<a id="get_object_position"></a>

### UI_get_object_position

```usecode
int[3] UI_get_object_position(object obj)
int[3] obj->get_object_position()
```


Gets the current location of the desired object.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose position is desired. |

#### Return

An array containing the (x, y, z) position of the object.

For an object inside a container, this is the position of its outermost container. Animated objects use their original tile coordinates, so animation offsets do not change the result. An invalid object returns `[0, 0, 0]`.


<a id="move_object"></a>

### UI_move_object

```usecode
UI_move_object(object obj, int pos[])
UI_move_object(object obj, int pos[], bool flag)
obj->move_object(int pos[])
obj->move_object(int pos[], bool flag)
```


Moves the specified object to a new location in the game world. This move is instantaneous, and is often used for teleportation in the original usecode.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object to be moved. Can be `PARTY = -357` to teleport the entire party. |
| `pos` | The object's new location. Can be a (x, y) position, a (x, y, z) position or a (x, y, z, map) position. If no z coordinate is specified, it is assumed to be zero. If no map is specified, or if it is set to a negative number, the avatar's map will be used instead. |
| `flag` | Optional, default `false`. When `obj` is `PARTY`, set to `true` to include paralyzed or sleeping party members in the teleport. Ignored for an individual object. |

If the avatar (or the party) is being teleported, the view will center in the new location and nearby eggs will be activated normally.


<a id="get_distance"></a>

### UI_get_distance

```usecode
int UI_get_distance(object obj1, object obj2)
int obj1->get_distance(obj2)
```


Gets the distance between two objects.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj1` | One of the two objects. |
| `obj2` | The other object. |

#### Return

The distance between the two objects, or zero if one or both of the parameters is not a valid object.

Objects in containers are measured from their outermost containers.


<a id="find_direction"></a>

### UI_find_direction

```usecode
int UI_find_direction(object param1, object param2)
int param1->find_direction(object param2)
int UI_find_direction(object param1, int param2[3])
int param1->find_direction(int param2[3])
int UI_find_direction(object param1, mixed param2[4])
int param1->find_direction(mixed param2[4])
int UI_find_direction(int param1[3], object param2)
int UI_find_direction(int param1[3], int param2[3])
int UI_find_direction(int param1[3], mixed param2[4])
int UI_find_direction(mixed param1[4], object param2)
int UI_find_direction(mixed param1[4], int param2[3])
int UI_find_direction(mixed param1[4], mixed param2[4])
```


Get the direction from one location or object to another.

This intrinsic is aliased under the name '`direction_from`'; they are both equivalent in Exult.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `param1` | The object or location **from** which we want the direction. |
| `param2` | The object or location **to** which we want the direction. |

Both parameters can be objects, arrays with 3 components or arrays with 4 components. Depending on what they are, Exult will treat them differently and expect them to conform to a certain format; specifically:

| Parameter / value | Description |
| --- | --- |
| Type | Interpretation/Format |
| `object` | The parameter is an object or NPC. |
| `int [3]` | The parameter is a (x, y, z) position, such as the return of a `UI_get_object_position` intrinsic call. |
| `mixed [4]` | The parameter is of the form (object, x, y, z), such as the return of a call to `UI_click_on_item` intrinsic. |

#### Return

The 8-cardinal-point direction of an arrow starting in the first parameter and pointing to the second parameter. It can be one of the following values:

| NORTH | = 0 | NORTHEAST | = 1 | EAST | = 2 | SOUTHEAST | = 3 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| SOUTH | = 4 | SOUTHWEST | = 5 | WEST | = 6 | NORTHWEST | = 7 |


<a id="add_cont_items"></a>

### UI_add_cont_items

[SI, Exult]


```usecode
int UI_add_cont_items(object obj, int count, int shape, int qual, int frame)
int UI_add_cont_items(object obj, int count, int shape, int qual, int frame, bool temp)
int obj->add_cont_items(int count, int shape, int qual, int frame)
int obj->add_cont_items(int count, int shape, int qual, int frame, bool temp)
```


Create a number of objects inside a container.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The container or NPC that will receive the created objects. |
| `count` | The amount of objects to be created. |
| `shape` | The shape in 'SHAPES.VGA' that should be used for the created objects. |
| `qual` | The quality of the objects being created. Can be `QUALITY_ANY = -359` to create items of any quality. |
| `frame` | What frame will be used for the created objects. Can be `FRAME_ANY = -359` to create items of any frame. |
| `temp` | Optional flag, default `false`, indicating whether the created items are temporary or not. |

#### Return

How many objects were actually added to the container.


<a id="remove_cont_items"></a>

### UI_remove_cont_items

[SI, Exult]


```usecode
int UI_remove_cont_items(object obj, int count, int shape, int qual, int frame)
int UI_remove_cont_items(object obj, int count, int shape, int qual, int frame, bool unk)
int obj->remove_cont_items(int count, int shape, int qual, int frame)
int obj->remove_cont_items(int count, int shape, int qual, int frame, bool unk)
```


Looks through the contents of a container and deletes a specified amount of objects matching a given search criteria.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose contents are to be searched. Can be a container, an NPC or `PARTY = -357` to search the whole party. |
| `count` | The amount of objects to be deleted. Can be `QUANTITY_ANY = -359` to delete all objects that match the criteria. |
| `shape` | The shape we are looking for. |
| `qual` | The quality that is being sought. Can be `QUALITY_ANY = -359` to match any quality. |
| `frame` | What frame is desired. Can be `FRAME_ANY = -359` to match any frame. |
| `unk` | Flag of unknown meaning. This parameter is ignored by Exult, and can be omitted. |

#### Return

The number of objects actually removed.


<a id="remove_from_area"></a>

### UI_remove_from_area

[SI, Exult]


```usecode
UI_remove_from_area(int shape, int frame, int ul[2], int lr[2])
```


Scans a rectangular area for objects matching the given criteria and deletes all found objects.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `shape` | The shape in 'SHAPES.VGA' that is to be searched. |
| `frame` | The frame being sought. |
| `ul` | A (x, y) position specifying the upper-left corner of the region to be searched. |
| `lr` | A (x, y) position specifying the lower-right corner of the region to be searched |


<a id="create_new_egg"></a>

### UI_create_new_egg

[Exult]


```usecode
object UI_create_new_egg(int type, int pos[], int criteria, int probability, int distance, int flags, int data[])
```


Creates an egg object at the specified location. Eggs are invisible trigger objects that activate under certain conditions.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `type` | The egg type (1-11). This also determines the egg's shape and frame: 1=monster, 2=jukebox, 3=soundsfx, 4=voice, 5=usecode, 6=missile, 7=teleport, 8=weather, 9=path, 10=button, 11=intermap |
| `pos` | The location where the egg is to be placed (same format as `UI_create_new_object2`). |
| `criteria` | Activation criteria (0-7): 0=cached_in, 1=party_near, 2=avatar_near, 3=avatar_far, 4=avatar_footpad, 5=party_footpad, 6=something_on, 7=external_criteria |
| `probability` | Probability of activation (1-100). |
| `distance` | Activation distance (0-31). |
| `flags` | Egg flags as a bitmask. Bit 0: nocturnal (only activates at night), Bit 1: once (only activates once), Bit 2: hatched (egg has already been triggered), Bit 3: auto_reset (can trigger again after hatching) |
| `data` | An array of type-specific data (see below). A single value can also be passed for types that only need one element. |

#### Return

An object reference to the newly created egg, or a null object reference if creation fails. On success, the egg is placed using `UI_update_last_created(pos)` and removed from the last-created stack.

#### Notes

All seven arguments are required by this intrinsic. The `data` array uses one-based Usecode indices. Missing data elements default to zero, and only the first five elements are read. The data is encoded internally as follows:

| Parameter / value | Description |
| --- | --- |
| Monster (1): [shape, frame, count, schedule, alignment] | `data[1]` = monster shape number, `data[2]` = monster frame number, `data[3]` = spawn count, `data[4]` = schedule type (0=combat, 1=horiz_pace, 2=vert_pace, etc.), `data[5]` = alignment (0=neutral, 1=good, 2=evil, 3=chaotic) |
| Jukebox (2): [song, continuous] | `data[1]` = music track number, `data[2]` = continuous (0=play once, 1=loop) |
| Soundsfx (3): [sfx_num, continuous] | `data[1]` = sound effect number, `data[2]` = continuous (0=play once, 1=loop) |
| Voice (4): [speech_num] | `data[1]` = speech number |
| Usecode (5): [usecode_fun, quality] | `data[1]` = usecode function number (or function name), `data[2]` = quality |
| Missile (6): [shape, direction, delay] | `data[1]` = missile shape number, `data[2]` = direction (0-7 for compass directions, 8 for party member), `data[3]` = delay |
| Teleport (7): [dest_x, dest_y, dest_z] or [egg_num] | If `data[2]` and `data[3]` are zero (including missing elements), and `data[1]`<256: `data[1]` = path egg number. Otherwise: `data[1]` = X, `data[2]` = Y, `data[3]` = Z (lift) |
| Weather (8): [weather_type, duration] | `data[1]` = weather type (0=halt/normal, 1=snow, 2=storm, 3=sparkle, 4=fog, 5=overcast, 6=clouds), `data[2]` = duration |
| Path (9): [path_num] | `data[1]` = path egg quality number |
| Button (10): [distance] | `data[1]` = distance (triggers nearby external_criteria eggs) |
| Intermap (11): [dest_x, dest_y, dest_z, mapnum] | `data[1]` = X, `data[2]` = Y, `data[3]` = Z (lift), `data[4]` = map number |


## Object Intrinsics

These intrinsics are used to get or set properties of an object, such as quantity, quality or current frame.

<a id="get_item_shape"></a>

### UI_get_item_shape

```usecode
int UI_get_item_shape(object obj)
int obj->get_item_shape()
```


Gets the current shape of the desired object.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose shape is desired. |

#### Return

The shape in 'SHAPES.VGA' of the object in question, or zero for an invalid object. For an NPC, this is the real shape, rather than the shape currently displayed by polymorph effects.


<a id="get_shape_flag"></a>

### UI_get_shape_flag

[Exult]


```usecode
int UI_get_shape_flag(object target, int flag)
int target->get_shape_flag(int flag)
int UI_get_shape_flag(int shape, int flag)
int UI_get_shape_flag(int target[2], int flag)
int UI_get_shape_flag(int target[3], int flag)
int UI_get_shape_flag(mixed target[4], int flag)
```


Tests one of Exult's shape information flags for an object, shape number, or map tile.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `target` | The object, NPC, shape number, or tile position to inspect. Tile positions may be passed as `[x, y]`, `[x, y, z]`, or a `click_on_item`-style `[object, x, y, z]` array. Coordinate targets check both the flat tile and any non-flat object whose footprint covers that tile coordinate. |
| `flag` | The shape flag ID to test. Values outside the valid range return `false`. |

#### Return

`true` if the target's shape has the requested flag, or `false` if it does not, the target is invalid, or the flag ID is out of range.

#### Flag IDs

| ID | Flag |
| --- | --- |
| 0 | SFX |
| 1 | strange movement |
| 2 | contact effect |
| 3 | water |
| 4 | fire field |
| 5 | sleep field |
| 6 | poison field |
| 7 | caltrops field |
| 8 | campfire field |
| 9 | any field type |
| 10 | animated |
| 11 | solid |
| 12 | transparent |
| 13 | translucent |
| 14 | occludes |
| 15 | light source |
| 16 | x obstacle |
| 17 | y obstacle |
| 18 | door |
| 19 | barge part |
| 20 | container locked |
| 21 | extradimensional storage |
| 22 | quantity frames |
| 23 | usecode events |
| 24 | explosive |
| 25 | on fire |
| 26 | lightweight |
| 27 | body |
| 28 | jawbone |
| 29 | mirror |
| 30 | spell |

#### Notes

When `target` is a tile position, Exult checks the flat at that tile and also checks object shapes occupying the tile. For `[x, y, z]` targets, the `z` value is used when matching object footprints; flat tile checks use the `x` and `y` coordinates.

A four-element array first tries the object in element 1 (`target[1]`). If that is not a valid item, Exult falls back to elements 2, 3, and 4 as `x`, `y`, and `z` and performs the same flat-tile and object-footprint checks.

This lets scripts test semantic shape flags such as `on fire`, `water`, `solid`, or `contact effect` at a map coordinate without maintaining hard-coded shape number lists.

Usecode arrays are one-based. A negative integer target is resolved as an NPC reference; a non-negative integer is treated as a shape number. Two-element coordinate targets use `z = 0`.


<a id="set_item_shape"></a>

### UI_set_item_shape

```usecode
UI_set_item_shape(object obj, int shape)
obj->set_item_shape(int shape)
```


Changes the shape of an object.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose shape is to be changed. |
| `shape` | The shape in 'SHAPES.VGA' which the object will assume. |


<a id="get_item_frame"></a>

### UI_get_item_frame

```usecode
int UI_get_item_frame(object obj)
int obj->get_item_frame()
```


Gets the current frame of the desired object, **without** the rotation bit.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose frame is desired. |

#### Return

The frame from 'SHAPES.VGA' of the object in question. Only the base frame is returned, without any rotation information.


<a id="set_item_frame"></a>

### UI_set_item_frame

```usecode
UI_set_item_frame(object obj, int frame)
obj->set_item_frame(int frame)
```


Sets the current frame of the desired object, preserving the rotation bit.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose frame is to be changed. |
| `frame` | The desired value for the frame. |


<a id="get_item_frame_rot"></a>

### UI_get_item_frame_rot

```usecode
int UI_get_item_frame_rot(object obj)
int obj->get_item_frame_rot()
```


Gets the current frame of the desired object, **with** the rotation bit.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose frame is desired. |

#### Return

The frame from 'SHAPES.VGA' of the object in question; including the rotation bit.


<a id="set_item_frame_rot"></a>

### UI_set_item_frame_rot

```usecode
UI_set_item_frame_rot(object obj, int frame)
obj->set_item_frame_rot(int frame)
```


Sets the current frame of the desired object; does **not** preserve the rotation bit.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose frame is to be changed. |
| `frame` | The desired value for the frame. |


<a id="get_item_gump_position"></a>

### UI_get_item_gump_position

[Exult]


```usecode
int UI_get_item_gump_position(object item)
```


Returns the coordinates of an item within its currently open container gump. This allows usecode to inspect where items are positioned in the container's display area.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `item` | The item object whose position is desired. |

#### Return

An array `[x, y]` containing the item's coordinates within the container gump if the item is valid, currently in a container, and the container gump is open. Returns an empty array otherwise.

#### Notes

The returned coordinates are relative to the container's display area, not the screen. This intrinsic is useful for checking item placement in custom container systems or ritual-based gameplay mechanics.


<a id="set_item_gump_position"></a>

### UI_set_item_gump_position

[Exult]


```usecode
int UI_set_item_gump_position(object item, int x, int y)
```


Programmatically repositions an item within its container gump to specific coordinates. This allows usecode to arrange items in precise positions for rituals, puzzles, or custom container layouts.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `item` | The item object to reposition. |
| `x` | The horizontal coordinate within the container's display area. |
| `y` | The vertical coordinate within the container's display area. |

#### Return

Returns `1` on success, `0` on failure. Failure occurs if the item is invalid, not in a container, the container gump is not open, or if negative coordinates are provided.

#### Notes

Coordinates must be non-negative. The positioning is relative to the container's display area. After setting a position, the display is automatically refreshed to show the new arrangement.


<a id="get_item_quality"></a>

### UI_get_item_quality

```usecode
int UI_get_item_quality(object obj)
int obj->get_item_quality()
```


Gets the quality of the desired object.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose quality is desired. |

#### Return

The quality of the object if it has it, zero otherwise.


<a id="set_item_quality"></a>

### UI_set_item_quality

```usecode
bool UI_set_item_quality(object obj, int qual)
bool obj->set_item_quality(int qual)
```


Sets the quality of the desired object.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose quality is to be changed. |
| `qual` | The desired value for the quality. Quality is unchanged if `qual == QUALITY_ANY`. |

#### Return

Returns `true` if the object has a quality or if `qual == QUALITY_ANY`, `false` otherwise.


<a id="get_item_quantity"></a>

### UI_get_item_quantity

```usecode
int UI_get_item_quantity(object obj)
int obj->get_item_quantity()
int UI_get_item_quantity(object obj, int unk)
int obj->get_item_quantity(int unk)
```


Gets the quantity of the desired object.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose quantity is desired. |
| `unk` | An unknown parameter from the original games. Can be omitted if desired, as Exult does not use it. |

#### Return

The quantity of the object if it has it, '1' otherwise.


<a id="set_item_quantity"></a>

### UI_set_item_quantity

```usecode
bool UI_set_item_quantity(object obj, int quant)
bool obj->set_item_quantity(int quant)
```


Sets the quantity of the desired object. The object's quantity is changed only if the object has it.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose quantity is to be changed. |
| `quant` | The desired value for the quantity. If this value is zero, the object will be deleted unless it is not in the game map -- likely because it has been created by Usecode but not yet placed. |

#### Return

Returns `true` if the object has a quantity, `false` otherwise.


<a id="get_lift"></a>

### UI_get_lift

```usecode
int UI_get_lift(object obj)
int obj->get_lift()
```


Gets how far above the ground the object is.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose lift is desired. |

#### Return

The object's z coordinate.


<a id="set_lift"></a>

### UI_set_lift

```usecode
UI_set_lift(object obj, int lift)
obj->set_lift(int lift)
```


Moves the object on the vertical direction to a desired height off the ground.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose lift is to be set. |
| `lift` | How far above the ground the object is going to be. Accepted values are 0 through 19 inclusive. Values outside this range leave the object at its current height. |


<a id="get_item_flag"></a>

### UI_get_item_flag

```usecode
int UI_get_item_flag(object obj, int flag)
int obj->get_item_flag(int flag)
```


Inspects the chosen flag of the given object.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose flag we wish to inspect. Not all flags are available for all objects. |
| `flag` | What we want to know. This is a number in the 0 to 63 range. |

The flag parameter is a numeric value describing a (usually boolean) bit of information about the object in question. Valid pre-defined values for the flag are:

| Parameter / value | Description |
| --- | --- |
| INVISIBLE           = 0 | Object is invisible (rendered with 'invisible' palette, or not at all). |
| ASLEEP              = 1 | Object is asleep. Only really useful for NPCs. |
| CHARMED             = 2 | Object is charmed (attacks allies). Only really useful for NPCs. |
| CURSED              = 3 | Object is cursed (stats malus). Only really useful for NPCs. |
| DEAD                = 4 | Object is dead. Only really useful for NPC/monster. *DONT set on dead bodies, causes bugs.* |
| IN_PARTY            = 6 | Object is an NPC in the party. |
| PARALYZED           = 7 | Object is paralyzed and unable to move. Normal objects will be stationary by default. |
| PARALYSED           = 7 | Canadian/British spelling. |
| POISONED            = 8 | Object is afflicted with poison. Can be set on objects, but will only do anything to NPCs. |
| PROTECTION          = 9 | Object is protected from harm. Only really useful for NPCs. |
| ON_MOVING_BARGE    = 10 | Object is on a 'barge' object that is moving. |
| OKAY_TO_TAKE       = 11 | Object will not trigger stealing usecode if you take it. |
| MIGHT              = 12 | Object has might (Stats boon).  Only really useful for NPCs. |
| IMMUNITIES         = 13 | Object has several immunities. Can be inspected only. |
| CANT_DIE           = 14 | Object can't die. Used for testing purposes, and certain critical NPCs (such as L.B.). Can be inspected only. |
| IN_ACTION          = 15 | Object is executing the usecode called by the Dance spell. |
| DONT_MOVE          = 16 | SI: The NPC cannot move. |
| DONT_RENDER        = 16 | BG: The NPC cannot move, and is not rendered at all. |
| SI_ON_MOVING_BARGE = 17 | SI: Object is on a special instance of the SI barges. (Turtle for example.) |
| TEMPORARY          = 18 | Object is temporary and will decay once the chunk it is in is un-cached. |
| ACTIVE_SAILOR      = 20 | Object is the 'captain' of a barge.  When getting the flag, you will actually get the current captain. |
| OKAY_TO_LAND       = 21 | Set in usecode for flying carpet, TRUE if you can land currently. |
| BG_DONT_MOVE       = 22 | BG: BG's version of DONT_MOVE. |
| SI_DONT_RENDER     = 22 | SI: SI's version of DONT_RENDER. |
| IN_DUNGEON         = 23 | Reports whether the avatar is in a dungeon. This intrinsic returns `false` for other objects. |
| IS_SOLID           = 24 | Used by gangplank usecode to determine if an object is solid. |
| CONFUSED           = 25 | This will be set if the avatar failed copyright protection questions in SI. Only really meaningful used on the avatar. |
| ACTIVE_BARGE       = 26 | Object is a barge object moving, or on a barge object that is moving. Set in usecode, and mostly used for the SI 'NPC' ships such as the turtle. |
| MET                = 28 | Object has been talked to previously. Should be set in a conversation Usecode script. This determines conversation behavior, and whether the NPC's real name or shape name is displayed when they are single-clicked on. BG originally used global flags for this, which amounts to an extra 250-odd flags. What a waste of time. |
| SI_TOURNAMENT      = 29 | Literal plot immunity: the NPC can die only if the plot allows it. If enough damage is taken in a single blow, the object will call usecode (eventid = 7) on death. Used for the List Field in SI. |
| SI_ZOMBIE          = 30 | Object will not respond to normal cues. Used for sick Neyobi, insane party members/Cantra. |
| NO_SPELL_CASTING   = 31 | Object cannot cast spells. Set when the magebane strikes the object. |
| POLYMORPH          = 32 | Object is polymorphed. |
| TATTOOED           = 33 | Object has been tattooed. Set after the lady tattoos you by usecode. Causes the avatar portraits to use a different 'tattooed' shape num. |
| READ               = 34 | Object can read serpent- or rune-script text. Only really meaningful used on the avatar. |
| PETRA              = 35 | Object has switched bodies with Petra. This changes the portraits of NPCs according to a list in the 'avatar_data.txt' file. Only really meaningful for NPCs in that list. |
| FLIGHT             = 36 | Object can fly. Only really useful for NPCs. |
| FREEZE             = 37 | Object is freezing. Only really useful for NPCs. |
| NAKED              = 38 | Object is naked. On the avatar, causes him/her to use the appropriate 'naked' shape. On other NPCs there no effect. To actually make NPCs naked, use set_polymorph instead. |

#### Return

For most flags, returns `true` if the flag is in effect, `false` otherwise. `ACTIVE_SAILOR` returns the current captain's object reference, regardless of which valid object is queried, or zero if no captain is set. `ACTIVE_BARGE`, `ON_MOVING_BARGE`, and SI's `SI_ON_MOVING_BARGE` return a boolean indicating whether the queried object is on, or part of, the currently moving barge. An invalid object returns `false` for every flag.

With the exception of the OKAY_TO_TAKE, TEMPORARY and INVISIBLE flags, these flags are valid for NPCs only. You can get, set or clear any of those values and Exult will not complain; however, it will also not save the values of the flags except for the three mentioned before. Moreover, the INVISIBLE flag will not be saved for objects unless they are of 'quality flags' class.

`IMMUNITIES` and `CANT_DIE` inspect powers granted by monster shape data or worn equipment. `IS_SOLID` tests the object's shape information. `OKAY_TO_LAND` checks whether the object's barge can land; these are computed queries rather than ordinary stored flags.


<a id="set_item_flag"></a>

### UI_set_item_flag

```usecode
UI_set_item_flag(object obj, int flag)
obj->set_item_flag(int flag)
```


Sets the desired flag for the given object. Many flags have immediate effects when set; most do not. See the `get_item_flag` intrinsic for flag values.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose flag we wish to set. Not all flags are available for all objects. |
| `flag` | What flag we want to set. This is a number in the 0 to 63 range; it can be any of the values for the 'flag' parameter of the `set_item_flag` intrinsic. |

`ACTIVE_SAILOR` stores `obj` as the current captain, which is returned by `UI_get_item_flag` queries for that flag. Setting `ACTIVE_BARGE`, `ON_MOVING_BARGE`, or SI's `SI_ON_MOVING_BARGE` also selects the barge containing `obj` as the moving barge. These barge queries return booleans.

With the exception of the OKAY_TO_TAKE, TEMPORARY and INVISIBLE flags, these flags are valid for NPCs only. You can get, set or clear any of those values and Exult will not complain; however, it will also not save the values of the flags except for the three mentioned before. Moreover, the INVISIBLE flag will not be saved for objects unless they are of 'quality flags' class.


<a id="clear_item_flag"></a>

### UI_clear_item_flag

```usecode
UI_clear_item_flag(object obj, int flag)
obj->clear_item_flag(int flag)
```


Clears the desired flag for the given object. Many flags have immediate effects when cleared; most do not. See the `get_item_flag` intrinsic for flag values.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose flag we wish to clear. Not all flags are available for all objects. |
| `flag` | What flag we want to clear. This is a number in the 0 to 63 range; it can be any of the values for the 'flag' parameter of the `set_item_flag` intrinsic. |

Clearing `ACTIVE_SAILOR` clears the stored captain. Clearing `ACTIVE_BARGE`, `ON_MOVING_BARGE`, or SI's `SI_ON_MOVING_BARGE` stops the object's barge if it is the currently moving barge. Clearing `ASLEEP` does not wake an NPC who is knocked out.

With the exception of the OKAY_TO_TAKE, TEMPORARY and INVISIBLE flags, these flags are valid for NPCs only. You can get, set or clear any of those values and Exult will not complain; however, it will also not save the values of the flags except for the three mentioned before. Moreover, the INVISIBLE flag will not be saved for objects unless they are of 'quality flags' class.


<a id="get_barge"></a>

### UI_get_barge

```usecode
object UI_get_barge(object obj)
object obj->get_barge()
```


Checks the barge upon which the supplied object is on.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object to be inspected. |

#### Return

Returns zero if no valid object is passed, or if the object is not lying on a barge. Otherwise, returns the object reference of the barge (shape 961) object upon which the supplied object lies.


<a id="set_barge_dir"></a>

### UI_set_barge_dir

[SI, Exult]


```usecode
UI_set_barge_dir(object obj, int dir)
obj->set_barge_dir(int dir)
```


If 'obj' is a barge object, sets its direction (along with all objects laying on 'obj') to 'dir'. 'dir' can be one of the constants defined for '`find_direction`' intrinsic.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object to be animated. |
| `dir` | The 8-cardinal-point direction that the barge should face. This can be one of the values returned by the `find_direction` intrinsic. |


<a id="on_barge"></a>

### UI_on_barge

```usecode
bool UI_on_barge()
```


Determines if the entire party is inside the same barge as the avatar.

> **SI SPECIFIC**
>
> In SI, this intrinsic also forces the barge to refresh the list of all objects lying on it.

#### Return

If the avatar is on a barge and all party members are on that same barge, returns `true`; in all other cases, returns `false`.


<a id="get_container"></a>

### UI_get_container

```usecode
object UI_get_container(object obj)
object obj->get_container()
```


Gets the container of the supplied object.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose container is desired. |

#### Return

Return is zero if the object is directly in the game world, or an object reference to its immediate container otherwise.


<a id="remove_item"></a>

### UI_remove_item

```usecode
UI_remove_item(object obj)
obj->remove_item()
```


Removes the object from the game world and from the 'last created' stack. If the object is not an NPC, it will de deleted; NPCs will still exist.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object to be deleted. |


<a id="get_usecode_fun"></a>

### UI_get_usecode_fun

[Exult]


```usecode
int UI_get_usecode_fun(object obj)
int obj->get_usecode_fun()
```


Gets the usecode function number of an object. This takes into consideration a NPC's assigned function (if any) and the correct shape `<>` function mapping for shapes `> 1024`.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose usecode function we desire. |

#### Return

The usecode function number that is assigned to the object.


<a id="set_usecode_fun"></a>

### UI_set_usecode_fun

[Exult]


```usecode
UI_set_usecode_fun(actor npc, function fun)
npc->set_usecode_fun(function fun)
```


Assigns a new usecode function to an NPC.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose usecode function will be changed. |
| `fun` | The new usecode function. |


<a id="get_map_num"></a>

### UI_get_map_num

[Exult]


```usecode
int UI_get_map_num(object obj)
int obj->get_map_num()
```


Checks which map an object is in.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose map is desired. |

#### Return

The map in which the object is located, or -1 if the object is nowhere.


<a id="get_item_weight"></a>

### UI_get_item_weight

[SI, Exult]


```usecode
int UI_get_item_weight(object obj)
int obj->get_item_weight()
```


Checks an object's weight.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose weight is desired. |

#### Return

The object's weight.


<a id="is_on_keyring"></a>

### UI_is_on_keyring

[SI]


```usecode
bool UI_is_on_keyring(int qual)
```


Checks if a key is in the keyring. Note that the keyring is a global object; multiple keyrings will share the same list of keys.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `qual` | The quality of the key to be checked. |

#### Return

`true` if the key is on the keyring, `false` otherwise.


<a id="add_to_keyring"></a>

### UI_add_to_keyring

[SI]


```usecode
UI_add_to_keyring(int qual)
```


Adds a key to the keyring. Note that the keyring is a global object; multiple keyrings will share the same list of keys.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `qual` | The quality of the key to be added. |


<a id="remove_from_keyring"></a>

### UI_remove_from_keyring

[Exult: SI]


```usecode
bool UI_remove_from_keyring(int qual)
```


Removes a key from the keyring. Note that the keyring is a global object; multiple keyrings will share the same list of keys.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `qual` | The quality of the key to be removed. |

#### Return

If the key was on the keyring, returns `true`; otherwise, returns `false`.


<a id="apply_damage"></a>

### UI_apply_damage

```usecode
int UI_apply_damage(int base, int hits, int type, object obj)
```


Causes random damage of a given type to the desired object, subject to armor.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `base` | The 'strength' of the attack; only one third of this value (rounded down) is used to deal damage. This parameter is ignored depending on damage type; specifically, damage of `LIGHTNING_DAMAGE` type ignores this parameter. |
| `hits` | The 'weapon points' of the attack. If this parameter is equal to 127 or more, this intrinsic will ignore all armor and deal a fixed 127 points of damage of the specified type, ignoring the 'base' parameter. |
| `type` | The type of the damage. If the target is immune to this damage type, he will take no damage from this intrinsic; conversely, a monster vulnerable to this type of damage takes doubled damage. Armor-granted immunities also protect the target from the effects of this intrinsic. If the damage type is `LIGHTNING_DAMAGE`, `ETHEREAL_DAMAGE` or `SONIC_DAMAGE`, this intrinsic will ignore armor. |
| `obj` | The object to be damaged. |

The type of the damage is a numerical value that describes what kind of damage should be caused. It can be one of the following values:

| Parameter / value | Description |
| --- | --- |
| NORMAL_DAMAGE = 0 | Normal damage. |
| FIRE_DAMAGE = 1 | Damage from heat sources. |
| MAGIC_DAMAGE = 2 | Damage from a magical source. |
| LIGHTNING_DAMAGE = 3 | Damage from electrical sources such as lightning. Extremely painful, even a single point of damage causes the screen to flash a red color. This damage ignores all armor and does not depend on the attacker's strength. |
| POISON_DAMAGE = 3 | Damage from poison. Extremely painful, even a single point of damage causes the screen to flash a red color. This damage ignores all armor and does not depend on the attacker's strength. |
| STARVATION_DAMAGE = 3 | Damage from starvation. Extremely painful, even a single point of damage causes the screen to flash a red color. This damage ignores all armor and does not depend on the attacker's strength. |
| FREEZING_DAMAGE = 3 | Damage from frostbite. Extremely painful, even a single point of damage causes the screen to flash a red color. This damage ignores all armor and does not depend on the attacker's strength. |
| ETHEREAL_DAMAGE = 4 | Special magical damage, basically magic damage not blocked by normal magic resistance or armor. |
| SONIC_DAMAGE = 5 | Sound-based damage. This damage type ignores armor. |

The damage is calculated from a series of generated random numbers. It proceeds along the following steps:

- If the target is wearing any armor that makes him/her immune to the specified damage type, he takes no damage from this intrinsic.
- If hits is 127 or more, 127 points of damage are caused to the target. This case ignores all armor.
- If base/3 (rounded down) is zero, it does not affect damage; likewise, if hits is zero, it does not affect damage. A random number will be generated for each of them that are nonzero, ranging from 1 to base/3 or from 1 to hits, as appropriate. These two random numbers are added together.
- If the damage type does not ignore armor, the armor value of everything worn by the target is computed. Monster-specified armor values are also added to this value. If the total armor is at least zero, a random number in the range 1 to (total armor) is generated and subtracted from the damage total above.
- If the target has immunities granted by his monster data, he will take no damage; if he has any vulnerabilities, the remaining damage, if positive, will be doubled.

#### Return

Returns `true` if `obj` resolves to a valid target and the damage routine is called, `false` otherwise. A `true` result does not guarantee that hit points were lost: armor or immunity may prevent damage.


<a id="reduce_health"></a>

### UI_reduce_health

```usecode
UI_reduce_health(object obj, int hits, int type)
obj->reduce_health(int hits, int type)
```


Causes a fixed amount of damage of the specified type to a target. This intrinsic ignores armor and armor-granted immunities.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object to be damaged. |
| `hits` | How much damage is to be caused. |
| `type` | The type of the damage. If the target is immune to this damage type, he will take no damage from this intrinsic; conversely, a monster vulnerable to this type of damage takes doubled damage. Armor-granted immunities are ignored. |

The type of the damage is a numerical value that describes what kind of damage should be caused. It can be one of the following values:

| Parameter / value | Description |
| --- | --- |
| NORMAL_DAMAGE = 0 | Normal damage. |
| FIRE_DAMAGE = 1 | Damage from heat sources. |
| MAGIC_DAMAGE = 2 | Damage from a magical source. |
| LIGHTNING_DAMAGE = 3 | Damage from electrical sources such as lightning. Extremely painful, even a single point of damage causes the screen to flash a red color. This damage ignores all armor and does not depend on the attacker's strength. |
| POISON_DAMAGE = 3 | Damage from poison. Extremely painful, even a single point of damage causes the screen to flash a red color. This damage ignores all armor and does not depend on the attacker's strength. |
| STARVATION_DAMAGE = 3 | Damage from starvation. Extremely painful, even a single point of damage causes the screen to flash a red color. This damage ignores all armor and does not depend on the attacker's strength. |
| FREEZING_DAMAGE = 3 | Damage from frostbite. Extremely painful, even a single point of damage causes the screen to flash a red color. This damage ignores all armor and does not depend on the attacker's strength. |
| ETHEREAL_DAMAGE = 4 | Special magical damage, basically magic damage not blocked by normal magic resistance or armor. |
| SONIC_DAMAGE = 5 | Sound-based damage. This damage type ignores armor. |


<a id="attack_object"></a>

### UI_attack_object

```usecode
int UI_attack_object(object attacker, object target, int weapon)
int attacker->attack_object(object target, int weapon)
```


Causes a NPC to attack the object with a given weapon. This intrinsic does not animate the NPC in any way; it only emulates a hit with the selected weapon.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `attacker` | Who is attacking. |
| `target` | What is being attacked. Must be an object or NPC. |
| `weapon` | What shape in 'SHAPES.VGA' to use as the source of the weapon information in the attack, or -1 for an unarmed attack. If this is a missile or thrown weapon, it will consume the appropriate ammunition (if any) and fire the correct projectile. |

This intrinsic requires an object target. To aim at a tile using a `UI_click_on_item()` result, use `UI_set_to_attack` instead.

#### Return

`true` if a melee attack hits or if a ranged attack fires its projectile; returns `false` if the melee attack missed or if the attack could not be realized for any reason (including an invalid attacker, an invalid target, being out of range, lack of adequate ammunition for the given weapon).


<a id="fire_projectile"></a>

### UI_fire_projectile

```usecode
UI_fire_projectile(object obj, int dir, int proj, int attpts, int weapon, int ammo)
obj->fire_projectile(int dir, int proj, int attpts, int weapon, int ammo)
```


Causes the given object to fire a high-speed missile in the supplied direction.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object that will fire the projectile. |
| `dir` | Numeric value indicating the direction in which the projectile will be fired. |
| `proj` | The shape in 'SHAPES.VGA' of the projectile to be fired. |
| `attpts` | Numeric value determining the likelihood that the missile will hit. Works like the 'att' parameter in the `roll_to_win` intrinsic in a contest against the target's `COMBAT` property. |
| `weapon` | The shape in 'SHAPES.VGA' whose weapon information will be used when the missile hits. |
| `ammo` | The shape in 'SHAPES.VGA' whose ammunition information will be used when the missile hits. |

The direction into which the projectile is filed is the 8-cardinal-point direction in which the projectile will travel. It can be one of the following values:

| NORTH | = 0 | NORTHEAST | = 1 | EAST | = 2 | SOUTHEAST | = 3 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| SOUTH | = 4 | SOUTHWEST | = 5 | WEST | = 6 | NORTHWEST | = 7 |


## NPC Intrinsics

These intrinsics allow you to get or set NPC-specific properties of a NPC, such as schedules, alignment or stats.

<a id="get_npc_object"></a>

### UI_get_npc_object

```usecode
object UI_get_npc_object(int npc)
object npc->get_npc_object()
object[] UI_get_npc_object(int npc[])
```


Gets the NPC object references of one or more NPC numbers.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC number, or array of NPC numbers, whose object references are desired. |

#### Return

If the input is an array of NPC numbers, the return is an array of NPC object references. The output array places the NPC references in the same order they are in the input array.

Otherwise, the return is the object reference of the given NPC number.

Invalid entries resolve to null object references; array positions are preserved. An existing object reference is also accepted and returned unchanged, even when it refers to a non-NPC object.


<a id="get_schedule_type"></a>

### UI_get_schedule_type

```usecode
int UI_get_schedule_type(actor npc)
int npc->get_schedule_type()
```


Gets the current schedule of an NPC.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose schedule is desired. |

#### Return

Zero if 'npc' is not a valid NPC; otherwise, an integer associated to the NPC's current schedule. This is the same value that is reported in the cheat screen, and can be one of the following values:

| IN_COMBAT | = 0 | PACE_HORIZONTAL | = 1 | PACE_VERTICAL | = 2 |
| --- | --- | --- | --- | --- | --- |
| TALK | = 3 | DANCE | = 4 | EAT | = 5 |
| FARM | = 6 | TEND_SHOP | = 7 | MINE | = 8 |
| MINER | = 8 | HOUND | = 9 | STANDTHERE | = 10 |
| LOITER | = 11 | WANDER | = 12 | BLACKSMITH | = 13 |
| SLEEP | = 14 | WAIT | = 15 | MAJOR_SIT | = 16 |
| GRAZE | = 17 | BAKE | = 18 | SEW | = 19 |
| SHY | = 20 | LAB | = 21 | THIEF | = 22 |
| WAITER | = 23 | SPECIAL | = 24 | KID_GAMES | = 25 |
| TAG | = 25 | EAT_AT_INN | = 26 | DUEL | = 27 |
| SPAR | = 27 | PREACH | = 28 | PATROL | = 29 |
| DESK_WORK | = 30 | FOLLOW_AVATAR | = 31 |

> **SI SPECIFIC**
>
> If the NPC is traversing a path created by a call to either, `UI_path_run_usecode` or `UI_si_path_run_usecode` intrinsics, this intrinsic will return the value `WALK_TO_SCHEDULE = 32` instead of the values above.


<a id="set_schedule_type"></a>

### UI_set_schedule_type

```usecode
UI_set_schedule_type(actor npc, int sched)
npc->set_schedule_type(int sched)
```


Gets the current schedule of an NPC.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose schedule we wish to change. |
| `sched` | The new schedule type. Can be any of the values listed as the return of a `get_schedule_type` intrinsic call. |

> **SI SPECIFIC**
>
>  The SI-specific value `WALK_TO_SCHEDULE = 32` **cannot** be set with this intrinsic; only a call to either, `UI_path_run_usecode` or `UI_si_path_run_usecode` intrinsics can do that.


<a id="modify_schedule"></a>

### UI_modify_schedule

[SI, Exult]


```usecode
UI_modify_schedule(actor npc, int time, int sched, int pos[2])
npc->modify_schedule(int time, int sched, int pos[2])
UI_modify_schedule(actor npc, int time, int sched, int pos[3])
npc->modify_schedule(int time, int sched, int pos[3])
```


Changes the schedule table of an NPC for the desired time period. This new schedule is persistent across save games. The NPC will execute his/her new schedule in the map he/she currently resides when the time comes.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose schedule table is to be changed. |
| `time` | An integer specifying the time period of the new schedule. Must be an integer in the 0-7 range, such as those returned by the `part_of_day` intrinsic. |
| `sched` | An integer specifying the new schedule. Must be an integer in the 0-31 range, such as those returned by the `get_schedule_type` intrinsic. |
| `loc` | A (x, y, z) location, specifying the location of the new schedule. Value of z is optional, defaults to 0. |


<a id="set_new_schedules"></a>

### UI_set_new_schedules

[SI, Exult]


```usecode
UI_set_new_schedules(actor npc, int time, int sched, int loc[2])
UI_set_new_schedules(actor npc, int time[n], int sched[n], int loc[2n])
npc->set_new_schedules(int time, int sched, int loc[2])
npc->set_new_schedules(int time[n], int sched[n], int loc[2n])
UI_set_new_schedules(actor npc, int time, int sched, int loc[3], bool use_3d)
UI_set_new_schedules(actor npc, int time[n], int sched[n], int loc[3n], bool use_3d)
npc->set_new_schedules(int time, int sched, int loc[3], bool use_3d)
npc->set_new_schedules(int time[n], int sched[n], int loc[3n], bool use_3d)
```


Changes the schedule table of an NPC. This new schedule table is persistent across save games. The NPC will execute his/her new schedules in the map he/she currently resides.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose schedule table is to be changed. |
| `time` | An integer, or array of integers, specifying the time periods of the new schedules. Each element of the array is an integer in the 0-7 range, such as those returned by the `part_of_day` intrinsic. |
| `sched` | An integer, or array of schedules, specifying the new schedules. There must be one such schedule for each time period supplied in the 'time' parameter. Each element of this array is an integer in the 0-31 range, such as those returned by the `get_schedule_type` intrinsic. |
| `loc` | A flat array of locations, with one position per time period: `[x1, y1, x2, y2, ...]` by default, or `[x1, y1, z1, x2, y2, z2, ...]` when `use_3d` is true. |
| `use_3d` | Optional fifth argument, default `false`. Set to `true` to read three coordinates per location. Otherwise, each location uses `z = 0`. |

`time` and `sched` may be scalars for one schedule or arrays for multiple schedules. For array input, supply at least one activity and one complete position per time entry. Invalid NPCs, a non-array `loc`, or insufficient activity or coordinate entries leave the schedule table unchanged. Use `UI_run_schedule(npc)` to apply the activity for the current time period after replacing the table.

```usecode
UI_set_new_schedules(npc, [0, 4], [WAIT, LOITER], [100, 200, 2, 120, 220, 3], true);
```


<a id="run_schedule"></a>

### UI_run_schedule

[SI, Exult]


```usecode
UI_run_schedule(actor npc)
npc->run_schedule()
```


Forces an NPC to return to his/her regularly scheduled activities.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC that should be minding his own business. |


<a id="revert_schedule"></a>

### UI_revert_schedule

[SI, Exult]


```usecode
UI_revert_schedule(actor npc)
npc->revert_schedule()
```


Reverts the schedules of a NPC to those stored in 'STATIC/schedule.dat'. Can be risky for NPCs not in the original games.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose schedule is to be reset. |


<a id="get_npc_prop"></a>

### UI_get_npc_prop

```usecode
int UI_get_npc_prop(object obj, int prop)
int UI_get_npc_prop(object obj, string prop)
int obj->get_npc_prop(int prop)
int obj->get_npc_prop(string prop)
```


Gets the current value of an object's or NPC's property. Despite the name, this intrinsic can also be called for any object, although most properties will be unavailable for anything but NPCs.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object or NPC whose property is desired. |
| `prop` | String or integer specifying the property to be inspected. |

If the desired property has been specified as an integer, it must be in the range from 0 to 11. It can be one of the following values:

| STRENGTH | = 0 | DEXTERITY | = 1 | INTELLIGENCE | = 2 |
| --- | --- | --- | --- | --- | --- |
| HEALTH | = 3 | COMBAT | = 4 | MANA | = 5 |
| MAX_MANA | = 6 | TRAINING | = 7 | EXPERIENCE | = 8 |
| FOODLEVEL | = 9 | SEX_FLAG | = 10 | MISSILE_WEAPON | = 11 |

#### Return

Returns the current value of the specified property. The following properties are special cases:

| Parameter / value | Description |
| --- | --- |
| `HEALTH` | This is the only meaningful property for non-NPC objects. Returns the current remaining hit points of the object or NPC. |
| `SEX_FLAG` | The return is `true` if the NPC is female, `false` otherwise. |
| `MISSILE_WEAPON` | The return is `true` if the NPC is using a missile weapon or a good thrown weapon (such as a spear), `false` otherwise. |

This intrinsic returns zero for any property which is not meaningful for the supplied object; this includes string properties that have not been previously set with a `UI_set_npc_prop` intrinsic call.

> **BG SPECIFIC**
>
> Standard BG does not have sex data for anyone but the avatar, so the return is almost always 'false'. This is not a limitation of the engine or of Exult, just a statement about the game data.


<a id="set_npc_prop"></a>

### UI_set_npc_prop

```usecode
int UI_set_npc_prop(object obj, int prop, int delta)
int UI_set_npc_prop(object obj, string prop, int delta)
int obj->set_npc_prop(int prop, int delta)
int obj->set_npc_prop(string prop, int delta)
```


Changes an object's or NPC's property by the specified value. Despite the name, this intrinsic can also be called for any object, although most properties will be unavailable for anything but NPCs.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object or NPC whose property is to be changed. |
| `prop` | String or integer specifying the property to be changed. |
| `delta` | The desired numeric **change** in the specified property. |

If the desired property has been specified as an integer, it must be one of the values listed in the `UI_get_npc_prop` intrinsic description. The following values are special cases:

| Parameter / value | Description |
| --- | --- |
| `HEALTH` | This is the only meaningful property for non-NPC objects. Changes the remaining hit points of the object or NPC. If the remaining hit points become zero, an object will become **indestructible**. |
| `EXPERIENCE` | This property is changed by *half* of the specified value. A negative value in the signed 16-bit range is first reinterpreted as unsigned, to handle experience bonuses stored by the original games; more negative values are ignored. |
| `SEX_FLAG` | A nonzero 'delta' sets the NPC to female, while a zero 'delta' sets the NPC to male. |
| `MISSILE_WEAPON` | Nothing happens when this property value is supplied. |

> **BG SPECIFIC**
>
> In the original BG, you could not set the sex flag except by hex-editing the game data. Exult does not suffer from this limitation.

If the desired property has been specified as a string, it will be created if it does not exist; its initial value is assumed to be zero in this case. Only NPCs can have such properties, and they will be displayed in the NPC's stats window.

#### Return

Returns `1` for a valid NPC, or when changing `HEALTH` on a valid non-NPC object. Returns `0` for an invalid object or a property that is unavailable on a non-NPC object. A result of `1` means the request was accepted; it does not guarantee that the property changed.


<a id="get_npc_name"></a>

### UI_get_npc_name

```usecode
string UI_get_npc_name(object obj)
string obj->get_npc_name()
string[] UI_get_npc_name(object obj[])
```


Gets the name of one or more objects. Despite the name, this intrinsic can be called for any object, not just NPCs; for non-NPCs, the return is the text displayed when you click the object.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object, or array of objects, whose names are desired. |

#### Return

If the input is an array of objects, the return is an array with the names of these objects. Valid names retain their input order, but invalid objects are omitted, so the output array may be shorter than the input.

Otherwise, the return is the name of the given object, or `"??name??"` for an invalid object.


<a id="set_npc_name"></a>

### UI_set_npc_name

```usecode
UI_set_npc_name(actor npc, string name)
npc->set_npc_name(string name)
```


Changes the name of an NPC.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose name is to be changed. |
| `name` | The new name. |


<a id="is_npc"></a>

### UI_is_npc

```usecode
UI_is_npc(mixed obj)
obj->is_npc()
```


Determines if a given object is an NPC.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object to be checked. |

#### Return

`true` if the object is an NPC, `false` otherwise.


<a id="is_dead"></a>

### UI_is_dead

```usecode
bool UI_is_dead(actor npc)
bool npc->is_dead()
```


Checks if the NPC is dead or alive. This is equivalent to checking if the '`DEAD`' flag is set.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | Who we want to check. |

#### Return

Returns `true` if the NPC is dead, `false` otherwise.


<a id="get_npc_number"></a>

### UI_get_npc_number

```usecode
int UI_get_npc_number(actor npc)
int npc->get_npc_number()
```


#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose number is desired. This is intended to receive an object reference, but accepts integers as normal. |

#### Return

The NPC's number. This is a negative number for all 'true' NPCs, -356 for the avatar, 1 for monsters or zero for anything that is not a valid NPC.


<a id="get_alignment"></a>

### UI_get_alignment

```usecode
int UI_get_alignment(actor npc)
int npc->get_alignment()
```


Gets the NPC's attitude towards the avatar.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose alignment is desired. |

#### Return

Returns a number describing the NPC's attitude towards the avatar. Can be one of the following values:

| NEUTRAL | = 0 | GOOD | = 1 | EVIL | = 2 |
| --- | --- | --- | --- | --- | --- |
| CHAOTIC | = 3 |


<a id="set_alignment"></a>

### UI_set_alignment

```usecode
UI_set_alignment(actor npc, int align)
npc->set_alignment(int align)
```


Sets an NPC's attitude towards the avatar. This change forces the NPC to reevaluate his targets, and may stop the NPC from fleeing.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC to be adjusted. |
| `align` | The new attitude towards the avatar. Can be any of the return values of `get_alignment` intrinsic. |


<a id="remove_npc"></a>

### UI_remove_npc

```usecode
UI_remove_npc(actor npc)
npc->remove_npc()
```


Removes the NPC from the game world. The NPC is effectively placed in 'limbo', and has its schedule set to 'WAIT'.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC to be removed. |


<a id="kill_npc"></a>

### UI_kill_npc

```usecode
UI_kill_npc(actor npc)
npc->kill_npc()
```


Kills the specified victim.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The soon-to-be-dead NPC. |


<a id="get_attack_mode"></a>

### UI_get_attack_mode

[SI, Exult]


```usecode
int UI_get_attack_mode(actor npc)
int npc->get_attack_mode()
```


Gets the NPC's attack mode.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose attack mode is desired. |

#### Return

A numeric value specifying how the NPC behaves in combat. Possible values are:

| NEAREST | = 0 | WEAKEST | = 1 | STRONGEST | = 2 | BERSERK | = 3 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| PROTECT | = 4 | DEFEND | = 5 | FLANK | = 6 | FLEE | = 7 |
| RANDOM | = 8 | MANUAL | = 9 |


<a id="set_attack_mode"></a>

### UI_set_attack_mode

```usecode
UI_set_attack_mode(actor npc, int mode)
npc->set_attack_mode(int mode)
```


Changes the NPC's attack mode to the desired value.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose attack mode is to be changed. |
| `mode` | Numeric value determining how the NPC should behave in combat. Can be one of the return values of `get_attack_mode` intrinsic. |


<a id="get_npc_id"></a>

### UI_get_npc_id

[SI, Exult]


```usecode
int UI_get_npc_id(actor npc)
int npc->get_npc_id()
```


Retrieves a NPC's identity. This identity has no real effect other than that given to it by usecode.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose identity is sought. |

#### Return

If given a valid NPC, this intrinsic returns that NPC's identity; otherwise it returns zero.


<a id="set_npc_id"></a>

### UI_set_npc_id

[SI, Exult]


```usecode
UI_set_npc_id(actor npc, int id)
npc->set_npc_id(int id)
```


Changes an NPC's identity. This identity has no real effect other than that given to it by usecode.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose identity is to be changed. |
| `id` | The new identity value. Must be an integer in the 0-31 range; values outside this range are accepted but incorrectly saved. |


<a id="get_oppressor"></a>

### UI_get_oppressor

[SI, Exult]


```usecode
actor UI_get_oppressor(actor npc)
actor npc->get_oppressor()
```


Gets whoever is attacking the supplied NPC. This oppressor can be set through usecode, but it is also automatically set to whomever has last attacked (caused damage to) the supplied NPC. It is also set whenever an NPC acquires a new target -- this NPC will be set as its target's oppressor.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose oppressor is desired. |

#### Return

Returns the NPC number of the supplied NPC's oppressor, or zero if no valid NPC was supplied. Ordinary NPC numbers are positive here, unlike `UI_get_npc_number`. The avatar is represented by zero, rather than -356.


<a id="set_oppressor"></a>

### UI_set_oppressor

[SI, Exult]


```usecode
UI_set_oppressor(actor npc, actor opp)
npc->set_oppressor(opp)
```


Sets the supplied NPCs oppressor to another NPC.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose oppressor is to be changed. |
| `opp` | The NPC that is to be the new oppressor, which must be distinct from the NPC we are altering. Must be a true NPC; an egg- or usecode- spawned monster has no real NPC number in Exult, and cannot be an oppressor. |


<a id="get_readied"></a>

### UI_get_readied

[SI, Exult]


```usecode
object UI_get_readied(actor npc, int spot)
object npc->get_readied(int spot)
```


Inspects the given equipment slot in an NPC.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC we wish to inspect. |
| `spot` | What equipment slot we wish to inspect. Can be any of the values of 'spot' for `is_readied` intrinsic. |

#### Return

Returns zero for an invalid spot or if the spot is empty, otherwise returns an object reference to the object that resides in the desired spot.


<a id="is_readied"></a>

### UI_is_readied

```usecode
bool UI_is_readied(actor npc, int spot, int shape, int frame)
npc->is_readied(int spot, int shape, int frame)
```


Returns '`true`' if an object with shape 'shape' and frame 'frame' is readied in ready spot 'spot' of 'npc', '`false`' otherwise. 'frame' can be the wildcard 'FRAME_ANY'. Valid values for 'spot' are:

For SI, it is better to use '`get_readied`' intrinsic instead.

Checks to see if the given NPC is wearing a piece of equipment on a specified location.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose equipment is to be inspected. |
| `spot` | The spot we are checking. |
| `shape` | The shape we are looking for. |
| `frame` | The frame we are looking for. Can be `FRAME_ANY = -359` to match any frame. |

> **BG SPECIFIC**
>
> In BG, the 'spot' parameter is limited to the 0-12 range. It can take any of the following values:
>
>
>
> | Parameter / value | Description |
> | --- | --- |
> | BG_BACKPACK     =  0 | Containers worn on the back, such as backpacks |
> | BG_WEAPON_HAND  =  1 | Item wielded in weapon hand |
> | BG_SHIELD_HAND  =  2 | Off-hand is also called the shield hand |
> | BG_OFF_HAND     =  2 | Item wielded in off-hand |
> | BG_BELT         =  3 | Items worn around the waist such as belts, and girdles |
> | BG_NECK         =  4 | Items worn around the neck such as amulets, and necklaces |
> | BG_TORSO        =  5 | Worn on the torso, such as armor |
> | BG_LEFT_RING    =  6 | Item worn on left finger (ring) |
> | BG_RIGHT_RING   =  7 | Item worn on right finger (ring) |
> | BG_QUIVER       =  8 | Arrows held in quiver |
> | BG_HEAD         =  9 | Items worn on the head such as headbands, helms, etc |
> | BG_LEGS         = 10 | Worn on legs, such as greaves |
> | BG_FEET         = 11 | Worn on feet, such as boots |
> | BG_USECODE      = 12 | Exult-only. Usecode container, used for containing eggs 'carried' by player |
> | BG_CLOAK        = 13 | Exult-only. Items worn around the neck and back such as cloaks and capes |
> | BG_GLOVES       = 14 | Exult-only. Items worn on both hands, such as gloves |
> | BG_TRIPLE_BOLTS = 15 | Special slot used by triple crossbow bolts |
> | BG_EARS         = 16 | Exult-only. Earrings, such as the serpent earrings |
> | BG_BACK_SHIELD  = 17 | Exult-only. Shield slung across the back |
> | BG_BACK_2H      = 19 | Exult-only. Weapon slung across the back |
> | BG_BOTH_HANDS   = 20 | Special slot for item wielded with both hands |
> | BG_LRGLOVES     = 21 | Special slot for item wielded with both ring slots |
> | BG_AMULET       = 22 | Exult-only. Items worn on the neck such as a collar |
>
>

> **SI SPECIFIC**
>
> In SI, the 'spot' parameter is limited to the 0-17 range. It can take any of the following values:
>
>
>
> | Parameter / value | Description |
> | --- | --- |
> | SI_OFF_HAND     =  0 | Items wielded in the off-hand |
> | SI_SHIELD_HAND  =  0 | Off-hand is also called shield hand |
> | SI_WEAPON_HAND  =  1 | Items wielded in the weapon hand |
> | SI_CLOAK        =  2 | Items worn around the neck and back such as cloaks and capes |
> | SI_AMULET       =  3 | Items worn on the neck such as a collar |
> | SI_HEAD         =  4 | Items worn on the head such as a helm |
> | SI_GLOVES       =  5 | Items worn on both hands, such as gloves |
> | SI_USECODE      =  6 | Usecode container, used for containing eggs 'carried' by player |
> | SI_RIGHT_RING   =  7 | Ring or item worn on right hand |
> | SI_LEFT_RING    =  8 | Ring or item worn on left hand |
> | SI_EARS         =  9 | Earrings, such as the serpent earrings |
> | SI_QUIVER       = 10 | Arrows held in quiver |
> | SI_BELT         = 11 | Worn across the waist, such as belts and girdles |
> | SI_TORSO        = 12 | Worn on the torso, such as armor |
> | SI_FEET         = 13 | Worn on feet, such as boots |
> | SI_LEGS         = 14 | Worn on legs, such as greaves |
> | SI_BACKPACK     = 15 | Containers worn on the back, such as backpacks |
> | SI_BACK_SHIELD  = 16 | Shield slung across the back |
> | SI_BACK_2H      = 17 | Weapon slung across the back |
> | SI_TRIPLE_BOLTS = 18 | Exult-only. Special slot used by triple crossbow bolts |
> | SI_BOTH_HANDS   = 20 | Special slot for item wielded with both hands |
> | SI_LRGLOVES     = 21 | Exult-only. Special slot for item wielded with both ring slots |
>
>

#### Return

Returns `true` if the NPC is wearing an object of the specified shape and frame on the inspected slot, `false` if not.


<a id="get_weapon"></a>

### UI_get_weapon

[SI, Exult]


```usecode
int UI_get_weapon(actor npc)
int npc->get_weapon()
```


Inspect what type of weapon an NPC is using.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose weapon is to be inspected. |

#### Return

Zero if no weapon, one- or two-handed, is wielded by the NPC. Otherwise, the shape in 'SHAPES.VGA' of the weapon being used.


<a id="set_opponent"></a>

### UI_set_opponent

```usecode
UI_set_opponent(actor npc, object obj)
npc->set_opponent(object obj)
```


Makes the supplied NPC target the desired object. This does not make the NPC attack unless his schedule is `IN_COMBAT`. This intrinsic will reevaluate if the NPC's oppressor is actually attacking him, and will invalidate the oppressor if not.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose opponent we wish to set. |
| `obj` | What the target should be set to. If the target is an NPC, his/her oppressor will be set to the attacking NPC. |


<a id="sit_down"></a>

### UI_sit_down

```usecode
UI_sit_down(actor npc, object obj)
npc->sit_down(object obj)
```


Causes the specified NPC to sit on the given chair. 'npc' sit on 'obj'. 'obj' must be a chair/seat shape (currently, hardcoded to 873/292). For barge seats, also runs the barge usecode when everyone in the party is seated.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC that is going to sit down. |
| `obj` | The chair that should be sat upon. |

 If the chair is shape 292 (a barge seat), if there is a barge containing the chair and if all party members are sitting, then Exult will run the barge's usecode (function 0x634).


<a id="get_body_npc"></a>

### UI_get_body_npc

[SI, Exult]


```usecode
UI_get_body_npc(object obj)
obj->get_body_npc()
```


Inspects what NPC (is associated with the given dead body.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object to be inspected. |

#### Return

If the supplied object is the dead body of an NPC, returns the NPC's number, as reported by `get_npc_num` intrinsic call; otherwise, returns zero.


<a id="resurrect"></a>

### UI_resurrect

```usecode
actor UI_resurrect(object obj)
actor obj->resurrect()
```


Brings a dead body back to life. If 'obj' is the body of an NPC, resurrects the NPC and returns a reference to the NPC; otherwise, returns a null ('0') reference.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The body that is to be resurrected. Monsters cannot be brought back to life in this manner, so it must be the body of a true NPC. This body will be deleted after the usecode ends, and all of its contents will be transferred to the resurrected NPC. |

#### Return

Zero if the supplied object was not the body of an NPC, otherwise an object reference to the resurrected NPC.

### Script Commands

There is also a script command with the same name and with a similar function. See '[resurrect](#scriptop-resurrect)' for more information.


<a id="resurrect_npc"></a>

### UI_resurrect_npc

[SI]


```usecode
UI_resurrect_npc(actor npc)
npc->resurrect_npc()
```


Brings the desired NPC back to life. If the NPC has a body, this body will be completely ignored; moreover, the NPC will be back without any equipment that was left on his body when he died.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | Who is to be resurrected. |


<a id="set_polymorph"></a>

### UI_set_polymorph

[SI, Exult]


```usecode
UI_set_polymorph(actor npc, int shape)
npc->set_polymorph(int shape)
```


Makes an NPC take a different, temporary shape. The polymorphed shape is persistent across save games. The NPC can be returned to its normal shape by unsetting the NPC's `POLYMORPH` flag with a call to `clear_item_flag` intrinsic.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC to be polymorphed. |
| `shape` | The shape in 'SHAPES.VGA' that the NPC will take. |


<a id="get_temperature"></a>

### UI_get_temperature

[SI, Exult]


```usecode
int UI_get_temperature(actor npc)
int npc->get_temperature()
```


Inspects a NPC's temperature. The NPC's temperature changes over time depending on whether or not his FREEZE flag is set.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose temperature is sought. |

#### Return

A numeric value in the 0-63 range specifying how cold the NPC is; 0 being normal temperature and 63 being freezing cold.


<a id="set_temperature"></a>

### UI_set_temperature

[SI, Exult]


```usecode
UI_set_temperature(actor npc, int temp)
npc->set_temperature(int temp)
```


Changes a NPC's temperature. The NPC's temperature changes over time depending on whether or not his FREEZE flag is set.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC whose temperature is to be changed. |
| `temp` | The new value for the temperature. This is value in the 0-63 range specifying how cold the NPC is; 0 being normal temperature and 63 being freezing cold. |


<a id="get_temperature_zone"></a>

### UI_get_temperature_zone

[SI, Exult]


```usecode
int UI_get_temperature_zone(object obj)
int obj->get_temperature_zone()
```


Inspects a NPC's temperature zone. The temperature zone is a grouping of temperature ranges that determine how the NPC is affected by the cold. Like the NPC's temperature, it changes over time depending on whether or not his FREEZE flag is set.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The NPC whose temperature zone is sought. |

#### Return

Zero if anything but an NPC is supplied; otherwise, a number in the 1-5 range, equal to the frame of the stats gump that should be displayed.


<a id="get_npc_warmth"></a>

### UI_get_npc_warmth

[SI, Exult]


```usecode
int UI_get_npc_warmth(object obj)
int obj->get_npc_warmth()
```


Inspects the warmth given by a NPC's equipment.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object to be checked. |

#### Return

The combined warmth given by all objects worn by the NPC, or -75 if supplied with anything but a valid NPC.


## Party Intrinsics

These intrinsics relate to obtaining or modifying the list of party members.

<a id="get_party_list"></a>

### UI_get_party_list

```usecode
actor[] UI_get_party_list()
```


Gets a list of all party members.

#### Return

An array containing object references to all of the party's members, including the avatar.


<a id="add_to_party"></a>

### UI_add_to_party

```usecode
UI_add_to_party(actor npc)
npc->add_to_party()
```


Makes the specified NPC join the avatar's party, if there is enough room. The NPC's schedule is changed to `FOLLOW_AVATAR`, and his/her alignment is changed to `GOOD`.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | Who should be added to the party. |


<a id="remove_from_party"></a>

### UI_remove_from_party

```usecode
UI_remove_from_party(actor npc)
npc->remove_from_party()
```


Removes the specified NPC from the avatar's party. The NPC's alignment is changed to `NEUTRAL`.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | Who should be removed from the party. |


<a id="get_dead_party"></a>

### UI_get_dead_party

```usecode
object[] UI_get_dead_party(object obj)
object[] obj->get_dead_party()
```


Gets a list of all dead party members near a given object.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object around which dead bodies will be searched. |

#### Return

An array with one element per recorded dead party member. Each element contains the body reference if the body exists and is fewer than 50 tiles from `obj`; otherwise, that element remains zero. The result is an empty array when there are no recorded dead party members. An invalid `obj` produces no meaningful return value.


## Avatar Intrinsics

All intrinsics in this section are about obtaining or modifying properties specific to the Avatar.

<a id="get_avatar_ref"></a>

### UI_get_avatar_ref

```usecode
actor UI_get_avatar_ref()
```


Gets an object reference to the avatar.

#### Return

The Avatar's object reference.


<a id="in_combat"></a>

### UI_in_combat

```usecode
bool UI_in_combat()
```


Checks if the avatar is in combat.

#### Return

`true` if the avatar is in combat mode, `false` otherwise.


<a id="is_pc_female"></a>

### UI_is_pc_female

```usecode
bool UI_is_pc_female()
```


Determines the avatar's gender.

#### Return

`true` if the avatar is a woman, `false` otherwise.


<a id="get_skin_colour"></a>

### UI_get_skin_colour

[SI, Exult]


```usecode
int UI_get_skin_colour()
```


Obtains the avatar's skin color.

#### Return

A numeric value describing the avatar's skin color. The return values are determined by a table in 'avatar_data.txt'.


<a id="is_pc_inside"></a>

### UI_is_pc_inside

```usecode
bool UI_is_pc_inside()
```


Determines if the avatar has a roof over his head.

#### Return

Returns `true` if there is a roof above the avatar's head, `false` otherwise.


<a id="wearing_fellowship"></a>

### UI_wearing_fellowship

```usecode
bool UI_wearing_fellowship()
```


Determines if the avatar is wearing a fellowship amulet.

#### Return

Returns '`true`' if the avatar is wearing a fellowship amulet (shape 955, frame 1), '`false`' otherwise.


<a id="approach_avatar"></a>

### UI_approach_avatar

[SI, Exult]


```usecode
bool UI_approach_avatar(actor npc)
bool npc->approach_avatar()
bool UI_approach_avatar(actor npc, int unk1, int unk2)
bool npc->approach_avatar(int unk1, int unk2)
```


Makes a NPC approach the avatar at fairly high speed.

**Warning:** If the NPC is initially at more than 10 tiles from the avatar, Exult will let him/her walk for about 1/5 seconds before returning, effectively halting the game in the mean time.

**NOTICE:** In the original, this intrinsic **seems:** to create the NPC off-screen, while it approaches the avatar due to si_path_run_usecode or the 'TALK' schedule. This requires further investigation; this means that **this intrinsic is subject to change**.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | Who should approach the avatar. |
| `unk1` | Unknown parameter from the original. Exult does not use this parameter, but **this is subject to change**. Ignore this parameter at your own peril. |
| `unk2` | Unknown parameter from the original. Exult does not use this parameter, but **this is subject to change**. Ignore this parameter at your own peril. |

#### Return

`false` if the NPC is dead or cannot reach the avatar, `true` otherwise.


<a id="call_guards"></a>

### UI_call_guards

```usecode
UI_call_guards()
```


Causes guard to come and attack a thieving avatar.


<a id="attack_avatar"></a>

### UI_attack_avatar

```usecode
UI_attack_avatar()
```


Causes everyone nearby to attack thieving avatar.


<a id="stop_arresting"></a>

### UI_stop_arresting

```usecode
UI_stop_arresting()
```


Any guards approaching the avatar to arrest will stop what they are doing and will start to wander instead.


<a id="save_pos"></a>

### UI_save_pos

[SI, Exult]


```usecode
UI_save_pos(object obj)
obj->save_pos()
```


Saves the current position/map of an object. This persists across save games, but there is only one 'slot' in which the position is saved. Therefore, only the last stored position is available.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose position we want. |

The current implementation records the object's tile and the current game map number. An invalid object leaves the saved position unchanged. This intrinsic is absent from the SI beta table.


<a id="teleport_to_saved_pos"></a>

### UI_teleport_to_saved_pos

[SI, Exult]


```usecode
UI_teleport_to_saved_pos(object obj)
UI_teleport_to_saved_pos(object obj, bool flag)
obj->teleport_to_saved_pos()
obj->teleport_to_saved_pos(bool flag)
```


Teleports the avatar and party to the last saved position. Note that only the last position stored by the `UI_save_pos` intrinsic is available.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | If this is not the avatar, nothing happens. Otherwise, the party is teleported to the stored position. |
| `flag` | This optional parameter controls whether avatar or party members that are paralyzed or asleep are teleported; if `true`, these members will be teleported, otherwise, they won't; defaults to `false`. |

This intrinsic is absent from the SI beta table. The saved map is used along with the saved tile; an invalid saved x coordinate falls back to the Monitor position `[719, 2608, 1]`.


## Weather/Time Intrinsics

These intrinsics allow you to check or alter the weather, the game clock and usecode timers.

<a id="get_timer"></a>

### UI_get_timer

```usecode
int UI_get_timer(int timer)
```


Gets how many hours elapsed since the timer was set.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `timer` | Numeric value indicating the timer to be inspected. |

#### Return

If the specified timer has been initialized by a previous call to '`UI_set_timer`' intrinsic, the return is the number of hours since the initialization (or reinitialization) of the timer. Uninitialized timers return a random number in the 0-12 hour range.


<a id="set_timer"></a>

### UI_set_timer

```usecode
UI_set_timer(int timer)
```


Resets a timer to the current game time.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `timer` | Numeric value indicating the timer to be initialized. |


<a id="game_day"></a>

### UI_game_day

```usecode
int UI_game_day()
```


Gets the game day.

#### Return

The current game day, starting with day 0.


<a id="game_hour"></a>

### UI_game_hour

```usecode
int UI_game_hour()
```


Gets the game hour.

#### Return

The current game hour in 24-hour format.


<a id="game_minute"></a>

### UI_game_minute

```usecode
int UI_game_minute()
```


Gets the game minutes.

#### Return

The current game minute (0-59).


<a id="part_of_day"></a>

### UI_part_of_day

```usecode
int UI_part_of_day()
```


Gets the number of 3-hour periods that have elapsed in full since midnight.

#### Return

Returns an integer in the 0-7 range corresponding to the number of 3-hour periods that have elapsed in full since midnight. More specifically, the return value is equal to '`UI_game_hour()%3`'. The following constants are defined for convenience:

| MIDNIGHT | = 0 | EARLY | = 1 | DAWN | = 2 | MORNING | = 3 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| NOON | = 4 | AFTERNOON | = 5 | EVENING | = 6 | NIGHT | = 7 |


<a id="nap_time"></a>

### UI_nap_time

```usecode
UI_nap_time(object obj)
obj->nap_time()
```


Makes the avatar lay down to sleep in the specified bed. What constitutes a bed is currently hard-coded (as of 2009/02/27). Once the avatar reaches the bed, Exult will call usecode function 0x622. If the bed is occupied, causes one of the party members to pop up and say so.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The bed where the avatar should sleep. This bed should be unoccupied. |


<a id="advance_time"></a>

### UI_advance_time

```usecode
UI_advance_time(int amount)
```


Advances the game time forward.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `amount` | Number of game ticks that should be advanced; 25 ticks are equal to one game minute, and 1500 ticks are equal to one game hour. The amount is divided by 25 using integer division, so fractions of a minute are discarded. |


<a id="stop_time"></a>

### UI_stop_time

```usecode
UI_stop_time(int length)
```


Stops time for all but the avatar and party.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `length` | How long the effect should last. The actual duration, in seconds, is equal to one fourth of this value. |


<a id="get_weather"></a>

### UI_get_weather

```usecode
int UI_get_weather()
```


Returns the current weather state. Return values are:

#### Return

A numeric value indicating what the current weather is. Possible values are:

| Parameter / value | Description |
| --- | --- |
| CLEAR_WEATHER = 0 | No clouds, no precipitation. |
| SNOWING = 1 | Snowing. |
| RAIN = 2 | Raining |
| SPARKLE = 3 | Magical storm, causes magic to fail. |
| FOG = 4 | Nothing happens. |
| OVERCAST = 5 | Light cloud cover. |
| CLOUDY = 6 | Heavy cloud cover. |


<a id="set_weather"></a>

### UI_set_weather

```usecode
UI_set_weather(int type)
```


Modifies the current weather to the desired type. The new weather lasts for 15 game minutes.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `type` | Numeric value indicating what the weather should be. Can be any of the values returned by `UI_get_weather` intrinsic. |


## User Interface Intrinsics

With these intrinsics, you can ask the user for input or display feedback for his actions.

<a id="click_on_item"></a>

### UI_click_on_item

```usecode
mixed[4] UI_click_on_item()
```


Displays the targeting square and waits for the player to click on something.

#### Return

An array with 4 elements. The first element is the object clicked, or zero if the player clicked a tile; the remaining 3 elements make up the (x, y, z) location of the object or tile clicked.

If used after a '`set_intercept_item`' intrinsic call, the preassigned object or tile is returned in the same manner as if the player had clicked it.

This intrinsic has a special case: when `event == WEAPON` (weapon hit), and if `item` is a valid object, the return will be overridden to be it (although a '`set_intercept_item`' intrinsic call takes precedence over this special case).

If the player cancels the selection, the return is zero instead of a four-element array. A pending `UI_set_intercept_item` selection is consumed before asking for a click; during a weapon event, the caller object can supply the target automatically.


<a id="set_intercept_item"></a>

### UI_set_intercept_item

[Exult]


```usecode
UI_set_intercept_item(object obj)
obj->set_intercept_item()
UI_set_intercept_item(int obj[2])
UI_set_intercept_item(int obj[3])
UI_set_intercept_item(mixed obj[4])
```


Preassigns the return of the next '`UI_click_on_item`' intrinsic' call.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | What we want to set the return of the next the next '`UI_click_on_item`' intrinsic' call to. Can be an object reference, a location (2 or 3 coordinates) or even the return of another '`UI_click_on_item`' intrinsic' call. |


<a id="input_numeric_value"></a>

### UI_input_numeric_value

```usecode
int UI_input_numeric_value(int min, int max, int step, int default)
```


Prompts the player for numerical input with the bar slider.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `min` | The lowest number allowed. |
| `max` | The highest number allowed. |
| `step` | The distance between any two consecutive numbers that can be chosen. |
| `default` | The initial value selected; this value is forced to conform to the boundaries specified above. |

#### Return

The number chosen by the player.


<a id="mouse_exists"></a>

### UI_mouse_exists

```usecode
bool UI_mouse_exists()
```


Determines if a mouse exists.

#### Return

Exult always returns `true`; the original games will return `true` if a mouse if present, `false` otherwise.


<a id="flash_mouse"></a>

### UI_flash_mouse

```usecode
UI_flash_mouse(int cursor)
```


Momentarily changes the mouse cursor to another shape.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `cursor` | Numeric parameter indicating what the new shape will be. Valid values are: |

| CURSOR_X | = 1 | CURSOR_OUT_OF_RANGE | = 2 |
| --- | --- | --- | --- |
| CURSOR_OUT_OF_AMMO | = 3 | CURSOR_TOO_HEAVY | = 4 |
| CURSOR_WONT_FIT | = 5 |


<a id="ambient_light"></a>

### UI_ambient_light

[SI, Exult]


```usecode
UI_ambient_light(bool onoff)
```


Toggles on or off a permanent version of the light spell.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `onoff` | Flag specifying whether we want to turn the light on or off: `true` to turn it off, `false` to turn it on. |


<a id="cause_light"></a>

### UI_cause_light

```usecode
UI_cause_light(int length)
```


Creates a light which illuminates the screen.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `length` | How long the created light will last. The duration is in game minutes. |


<a id="infravision"></a>

### UI_infravision

[SI, Exult]


```usecode
UI_infravision(actor npc, bool onoff)
npc->infravision(bool onoff)
```


Toggles infravision for the given NPC.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC that should have infravision toggled. This intrinsic has effect only for party members. |
| `onoff` | Flag specifying whether we want infravision or not: `true` to turn it on, `false` to turn it off. |


<a id="set_light"></a>

### UI_set_light

```usecode
UI_set_light(object obj, bool onoff)
obj->set_light(bool onoff)
```


Marks the given light source as being added to or removed from its containing NPC.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The light source that is being added or removed. |
| `onoff` | Flag specifying whether we want to add or remove the light source: `true` to add it to its containing NPC, `false` to remove it. |


<a id="earthquake"></a>

### UI_earthquake

```usecode
UI_earthquake(int length)
```


Causes the screen to shake. Also plays the earthquake SFX at the start of the effect.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `length` | The earthquake's duration, in ticks. |


<a id="armageddon"></a>

### UI_armageddon

```usecode
UI_armageddon()
```


Ends the world and kills everyone in it. This is the Armageddon spell from BG; use with care :-). This intrinsic also exists in the original SI, though unlike what happens in BG, it seems to leave no survivors.

Who survives armageddon seems to be hard-coded in the original games; in Exult, you can control who survives this intrinsic by setting the 'Survives Armageddon' flag in the 'NPC Flags' tab in Exult Studio.

Those 'killed' by this intrinsic cannot be resurrected, and they do not leave lootable corpses.

Monster eggs are disabled, as are intrinsics that call guards to attack a thieving avatar.


<a id="restart_game"></a>

### UI_restart_game

```usecode
UI_restart_game()
```


Stops the music and restarts the game from the last save. The original games started from the beginning of the game; Exult does it differently to preserve the last quicksave.


<a id="run_endgame"></a>

### UI_run_endgame

```usecode
UI_run_endgame(bool flag)
```


Runs the endgame.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `flag` | Boolean value specifying the manner in which the game was finished. If it is `true`, will set the game-completion flag that allows the endgame to be viewed from the title screen. |

> **BG SPECIFIC**
>
> If 'flag' is '`false`', the avatar stepped through the Black Gate; if '`true`', the avatar destroyed the Black Gate.

> **SI SPECIFIC**
>
> 'flag' has no effect in the endgame.


<a id="error_message"></a>

### UI_error_message

```usecode
UI_error_message([special])
```


Prints the input variables to 'stdout'.

#### Parameters

Accepts values to print consecutively, followed by a newline. Integers, object references, strings, and arrays of those values are supported; nested array elements are skipped. The original games accepted only one parameter.


<a id="printf"></a>

### UI_printf

[Exult]


```usecode
string UI_printf(mixed out[])
```


Prints formatted text to 'stdout'.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `out` | An array whose first element is the format string, followed by the values to print. Integers and other usecode values are also accepted as substitutions. The format string gives the basic text, and has a placeholder for each of the other elements of the array; each element past the first writes its text at the next available placeholder. Placeholders are the '%s' sequence, without the single quotes, embedded in the format string. |

#### Return

An empty string.


## Display Intrinsics

All intrinsics in this section are related to showing things to the player, be they other portions of the game map or objects from 'SPRITES.VGA'.

<a id="center_view"></a>

### UI_center_view

[Exult]


```usecode
UI_center_view(object obj)
obj->center_view()
```


Centers the screen on a given object.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object that the screen should be centered on. |


<a id="display_area"></a>

### UI_display_area

```usecode
UI_display_area(int pos[])
```


Displays the specified location on-screen. Overlays sprite shape 10 over the screen.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `pos` | The location about which the screen is to be centered. Can be a 3-element (x, y, z) location or a 4-element (x, y, z, map) location. If map is omitted or is -1, the current map is used. |


<a id="set_camera"></a>

### UI_set_camera

```usecode
UI_set_camera(object obj)
obj->set_camera()
```


Centers the screen on a given object.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object that the screen should be centered on. If it is an NPC, the camera will follow that NPC. |


<a id="view_tile"></a>

### UI_view_tile

```usecode
UI_view_tile(int[2] pos)
```


Centers screen around a given tile. It is usually better to use `UI_display_area` instead.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `pos` | The (x, y) location to be displayed. |


<a id="wizard_eye"></a>

### UI_wizard_eye

```usecode
UI_wizard_eye(int length)
```


Lets the player move the screen around, keeping the avatar in its present location. Overlays sprite shape 10 over the screen.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `length` | The duration of the effect, in ticks. Actual duration is increased by 50% in Exult. |


<a id="fade_palette"></a>

### UI_fade_palette

```usecode
UI_fade_palette(int length, int unk, bool type)
```


Causes the screen to fade to/from black and plays music.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `length` | The length, in ticks, of the fade; that is, how long it will take for the fade to happen. |
| `unk` | Reserved; unknown. This is an unknown parameter from the original games; it is unimplemented in Exult. It cannot be left out, however. |
| `type` | The type of fade. If `true`, fades from black; if `false`, fades to black. |


<a id="fade_palette_sleep"></a>

### UI_fade_palette_sleep

[SI, Exult]


```usecode
UI_fade_palette_sleep(int length, int unk, bool type)
```


Causes the screen to fade to/from black.

When fading to black, plays music 24; otherwise, plays music 22.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `length` | The length, in ticks, of the fade; that is, how long it will take for the fade to happen. |
| `unk` | Reserved; unknown. This is an unknown parameter from the original games; it is unimplemented in Exult. It cannot be left out, however. |
| `type` | The type of fade. If `true`, fades from black; if `false`, fades to black. |


<a id="set_time_palette"></a>

### UI_set_time_palette

```usecode
UI_set_time_palette()
```


Resets palette to default palette, based on time of day and number of lights.


<a id="item_say"></a>

### UI_item_say

```usecode
UI_item_say(object obj, string bark)
obj->item_say(string bark)
```


Causes text to be displayed near the object.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object which is to display the text. |
| `bark` | Text to be displayed. |

The intrinsic does not show a bark while NPC conversation text is pending.


<a id="clear_item_say"></a>

### UI_clear_item_say

[SI, Exult]


```usecode
UI_clear_item_say(object obj)
obj->clear_item_say()
```


Deletes the string (if any) being displayed near the desired object.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object in question. |

This also disables pending bark commands in all currently scheduled scripts for the object, so those scripts cannot immediately restore the cleared text.


<a id="display_runes"></a>

### UI_display_runes

```usecode
UI_display_runes(int gump, string runes)
```


Displays the selected gump on screen and prints lines of runic text on-screen. What font is used to draw the text depends on the gump shown, and is currently hard-coded (as of 2009/02/26).

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `gump` | The shape in 'GUMPS.VGA' that will be used. |
| `runes` | An array containing one element per line of runes to display. |

If the avatar's `READ` flag is set, some symbols are converted into pairs of letters. Specifically:

| Parameter / value | Description |
| --- | --- |
| ( | TH |
| ) | EE |
| * | NG |
| + | EA |
| , | ST |

Additionally, all lowercase letters will be turned into uppercase letters, and pipes ('|') will be converted to spaces.

When compiling code, UCC accepts some escape sequences to make these strings easier to read:

| Parameter / value | Description |
| --- | --- |
| ( | \{th} |
| ) | \{ee} |
| * | \{ng} |
| + | \{ea} |
| , | \{st} |
| &#124; | \{dot} |


<a id="book_mode"></a>

### UI_book_mode

```usecode
UI_book_mode(object obj)
obj->book_mode()
```


Displays a book or scroll on screen. Text in the book or scroll is supplied through normal conversation methods.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose shape is to be used to determine what shape from 'GUMPS.VGA' is to be shown. This association is currently hard-coded (as of 2009/02/27): if the shape of the supplied object is equal to 797, a scroll gump will be displayed; otherwise, a book gump will be displayed. |

> **SI SPECIFIC**
>
> There is one additional shape which displays a scroll gump: shape 707. This shape, and shape 705 (serpentine book), will both display text using serpentine characters; if the avatar's 'READ' flag is set, any serpentine runes will be translated into normal runes.


<a id="book_mode_ex"></a>

### UI_book_mode_ex

[Exult]


```usecode
UI_book_mode_ex(bool is_scroll, int font)
UI_book_mode_ex(bool is_scroll, int font, int gump)
```


Displays a book or scroll on screen. Text in the book or scroll is supplied through normal conversation methods.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `is_scroll` | Flag determining whether to display a scroll (`true`) or a book gump (`false`). This determines the area occupied by the text, and will determine what shape from 'GUMPS.VGA' to display if none is specified. |
| `font` | Integer that determines what shape from 'FONTS.VGA' will be used to render the text. |
| `gump` | Numerical value determining what shape from 'GUMPS.VGA' will be used. The text area is not affected by this parameter. Optional; if omitted or <code>-1</code>, uses the standard book or scroll shape. |

> **IMPORTANT NOTES**
>
> Unlike the '`UI_book_mode`' intrinsic, this intrinsic performs **no** translation of any kind if the avatar's 'READ' flag is set. This is an intentional design feature -- for example, text written in Gargish and being displayed in a Gargish font should be translated by more than a mere font substitution. Also, the gump shapes should be 'compatible' with the standard book/scroll shapes as Exult will write the text in the same region of the screen as it does for standard books/scrolls.


<a id="close_gumps"></a>

### UI_close_gumps

```usecode
UI_close_gumps()
```


Closes all gumps.

> **IMPORTANT NOTES**
>
> This intrinsic does nothing while the player is dragging an object. This often matters during `READIED` and `UNREADIED` events, but the check is the dragging state rather than the event ID. Use `UI_close_gumps2` to close gumps even while dragging.


<a id="close_gumps2"></a>

### UI_close_gumps2

[Exult]


```usecode
UI_close_gumps2()
```


Closes all gumps.

This variant closes gumps even while an object is being dragged.


<a id="close_gump"></a>

### UI_close_gump

```usecode
UI_close_gump(object obj)
obj->close_gump()
```


Closes an object's gump.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose gump is to be closed. |

> **IMPORTANT NOTES**
>
> This intrinsic does nothing while the player is dragging an object. This often matters during `READIED` and `UNREADIED` events, but the check is the dragging state rather than the event ID. Use `UI_close_gump2` to close gumps even while dragging.


<a id="close_gump2"></a>

### UI_close_gump2

[Exult]


```usecode
UI_close_gump2(object obj)
obj->close_gump2()
```


Closes an object's gump.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object whose gump is to be closed. |

This variant closes gumps even while an object is being dragged.


<a id="in_gump_mode"></a>

### UI_in_gump_mode

```usecode
bool UI_in_gump_mode()
```


Returns '`true`' if showing gumps, '`false`' otherwise.


<a id="display_map"></a>

### UI_display_map

[BG]


```usecode
UI_display_map()
```


Displays the main game map. Will also display the sextant crosshairs if the party has a sextant.


<a id="si_display_map"></a>

### UI_si_display_map

[SI]


```usecode
UI_si_display_map(int num)
```


Displays the specified map on screen.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `num` | Index of the map to be displayed. This is converted into a shape in 'SPRITES.VGA' in a hard-coded manner. |


<a id="display_map_ex"></a>

### UI_display_map_ex

[Exult]


```usecode
UI_display_map_ex(int mapshp, bool show_loc)
```


Displays the given map centered in the screen.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `mapshp` | The shape in 'SPRITES.VGA' to be displayed as a map. |
| `show_loc` | `true` to show the sextant crosshairs, `false` otherwise. Showing the crosshairs does not depend on being underground or actually having sextants. |


<a id="lightning"></a>

### UI_lightning

```usecode
UI_lightning()
```


Causes a lightning effect. The screen flashes, and the corresponding sound effect is played.


<a id="sprite_effect"></a>

### UI_sprite_effect

```usecode
UI_sprite_effect(int sprite, int x, int y, int vel_x, int vel_y, int frame, int reps)
```


Displays a graphical effect at the given location.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `sprite` | The shape in 'SPRITES.VGA' to be displayed. |
| `x` | The initial horizontal location at which the sprite will be displayed. |
| `y` | The initial vertical location at which the sprite will be displayed. |
| `vel_x` | The horizontal speed at which the sprite will move. This is added to the location at every tick. |
| `vel_y` | The vertical speed at which the sprite will move. This is added to the location at every tick. |
| `frame` | The initial frame in 'SPRITES.VGA' of the sprite. This is increased by one every tick, and may loop back to zero when it reaches the last frame (depending on the value of the 'reps' parameter). |
| `reps` | Number of repetitions to execute. If zero, the sprite will be displayed for a tick and then go away. If positive, number of ticks that the sprite will be displayed. If <code>-1</code>, the sprite will go through all of its frames **once**. If <code>-2</code> the sprite will go through all of its frames until it goes out of screen. If <code>-3</code>, the sprite will go through all of its frames **once**, but in reverse order. |


<a id="obj_sprite_effect"></a>

### UI_obj_sprite_effect

```usecode
UI_obj_sprite_effect(object obj, int sprite, int rel_x, int rel_y, int vel_x, int vel_y, int frnum, int reps)
obj->obj_sprite_effect(int sprite, int rel_x, int rel_y, int vel_x, int vel_y, int frnum, int reps)
```


Creates a sprite effect from 'SPRITES.VGA'. This sprite effect is tied to 'obj'; if 'obj' moves, the sprite will move along. 'sprite' is the sprite's shape number, ('rel_x', 'rel_y') is the relative (x, y) offset of the sprite relative to the position of 'obj', ('vel_x', 'vel_y') is the (x, y) speed (i.e., change in coordinates at every tick, and in addition to any motion of 'obj'). 'frnum' is the initial frame of the sprite effect. If 'reps' `< 0`, the sprite effect goes through all frames (at the rate of one frame per tick) and deletes itself; otherwise, 'reps' is the total number of ticks the animation will last before deleting itself, looping the sprite's frames if it is needed.

Displays a graphical effect around a given object. This sprite effect is linked to the object, and will move with it.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The object about which the effect will happen. |
| `sprite` | The shape in 'SPRITES.VGA' to be displayed. |
| `rel_x` | The initial horizontal offset from the object at which the sprite will be displayed. |
| `rel_y` | The initial vertical offset from the object at which the sprite will be displayed. |
| `vel_x` | The horizontal speed at which the sprite will move. This is added to the horizontal offset at every tick. |
| `vel_y` | The vertical speed at which the sprite will move. This is added to the vertical offset at every tick. |
| `frame` | The initial frame in 'SPRITES.VGA' of the sprite. This is increased by one every tick, and may loop back to zero when it reaches the last frame (depending on the value of the 'reps' parameter). |
| `reps` | Number of repetitions to execute. If zero, the sprite will be displayed for a tick and then go away. If positive, number of ticks that the sprite will be displayed. If <code>-1</code>, the sprite will go through all of its frames **once**. If <code>-2</code> the sprite will go through all of its frames until it goes out of screen. If <code>-3</code>, the sprite will go through all of its frames **once**, but in reverse order. |


<a id="play_scene"></a>

### UI_play_scene

[Exult]


```usecode
UI_play_scene(string text)
UI_play_scene(string text, bool fade_out)
UI_play_scene(string text, bool fade_out, bool fade_in)
```


Plays scripted animated and/or text cutscene after checking whether the files text_info.txt and text.flx exist in the patch folder.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `text` | The base name of the cutscene files. |
| `fade_out` | Optional, default `true`. Controls whether the game fades out to the scene. |
| `fade_in` | Optional, default `true`. Controls whether the game fades back in after the scene. |

The game time queue is paused during an available scene and resumed afterward; an unavailable scene is ignored.


<a id="get_slider_value"></a>

### UI_get_slider_value

[Exult]


```usecode
int UI_get_slider_value(object item, int field_id)
int item->get_slider_value(int field_id)
```


Returns the current value of a slider widget within a dynamic gump.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `item` | The object whose dynamic gump contains the slider widget. |
| `field_id` | The numeric identifier of the slider widget to read. |

#### Return

The current slider value, or 0 on failure (e.g. if the gump is not found or the field does not exist).

Exult first looks for an open gump belonging to `item`, then tries its immediate owner's gump. It uses the first slider widget whose field ID matches; an invalid item, missing gump, or missing slider returns `0`.


<a id="set_gump_shape"></a>

### UI_set_gump_shape

[Exult]


```usecode
int UI_set_gump_shape(object item, int field_id, int shape, int frame)
int item->set_gump_shape(int field_id, int shape, int frame)
```


Sets the shape and frame of a shape widget within a dynamic gump.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `item` | The object whose dynamic gump contains the shape widget. |
| `field_id` | The numeric identifier of the shape widget to update. |
| `shape` | The shape number from shapes.vga to display. |
| `frame` | The frame number of the shape to display. |

#### Return

Returns 1 on success, 0 on failure (e.g. if the gump is not found or the field does not exist).

#### Notes

The gump is located by searching for the gump associated with `item`, with a fallback to `item`'s owner.


<a id="set_gump_text"></a>

### UI_set_gump_text

[Exult]


```usecode
int UI_set_gump_text(object item, int field_id, string text)
int item->set_gump_text(int field_id, string text)
```


Sets the text content of a text field widget within a dynamic gump.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `item` | The object whose dynamic gump contains the text field. |
| `field_id` | The numeric identifier of the text field widget to update. |
| `text` | The string to display in the text field. |

#### Return

Returns 1 on success, 0 on failure (e.g. if the gump is not found or the field does not exist).

#### Notes

The gump is located by searching for the gump associated with `item`, with a fallback to `item`'s owner. Returns 0 if the text string is null.

The text passes through Exult's usecode text translation before the first matching dynamic text field is updated.


<a id="set_gump_text_font"></a>

### UI_set_gump_text_font

[Exult]


```usecode
int UI_set_gump_text_font(object item, int field_id, int font_num)
int item->set_gump_text_font(int field_id, int font_num)
```


Sets the font used by a text field widget within a dynamic gump.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `item` | The object whose dynamic gump contains the text field. |
| `field_id` | The numeric identifier of the text field widget to update. |
| `font_num` | The font number referring to a font shape in the active font file (e.g. fonts_original.vga). |

#### Return

Returns 1 on success, 0 on failure (e.g. if the gump is not found or the field does not exist).

#### Notes

The gump is located by searching for the gump associated with `item`, with a fallback to `item`'s owner.


<a id="set_slider_value"></a>

### UI_set_slider_value

[Exult]


```usecode
int UI_set_slider_value(object item, int field_id, int value)
int item->set_slider_value(int field_id, int value)
```


Sets the current value of a slider widget within a dynamic gump.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `item` | The object whose dynamic gump contains the slider widget. |
| `field_id` | The numeric identifier of the slider widget to update. |
| `value` | The value to set on the slider. |

#### Return

Returns 1 on success, 0 on failure (e.g. if the gump is not found or the field does not exist).

#### Notes

The value is clamped to the slider's configured minimum and maximum range.

Exult first looks for an open gump belonging to `item`, then tries its immediate owner's gump. It uses the first slider widget whose field ID matches; an invalid item, missing gump, or missing slider returns `0`.


## Other Intrinsics

All intrinsics that did not fit in the previous categories.

<a id="add_spell"></a>

### UI_add_spell

```usecode
bool UI_add_spell(int index, int unk, object obj)
```


Adds a spell to a spellbook.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `index` | The spell to be added. This index starts at zero, and must be in the 0-71 range. |
| `unk` | Reserved, must be zero. This is an unknown parameter from the original games; it doesn't seem to have any effects there, and is unused in Exult. You must still pass it to calls to this intrinsic, though. |
| `obj` | What spellbook the spell should be added to. |

#### Return

`false` if the supplied object is not a spellbook or if it already has the spell; `true` otherwise.


<a id="has_spell"></a>

### UI_has_spell

[Exult]


```usecode
bool UI_has_spell(object obj, int index)
```


Checks if a spellbook has one particular spell.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The spellbook to be inspected. |
| `index` | The spell to be checked. This index starts at zero, and must be in the 0-71 range. |

#### Return

`true` if the supplied object is a spellbook and contains the spell; `false` otherwise.


<a id="remove_spell"></a>

### UI_remove_spell

[Exult]


```usecode
bool UI_remove_spell(object obj, int index)
```


Removes the specified spell from the desired spellbook.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The spellbook that will be modified. |
| `index` | The spell to be removed. This index starts at zero, and must be in the 0-71 range. |

#### Return

`true` if the supplied object is a spellbook and had the spell; `false` otherwise.


<a id="remove_all_spells"></a>

### UI_remove_all_spells

[SI, Exult]


```usecode
UI_remove_all_spells(object obj)
obj->remove_all_spells()
```


Removes all spells from the desired spellbook.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The spellbook to be cleared. This intrinsic has no effect if this is not a valid spellbook. |


<a id="get_array_size"></a>

### UI_get_array_size

```usecode
int UI_get_array_size([special])
```


Gets how many elements an array has.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `[special]` | A single array of any type or size. |

#### Return

If supplied with a valid array, returns the number of elements in the array; otherwise, returns 1.


<a id="mark_virtue_stone"></a>

### UI_mark_virtue_stone

[BG]


```usecode
UI_mark_virtue_stone(object obj)
obj->mark_virtue_stone()
```


Imprints a virtue stone with its current location. Any subsequent calls to `recall_virtue_stone` that target that same stone will teleport the avatar's party to this stored location. The stored location includes the map where the stone is.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The virtue stone to be marked. |


<a id="recall_virtue_stone"></a>

### UI_recall_virtue_stone

[BG]


```usecode
UI_recall_virtue_stone(object obj)
UI_recall_virtue_stone(object obj, bool flag)
obj->recall_virtue_stone()
obj->recall_virtue_stone(bool flag)
```


Teleports the party to the virtue stone's stored location. The stone is forcibly given to the avatar if needed.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `obj` | The virtue stone that contains the destination. |
| `flag` | Optional, default `false`. Set to `true` to include paralyzed or sleeping party members in the teleport. |


<a id="set_orrery"></a>

### UI_set_orrery

[BG]


```usecode
UI_set_orrery(int pos[3], int state)
```


Sets the position of the planets in the orrery at Moonglow.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `pos` | The location of the orrery. |
| `state` | Numeric value in the 0 to 9 range determining the relative positions of the planets. |

The intrinsic tries to find 'Planet Britannia' near the specified location; it will delete the nearby planets, if any, and create new ones in the required positions.


<a id="is_water"></a>

### UI_is_water

```usecode
int UI_is_water(int param[2])
int UI_is_water(int param[3])
int UI_is_water(mixed param[4])
```


Tests the water shape flag of a flat map tile or of the object in a four-element click result.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `param` | The object or location to be tested. |

The parameter must be an array with 2, 3, or 4 components. The interpretation of the parameter is as follows:

| Parameter / value | Description |
| --- | --- |
| Type | Interpretation/Format |
| `int [2]` | The parameter is a (x, y) position specifying a tile to be tested. |
| `int [3]` | The parameter is a (x, y, z) position, such as the return of a `UI_get_object_position` intrinsic call. Of this position, (x, y) specifies a tile to be tested. |
| `mixed [4]` | Exult specific: The parameter is of the form (object, x, y, z), such as the return of a call to `UI_click_on_item` intrinsic. If the first element resolves to an object, its shape is tested for the water flag. Otherwise, (x, y) is used to find and test the corresponding tile. |

#### Return

`true` if the specified tile or object is a water tile, `false` otherwise.

The `z` coordinate is ignored when testing the flat map tile. A direct object reference is not accepted by this implementation; pass `[obj, x, y, z]` or use `UI_get_shape_flag(obj, 3)` instead. Invalid inputs return `false`.


<a id="a_or_an"></a>

### UI_a_or_an

```usecode
string UI_a_or_an(string text)
```


Checks whether a bit of text starts with a vowel or not.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `text` | The string being inspected. |

#### Return

If the first letter of the supplied string is in the "aeiouyAEIOUY" set, returns 'an'; otherwise, returns 'a'.


<a id="is_dest_reachable"></a>

### UI_is_dest_reachable

[Exult]


```usecode
bool UI_is_dest_reachable(actor npc, int pos[])
bool npc->is_dest_reachable(int pos[])
```


Checks if a NPC can walk from its present location to a specified position.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `npc` | The NPC we wish to check. |
| `pos` | Destination `[x, y]` or `[x, y, z]`. With two elements, `z` defaults to zero. |

#### Return

`true` if the NPC can walk to the supplied destination, `false` otherwise.

Invalid NPCs, positions with fewer than two elements, and negative `z` values return `false`. The path test accepts reaching within one tile of the destination; it does not require occupying that exact tile. SI beta also has `UI_sib_is_dest_reachable(pos, npc)`, with the arguments reversed.


<a id="can_avatar_reach_pos"></a>

### UI_can_avatar_reach_pos

[SI]


```usecode
bool UI_can_avatar_reach_pos(int pos[])
```


Checks if the avatar can walk from its present location to a specified position. This is similar to, but more limited than, the `is_dest_reachable` Exult intrinsic.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `pos` | Where we want check if the avatar can go. Must have at least two elements; if it has exactly two elements, Exult will assume the z coordinate of the destination to be zero. |

#### Return

`false` if the supplied position is invalid or if the avatar cannot reach the supplied destination, `true` otherwise.

A negative `z` or a position with fewer than two elements returns `false`. The path test accepts reaching within one tile of the destination, rather than requiring the exact tile.


<a id="sib_is_dest_reachable"></a>

### UI_sib_is_dest_reachable

```usecode
bool UI_sib_is_dest_reachable(int pos[], actor npc)
```


Checks whether an NPC can walk to a destination. This SI beta intrinsic takes its arguments in the opposite order from `UI_is_dest_reachable(npc, pos)`.

#### Parameters

| Parameter / value | Description |
| --- | --- |
| `pos` | Destination `[x, y]` or `[x, y, z]`. With two elements, `z` defaults to zero. |
| `npc` | The NPC whose path is checked. |

#### Return

`true` if the NPC can walk to the destination; `false` for an invalid NPC, a position with fewer than two elements, or an unreachable destination.

This intrinsic is available only with `#game "serpentbeta"`. It did not work in the original SI beta; Exult implements the behavior expected by that game's usecode.

A negative `z` returns `false`. As with `UI_is_dest_reachable`, reaching within one tile of the destination is sufficient.


## Other documented intrinsics

<a id="direction_from"></a>

### UI_direction_from

```usecode
int UI_direction_from(object param1, object param2)
int param1->direction_from(object param2)
int UI_direction_from(object param1, int param2[3])
int param1->direction_from(int param2[3])
int UI_direction_from(object param1, mixed param2[4])
int param1->direction_from(mixed param2[4])
int UI_direction_from(int param1[3], object param2)
int UI_direction_from(int param1[3], int param2[3])
int UI_direction_from(int param1[3], mixed param2[4])
int UI_direction_from(mixed param1[4], object param2)
int UI_direction_from(mixed param1[4], int param2[3])
int UI_direction_from(mixed param1[4], mixed param2[4])
```


Alias of `UI_find_direction`. It accepts the same object or coordinate arguments and returns a compass direction from the first position to the second. See [UI_find_direction](#find_direction) for the argument formats and direction values.


<a id="get_party_list2"></a>

### UI_get_party_list2

```usecode
actor[] UI_get_party_list2()
```


Alias of `UI_get_party_list` in Exult. Returns the same array of party members, including the avatar; see [UI_get_party_list](#get_party_list).


## Related script commands

<a id="scriptop-resurrect"></a>

### resurrect

```usecode
resurrect
```

Exult only. If the actor is the **body of** a dead NPC, the NPC is resurrected. All items in the body are transferred over to the NPC.
