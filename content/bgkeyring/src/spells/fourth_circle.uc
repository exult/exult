/*
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
 *
 *
 *	This source file contains some reimplementations of almost all
 *	fourth circle spells. The exceptions are 'Mark' and 'Recall' (but
 *	see the spellbook override for the 'Mark' reimplementation).
 *
 *	There is also 2 new spells in the list: 'Recharge Magic', 'Blink'
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

/*
	Fourth circle Spells

	extern void spellConjure ();
	extern void spellLightning (struct<ObjPos> target);
	extern void spellMassCurse ();
	extern void spellReveal ();
	extern void spellSeance ();
	extern void spellUnlockMagic (struct<ObjPos> target);
	extern void spellRechargeMagic (struct<ObjPos> target);
	extern void spellBlink (struct<ObjPos> target);
*/

enum fourth_circle_spells {
	SPELL_CONJURE					= 0,
	SPELL_LIGHTNING					= 1,
	SPELL_MASS_CURSE				= 2,
	SPELL_REVEAL					= 3,
	SPELL_SEANCE					= 4,
	SPELL_UNLOCK_MAGIC				= 5,
	SPELL_RECHARGE_MAGIC			= 6,		//NPC-only spell
	SPELL_BLINK						= 7			//NPC-only spell
};

void spellConjure () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Kal Xen@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				sfx 65;
				actor frame raise_1h;
				actor frame reach_1h;
				actor frame strike_1h;
				call spellConjureEffect;
			}
		} else {
			script item {
				nohalt;
				actor frame raise_1h;
				actor frame reach_1h;
				actor frame strike_1h;
				call spellFails;
			}
		}
	}
}

void spellLightning (struct<ObjPos> target) {
	if ((event == DOUBLECLICK) || (event == WEAPON)) {
		//struct<ObjPos> target = UI_click_on_item();
		halt_scheduled();
		var dir = direction_from(target);
		item_say("@Ort Grav@");
		if (inMagicStorm()) {
			set_to_attack(target, SHAPE_LIGHTNING);
			script item {
				nohalt;
				face dir;
				actor frame raise_1h;
				actor frame cast_up;
				sfx 65;
				actor frame cast_out;
				actor frame strike_2h;
				actor frame strike_2h;
				attack;
				actor frame standing;
			}
		} else {
			script item {
				nohalt;
				face dir;
				actor frame raise_1h;
				actor frame cast_up;
				actor frame cast_out;
				actor frame strike_2h;
				call spellFails;
			}
		}
	}
}

void spellMassCurse () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Vas Des Sanct@");
		if (inMagicStorm()) {
			struct<Position> pos = get_object_position();
			UI_sprite_effect(ANIMATION_TELEPORT, (pos.x - 2), (pos.y - 2), 0, 0, 0, -1);
			script item {
				nohalt;
				actor frame cast_up;
				sfx 65;
				actor frame cast_out;
				actor frame cast_up;
				actor frame strike_2h;
			}
			var targets = getEnemyTargetList(item, 31);
			for (npc in targets) {
				if (!(UI_die_roll(1, 3) == 1)) {
					var delay = ((get_distance(npc) / 3) + 5);
					script npc after delay ticks {
						nohalt;
						call spellSetFlag, CURSED;
					}
				}
			}
		} else {
			script item {
				nohalt;
				actor frame cast_up;
				actor frame cast_out;
				actor frame cast_up;
				actor frame strike_2h;
				call spellFails;
			}
		}
	}
}

void spellReveal () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		struct<Position> pos = get_object_position();
		item_say("@Wis Quas@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				sfx 67;
				actor frame raise_1h;
				actor frame reach_1h;
				actor frame strike_1h;
			}

			struct<Position> findpos = get_object_position();
			var offset_x = [-15, -15, -15, -5, -5, -5, 5, 5, 5, 15, 15, 15];
			var offset_y = [-7, 2, 11, -7, 2, 11, -7, 2, 11, -7, 2, 11];
			var dist = 7;
			var counter = 0;
			var revealables = [];
			while (counter != 12) {
				counter += 1;
				var find_x = (pos.x + offset_x[counter]);
				var find_y = (pos.y + offset_y[counter]);
				findpos = [find_x, find_y, 0];
				var invisibles = findpos->find_nearby(SHAPE_ANY, dist, MASK_INVISIBLE);
				for (obj in invisibles) {
					if (obj->get_item_flag(INVISIBLE) && (!(obj in revealables))) {
						revealables &= obj;
					}
				}
			}
			if (revealables) {
				for (obj in revealables) {
					script obj after 5 ticks {
						nohalt;
						call spellClearFlag, INVISIBLE;
					}
					obj->obj_sprite_effect(ANIMATION_GREEN_BUBBLES, -1, -1, 0, 0, 0, -1);
				}
			} else {
				obj_sprite_effect(ANIMATION_GREEN_BUBBLES, -1, -1, 0, 0, 0, -1);
			}
		} else {
			script item {
				nohalt;
				actor frame raise_1h;
				actor frame reach_1h;
				actor frame strike_1h;
				call spellFails;
			}
		}
	}
}

