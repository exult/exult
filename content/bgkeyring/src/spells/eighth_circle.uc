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
 *	eighth circle spells. The exception is 'Armaggedon'.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

/*
	Eighth circle Spells

	extern void spellDeathVortex (struct<ObjPos> target);
	extern void spellInvisibilityAll ();
	extern void spellMassDeath ();
	extern void spellResurrect (struct<ObjPos> target);
	extern void spellSummon ();
	extern void spellSwordStrike (struct<ObjPos> target);
	extern void spellTimeStop ();
	extern void spellMassResurrect ();
*/

enum eighth_circle_spells {
	SPELL_DEATH_VORTEX				= 0,
	SPELL_INVISIBILITY_ALL			= 1,
	SPELL_MASS_DEATH				= 2,
	SPELL_RESURRECT					= 3,
	SPELL_SUMMON					= 4,
	SPELL_SWORDSTRIKE				= 5,
	SPELL_TIME_STOP					= 6,			//NPC-only spell
	SPELL_MASS_RESURRECT			= 7			//Special NPC-only spell
};

void spellDeathVortex (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		//struct<ObjPos> target = UI_click_on_item();
		var dir = direction_from(target);
		halt_scheduled();
		item_say("@Vas Corp Hur@");
		if (inMagicStorm()) {
			set_to_attack(target, SHAPE_DEATH_VORTEX);
			script item {
				nohalt;
				face dir;
				sfx 65;
				actor frame cast_up;
				actor frame strike_1h;
				attack;
			}
		} else {
			script item {
				nohalt;
				face dir;
				actor frame cast_up;
				actor frame strike_1h;
				call spellFails;
			}
		}
	}
}

void spellInvisibilityAll () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Vas Sact Lor@");
		if (inMagicStorm()) {
			struct<Position> pos = get_object_position();
			UI_sprite_effect(ANIMATION_TELEPORT, (pos.x - 2), (pos.y - 2), 0, 0, 0, -1);
			script item {
				nohalt;
				actor frame raise_1h;
				sfx 67;
				actor frame standing;
				actor frame cast_up;
				actor frame standing;
				actor frame strike_2h;
			}
			var targets = getFriendlyTargetList(item, 25);
			for (npc in targets) {
				var delay = ((get_distance(npc) / 3) + 5);
				script npc after delay ticks {
					nohalt;
					call spellSetFlag, INVISIBLE;
				}
			}
		} else {
			script item {
				nohalt;
				actor frame raise_1h;
				actor frame standing;
				actor frame cast_up;
				actor frame standing;
				actor frame strike_2h;
				call spellFails;
			}
		}
	}
}

void spellMassDeath () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Vas Corp@");
		if (inMagicStorm()) {
			struct<Position> pos = get_object_position();
			UI_sprite_effect(ANIMATION_TELEPORT, (pos.x - 2), (pos.y - 2), 0, 0, 0, -1);
			script item {
				nohalt;
				sfx 65;
				actor frame kneeling;
				actor frame standing;
				actor frame cast_up;
				actor frame cast_out;
				sfx 67;
			}
			var nearby_npcs = find_nearby(-1, 25, MASK_NPC);
			var safenpcs = [
				UI_get_party_list2(),
				LORD_BRITISH->get_npc_object(),
				BATLIN->get_npc_object()
			];
			var killed_anyone = false;
			for (npc in nearby_npcs) {
				if (!(npc in safenpcs)) {
					var delay = ((get_distance(npc) / 3) + 5);
					npc->halt_scheduled();
					script npc after delay ticks {
						nohalt;
						call spellCauseDeath;
					}
					killed_anyone = true;
				}
			}

			if (killed_anyone == true) {
				var party = UI_get_party_list();
				for (npc in party) {
					var hps = npc->get_npc_prop(HEALTH);
					hurtNPC(npc, (hps - 2));
				}
			}
		} else {
			script item {
				nohalt;
				actor frame kneeling;
				actor frame standing;
				actor frame cast_up;
				actor frame cast_out;
				call spellFails;
			}
		}
	}
}

