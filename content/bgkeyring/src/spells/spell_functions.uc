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
 *	This source file contains some spellcasting related constants, as well as
 *	a few general-purpose spell functions. IT IS NOT FINISHED YET!
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

const int SCRIPT_FACE_DIR			= 0x59;
const int SCRIPT_NPC_FRAME			= 0x61;

//Cool new spell stuff:
enum npc_wizard_flags {
	//New spell effects:
	CONFUSION						= 40,	//Under the effects of Consufion spell
	DEATH_PROTECTION				= 41,	//Protected from death spells
	MAGIC_PROTECTION				= 42,	//Protected from spells -- e.g., LB's crown
	//Spellcasting power:
	ARCHWIZARD						= 45,	//Cheat mode
	MAGE_CLASS						= 46,	//Full spellcasting capabilities
	BARD_CLASS						= 47,	//1/2 level spellcasting
	WARMAGE							= 48,	//Combat spells cost half as much mana (round up)
	SUMMONER						= 49,	//Summoning spells cost half as much mana (round up)
	HEALER							= 50,	//Casts healing as if level 8, no death spells
	NECROMANCER						= 51,	//Casts death as if level 8, no healing spells
	//The next constants are used to control spellcaster AI; the AI is NOT
	//IMPLEMENTED YET.
	AI_CAST_DONT_CAST				= 52,	//AI will not cast any spells
	AI_CAST_VERY_FEW				= 53,	//AI will cast every 32-36 ticks
	AI_CAST_FEW						= 54,	//AI will cast every 24-28 ticks
	//AI_CAST_DEFAULT						//AI will cast every 16-20 ticks
	AI_CAST_MANY					= 55,	//AI will cast every 8-12 ticks
	AI_HEALING						= 56,	//AI will try to cast healing spells first
	AI_SUPPORT						= 57,	//AI will try to cast support spells first
	AI_OFFENSE						= 58,	//AI will try to cast damaging combat first
	AI_SUMMON						= 59,	//AI will try to summon creatures first
	AI_ENCHANT						= 60,	//AI will try to cast non-damage combat spells
	AI_DEATH						= 61,	//AI will try to cast death spells first
	AI_MISDIRECTION					= 62,	//AI will try to use invisibility+blink+mass confusion very early and often
	AI_TEMPORARY					= 63	//AI will stop casting when it runs out of targets
};

void greatDouseIgnite (var obj, var shapes) {
	var dist = 25;
	for (obj in shapes) {
		//For each shape # in the array,
		//Find all nearby objects with that shape...
		var lsources = obj->find_nearby(obj, dist, MASK_NONE);
		for (lightsource in lsources) {
			//For each object in the array,
			//calculate a delay based on distance:
			var delay = ((obj->get_distance(lightsource) / 3) + 2);
			//Call the obj's usecode function after delay ticks:
			script lightsource after delay ticks {
				nohalt;
				call lightsource->get_usecode_fun(), DOUBLECLICK;
			}
		}
	}
}

void DeathBoltHit object#(0xB8F) () {
	//Bail out unless an NPC is hit:
	if (is_npc()) {
		//See if he can't die:
		if (get_item_flag(CANT_DIE) || get_item_flag(DEATH_PROTECTION)) {
			return;
		}
		//Get NPC's intelligence:
		var targetint = get_npc_prop(INTELLIGENCE);
		//Opposed roll to see if we have a corpse in hands:
		if (UI_roll_to_win(20, targetint)) {
			script item hit 127, MAGIC_DAMAGE;
		}
	}
}

void spellAwakenEffect object#() () {
	if (is_npc()) {
		halt_scheduled();
		clear_item_flag(ASLEEP);
	} else {
		flashBlocked(60);
	}
}

void spellSleepEffect object#() () {
	if (is_npc()) {
		halt_scheduled();
		set_item_flag(ASLEEP);
	} else {
		flashBlocked(60);
	}
}

void spellClearFlag object#() () {
	clear_item_flag(event);
}

