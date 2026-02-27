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
 *	This source file contains some reimplementations of all second
 *	circle spells.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-03-19
 */

/*
	Second circle Spells

	extern void spellDestroyTrap (struct<ObjPos> target);
	extern void spellEnchant (struct<ObjPos> target);
	extern void spellFireBlast (struct<ObjPos> target);
	extern void spellGreatLight ();
	extern void spellMassCure ();
	extern void spellProtection (struct<ObjPos> target);
	extern void spellTelekinesis (struct<ObjPos> target);
	extern void spellWizardEye ();
*/

enum second_circle_spells {
	SPELL_DESTROY_TRAP				= 0,
	SPELL_ENCHANT					= 1,
	SPELL_FIRE_BLAST				= 2,
	SPELL_GREAT_LIGHT				= 3,
	SPELL_MASS_CURE					= 4,
	SPELL_PROTECTION				= 5,
	SPELL_TELEKINESIS				= 6,
	SPELL_WIZARD_EYE				= 7
};

void spellDestroyTrap (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		//struct<ObjPos> target = UI_click_on_item();
		var dir = direction_from(target);
		halt_scheduled();
		item_say("@An Jux@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				face dir;
				actor frame cast_up;
				actor frame cast_out;
				actor frame strike_2h;
			}
			var nearby_traps = target->find_nearby(SHAPE_TRAP, 2, MASK_ALL_UNSEEN);
			struct<Position>& trappos = target;
			for (trap in nearby_traps) {
				trappos = trap->get_object_position();

				trap->halt_scheduled();
				trap->remove_item();
				UI_sprite_effect(ANIMATION_GREEN_BUBBLES, trappos.x, trappos.y, 0, 0, 0, -1);
				UI_play_sound_effect(66);
			}
			nearby_traps = find_nearby(SHAPE_LOCKED_CHEST, 2, MASK_ALL_UNSEEN);
			for (trap in nearby_traps) {
				trappos = trap->get_object_position();
				if (trap->get_item_quality() == KEY_PICKABLE_TRAPPED) {
					trap->set_item_quality(0);
					UI_sprite_effect(ANIMATION_GREEN_BUBBLES, trappos.x, trappos.y, 0, 0, 0, -1);
					UI_play_sound_effect(66);
				}
			}
		} else {
			script item {
				nohalt;
				actor frame cast_up;
				actor frame cast_out;
				actor frame strike_2h;
				call spellFails;
			}
		}
	}
}

void spellEnchant (struct<ObjPos> target) {
	var normal_missiles = [SHAPE_ARROWS, SHAPE_BOLTS];
	var magic_missiles = [SHAPE_MAGIC_ARROWS, SHAPE_MAGIC_BOLTS];
	if (event == DOUBLECLICK) {
		halt_scheduled();
		//struct<ObjPos> target = UI_click_on_item();
		var target_shape = target->get_item_shape();
		var dir = direction_from(target);
		item_say("@Ort Ylem@");
		if (inMagicStorm() && (target_shape in normal_missiles)) {
			script item {
				nohalt;
				face dir;
				sfx 67;
				actor frame reach_1h;
				actor frame raise_1h;
				actor frame strike_1h;
			}
			script target after 4 ticks {
				nohalt;
				call spellEnchantEffect;
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

void spellFireBlast (struct<ObjPos> target) {
	if ((event == DOUBLECLICK) || (event == WEAPON)) {
		//struct<ObjPos> target = UI_click_on_item();
		halt_scheduled();
		var dir = direction_from(target);
		item_say("@Vas Flam@");
		if (inMagicStorm()) {
			set_to_attack(target, SHAPE_FIREBOLT);
			script item {
				nohalt;
				face dir;
				sfx 65;
				actor frame cast_up;
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
				actor frame cast_up;
				actor frame cast_out;
				actor frame strike_2h;
				call spellFails;
			}
		}
	}
}

void spellGreatLight () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Vas Lor@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				sfx 68;
				actor frame raise_1h;
				actor frame strike_1h;
				call spellCauseLight, 500;
			}
		} else {
			script item {
				nohalt;
				actor frame raise_1h;
				actor frame strike_1h;
				call spellFails;
			}
		}
	}
}

void spellMassCure () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		struct<Position> pos = get_object_position();
		UI_sprite_effect(ANIMATION_TELEPORT, pos.x, pos.y, 0, 0, 0, -1);
		item_say("@Vas An Nox@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				actor frame reach_1h;
				actor frame raise_1h;
				actor frame strike_1h;
				sfx 64;
			}
			var targets = getFriendlyTargetList(item, 25);
			for (npc in targets) {
				script npc after 6 ticks {
					nohalt;
					call spellClearFlag, POISONED;
					call spellClearFlag, PARALYZED;
				}
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

void spellProtection (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		//struct<ObjPos> target = UI_click_on_item();
		var dir = direction_from(target);
		item_say("@Uus Sanct@");
		if (inMagicStorm() && target->is_npc()) {
			script item {
				nohalt;
				face dir;
				sfx 109;
				actor frame cast_up;
				actor frame raise_1h;
				actor frame strike_2h;
			}

			script target after 5 ticks {
				nohalt;
				call spellClearFlag, PROTECTION;
			}

			obj_sprite_effect(ANIMATION_GREEN_BUBBLES, -2, -2, 0, 0, 0, -1);
		} else {
			script item {
				nohalt;
				face dir;
				actor frame cast_up;
				actor frame raise_1h;
				actor frame strike_2h;
				call spellFails;
			}
		}
	}
}

void spellTelekinesis (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		//struct<ObjPos> target = UI_click_on_item();
		if (target.obj == 0) {
			return;
		}

		var dir = direction_from(target);
		item_say("@Ort Por Ylem@");
		if (inMagicStorm() && ((!target->is_npc()) && (target.obj != 0))) {
			set_to_attack(target, SHAPE_TELEKINESIS);
			script item {
				nohalt;
				face dir;
				sfx 67;
				actor frame raise_1h;
				actor frame strike_1h;
				attack;
			}
		} else {
			script item {
				nohalt;
				face dir;
				actor frame raise_1h;
				actor frame strike_1h;
				call spellFails;
			}
		}
	} else if (event == WEAPON) {
		var unusables = [SHAPE_LOOM, SHAPE_THREAD, SHAPE_SPINNING_WHEEL, SHAPE_WOOL, SHAPE_KITE, SHAPE_BUCKET, SHAPE_BELLOWS, SHAPE_KEG, SHAPE_CASK, SHAPE_STRENGTH_TESTER, SHAPE_WELLBASE, SHAPE_WELL, SHAPE_CHAIR, SHAPE_BEDROLL, SHAPE_BED_HORIZONTAL, SHAPE_BED_VERTICAL, SHAPE_ORB];
		var usables2 = [SHAPE_WINCH_HORIZONTAL, SHAPE_WINCH_VERTICAL, SHAPE_SWITCH, SHAPE_LEVER];
		var target_shape = get_item_shape();
		if (target_shape in usables2) {
			script item
				call get_usecode_fun();
		} else if (!(target_shape in unusables)) {
			script item
				call get_usecode_fun(), DOUBLECLICK;
		}
	}
}

void spellWizardEye () {
	if (event == DOUBLECLICK) {
		item_say("@Por Ort Wis@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				actor frame cast_up;
				sfx 67;
				actor frame cast_out;
				actor frame cast_up;
				actor frame strike_2h;
				call spellWizardEyeEffect;
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