void spellResurrect (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		//struct<ObjPos> target = UI_click_on_item();
		var target_shape = target->get_item_shape();
		struct<Position> pos = target->get_object_position();
		var dir = direction_from(target);
		target->halt_scheduled();
		halt_scheduled();
		var canresurrect = false;
		if (target->get_item_shape() in [SHAPE_BODIES_1, SHAPE_BODIES_2, SHAPE_LARGE_BODIES, SHAPE_NEW_BODIES]) {
			var target_quality = target->get_item_quality();
			var quant = target->get_item_quantity(target_shape);
			if ((target_quality == 0) && (quant == 0)) {
				canresurrect = false;
			} else {
				canresurrect = target->resurrect();
			}
		} else {
			canresurrect = false;
		}

		item_say("@In Mani Corp@");
		if (inMagicStorm() && canresurrect) {
			script item {
				nohalt;
				face dir;
				sfx 64;
				actor frame kneeling;
				actor frame standing;
				actor frame cast_up;
			}
			UI_play_music(15, 0);
			UI_sprite_effect(ANIMATION_LIGHTNING, pos.x, pos.y, 0, 0, 0, -1);
			UI_sprite_effect(ANIMATION_GREEN_BUBBLES, (pos.x - 2), (pos.y - 2), 0, 0, 0, -1);
		} else {
			script item {
				nohalt;
				face dir;
				actor frame kneeling;
				actor frame standing;
				actor frame cast_up;
				call spellFails;
			}
		}
	}
}

void spellSummon () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Kal Vas Xen@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				actor frame kneeling;
				actor frame standing;
				actor frame cast_up;
				actor frame cast_out;
				actor frame strike_2h;
				sfx 65;
				call spellSummonEffect;
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

void spellSwordStrike (struct<ObjPos> target) {
	if ((event == DOUBLECLICK) || (event == WEAPON)) {
		//struct<ObjPos> target = UI_click_on_item();
		halt_scheduled();
		var dir = direction_from(target);
		item_say("@In Jux Por Ylem@");
		if (inMagicStorm()) {
			set_to_attack(target, SHAPE_SWORDSTRIKE);
			script item {
				nohalt;
				face dir;
				sfx 65;
				actor frame raise_1h;
				actor frame standing;
				actor frame cast_up;
				actor frame standing;
				actor frame strike_1h;
				actor frame strike_2h;
				attack;
				actor frame standing;
			}
		} else {
			script item {
				nohalt;
				face dir;
				actor frame raise_1h;
				actor frame standing;
				actor frame cast_up;
				actor frame standing;
				actor frame strike_2h;
				call spellFails;
			}
		}
	}
}

void spellTimeStop () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@An Tym@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				sfx 67;
				actor frame strike_2h;
				actor frame cast_out;
				call spellStopTime, 100;
			}
		} else {
			script item {
				nohalt;
				actor frame strike_2h;
				actor frame cast_out;
				call spellFails;
			}
		}
	}
}

void spellMassResurrect () {
	var bodyshapes = [SHAPE_BODIES_1, SHAPE_BODIES_2, SHAPE_LARGE_BODIES, SHAPE_NEW_BODIES];
	var bodies = [];
	for (shnum in bodyshapes) {
		bodies = [bodies, find_nearby(shnum, 25, MASK_NONE)];
	}

	if (event == DOUBLECLICK) {
		item_say("@Vas Mani Corp Hur@");

		var have_resurrectables = false;
		for (body in bodies) {
			var qual = body->get_item_quality();
			var quant = body->get_item_quantity(1);
			if ((qual != 0) || (quant != 0)) {
				have_resurrectables = true;
				break;
			}
		}

		if (inMagicStorm() && have_resurrectables) {
			script item {
				nohalt;
				sfx 64;
				actor frame kneeling;
				actor frame standing;
				actor frame cast_up;
				call spellMassResurrectEffect;
			}
			UI_play_music(15, 0);
		} else {
			script item {
				nohalt;
				actor frame kneeling;
				actor frame standing;
				actor frame cast_up;
				call spellFails;
			}
		}
	}
}