void spellSetFlag object#() () {
	set_item_flag(event);
}

void spellEnchantEffect object#() () {
	var normal_missiles = [SHAPE_ARROWS, SHAPE_BOLTS];
	var magic_missiles = [SHAPE_MAGIC_ARROWS, SHAPE_MAGIC_BOLTS];
	for (missile in normal_missiles with index) {
		if (missile == get_item_shape()) {
			set_item_shape(magic_missiles[index]);
		}
	}
}

void spellHealEffect object#() () {
	var str = get_npc_prop(STRENGTH);
	var hps = get_npc_prop(HEALTH);
	if (hps <= str) {
		var healquant = ((str - hps) / 2);
		set_npc_prop(HEALTH, healquant);
	}
}

void spellCloneEffect object#() () {
	var summoned = clone();
	summoned->set_alignment(event);
}

void spellFireRingEffect object#() () {
	struct<Position> pos = get_object_position();
	remove_item();
	var ring = UI_create_new_object(SHAPE_FIRE_FIELD);
	if (ring) {
		ring->set_item_flag(TEMPORARY);
		UI_update_last_created(pos);
		var delay = 31;
		delay += UI_die_roll(1, 5);
		script ring after delay ticks {
			nohalt;
			remove;
		}
	}
}

void spellSetShape object#() () {
	set_item_shape(event);
}

void spellCauseLight object#() () {
	UI_cause_light(event);
}

void spellCenteredSpriteEffect object#() () {
	struct<Position> pos = get_object_position();
	UI_sprite_effect(event, pos.x, pos.y, 0, 0, 0, -1);
}

void spellOffCenterSpriteEffect object#() () {
	struct<Position> pos = get_object_position();
	UI_sprite_effect(event, (pos.x - 2), (pos.y - 2), 0, 0, 0, -1);
}

void spellCreateFoodEffect object#() () {
	//If the caster is not in party, this was just for 'show', i.e.,
	//a usecode-schedule display.
	if (!get_item_flag(IN_PARTY)) {
		return;
	}

	var party = UI_get_party_list();

	for (npc in party) {
		struct<Position> pos = npc->get_object_position();
		var fooditem = UI_create_new_object(SHAPE_FOOD);
		if (fooditem) {
			fooditem->set_item_frame(UI_die_roll(0, 31));
			fooditem->set_item_flag(TEMPORARY);
			UI_update_last_created(pos);
		}
	}
}

void spellWizardEyeEffect object#() () {
	UI_wizard_eye(45, 200);
}

void spellShowMap object#() () {
	UI_display_map();
}


void spellProtectAllEffect object#() () {
	UI_play_sound_effect(109);
	struct<Position> pos = get_object_position();
	UI_sprite_effect(ANIMATION_TELEPORT, (pos.x - 2), (pos.y - 2), 0, 0, 0, -1);
	var targets = getFriendlyTargetList(item, 25);
	for (npc in targets) {
		npc->set_item_flag(PROTECTION);
	}
}

void spellSwarmEffect object#() () {
	var rand = UI_die_roll(7, 10);
	while (rand > 0) {
		rand -= 1;
		UI_summon(SHAPE_FLY, false);
	}
}

void spellConjureEffect object#() () {
	var conjurables = [SHAPE_BIRD, SHAPE_RABBIT, SHAPE_RAT, SHAPE_FOX, SHAPE_SNAKE, SHAPE_DEER, SHAPE_WOLF];
	var arraysize = UI_get_array_size(conjurables);
	var npclevel = getNPCLevel(item);
	if (npclevel > arraysize) {
		npclevel = arraysize;
	}
	if (npclevel < 2) {
		npclevel = 2;
	}
	var minroll = (npclevel / 2);
	if (minroll < 1) {
		minroll = 1;
	}
	var rand = UI_die_roll(minroll, npclevel);

	while (rand > 0) {
		rand -= 1;
		var rand2 = UI_die_roll(minroll, npclevel);
		var summoned = conjurables[rand2]->summon(false);
		summoned->set_alignment(get_alignment());
	}
}

