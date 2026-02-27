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
 *	This source file contains functions used by NPCs for spellcasting.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

enum cast_spell_results {
	CASTING_SUCCESSFUL				= 0,
	NOT_ENOUGH_REAGENTS				= 1,
	NOT_ENOUGH_MANA					= 2,
	NO_SUCH_SPELL					= 3
};

enum reagents {
	BLACK_PEARL						= 1,
	BLOOD_MOSS						= 2,
	NIGHTSHADE						= 3,
	MANDRAKE_ROOT					= 4,
	GARLIC							= 5,
	GINSENG							= 6,
	SPIDER_SILK						= 7,
	SULPHUROUS_ASH					= 8

};

var isTargetedSpell(var circle, var spell) {
	var targeted_spells;
	switch (circle) {
		case 0:
			targeted_spells = [
				SPELL_AWAKEN, SPELL_DOUSE, SPELL_IGNITE, SPELL_DETECT_CHARGES
			];
			break;
		case 1:
			targeted_spells = [SPELL_CURE];
			break;
		case 2:
			targeted_spells = [
				SPELL_DESTROY_TRAP, SPELL_ENCHANT, SPELL_FIRE_BLAST,
				SPELL_PROTECTION, SPELL_TELEKINESIS
			];
			break;
		case 3:
			targeted_spells = [
				SPELL_CURSE, SPELL_HEAL, SPELL_PARALYZE, SPELL_POISON,
				SPELL_SLEEP, SPELL_REMOVE_CURSE
			];
			break;
		case 4:
			targeted_spells = [
				SPELL_LIGHTNING, SPELL_UNLOCK_MAGIC, SPELL_RECHARGE_MAGIC,
				SPELL_BLINK
			];
			break;
		case 5:
			targeted_spells = [
				SPELL_CHARM, SPELL_DISPEL_FIELD, SPELL_EXPLOSION,
				SPELL_FIRE_FIELD, SPELL_GREAT_HEAL, SPELL_INVISIBILITY
			];
			break;
		case 6:
			targeted_spells = [
				SPELL_CLONE, SPELL_FIRE_RING, SPELL_POISON_FIELD,
				SPELL_SLEEP_FIELD
			];
			break;
		case 7:
			targeted_spells = [
				SPELL_CREATE_GOLD, SPELL_DEATH_BOLT, SPELL_DELAYED_BLAST,
				SPELL_ENERGY_FIELD, SPELL_ENERGY_MIST
			];
			break;
		case 8:
			targeted_spells = [
				SPELL_DEATH_VORTEX, SPELL_RESURRECT, SPELL_SWORDSTRIKE
			];
			break;
	}
	return (spell in targeted_spells);
}

