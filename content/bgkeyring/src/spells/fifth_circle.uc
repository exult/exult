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
 *	This source file contains some reimplementations of all fifth
 *	circle spells.
 *
 *	There is also a new spell in the list: 'Summon Skeletons'.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

/*
	Fifth circle Spells

	extern void spellCharm (struct<ObjPos> target);
	extern void spellDance ();
	extern void spellDispelField (struct<ObjPos> target);
	extern void spellExplosion (struct<ObjPos> target);
	extern void spellFireField (struct<ObjPos> target);
	extern void spellGreatHeal (struct<ObjPos> target);
	extern void spellInvisibility (struct<ObjPos> target);
	extern void spellMassSleep ();
	extern void spellSummonSkeletons ();
*/

enum fifth_circle_spells {
	SPELL_CHARM						= 0,
	SPELL_DANCE						= 1,
	SPELL_DISPEL_FIELD				= 2,
	SPELL_EXPLOSION					= 3,
	SPELL_FIRE_FIELD				= 4,
	SPELL_GREAT_HEAL				= 5,
	SPELL_INVISIBILITY				= 6,
	SPELL_MASS_SLEEP				= 7,
	SPELL_SUMMON_SKELETONS			= 8
};

void spellCharm (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		//struct<ObjPos> target = UI_click_on_item();
		var dir = direction_from(target);
		halt_scheduled();
		item_say("@An Xen Ex@");
		if (inMagicStorm()) {
			set_to_attack(target, SHAPE_SPELL_CHARM);
			script item {
				nohalt;
				face dir;
				sfx 68;
				actor frame raise_1h;
				actor frame cast_out;
				actor frame strike_2h;
				attack;
			}
		} else {
			script item {
				nohalt;
				face dir;
				actor frame raise_1h;
				actor frame cast_out;
				actor frame strike_2h;
				call spellFails;
			}
		}
	}
}

void spellDance () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Por Xen@");
		if (inMagicStorm()) {
			item_say("@Everybody DANCE now!@");
			script item {
				nohalt;
				sfx 67;
				actor frame raise_1h;
				actor frame cast_out;
				actor frame strike_2h;
				call spellCauseDancing;
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

void spellDispelField (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		//struct<ObjPos> target = UI_click_on_item();
		var dir = direction_from(target);
		halt_scheduled();
		var fields = [SHAPE_ENERGY_FIELD, SHAPE_FIRE_FIELD, SHAPE_POISON_FIELD, SHAPE_SLEEP_FIELD];
		item_say("@An Grav@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				sfx 65;
				face dir;
				actor frame cast_up;
				actor frame cast_out;
				actor frame strike_2h;
			}

			if (target->get_item_shape() in fields) {
				target->halt_scheduled();
				target->remove_item();
			}
		} else {
			script item {
				nohalt;
				face dir;
				actor frame cast_up;
				actor frame cast_out;
				actor frame strike_2h;
				call spellFails;
			}
		}
	}
}

void spellExplosion (struct<ObjPos> target) {
	if ((event == DOUBLECLICK) || (event == WEAPON)) {
		//struct<ObjPos> target = UI_click_on_item();
		halt_scheduled();
		var dir = direction_from(target);
		item_say("@Vas Flam Hur@");
		if (inMagicStorm()) {
			set_to_attack(target, SHAPE_SPELL_EXPLOSION);
			script item {
				nohalt;
				face dir;
				sfx 65;
				actor frame cast_up;
				actor frame cast_out;
				actor frame strike_1h;
				actor frame strike_1h;
				attack;
				actor frame standing;
			}
		} else {
			script item {
				nohalt;
				face dir;
				actor frame cast_up;
				actor frame cast_out;
				actor frame strike_1h;
				call spellFails;
			}
		}
	}
}

void spellFireField (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		//struct<ObjPos> target = UI_click_on_item();
		var dir = direction_from(target);
		halt_scheduled();
		item_say("@In Flam Grav@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				sfx 65;
				face dir;
				actor frame raise_1h;
				actor frame reach_1h;
				actor frame strike_1h;
			}

			var field = UI_create_new_object(SHAPE_FIRE_FIELD);
			if (field) {
				var field_x = (target.x + 1);
				var field_y = (target.y + 1);
				var field_z = target.z;
				var pos = [field_x, field_y, field_z];
				UI_update_last_created(pos);
				var duration = 100;
				field->set_item_quality(duration);
				field->set_item_flag(TEMPORARY);
				script field after duration ticks {
					nohalt;
					remove;
				}
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

void spellGreatHeal (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		//struct<ObjPos> target = UI_click_on_item();
		target->halt_scheduled();
		var dir = direction_from(target);
		item_say("@Vas Mani@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				face dir;
				actor frame reach_1h;
				actor frame raise_1h;
				actor frame strike_1h;
				sfx 64;
			}
			if (target->is_npc()) {
				var str = target->get_npc_prop(STRENGTH);
				var hps = target->get_npc_prop(HEALTH);
				target->set_npc_prop(HEALTH, (str - hps));
			}
		} else {
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
}

void spellInvisibility (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		//struct<ObjPos> target = UI_click_on_item();
		halt_scheduled();
		var dir = direction_from(target);
		item_say("@Sanct Lor@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				face dir;
				sfx 67;
				actor frame cast_up;
				actor frame cast_out;
				actor frame strike_2h;
			}

			script target after 4 ticks {
				nohalt;
				call spellSetFlag, INVISIBLE;
			}
		} else {
			script item {
				nohalt;
				face dir;
				actor frame cast_up;
				actor frame cast_out;
				actor frame strike_2h;
				call spellFails;
			}
		}
	}
}

void spellMassSleep () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		struct<Position> pos = get_object_position();
		item_say("@Vas Zu@");
		if (inMagicStorm()) {
			UI_sprite_effect(ANIMATION_TELEPORT, (pos.x - 2), (pos.y - 2), 0, 0, 0, -1);
			script item {
				nohalt;
				sfx 65;
				actor frame standing;
				actor frame raise_1h;
				actor frame cast_up;
				actor frame strike_1h;
				actor frame strike_2h;
			}

			var targets = getEnemyTargetList(item, 25);
			for (npc in targets) {
				script npc after 7 ticks call spellSleepEffect;
			}
		} else {
			script item {
				nohalt;
				actor frame standing;
				actor frame raise_1h;
				actor frame cast_up;
				actor frame strike_1h;
				actor frame strike_2h;
				call spellFails;
			}
		}
	} else if (event == SCRIPTED) {
	}
}

void spellSummonSkeletons () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Kal Corp Xen@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				actor frame kneeling;
				actor frame standing;
				actor frame cast_up;
				actor frame cast_out;
				actor frame strike_2h;
				sfx 65;
				call spellSummonSkeletonsEffect;
			}
		} else {
			script item {
				nohalt;
				actor frame kneeling;
				actor frame standing;
				actor frame cast_up;
				actor frame cast_out;
				actor frame strike_2h;
				call spellFails;
			}
		}
	}
}
