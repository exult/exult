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
 *	This source file contains the spellcasting pseudo-AI for NPCs.
 *	The functions here are NOT intended to be called directly from
 *	eggs or weapons or other functions for that matter. The ONLY
 *	function which should be called from "outside" is the aiMain
 *	function, in calle form (i.e., 'npc->aiMain();').
 *	or from spellitem usecode.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

/* FOR EASY REFERENCE:
	//New spell effects:
	CONFUSION						= 40,	//Under the effects of Consufion spell
	DEATH_PROTECTION				= 41,	//Protected from death spells
	//Spellcasting power:
	ARCHWIZARD						= 45,	//Cheat mode
	MAGE_CLASS						= 46,	//Full spellcasting capabilities
	BARD_CLASS						= 47,	//1/2 level spellcasting
	HEALER							= 50,	//Casts healing as if level 8, no death spells
	NECROMANCER						= 51,	//Casts death as if level 8, no healing spells
	//The next constants are used to control spellcaster AI; the AI is NOT
	//IMPLEMENTED YET.
	AI_HEALER						= 56,
	AI_KINETICIST					= 57,
	AI_DEATH_MAGE					= 58,
	AI_ENCHANTER					= 59,
	AI_WARMAGE						= 60,
	AI_CONJURER						= 61,
	AI_DIVINER						= 62,
	AI_THAUMATURGE					= 63
*/

void aiCastHealing (var allies, var eff_level, var in_party, var in_combat) {
	if (get_item_flag(HEALER)) {
		eff_level = 8;
		if (get_item_flag(BARD_CLASS)) {
			//Effectively rounding up
			eff_level = (eff_level+1)/2;
		}
	}

	//Filter list by flags and by Health:
	var poisoned = filterListByFlag(allies, POISONED, true);
	var cursed = filterListByFlag(allies, CURSED, true);
	var paralyzed = filterListByFlag(allies, PARALYZED, true);
	var asleep = filterListByFlag(allies, ASLEEP, true);
	var badlywounded = filterListByRelHits(allies, 1);
	var wounded = filterListByRelHits(allies, 2);
	var hurt = filterListByRelHits(allies, 3);
	var dead = [];
	if (in_party) {
		dead = UI_get_dead_party();
	}
	var poisoned_size = UI_get_array_size(poisoned);
	var cursed_size = UI_get_array_size(cursed);
	var paralyzed_size = UI_get_array_size(paralyzed);
	var asleep_size = UI_get_array_size(asleep);
	var badlywounded_size = UI_get_array_size(badlywounded);
	var wounded_size = UI_get_array_size(wounded);
	var hurt_size = UI_get_array_size(hurt);
	var dead_size = UI_get_array_size(dead);

	//See if we should resurrect or heal:
	if (eff_level == 8) {
		if (dead_size && !(badlywounded_size > 3 && dead_size == 1)) {
			//We should resurrect
			var target = dead[UI_get_random(dead_size)];
			npcCastSpellBark(item, target, 8, SPELL_RESURRECT);
		}
	}
	if (eff_level >= 7) {
		//See if it is worthwhile to use restoration:
		if ((badlywounded_size > 1) || (wounded_size > 3 ) || (hurt_size > 5)) {
			//It is; cast it:
			npcCastSpellBark(item, 0, 7, SPELL_RESTORATION);
		}
	}
	//Not worthwhile (or possible) to use restoration;
	//see if we should use great heal or cure:
	if (eff_level >= 5) {
		if (wounded_size &&
			(poisoned_size + paralyzed_size < 4)) {
			//It is; cast it:
			var target = wounded[UI_get_random(wounded_size)];
			npcCastSpellBark(item, target, 5, SPELL_GREAT_HEAL);
		}
	}
	//Not worthwhile (or possible) to use great heal;
	//see if we should use heal or cure:
	if (eff_level >= 3) {
		if ((wounded_size &&
			(poisoned_size + paralyzed_size < 4)) ||
			(!in_combat &&
			 (hurt_size > 1))) {
				//It is; cast it:
			var target = wounded[UI_get_random(wounded_size)];
			if (!in_combat && !target) {
				target = hurt[UI_get_random(hurt_size)];
			}
			npcCastSpellBark(item, target, 3, SPELL_HEAL);
		}
		//See if we should cast remove curse:
		if (cursed_size) {
			//Yes; cast it:
			var target = cursed[UI_get_random(cursed_size)];
			npcCastSpellBark(item, target, 3, SPELL_REMOVE_CURSE);
		}
	}
	//Not worthwhile (or possible) to use heal or
	//remove curse; see if we should use mass cure
	//or awaken all:
	if (eff_level >= 2) {
		if (poisoned_size + paralyzed_size > 2) {
			//Mass Cure it is; cast it:
			npcCastSpellBark(item, 0, 2, SPELL_MASS_CURE);
		}
		if (asleep_size > 5) {
			//Awaken All it is; cast it:
			npcCastSpellBark(item, 0, 2, SPELL_AWAKEN_ALL);
		}
	}
	if (poisoned_size || paralyzed_size) {
		//Cure it is; cast it:
		var target;
		if (poisoned_size) {
			target = poisoned[UI_get_random(poisoned_size)];
		} else {
			target = paralyzed[UI_get_random(paralyzed_size)];
		}
		npcCastSpellBark(item, target, 1, SPELL_CURE);
	}
	if (asleep_size) {
		//Awaken it is; cast it:
		var target = asleep[UI_get_random(asleep_size)];
		npcCastSpellBark(item, target, 0, SPELL_AWAKEN);
	}
}