var getSpellBark(var circle, var spell) {
	switch (circle) {
		case 0:
			switch (spell) {
				case SPELL_AWAKEN:
					return "@An Zu@";
				case SPELL_DOUSE:
					return "@An Flam@";
				case SPELL_FIREWORKS:
					return "@Bet Ort@";
				case SPELL_GLIMMER:
					return "@Bet Lor@";
				case SPELL_IGNITE:
					return "@In Flam@";
				case SPELL_THUNDER:
					return "@Vas Kal@";
				case SPELL_WEATHER:
					return "@Rel Hur@";
				case SPELL_DETECT_CHARGES:
					return "@Wis Ort@";
			}
		case 1:
			switch (spell) {
				case SPELL_AWAKEN_ALL:
					return "@Vas An Zu@";
				case SPELL_CREATE_FOOD:
					return "@In Mani Ylem@";
				case SPELL_CURE:
					return "@An Nox@";
				case SPELL_DETECT_TRAP:
					return "@Wis Jux@";
				case SPELL_GREAT_DOUSE:
					return "@Vas An Flam@";
				case SPELL_GREAT_IGNITE:
					return "@Vas In Flam@";
				case SPELL_LIGHT:
					return "@In Lor@";
				case SPELL_LOCATE:
					return "@In Wis@";
				case SPELL_TRANSLATE:
					return "@Rel Wis@";
			}
		case 2:
			switch (spell) {
				case SPELL_DESTROY_TRAP:
					return "@An Jux@";
				case SPELL_ENCHANT:
					return "@Ort Ylem@";
				case SPELL_FIRE_BLAST:
					return "@Vas Flam@";
				case SPELL_GREAT_LIGHT:
					return "@Vas Lor@";
				case SPELL_MASS_CURE:
					return "@Vas An Nox@";
				case SPELL_PROTECTION:
					return "@Uus Sanct@";
				case SPELL_TELEKINESIS:
					return "@Ort Por Ylem@";
				case SPELL_WIZARD_EYE:
					return "@Por Ort Wis@";
			}
		case 3:
			switch (spell) {
				case SPELL_CURSE:
					return "@Des Sanct@";
				case SPELL_HEAL:
					return "@Mani@";
				case SPELL_PARALYZE:
					return "@An Por@";
				case SPELL_PEER:
					return "@Vas Wis@";
				case SPELL_POISON:
					return "@In Nox@";
				case SPELL_PROTECT_ALL:
					return "@Vas Uus Sanct@";
				case SPELL_SLEEP:
					return "@In Zu@";
				case SPELL_SWARM:
					return "@Kal Bet Xen@";
				case SPELL_REMOVE_CURSE:
					return "@An Des Sanct@";
			}
		case 4:
			switch (spell) {
				case SPELL_CONJURE:
					return "@Kal Xen@";
				case SPELL_LIGHTNING:
					return "@Ort Grav@";
				case SPELL_MASS_CURSE:
					return "@Vas Des Sanct@";
				case SPELL_REVEAL:
					return "@Wis Quas@";
				case SPELL_SEANCE:
					return "@Kal Wis Corp@";
				case SPELL_UNLOCK_MAGIC:
					return "@Ex Por@";
				case SPELL_RECHARGE_MAGIC:
					return "@Uus Ort@";
				case SPELL_BLINK:
					return "@Rel Por@";
			}
		case 5:
			switch (spell) {
				case SPELL_CHARM:
					return "@An Xen Ex@";
				case SPELL_DANCE:
					return "@Por Xen@";
				case SPELL_DISPEL_FIELD:
					return "@An Grav@";
				case SPELL_EXPLOSION:
					return "@Vas Flam Hur@";
				case SPELL_FIRE_FIELD:
					return "@In Flam Grav@";
				case SPELL_GREAT_HEAL:
					return "@Vas Mani@";
				case SPELL_INVISIBILITY:
					return "@Sanct Lor@";
				case SPELL_MASS_SLEEP:
					return "@Vas Zu@";
				case SPELL_SUMMON_SKELETONS:
					return "@Kal Corp Xen@";
			}
		case 6:
			switch (spell) {
				case SPELL_CAUSE_FEAR:
					return "@Quas Wis@";
				case SPELL_CLONE:
					return "@In Quas Xen@";
				case SPELL_FIRE_RING:
					return "@Kal Flam Grav@";
				case SPELL_FLAME_STRIKE:
					return "@Vas In Flam Grav@";
				case SPELL_MAGIC_STORM:
					return "@Vas Oort Hur@";
				case SPELL_POISON_FIELD:
					return "@In Nox Grav@";
				case SPELL_SLEEP_FIELD:
					return "@In Zu Grav@";
				case SPELL_TREMOR:
					return "@Vas Por Ylem@";
			}
		case 7:
			switch (spell) {
				case SPELL_CREATE_GOLD:
					return "@Rel Ylem@";
				case SPELL_DEATH_BOLT:
					return "@Corp Por@";
				case SPELL_DELAYED_BLAST:
					return "@Tym Vas Flam@";
				case SPELL_ENERGY_FIELD:
					return "@In Sanct Grav@";
				case SPELL_ENERGY_MIST:
					return "@In Hur Grav Ylem@";
				case SPELL_MASS_CHARM:
					return "@Vas An Xen Ex@";
				case SPELL_MASS_MIGHT:
					return "@In Vas Por@";
				case SPELL_RESTORATION:
					return "@Vas Mani Hur@";
				case SPELL_MASS_DISPEL_FIELD:
					return "@Vas An Grav@";
			}
		case 8:
			switch (spell) {
				case SPELL_DEATH_VORTEX:
					return "@Vas Corp Hur@";
				case SPELL_INVISIBILITY_ALL:
					return "@Vas Sact Lor@";
				case SPELL_MASS_DEATH:
					return "@Vas Corp@";
				case SPELL_RESURRECT:
					return "@In Mani Corp@";
				case SPELL_SUMMON:
					return "@Kal Vas Xen@";
				case SPELL_SWORDSTRIKE:
					return "@In Jux Por Ylem@";
				case SPELL_TIME_STOP:
					return "@An Tym@";
				case SPELL_MASS_RESURRECT:
					return "@Vas Mani Corp Hur@";
			}
	}
	return "No such spell";
}

