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
 *	This source file contains some reimplementations of all sixth
 *	circle spells.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

/*
	Sixth circle Spells

	extern void spellCauseFear ();
	extern void spellClone (struct<ObjPos> target);
	extern void spellFireRing (struct<ObjPos> target);
	extern void spellFlameStrike ();
	extern void spellMagicStorm ();
	extern void spellPoisonField (struct<ObjPos> target);
	extern void spellSleepField (struct<ObjPos> target);
	extern void spellTremor ();
*/

enum sixth_circle_spells {
	SPELL_CAUSE_FEAR				= 0,
	SPELL_CLONE						= 1,
	SPELL_FIRE_RING					= 2,
	SPELL_FLAME_STRIKE				= 3,
	SPELL_MAGIC_STORM				= 4,
	SPELL_POISON_FIELD				= 5,
	SPELL_SLEEP_FIELD				= 6,
	SPELL_TREMOR					= 7
};

void spellCauseFear () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Quas Wis@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				sfx 65;
				actor frame kneeling;
				actor frame standing;
				actor frame reach_1h;
				actor frame raise_1h;
				actor frame strike_1h;
				call spellCauseFearEffect;
			}
		} else {
			script item {
				nohalt;
				actor frame kneeling;
				actor frame standing;
				actor frame reach_1h;
				actor frame raise_1h;
				actor frame strike_1h;
				call spellFails;
			}
		}
	}
}

void spellClone (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		//struct<ObjPos> target = UI_click_on_item();
		var dir = direction_from(target);
		halt_scheduled();
		item_say("@In Quas Xen@");
		if (inMagicStorm() && (target->is_npc() && (!(UI_get_item_flag(0, 27) == -1)))) {
			script item {
				nohalt;
				actor frame raise_1h;
				actor frame cast_out;
				actor frame strike_2h;
			}
			script target after 4 ticks {
				nohalt;
				call spellCloneEffect, get_alignment();
			}
		} else {
			script item {
				nohalt;
				actor frame raise_1h;
				actor frame cast_out;
				actor frame strike_2h;
				call spellFails;
			}
		}
	}
}

void spellFireRing (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		//struct<ObjPos> target = UI_click_on_item();
		halt_scheduled();
		var dir = direction_from(target);
		item_say("@Kal Flam Grav@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				face dir;
				sfx 65;
				actor frame reach_1h;
				actor frame strike_1h;
				actor frame raise_1h;
			}

			var offset_x = [-1,  0,  1,  2,  2,  2,  1,  0, -1, -2, -2, -2];
			var offset_y = [-2, -2, -2, -1,  0,  1,  2,  2,  2,  1,  0, -1];
			var counter = 0;
			while (counter < 12) {
				counter += 1;
				var ring_x = (target.x + offset_x[counter]);
				var ring_y = (target.y + offset_y[counter]);
				var ring_z = target.z;
				var pos = [ring_x, ring_y, ring_z];
				var pos2 = [ring_x, ring_y, (ring_z + 1)];
				if (!UI_is_not_blocked(pos, SHAPE_DELAYED_EXPLOSION, 0)) {
					pos = pos2;
				}
				if (UI_is_not_blocked(pos, SHAPE_DELAYED_EXPLOSION, 0)) {
					var ring = UI_create_new_object(SHAPE_DELAYED_EXPLOSION);
					if (ring) {
						ring->set_item_flag(TEMPORARY);
						ring->set_item_flag(INVISIBLE);
						UI_update_last_created(pos);
						ring->set_npc_prop(HEALTH, 1);
						script ring after counter ticks {
							nohalt;
							call spellFireRingEffect;
						}
					}
				}
			}
		} else {
			script item {
				nohalt;
				face dir;
				actor frame reach_1h;
				actor frame strike_1h;
				actor frame raise_1h;
				call spellFails;
			}
		}
	}
}

void spellFlameStrike () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Vas In Flam Grav@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				sfx 65;
				actor frame bowing;
				actor frame kneeling;
				actor frame bowing;
				actor frame cast_out;
				actor frame strike_2h;
			}

			var targets = getEnemyTargetList(item, 25);
			for (npc in targets) {
				struct<Position> pos = npc->get_object_position();
				var strike_x = pos.x;
				var strike_y = pos.y;
				var strike_z = pos.z;
				pos = [strike_x, strike_y, 0];
				var strike = UI_create_new_object(SHAPE_FIRE_FIELD);
				if (strike) {
					var last_obj = UI_update_last_created(pos);
					var delay = 31 + UI_die_roll(1, 15);
					last_obj = strike->set_item_quality(delay);
					strike->set_item_flag(TEMPORARY);
					script strike after delay ticks {
						nohalt;
						remove;
					}
				}
			}
		} else {
			script item {
				nohalt;
				actor frame bowing;
				actor frame kneeling;
				actor frame bowing;
				actor frame cast_out;
				actor frame strike_2h;
				call spellFails;
			}
		}
	}
}

void spellMagicStorm () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Vas Oort Hur@");
		if (inMagicStorm()) {
			var delay = 70;
			gflags[MAGIC_STORM_SPELL] = true;
			UI_set_weather(2);
			script item {
				nohalt;
				sfx 65;
				actor frame cast_up;
				actor frame cast_out;
				actor frame cast_up;
				actor frame strike_2h;
				actor frame standing;
			}

			script item after 8 ticks {
				nohalt;
				call spellMagicStormEffect;
			}

			script item after delay ticks {
				nohalt;
				call stopMagicStorm;
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

void spellPoisonField (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		//struct<ObjPos> target = UI_click_on_item();
		halt_scheduled();
		var dir = direction_from(target);
		item_say("@In Nox Grav@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				face dir;
				sfx 110;
				actor frame raise_1h;
				actor frame reach_1h;
				actor frame strike_1h;
			}
			var field = UI_create_new_object(SHAPE_POISON_FIELD);
			if (field) {
				var field_x = (target.x + 1);
				var field_y = (target.y + 1);
				var field_z = target.z;
				var pos = [field_x, field_y, field_z];
				field->set_item_flag(TEMPORARY);
				UI_update_last_created(pos);
			}
		} else {
			script item {
				nohalt;
				face dir;
				actor frame raise_1h;
				actor frame reach_1h;
				actor frame strike_1h;
				call spellFails;
			}
		}
	}
}

void spellSleepField (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		//struct<ObjPos> target = UI_click_on_item();
		halt_scheduled();
		var dir = direction_from(target);
		item_say("@In Zu Grav@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				sfx 65;
				face dir;
				actor frame raise_1h;
				actor frame reach_1h;
				actor frame strike_1h;
			}

			var field_x = (target.x + 1);
			var field_y = (target.y + 1);
			var field_z = target.z;
			var pos = [field_x, field_y, field_z];
			var field = UI_create_new_object(SHAPE_SLEEP_FIELD);
			if (field) {
				field->set_item_flag(TEMPORARY);
				UI_update_last_created(pos);
			}
		} else {
			script item {
				nohalt;
				face dir;
				actor frame raise_1h;
				actor frame reach_1h;
				actor frame strike_1h;
				call spellFails;
			}
		}
	}
}

void spellTremor () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Vas Por Ylem@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				actor frame raise_2h;
				actor frame standing;
				actor frame kneeling;
				sfx 67;
				call spellTremorEffect;
			}
		} else {
			script item {
				nohalt;
				actor frame raise_2h;
				actor frame standing;
				actor frame kneeling;
				call spellFails;
			}
		}
	}
}