void spellEndSeance object#() () {
	//I have NO idea why they had one flag per ghost,
	//instead of having one for them all...
	gflags[SEANCE_CAINE] = false;
	gflags[SEANCE_FERRYMAN] = false;
	gflags[SEANCE_MARKHAM] = false;
	gflags[SEANCE_HORANCE] = false;
	gflags[SEANCE_TRENT] = false;
	gflags[SEANCE_MORDRA] = false;
	gflags[SEANCE_ROWENA] = false;
	gflags[SEANCE_PAULETTE] = false;
	gflags[SEANCE_QUENTON] = false;
	gflags[SEANCE_FORSYTHE] = false;
}

void spellCauseDancing object#() () {
	var dist = 25;
	var nonparty_npcs = getNearbyNonPartyNPCs(dist);
	for (npc in nonparty_npcs) {
		var intelligence = npc->get_npc_prop(INTELLIGENCE);
		if ((intelligence > 5) && (intelligence < 25)) {
			struct<Position> pos = npc->get_object_position();
			UI_sprite_effect(ANIMATION_SPARKLES, pos.x, pos.y, 0, 0, 0, -1);
			setNonpartySchedule(npc, DANCE);
			npc->set_item_flag(IN_ACTION);
			var barks = ["@Dance!@", "@Yeah!@", "@Huh!@", "@Oh, yeah!@", "@I'm bad!@", "@Boogie!@", "@Yow!@"];
			var rand = UI_die_roll(1, 7);
			var delay = UI_die_roll(10, 40);
			delayedBark(npc, barks[rand], delay);
			delay = UI_die_roll(50, 75);
			script npc after delay ticks {
				nohalt;
				call stopDancing;
			}
		}
	}
}

void spellSummonSkeletonsEffect object#() () {
	var npclevel = getNPCLevel(item);
	var minroll = npclevel / 2;
	if (npclevel < 2) {
		npclevel = 2;
		minroll = 1;
	}
	var rand = UI_die_roll(minroll, npclevel);
	while (rand) {
		var summoned = SHAPE_SKELETON->summon(true);
		summoned->set_alignment(get_alignment());
		rand -= 1;
	}
}

void spellCauseFearEffect object#() () {
	var targets = getEnemyTargetList(item, 25);
	for (npc in targets) {
		if (npc->get_npc_prop(INTELLIGENCE) > 5) {
			npc->set_schedule_type(IN_COMBAT);
			npc->set_attack_mode(FLEE);
			npc->set_oppressor(item);
		}
	}
}

void spellMagicStormEffect object#() () {
	if (gflags[MAGIC_STORM_SPELL] == true) {
		var targets = getEnemyTargetList(item, 45);
		var npc = targets[UI_get_random(UI_get_array_size(targets))];
		if (npc) {
			script npc {
				nohalt;
				call callLightning;
			}
		}
		var rand = UI_die_roll(3, 8);
		script item after rand ticks {
			nohalt;
			call spellMagicStormEffect;
		}
	}
}

void spellTremorEffect object#() () {
	var targets = getEnemyTargetList(item, 40);
	var duration = 12;
	for (npc in targets) {
		var counter = 0;
		var usecodearray = [];
		var directions = [NORTH, EAST, SOUTH, WEST];
		while (counter < duration) {
			var rand = UI_die_roll(0, 8);

			if (rand == 0) {
				usecodearray << {
					actor frame kneeling;
					actor frame bowing;
					actor frame standing;
				};
			} else if (rand == 1) {
				usecodearray << {
					actor frame kneeling;
					actor frame standing;
					actor frame standing;
				};
			} else if (rand == 2) {
				usecodearray << {
					actor frame bowing;
					actor frame sleeping;
					actor frame standing;
				};
			} else if (rand == 3) {
				usecodearray << {
					actor frame standing;
					actor frame standing;
					actor frame standing;
				};
			} else if (rand == 4) {
				usecodearray << {
					actor frame kneeling;
					actor frame ready;
					actor frame standing;
				};
			} else if (rand == 5) {
				usecodearray << {
					actor frame ready;
					actor frame kneeling;
					actor frame standing;
				};
			} else if (rand == 6) {
				usecodearray << {
					face directions[UI_get_random(4)];
					actor frame bowing;
					actor frame standing;
				};
			} else if (rand == 7) {
				usecodearray << {
					face directions[UI_get_random(4)];
					actor frame kneeling;
					actor frame standing;
				};
			} else if (rand == 8) {
				usecodearray << {
					face directions[UI_get_random(4)];
					actor frame ready;
					actor frame standing;
				};
			}
			counter += 1;
		}
		npc->halt_scheduled();
		npc->run_script(usecodearray);
	}
	UI_earthquake((duration * 3));
}