var getSpellRitual(var circle, var spell, var npc, var target) {
	var scr = new script {
		nohalt;
		say getSpellBark(circle, spell);
	};
	if (isTargetedSpell(circle, spell)) {
		scr << {
			face npc->direction_from(target);
		};
	}

	switch (circle) {
		case 0:
			switch (spell) {
				case SPELL_AWAKEN:
				case SPELL_DOUSE:
				case SPELL_GLIMMER:
				case SPELL_WEATHER:
					scr << {
						actor frame raise_1h;
						actor frame strike_1h;
					};
					break;
				case SPELL_FIREWORKS:
					scr << {
						actor frame cast_out;
						actor frame cast_up;
					};
					break;
				case SPELL_IGNITE:
					scr << {
						actor frame cast_up;
						actor frame cast_out;
								actor frame strike_2h;
							};
					break;
				case SPELL_THUNDER:
					scr << {
						actor frame reach_2h;
						actor frame strike_2h;
					};
					break;
				case SPELL_DETECT_CHARGES:
					scr << {
						actor frame cast_up;
						actor frame cast_out;
						actor frame ready;
					};
					break;
			}
			break;
		case 1:
			switch (spell) {
				case SPELL_AWAKEN_ALL:
				case SPELL_DETECT_TRAP:
				case SPELL_LIGHT:
					scr << {
						actor frame raise_1h;
						actor frame strike_1h;
					};
					break;
				case SPELL_CREATE_FOOD:
				case SPELL_CURE:
				case SPELL_GREAT_DOUSE:
					scr << {
						actor frame reach_1h;
						actor frame raise_1h;
						actor frame strike_1h;
					};
					break;
				case SPELL_GREAT_IGNITE:
					scr << {
						actor frame raise_1h;
						actor frame reach_1h;
						actor frame strike_1h;
					};
					break;
				case SPELL_LOCATE:
					scr << {
						actor frame kneeling;
						actor frame standing;
						actor frame cast_up;
					};
					break;
				case SPELL_TRANSLATE:
					scr << {
						actor frame raise_1h;
						actor frame strike_1h;
						actor frame strike_1h;
					};
					break;
			}
			break;
		case 2:
			switch (spell) {
				case SPELL_DESTROY_TRAP:
					scr << {
						actor frame cast_up;
						actor frame cast_out;
						actor frame strike_2h;
					};
					break;
				case SPELL_ENCHANT:
				case SPELL_MASS_CURE:
					scr << {
						actor frame reach_1h;
						actor frame raise_1h;
						actor frame strike_1h;
					};
					break;
				case SPELL_FIRE_BLAST:
					scr << {
						actor frame cast_up;
						actor frame cast_out;
						actor frame strike_2h;
						actor frame strike_2h;
					};
					break;
				case SPELL_GREAT_LIGHT:
				case SPELL_TELEKINESIS:
					scr << {
						actor frame raise_1h;
						actor frame strike_1h;
					};
					break;
				case SPELL_PROTECTION:
					scr << {
						actor frame cast_up;
						actor frame raise_1h;
						actor frame strike_2h;
					};
					break;
				case SPELL_WIZARD_EYE:
					scr << {
						actor frame cast_up;
						actor frame cast_out;
						actor frame cast_up;
						actor frame strike_2h;
					};
					break;
			}
			break;
		case 3:
			switch (spell) {
				case SPELL_CURSE:
				case SPELL_REMOVE_CURSE:
					scr << {
						actor frame raise_1h;
						actor frame cast_out;
						actor frame strike_2h;
					};
					break;
				case SPELL_HEAL:
					scr << {
						actor frame kneeling;
						actor frame standing;
						actor frame reach_1h;
						actor frame raise_1h;
						actor frame strike_1h;
					};
					break;
				case SPELL_PARALYZE:
				case SPELL_POISON:
				case SPELL_SLEEP:
					scr << {
						actor frame raise_1h;
						actor frame strike_1h;
						actor frame strike_1h;
					};
					break;
				case SPELL_PEER:
					scr << {
						actor frame cast_up;
						actor frame cast_out;
						actor frame cast_up;
					};
					break;
				case SPELL_PROTECT_ALL:
					scr << {
						actor frame cast_up;
						actor frame cast_out;
						actor frame cast_up;
						actor frame strike_2h;
					};
					break;
				case SPELL_SWARM:
					scr << {
						actor frame cast_up;
						actor frame cast_out;
						actor frame strike_2h;
					};
					break;
			}
			break;
		case 4:
			switch (spell) {
				case SPELL_CONJURE:
				case SPELL_REVEAL:
					scr << {
						actor frame raise_1h;
						actor frame reach_1h;
						actor frame strike_1h;
					};
					break;
				case SPELL_LIGHTNING:
					scr << {
						actor frame raise_1h;
						actor frame cast_up;
						actor frame cast_out;
						actor frame strike_2h;
						actor frame strike_2h;
					};
					break;
				case SPELL_MASS_CURSE:
					scr << {
						actor frame cast_up;
						actor frame cast_out;
						actor frame cast_up;
						actor frame strike_2h;
					};
					break;
				case SPELL_SEANCE:
				case SPELL_UNLOCK_MAGIC:
					scr << {
						actor frame reach_1h;
						actor frame raise_1h;
						actor frame strike_1h;
					};
					break;
				case SPELL_RECHARGE_MAGIC:
					scr << {
						actor frame cast_out;
						actor frame cast_up;
						actor frame ready;
					};
					break;
				case SPELL_BLINK:
					scr << {
						actor frame raise_1h;
						actor frame reach_2h;
						actor frame raise_1h;
					};
					break;
			}
			break;
		case 5:
			switch (spell) {
				case SPELL_CHARM:
				case SPELL_DANCE:
					scr << {
						actor frame raise_1h;
						actor frame cast_out;
						actor frame strike_2h;
					};
					break;
				case SPELL_DISPEL_FIELD:
				case SPELL_INVISIBILITY:
					scr << {
						actor frame cast_up;
						actor frame cast_out;
						actor frame strike_2h;
					};
					break;
				case SPELL_EXPLOSION:
					scr << {
						actor frame cast_up;
						actor frame cast_out;
						actor frame strike_1h;
						actor frame strike_1h;
					};
					break;
				case SPELL_FIRE_FIELD:
					scr << {
						actor frame raise_1h;
						actor frame reach_1h;
						actor frame strike_1h;
					};
					break;
				case SPELL_GREAT_HEAL:
					scr << {
						actor frame reach_1h;
						actor frame raise_1h;
						actor frame strike_1h;
					};
					break;
				case SPELL_MASS_SLEEP:
					scr << {
						actor frame raise_1h;
						actor frame cast_up;
						actor frame strike_1h;
						actor frame strike_2h;
					};
					break;
				case SPELL_SUMMON_SKELETONS:
					scr << {
						actor frame kneeling;
						actor frame standing;
						actor frame cast_up;
						actor frame cast_out;
					};
					break;
			}
			break;
		case 6:
			switch (spell) {
				case SPELL_CAUSE_FEAR:
					scr << {
						actor frame kneeling;
						actor frame standing;
						actor frame reach_1h;
						actor frame raise_1h;
						actor frame strike_1h;
					};
					break;
				case SPELL_CLONE:
					scr << {
						actor frame raise_1h;
						actor frame cast_out;
						actor frame strike_2h;
					};
					break;
				case SPELL_FIRE_RING:
					scr << {
						actor frame reach_1h;
						actor frame strike_1h;
						actor frame raise_1h;
					};
					break;
				case SPELL_FLAME_STRIKE:
					scr << {
						actor frame bowing;
						actor frame kneeling;
						actor frame bowing;
						actor frame cast_out;
						actor frame strike_2h;
					};
					break;
				case SPELL_MAGIC_STORM:
					scr << {
						actor frame cast_up;
						actor frame cast_out;
						actor frame cast_up;
						actor frame strike_2h;
					};
					break;
				case SPELL_POISON_FIELD:
				case SPELL_SLEEP_FIELD:
					scr << {
						actor frame raise_1h;
						actor frame reach_1h;
						actor frame strike_1h;
					};
					break;
				case SPELL_TREMOR:
					scr << {
						actor frame raise_2h;
						actor frame standing;
						actor frame kneeling;
					};
					break;
			}
			break;
		case 7:
			switch (spell) {
				case SPELL_CREATE_GOLD:
					scr << {
						actor frame raise_1h;
						actor frame strike_1h;
					};
					break;
				case SPELL_DEATH_BOLT:
				case SPELL_ENERGY_MIST:
				case SPELL_MASS_CHARM:
				case SPELL_MASS_MIGHT:
					scr << {
						actor frame cast_up;
						actor frame cast_out;
						actor frame cast_up;
						actor frame strike_2h;
					};
					break;
				case SPELL_DELAYED_BLAST:
					scr << {
						actor frame strike_2h;
						actor frame cast_out;
						actor frame cast_up;
					};
					break;
				case SPELL_ENERGY_FIELD:
					scr << {
						actor frame raise_1h;
						actor frame reach_1h;
						actor frame strike_1h;
					};
					break;
				case SPELL_RESTORATION:
					scr << {
						actor frame raise_1h;
						actor frame cast_out;
						actor frame strike_2h;
					};
					break;
				case SPELL_MASS_DISPEL_FIELD:
					scr << {
						actor frame cast_up;
						actor frame cast_out;
						actor frame strike_2h;
					};
					break;
			}
			break;
		case 8:
			switch (spell) {
				case SPELL_DEATH_VORTEX:
					scr << {
						actor frame cast_up;
						actor frame strike_1h;
					};
					break;
				case SPELL_INVISIBILITY_ALL:
					scr << {
						actor frame raise_1h;
						actor frame standing;
						actor frame cast_up;
						actor frame standing;
						actor frame strike_2h;
					};
					break;
				case SPELL_MASS_DEATH:
					scr << {
						actor frame kneeling;
						actor frame standing;
						actor frame cast_up;
						actor frame cast_out;
					};
					break;
				case SPELL_RESURRECT:
				case SPELL_MASS_RESURRECT:
					scr << {
						actor frame kneeling;
						actor frame standing;
						actor frame cast_up;
					};
					break;
				case SPELL_SUMMON:
					scr << {
						actor frame kneeling;
						actor frame standing;
						actor frame cast_up;
						actor frame cast_out;
						actor frame strike_2h;
					};
					break;
				case SPELL_SWORDSTRIKE:
					scr << {
						actor frame raise_1h;
						actor frame standing;
						actor frame cast_up;
						actor frame standing;
						actor frame strike_1h;
						actor frame strike_2h;
					};
					break;
				case SPELL_TIME_STOP:
					scr << {
						actor frame strike_2h;
						actor frame cast_out;
					};
					break;
			}
			break;
	 }
	return scr;
}