void spellSeance () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Kal Wis Corp@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				sfx 67;
				actor frame reach_1h;
				actor frame raise_1h;
				actor frame strike_1h;
			}

			struct<Position> pos = get_object_position();
			var sprite_x = (pos.x - 2);
			var sprite_y = (pos.y - 2);
			UI_sprite_effect(ANIMATION_GREEN_BUBBLES, sprite_x, sprite_y, 0, 0, 0, -1);
			UI_sprite_effect(ANIMATION_TELEPORT, sprite_x, sprite_y, 0, 0, 0, -1);
			//I have NO idea why they had one flag per ghost,
			//instead of having one for them all...
			gflags[SEANCE_CAINE] = true;
			gflags[SEANCE_FERRYMAN] = true;
			gflags[SEANCE_MARKHAM] = true;
			gflags[SEANCE_HORANCE] = true;
			gflags[SEANCE_TRENT] = true;
			gflags[SEANCE_MORDRA] = true;
			gflags[SEANCE_ROWENA] = true;
			gflags[SEANCE_PAULETTE] = true;
			gflags[SEANCE_QUENTON] = true;
			gflags[SEANCE_FORSYTHE] = true;

			var hour = UI_game_hour();
			var minute = UI_game_minute();
			var delay;

			if (hour < 6) {
				delay = ((6 - hour) * 60);
				delay += (60 - minute);
				delay *= 25;
			} else {
				delay = ((23 - hour) * 60);
				delay += (60 - minute);
				delay *= 25;
			}

			script item after delay ticks {
				nohalt;
				finish;
				call spellEndSeance;
			}
		} else {
			script item {
				nohalt;
				actor frame reach_1h;
				actor frame raise_1h;
				actor frame strike_1h;
				call spellFails;
			}
		}
	}
}

void spellUnlockMagic (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		//struct<ObjPos> target = UI_click_on_item();
		var target_shape = target->get_item_shape();
		var dir = direction_from(target);
		halt_scheduled();
		var unlockables = [
			SHAPE_DOOR_HORIZONTAL, SHAPE_DOOR_VERTICAL,
			SHAPE_DOOR2_HORIZONTAL, SHAPE_DOOR2_VERTICAL,
			SHAPE_FALSE_WALL_HORIZONTAL, SHAPE_FALSE_WALL_VERTICAL
		];
		item_say("@Ex Por@");
		if (inMagicStorm()) {
			if (target_shape in unlockables) {
				var target_frame = target->get_item_frame();
				if (((target_frame + 1) % 4) == 0) {
					script item {
						nohalt;
						face dir;
						actor frame reach_1h;
						actor frame raise_1h;
						actor frame strike_1h;
						sfx 66;
					}

					script target after 6 ticks {
						nohalt;
						frame target_frame - 3;
					}
					return;
				}
			}
		}
		script item {
			nohalt;
			face dir;
			actor frame reach_1h;
			actor frame raise_1h;
			actor frame strike_1h;
			call spellFails;
		}
	}
}

void spellRechargeMagic (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		//struct<ObjPos> target = UI_click_on_item();
		var target_shape = target->get_item_shape();
		var dir = direction_from(target);
		item_say("@Uus Ort@");
		var charges = target->get_item_quality();
		if (inMagicStorm() && (target_shape in [SHAPE_LIGHTNING_WAND, SHAPE_FIRE_WAND, SHAPE_FIREDOOM_STAFF]) && (charges < 100)) {
			if (charges < 50) {
				charges += UI_die_roll(12, 25);
			} else if (charges < 70) {
				charges += UI_die_roll(6, 18);
			} else if (charges < 200) {
				charges += UI_die_roll(3, 9);
			}
			target->set_item_quality(charges);
			UI_sprite_effect(ANIMATION_PURPLE_BUBBLES, target.x, target.y, 0, 0, 0, -1);
			script item {
				nohalt;
				face dir;
				sfx 67;
				actor frame cast_out;
				actor frame cast_up;
				actor frame ready;
			}
		} else {
			script item {
				nohalt;
				face dir;
				actor frame cast_out;
				actor frame cast_up;
				actor frame ready;
				call spellFails;
			}
		}
	}
}

const int CREATE_CLOUDS					= 15;
void spellBlink (struct<ObjPos> target) {

	var nearbyobjs;
	var party;
	var index;
	var max;
	var obj;
	var delay;
	var rand;
	var pathegg;

	if (event == DOUBLECLICK) {
		halt_scheduled();
		struct<Position> destpos = getClickPosition(target);
		var dir = direction_from(target);
		UI_item_say(item, "@Rel Por@");
		if (inMagicStorm() && is_dest_reachable(destpos)) {
			var move_party = (get_npc_object() in UI_get_party_list()) &&
			                 (get_schedule_type() != IN_COMBAT);
			script item {
				nohalt;
				face dir;
				actor frame raise_1h;
				actor frame reach_2h;
				actor frame raise_1h;
				actor frame standing;
			}

			var targets;
			if (move_party) {
				targets = party;
			} else {
				targets = [item];
			}
			for (npc in targets) {
				struct<Position> pos = npc->get_object_position();
				npc->obj_sprite_effect(ANIMATION_TELEPORT, 0, 0, 0, 0, 0, -1);
				UI_play_sound_effect2(SOUND_TELEPORT, npc);
				var field = UI_create_new_object(SHAPE_FIRE_FIELD);
				if (field) {
					//var fieldpos = [pos.x + 1, pos.y + 1, pos.z];
					var fieldpos = [pos.x, pos.y, pos.z];
					UI_update_last_created(fieldpos);
					var duration = 50;
					field->set_item_quality(duration);
					field->set_item_flag(TEMPORARY);
					script field after duration ticks {
						nohalt;
						remove;
					}
				}
			}

			if (move_party) {
				PARTY->move_object(destpos);
			} else {
				move_object(destpos);
			}
			return;
		}

		script item {
			nohalt;
			face dir;
			actor frame raise_1h;
			actor frame reach_2h;
			actor frame raise_1h;
			call spellFails;
		}
	} else if (event == SCRIPTED) {
		clear_item_flag(BG_DONT_MOVE);
	}
}