void aiCastBuffing (var allies, var enemies, var eff_level, var in_party, var in_combat) {
	//No need to cast buffing spells if outside combat
	if (!in_combat) {
		return;
	}

	var badlywounded = [];
	var wounded = [];
	var hurt = [];

	var badlywounded_size = 0;
	var wounded_size = 0;
	var hurt_size = 0;

	var enemy_count = UI_get_array_size(enemies);

	//See if we should protect anyone:
	var unprotected = filterListByFlag(allies, PROTECTION, false);
	var unprotected_size = UI_get_array_size(unprotected);
	badlywounded = filterListByRelHits(unprotected, 1);
	badlywounded_size = UI_get_array_size(badlywounded);
	if (eff_level >= 3) {
		if ((badlywounded_size > 1) && (enemy_count > 3)) {
			npcCastSpellBark(item, 0, 3, SPELL_PROTECT_ALL);
		}
	}
	if (eff_level >= 2) {
		if ((badlywounded_size) && (enemy_count > 3)) {
			var target = badlywounded_size[UI_get_random(badlywounded_size)];
			npcCastSpellBark(item, target, 2, SPELL_PROTECTION);
		}
	}

	//See if we should make anyone invisible:
	var visible = filterListByFlag(allies, INVISIBLE, false);
	var visible_size = UI_get_array_size(visible);
	badlywounded = filterListByRelHits(visible_size, 1);
	badlywounded_size = UI_get_array_size(badlywounded);
	if (eff_level == 8) {
		if ((badlywounded_size > 3) && (enemy_count > 5)) {
			npcCastSpellBark(item, 0, 8, SPELL_INVISIBILITY_ALL);
		}
	}
	if (eff_level >= 5) {
		if ((badlywounded_size) && (enemy_count > 5)) {
			var target = badlywounded_size[UI_get_random(badlywounded_size)];
			npcCastSpellBark(item, target, 2, SPELL_PROTECTION);
		}
	}

	//See if it is worthwhile to use mass might:
	var nomassmight = filterListByFlag(allies, MIGHT, false);
	var nomassmight_size = UI_get_array_size(nomassmight);
	if (eff_level >= 7) {
		if (nomassmight && 3*UI_get_array_size(allies) < enemy_count) {
			//It is; cast it:
			npcCastSpellBark(item, 0, 7, SPELL_MASS_MIGHT);
		}
	}

	//See if anyone needs magic ammo:
	var xbowmen = filterListByEquipedObject(allies, [SHAPE_CROSSBOW, SHAPE_TRIPLE_XBOW], BG_WEAPON_HAND);
	var bowmen = filterListByEquipedObject(allies, [SHAPE_BOW, SHAPE_MAGIC_BOW], BG_WEAPON_HAND);
	xbowmen = filterListByEquipedObject(xbowmen, SHAPE_BOLTS, BG_QUIVER);
	bowmen = filterListByEquipedObject(bowmen, SHAPE_ARROWS, BG_QUIVER);
	var xbowmen_size = UI_get_array_size(xbowmen);
	var bowmen_size = UI_get_array_size(bowmen);
	if (eff_level >= 2) {
		if (xbowmen_size || bowmen_size) {
			var target;
			var obj;
			var total = xbowmen_size + bowmen_size;
			if (UI_get_random(total) <= xbowmen_size) {
				obj = xbowmen[UI_get_random(xbowmen_size)];
			} else {
				obj = bowmen[UI_get_random(bowmen_size)];
			}
			target = obj->get_readied(BG_QUIVER);
			npcCastSpellBark(item, target, 2, SPELL_ENCHANT);
		}
	}

	//See if anyone needs a recharge
	var wand_users = filterListByEquipedObject(allies,
									[SHAPE_LIGHTNING_WAND, SHAPE_FIRE_WAND, SHAPE_FIREDOOM_STAFF],
									BG_WEAPON_HAND);
	var wand_users_size = UI_get_array_size(wand_users);
	if (eff_level >= 4) {
		if (wand_users_size) {
			var target = false;
			while (wand_users_size > 0) {
				var obj = (wand_users[wand_users_size])->get_readied(BG_WEAPON_HAND);
				if (obj->get_item_quality() < 10) {
					target = obj;
					break;
				}
				wand_users_size -= 1;
			}

			if (target) {
				npcCastSpellBark(item, target, 4, SPELL_RECHARGE_MAGIC);
			}
		}
	}
}