var getSpellList(var circle) {
	var spell_list;

	switch (circle) {
		case 0:
			spell_list = [
				"Awaken", "Douse", "Fireworks", "Glimmer",
				"Ignite", "Thunder", "Weather", "Detect charges"
			];
			break;
		case 1:
			spell_list = [
				"Awaken all", "Create food", "Cure", "Detect trap",
				"Great douse", "Great ignite", "Light", "Locate",
				"Translate"
			];
			break;
		case 2:
			spell_list = [
				"Destroy trap", "Enchant", "Fire blast", "Great light",
				"Mass cure", "Protection", "Telekinesis", "Wizard eye"
			];
			break;
		case 3:
			spell_list = [
				"Curse", "Heal", "Paralyze", "Peer",
				"Poison", "Protect all", "Sleep", "Swarm",
				"Remove Curse"
			];
			break;
		case 4:
			spell_list = [
				"Conjure", "Lightning", "Mass curse", "Reveal",
				"Seance", "Unlock magic", "Recharge Magic", "Blink"
			];
			break;
		case 5:
			spell_list = [
				"Charm", "Dance", "Dispel field", "Explosion",
				"Fire field", "Great heal", "Invisibility", "Mass sleep",
				"Summon Skeletons"
			];
			break;
		case 6:
			spell_list = [
				"Cause fear", "Clone", "Fire ring", "Flame strike",
				"Magic storm", "Poison field", "Sleep field", "Tremor"
			];
			break;
		case 7:
			spell_list = [
				"Create gold", "Death bolt", "Delayed blast", "Energy field",
				"Energy mist", "Mass charm", "Mass might", "Restoration",
				"Mass Dispel Field"
			];
			break;
		case 8:
			spell_list = [
				"Death vortex", "Invisibility all", "Mass death", "Resurrect",
				"Summon", "Swordstrike", "Time stop", "Mass resurrect"
			];
			break;
	}
	return ["none", spell_list];
}