void spellCauseDeath object#() () {
	var cantdie = get_item_flag(CANT_DIE) || get_item_flag(DEATH_PROTECTION);
	if (!cantdie) {
		var hps = get_npc_prop(HEALTH);
		hurtNPC(item, (hps - 2));
		hurtNPC(item, 50);
	}
}

void spellSummonEffect object#() () {
	var summonables = [
		SHAPE_SKELETON, SHAPE_DRAGON, SHAPE_GHOST3, SHAPE_MONSTER_TROLL,
		SHAPE_MONSTER_MAGE, SHAPE_MONSTER_CYCLOPS, SHAPE_DRAKE,
		SHAPE_MONSTER_GARGOYLE, SHAPE_GARGOYLE_WINGED, SHAPE_HEADLESS,
		SHAPE_LICHE, SHAPE_MONGBAT, SHAPE_SCORPION
	];
	var max_rand2 = [5, 15, 5, 5, 5, 5, 5, 5, 14, 5, 10, 5, 5];
	var creature_count = [5, 1, 5, 1, 2, 2, 2, 1, 1, 3, 1, 5, 2];
	var array_size = UI_get_array_size(summonables);

	var rand1 = UI_get_random(array_size);
	var rand2 = UI_die_roll(1, 100);
	while (max_rand2[rand1] < rand2) {
		rand1 = UI_get_random(array_size);
		rand2 = UI_die_roll(1, 100);
	}

	var counter = creature_count[rand1];
	var randmax = (counter / 2);
	if (randmax < 1) {
		randmax = 1;
	}

	var rand3 = UI_die_roll(1, randmax);
	if (UI_die_roll(1, 2) == 1) {
		rand3 = (-1 * rand3);
	}

	counter += rand3;
	while (counter) {
		var summoned = summonables[rand1]->summon(true);
		summoned->set_alignment(get_alignment());
		counter -= 1;
	}
}

void spellStopTime object#() () {
	UI_stop_time(event);
}


void spellMassResurrectEffect object#() () {
	var bodyshapes = [SHAPE_BODIES_1, SHAPE_BODIES_2, SHAPE_LARGE_BODIES, SHAPE_NEW_BODIES];
	var bodies = [];
	for (shnum in bodyshapes) {
		bodies = [bodies, find_nearby(shnum, 25, MASK_NONE)];
	}

	var body;
	var xoff = [0, 1, 2, 1, 0, -1, -2, -1];
	var yoff = [2, 1, 0, -1, -2, -1, 0, 1];
	for (body in bodies) {
		var qual = body->get_item_quality();
		var quant = body->get_item_quantity(1);
		if ((qual != 0) || (quant != 0)) {
			struct<Position> pos = get_object_position();
			UI_sprite_effect(ANIMATION_LIGHTNING, pos.x, pos.y, 0, 0, 0, -1);
			UI_sprite_effect(ANIMATION_GREEN_BUBBLES, (pos.x - 2), (pos.y - 2), 0, 0, 0, -1);
			var dist = get_distance(body);
			script body after 2+dist/3 ticks resurrect;
		}
	}
}