void aiMain object#() () {
	//Queue reentry:
	script item after 10 ticks {
		nohalt;
		call aiMain;
	}

	if (get_item_flag(DEAD) || get_item_flag(ASLEEP) ||
		get_item_flag(CHARMED) || get_item_flag(CONFUSION)) {
		//NPC is dead, asleep, paralyzed, charmed or confused...
		//Will make characters more dangerous when they are
		//charmed or confused, but it is enough for now.
		abort;
	}
	//Store party flag:
	var in_party = get_npc_object() in UI_get_party_list();

	//Search for spellcasting item, if any:
	var spellitem;
	var party_spellcasters = [
		IOLO,					SHAMINO,
		DUPRE, 					MARIAH,
		JAANA,					LAURIANNA,
		JULIA
	];
	var spellitem_shapes   = [
		SHAPE_IOLOS_LUTE,		SHAPE_SPELL_AMULET,
		SHAPE_SPELL_AMULET,		SHAPE_SPELL_SPELLBOOK,
		SHAPE_SPELL_SPELLBOOK,	SHAPE_SPELL_SPELLBOOK,
		SHAPE_JULIAS_HAMMER
	];

	if (in_party && get_npc_number() in party_spellcasters) {
		var i = 1;
		var item_shape = 0;
		while (i <= UI_get_array_size(spellitem_shapes)) {
			if (get_npc_object() == party_spellcasters[i]->get_npc_object()) {
				item_shape = spellitem_shapes[i];
				break;
			}
			i += 1;
		}
		//Spellitem missing; leave immediatelly
		if (count_objects(item_shape, -get_npc_number(), FRAME_ANY) == 0) {
			abort;
		}
	}

	var in_combat = false;

	if (in_party && UI_in_combat()) {
		in_combat = true;
	} else if (get_schedule_type() == IN_COMBAT) {
		in_combat = true;
	}

	var align = get_alignment();
	var allies = [];
	var enemies = [];

	if (align == 1 || in_party) {
		allies = getFriendlyTargetList(item, 30);
		enemies = getEnemyTargetList(item, 30);
	} else if (align == 2) {
		allies = getEnemyTargetList(item, 30);
		enemies = getFriendlyTargetList(item, 30);
	} else {
		//if (align == 0)
		//No enemies?????
		allies = [item];
	}

	var level = getNPCLevel(item);
	//Cheat mode:
	if (get_item_flag(ARCHWIZARD) || level > 8) {
		level = 8;
	}
	//Bard-class cast spells at lower level, even
	//if using archwizard mode:
	if (get_item_flag(BARD_CLASS)) {
		//Effectively rounding up
		level = (level+1)/2;
	} else if (!get_item_flag(MAGE_CLASS)) {
		//If not bard- or mage-class, leave:
		//Should NEVER happen...
		abort;
	}

	var str = get_npc_prop(STRENGTH);
	var hps = get_npc_prop(HEALTH);

	//Loop variables:
	var index;
	var max;
	var npc;
	var target;

	//The flags set the order in which the NPC will cast spells:
	if (get_item_flag(AI_HEALING)) {
		aiCastHealing(allies, level, in_party, in_combat);
		aiCastBuffing(allies, enemies, level, in_party, in_combat);

	}












}