var getSpellReagents (var circle, var spell) {
	switch (circle) {
		case 0:
			return 0;
		case 1:
			switch (spell) {
				case SPELL_AWAKEN_ALL:
				case SPELL_CURE:
					return [GARLIC, GINSENG];
				case SPELL_CREATE_FOOD:
					return [MANDRAKE_ROOT, GARLIC, GINSENG];
				case SPELL_DETECT_TRAP:
					return [NIGHTSHADE, SPIDER_SILK];
				case SPELL_GREAT_DOUSE:
					return [GARLIC, SPIDER_SILK];
				case SPELL_GREAT_IGNITE:
					return [SPIDER_SILK, SULPHUROUS_ASH];
				case SPELL_LIGHT:
					return [SULPHUROUS_ASH];
				case SPELL_LOCATE:
				case SPELL_TRANSLATE:
					return [NIGHTSHADE];
			}
		case 2:
			switch (spell) {
				case SPELL_DESTROY_TRAP:
					return [BLOOD_MOSS, SULPHUROUS_ASH];
				case SPELL_ENCHANT:
					return [BLACK_PEARL, MANDRAKE_ROOT];
				case SPELL_FIRE_BLAST:
					return [BLACK_PEARL, SULPHUROUS_ASH];
				case SPELL_GREAT_LIGHT:
					return [MANDRAKE_ROOT, SULPHUROUS_ASH];
				case SPELL_MASS_CURE:
					return [MANDRAKE_ROOT, GARLIC, GINSENG];
				case SPELL_PROTECTION:
					return [GARLIC, GINSENG, SULPHUROUS_ASH];
				case SPELL_TELEKINESIS:
					return [BLACK_PEARL, BLOOD_MOSS, MANDRAKE_ROOT];
				case SPELL_WIZARD_EYE:
					return [
						BLACK_PEARL, BLOOD_MOSS, NIGHTSHADE, MANDRAKE_ROOT,
						SPIDER_SILK, SULPHUROUS_ASH
					];
			}
		case 3:
			switch (spell) {
				case SPELL_CURSE:
					return [NIGHTSHADE, GARLIC, SULPHUROUS_ASH];
				case SPELL_HEAL:
					return [GARLIC, GINSENG, SPIDER_SILK];
				case SPELL_PARALYZE:
					return [NIGHTSHADE, SPIDER_SILK];
				case SPELL_PEER:
					return [NIGHTSHADE, MANDRAKE_ROOT];
				case SPELL_POISON:
					return [BLACK_PEARL, BLOOD_MOSS, NIGHTSHADE];
				case SPELL_PROTECT_ALL:
					return [MANDRAKE_ROOT, GARLIC, GINSENG, SULPHUROUS_ASH];
				case SPELL_SLEEP:
					return [BLACK_PEARL, NIGHTSHADE, SPIDER_SILK];
				case SPELL_SWARM:
					return [BLOOD_MOSS, NIGHTSHADE, MANDRAKE_ROOT];
				case SPELL_REMOVE_CURSE:
					return [GARLIC, GINSENG, SULPHUROUS_ASH];
			}
		case 4:
			switch (spell) {
				case SPELL_CONJURE:
					return [MANDRAKE_ROOT, SPIDER_SILK];
				case SPELL_LIGHTNING:
					return [BLACK_PEARL, MANDRAKE_ROOT, SULPHUROUS_ASH];
				case SPELL_MASS_CURSE:
					return [NIGHTSHADE, MANDRAKE_ROOT, GARLIC, SULPHUROUS_ASH];
				case SPELL_REVEAL:
				case SPELL_UNLOCK_MAGIC:
					return [BLOOD_MOSS, SULPHUROUS_ASH];
				case SPELL_SEANCE:
					return [
						BLOOD_MOSS, NIGHTSHADE, MANDRAKE_ROOT,
						SPIDER_SILK, SULPHUROUS_ASH
					];
				case SPELL_RECHARGE_MAGIC:
					return [MANDRAKE_ROOT, SPIDER_SILK, SULPHUROUS_ASH];
				case SPELL_BLINK:
					return [BLOOD_MOSS, MANDRAKE_ROOT];
			}
		case 5:
			switch (spell) {
				case SPELL_CHARM:
					return [BLACK_PEARL, NIGHTSHADE, SPIDER_SILK];
				case SPELL_DANCE:
				case SPELL_SUMMON_SKELETONS:
					return [BLOOD_MOSS, MANDRAKE_ROOT, GARLIC];
				case SPELL_DISPEL_FIELD:
					return [BLACK_PEARL, GARLIC, SPIDER_SILK, SULPHUROUS_ASH];
				case SPELL_EXPLOSION:
					return [BLACK_PEARL, BLOOD_MOSS, MANDRAKE_ROOT, SULPHUROUS_ASH];
				case SPELL_FIRE_FIELD:
					return [BLACK_PEARL, SPIDER_SILK, SULPHUROUS_ASH];
				case SPELL_GREAT_HEAL:
					return [MANDRAKE_ROOT, GARLIC, GINSENG, SPIDER_SILK];
				case SPELL_INVISIBILITY:
					return [BLOOD_MOSS, NIGHTSHADE];
				case SPELL_MASS_SLEEP:
					return [NIGHTSHADE, GINSENG, SPIDER_SILK];
			}
		case 6:
			switch (spell) {
				case SPELL_CAUSE_FEAR:
					return [NIGHTSHADE, MANDRAKE_ROOT, GARLIC];
				case SPELL_CLONE:
					return [
						BLOOD_MOSS, NIGHTSHADE, MANDRAKE_ROOT,
						GINSENG, SPIDER_SILK, SULPHUROUS_ASH
					];
				case SPELL_FIRE_RING:
					return [BLACK_PEARL, MANDRAKE_ROOT, SPIDER_SILK, SULPHUROUS_ASH];
				case SPELL_FLAME_STRIKE:
					return [BLACK_PEARL, BLOOD_MOSS, SULPHUROUS_ASH];
				case SPELL_MAGIC_STORM:
					return [BLOOD_MOSS, NIGHTSHADE, MANDRAKE_ROOT, SULPHUROUS_ASH];
				case SPELL_POISON_FIELD:
					return [BLACK_PEARL, NIGHTSHADE, SPIDER_SILK];
				case SPELL_SLEEP_FIELD:
					return [BLACK_PEARL, GINSENG, SPIDER_SILK];
				case SPELL_TREMOR:
					return [BLOOD_MOSS, MANDRAKE_ROOT, SULPHUROUS_ASH];
			}
		case 7:
			switch (spell) {
				case SPELL_CREATE_GOLD:
					return [MANDRAKE_ROOT, SPIDER_SILK];
				case SPELL_DEATH_BOLT:
					return [BLACK_PEARL, NIGHTSHADE, SULPHUROUS_ASH];
				case SPELL_DELAYED_BLAST:
					return [
						BLACK_PEARL, BLOOD_MOSS, MANDRAKE_ROOT,
						SPIDER_SILK, SULPHUROUS_ASH
					];
				case SPELL_ENERGY_FIELD:
					return [BLACK_PEARL, MANDRAKE_ROOT, SPIDER_SILK, SULPHUROUS_ASH];
				case SPELL_ENERGY_MIST:
					return [BLOOD_MOSS, NIGHTSHADE, MANDRAKE_ROOT, SULPHUROUS_ASH];
				case SPELL_MASS_CHARM:
					return [BLACK_PEARL, NIGHTSHADE, MANDRAKE_ROOT, SPIDER_SILK];
				case SPELL_MASS_MIGHT:
					return [BLACK_PEARL, MANDRAKE_ROOT, GINSENG];
				case SPELL_RESTORATION:
					return [MANDRAKE_ROOT, GARLIC, GINSENG, SULPHUROUS_ASH];
				case SPELL_MASS_DISPEL_FIELD:
					return [
						BLACK_PEARL, MANDRAKE_ROOT, GARLIC,
						SPIDER_SILK, SULPHUROUS_ASH
					];
			}
		case 8:
			switch (spell) {
				case SPELL_DEATH_VORTEX:
					return [BLOOD_MOSS, NIGHTSHADE, MANDRAKE_ROOT, SULPHUROUS_ASH];
				case SPELL_INVISIBILITY_ALL:
					return [BLACK_PEARL, BLOOD_MOSS, NIGHTSHADE, MANDRAKE_ROOT];
				case SPELL_MASS_DEATH:
					return [
						BLOOD_MOSS, NIGHTSHADE, MANDRAKE_ROOT,
						GARLIC, GINSENG
					];
				case SPELL_RESURRECT:
					return [GARLIC, GINSENG, SPIDER_SILK, SULPHUROUS_ASH];
				case SPELL_SUMMON:
					return [BLOOD_MOSS, MANDRAKE_ROOT, GARLIC];
				case SPELL_SWORDSTRIKE:
					return [BLACK_PEARL, NIGHTSHADE, MANDRAKE_ROOT];
				case SPELL_TIME_STOP:
					return [BLOOD_MOSS, MANDRAKE_ROOT, GARLIC];
				case SPELL_MASS_RESURRECT:
					return [MANDRAKE_ROOT, GARLIC, GINSENG, SPIDER_SILK, SULPHUROUS_ASH];
			}
	}
}

var getCircleList(var npc) {
	var circles;

	circles = ["Linear", "First"];

	var npclevel = getNPCLevel(npc);
	if (npclevel > 8 || npc->get_item_flag(ARCHWIZARD)) {
		npclevel = 8;
	}
	if (npc->get_item_flag(BARD_CLASS)) {
		//Effectively rounding up
		npclevel = (npclevel+1)/2;
	}

	var levels = [
		"Second", "Third", "Fourth", "Fifth",
		"Sixth", "Seventh", "Eighth"
	];
	var i = 1;
	while (i < npclevel) {
		circles << levels[i];
		i += 1;
	}

	circles = ["none", circles];
	return circles;
}

var getSpellCircle (var spellname) {
	var circle = 0;
	var circlelist;

	while (circle < 8) {
		circlelist = getSpellList(circle);
		if (spellname in circlelist) {
			break;
		}
		circle += 1;
	}
	return circle;
}

var getIndexForSpell(var circle, var spellname) {
	var spelllist = getSpellList(circle);
	for (spell in spelllist with index) {
		if (spell == spellname) {
			return index - 2;
		}
	}
}

var getLeveledSpellList (var npc, var ignore_npc_level, var spelllist, var levels, var removespells) {
	var npclevel = getNPCLevel(npc);
	if (ignore_npc_level || npclevel > 8 || npc->get_item_flag(ARCHWIZARD)) {
		npclevel = 8;
	}
	if (npc->get_item_flag(BARD_CLASS)) {
		//Effectively rounding up
		npclevel = (npclevel+1)/2;
	}

	var index = 1;
	var list = [];
	while (index <= UI_get_array_size(spelllist)) {
		if (levels[index] > npclevel) {
			break;
		}
		if (!(spelllist[index] in removespells)) {
			list = [list, spelllist[index]];
		}
		index += 1;
	}
	return list;
}

var getSpellFunction (var circle, var spell) {
	switch (circle) {
		case 0:
			return &spellAwaken + spell;
		case 1:
			return &spellAwakenAll + spell;
		case 2:
			return &spellDestroyTrap + spell;
		case 3:
			return &spellCurse + spell;
		case 4:
			return &spellConjure + spell;
		case 5:
			return &spellCharm + spell;
		case 6:
			return &spellCauseFear + spell;
		case 7:
			return &spellCreateGold + spell;
		case 8:
			return &spellDeathVortex + spell;
	}
}

var npcCastSpell (var npc, var target, var circle, var spell) {
	var party = UI_get_party_list();
	var npcmana = npc->get_npc_prop(MANA);
	var maxmana = npc->get_npc_prop(MAX_MANA);

	if (((npc in party) && !npc->get_item_flag(ARCHWIZARD) ||
		(!(npc in party) && (npc->get_schedule_type() == WAIT)))) {
		var needed = getSpellReagents(circle, spell);

		var names =		[
			"black pearl", "blood moss",  "nightshade",  "mandrake root",
			"garlic",      "ginseng",     "spider silk", "sulphurous ash"
		];
		var prefixes =	[
			"",            "portion of ", "button of ",  "",
			"clove of ",   "portion of ", "portion of ", "portion of "
		];
		var lacking_reagents = [];
		var ret_str;

		if (needed) {
			for (reagent in needed) {
				if (!npc->count_objects(SHAPE_REAGENT, QUALITY_ANY, reagent - 1)) {
					lacking_reagents = [lacking_reagents, reagent];
				}
			}
			var missing_count = UI_get_array_size(lacking_reagents);
			if (missing_count) {
				var currcount = 0;
				for (reagent in lacking_reagents) {
					currcount += 1;
					if ((currcount == missing_count) && (currcount != 1)) {
						ret_str += " and ";
					} else if (currcount != 1) {
						ret_str += ", ";
					}

					ret_str += "one " + prefixes[reagent] + names[reagent];
				}
				ret_str += ".@";
				return [NOT_ENOUGH_REAGENTS, ret_str];
			}
		}

		if (npcmana < circle) {
			return [NOT_ENOUGH_MANA, (maxmana < circle)];
		}

		if (needed) {
			for (reagent in needed) {
				npc->remove_cont_items(1, SHAPE_REAGENT, QUALITY_ANY, reagent, true);
			}
		}
	}

	npc->begin_casting_mode();

	npc->halt_scheduled();
	var spell_function = getSpellFunction(circle, spell);
	event = DOUBLECLICK;
	if (isTargetedSpell(circle, spell)) {
		if (!target) {
			target = UI_click_on_item();
		}
		npc->(*spell_function)(target);
	} else {
		npc->(*spell_function)();
	}

	//Return NPC to standing frame once spellcasting is done
	script npc after 12 ticks actor frame standing;
	if (!(npc in party) || npc->get_item_flag(ARCHWIZARD)) {
		circle = 0;
	}
	return [CASTING_SUCCESSFUL, circle];
}

void npcAskSpellToCast (var npc, var talk, var removespells) {
	UI_push_answers();

	var circle;
	var spelllist;
	var spell;
	var casting_return;
	var casting_status;
	var circle_list;

	circle_list = getCircleList(npc);

	say(talk[1]);
	while (true) {
		circle = chooseFromMenu2(circle_list) - 2;

		if (circle == -1) {
			break;
		} else {
			spelllist = removeItemsFromList(getSpellList(circle), removespells);

			say(talk[2]);
			spell = chooseFromMenu2(spelllist);
			if (spell == 1) {
				say(talk[3]);
			} else {
				//Determine the true spell index:
				spell = getIndexForSpell(circle, spelllist[spell]);
				casting_return = npcCastSpell(npc, 0, circle, spell);
				switch (casting_return[1]) {
					case CASTING_SUCCESSFUL:
						//casting_return[2] is the mana cost of the spell.
						//Only do this if the spell cost any mana at all.
						if (casting_return[2] != 0) {
							npc->set_npc_prop(MANA, -casting_return[2]);
						}
						//The casting succeeded; abort so the spell can be cast:
						abort;
					case NOT_ENOUGH_REAGENTS:
						//Not enough reagents; casting_return[2] is the string telling which
						//reagents are missing:
						say(talk[5]);
						say(talk[6], casting_return[2]);
						break;
					case NOT_ENOUGH_MANA:
						//Not enough mana; casting_return[2] tells if the mana cost is higher
						//than the npc's max mana, case in which he couldn't cast the spell
						//even after resting:
						if (casting_return[2]) {
							say(talk[7]);
						} else {
							say(talk[8]);
						}
						break;
					default:
						//This should never happen...
						say("@Alas, I am afraid that spell doesn't exist. Warn the programmer, something is amiss!@");
				}
			}
		}
	}

	say(talk[4]);
	UI_pop_answers();
}

void npcCastSpellDialog (var npc, var spell, var talk) {
	var circle = getSpellCircle(spell);

	npc.hide();
	var casting_return = npcCastSpell(npc, 0, circle, getIndexForSpell(circle, spell));

	switch(casting_return[1]) {
		case CASTING_SUCCESSFUL:
			//casting_return[2] is the mana cost of the spell.
			//Only do this if the spell cost any mana at all.
			if (casting_return[2] != 0) {
				npc->set_npc_prop(MANA, -casting_return[2]);
			}
			//The casting succeeded; abort so the spell can be cast:
			abort;
		case NOT_ENOUGH_REAGENTS:
			//Not enough reagents; casting_return[2] is the string telling which
			//reagents are missing:
			npc.say(talk[5]);
			say(talk[6], casting_return[2]);
			break;
		case NOT_ENOUGH_MANA:
			if (casting_return[2]) {
				npc.say(talk[7]);
			} else {
				npc.say(talk[8]);
			}
			break;
		default:
			//This should never happen...
			npc.say("@Alas, I am afraid that spell doesn't exist. Warn the programmer, something is amiss!@");
	}
	npc.hide();
}

void npcCastSpellBark (var npc, var target, var circle, var spell) {
	var casting_return = npcCastSpell(npc, target, circle, spell);
	var casting_result = casting_return[1];

	if (casting_result == CASTING_SUCCESSFUL) {
		//casting_return[2] is the mana cost of the spell.
		//Only do this if the spell cost any mana at all.
		if (casting_return[2] != 0) {
			npc->set_npc_prop(MANA, -casting_return[2]);
		}
		//The casting succeeded; abort so the spell can be cast:
		abort;
	} else if (casting_result == NO_SUCH_SPELL) {
		//This should never happen...
		say("@Alas, I am afraid that spell doesn't exist. Warn the programmer, something is amiss!@");
	}
}

void npcCastWeaponSpell (var npc, var target, var spellitem, var spell, var barks) {
	if (!spellitem) {
		if (npc in UI_get_party_list()) {
			npc->item_say("@That is -my- spellbook!@");
		} else {
			randomPartyMember()->item_say(barks[1]);
		}
		flashBlocked(0);
		abort;
	} else if (!spell) {
		npc->item_say(barks[2]);
		flashBlocked(0);
		abort;
	}

	var circle;
	switch (spell) {
		case "Fire blast":
			circle = 2;
			break;
		case "Lightning":
			circle = 4;
			break;
		case  "Explosion":
			circle = 5;
			break;
		case  "Death bolt":
			circle = 7;
			break;
		case  "Swordstrike":
			circle = 8;
			break;
	}

	var casting_return = npcCastSpell(npc, target, circle, getIndexForSpell(circle, spell));

	switch (casting_return[1]) {
		case CASTING_SUCCESSFUL:
			//casting_return[2] is the mana cost of the spell.
			//Only do this if the spell cost any mana at all.
			if (casting_return[2] != 0) {
				npc->set_npc_prop(MANA, -casting_return[2]);
			}

			//The casting succeeded; abort so the spell can be cast:
			abort;
		case NOT_ENOUGH_REAGENTS:
			npc->item_say("Need reagents");
			break;

		case NOT_ENOUGH_MANA:
			if (casting_return[2]) {
				npc->item_say("Can't cast");
			} else {
				npc->item_say("Must rest");
			}
			break;
		default:
			//This should never happen...
			npc->item_say("No spell");
	}
}

void npcCastHealing (var npc, var talk_cast, var talk, var healing_spells) {
	var choice;
	while (true) {
		npc.say(talk[1]);
		choice = askForResponse(["none", healing_spells]);
		if (choice != "none") {
			/*
			if ((choice == "Restoration") || (choice == "Mass cure")) {
				say(talk[2]);
			} else {
				message(talk[3]);
				if ((choice == "Heal") || (choice == "Great heal")) {
					message("healed");
				} else if (choice == "Cure") {
					message("cured of poison");
				} else {
					message("resurrected");
				}
				message("?@");
				say();
			}
			*/

			npcCastSpellDialog(npc,
				choice,
				talk_cast);
		} else {
			break;
		}
	}
}
