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
 *	This source file contains some reimplementations of all first
 *	circle spells.
 *
 *	There is also a new spell in the list: 'Translate'.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-03-19
 */

/*
	First circle Spells

	extern void spellAwakenAll ();
	extern void spellCreateFood ();
	extern void spellCure (struct<ObjPos> target);
	extern void spellDetectTrap ();
	extern void spellGreatDouse ();
	extern void spellGreatIgnite ();
	extern void spellLight ();
	extern void spellLocate ();
	extern void spellTranslate ();
*/

enum first_circle_spells {
	SPELL_AWAKEN_ALL				= 0,
	SPELL_CREATE_FOOD				= 1,
	SPELL_CURE						= 2,
	SPELL_DETECT_TRAP				= 3,
	SPELL_GREAT_DOUSE				= 4,
	SPELL_GREAT_IGNITE				= 5,
	SPELL_LIGHT						= 6,
	SPELL_LOCATE					= 7,
	SPELL_TRANSLATE					= 8
};

void spellAwakenAll () {
	if (event == DOUBLECLICK) {
		struct<Position> pos = get_object_position();
		halt_scheduled();
		item_say("@Vas An Zu@");
		if (inMagicStorm()) {
			UI_sprite_effect(ANIMATION_TELEPORT, (pos.x - pos.z/2), (pos.y - pos.z/2), 0, 0, 0, -1);
			script item {
				nohalt;
				sfx 68;
				actor frame raise_1h;
				actor frame strike_1h;
			}
			var dist = 25;
			var nearby_npcs = find_nearby(-1, dist, MASK_NPC);
			for (npc in nearby_npcs) {
				script npc {
					nohalt;
					call spellAwakenEffect;
				}
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

void spellCreateFood () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@In Mani Ylem@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				sfx 68;
				actor frame reach_1h;
				actor frame raise_1h;
				actor frame strike_1h;
				call spellCreateFoodEffect;
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

void spellCure (var target) {
	if (event == DOUBLECLICK) {
		//struct<ObjPos> target = UI_click_on_item();
		halt_scheduled();
		var dir = direction_from(target);
		item_say("@An Nox@");
		if (inMagicStorm()) {
			if (target->is_npc()) {
				script item {
					nohalt;
					face dir;
					sfx 64;
					actor frame reach_1h;
					actor frame raise_1h;
					actor frame strike_1h;
				}
				script target after 6 ticks {
					nohalt;
					call spellClearFlag, POISONED;
					call spellClearFlag, PARALYZED;
				}
				return;
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

void spellDetectTrap () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Wis Jux@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				sfx 66;
				actor frame raise_1h;
				actor frame strike_1h;
			}
			var npclevel = getNPCLevel(item);
			var dist = (21 + npclevel);
			var nearby_traps = find_nearby(SHAPE_TRAP, dist, MASK_ALL_UNSEEN);
			for (trap in nearby_traps) {
				script trap after 5 ticks {
					nohalt;
					call spellCenteredSpriteEffect, 16;
				}
			}

			var openchests = find_nearby(SHAPE_CHEST, dist, MASK_ALL_UNSEEN);
			var closedchests = find_nearby(SHAPE_LOCKED_CHEST, dist, MASK_ALL_UNSEEN);
			var chests = (openchests & closedchests);
			for (trap in chests) {
				if (trap->get_item_quality() == KEY_PICKABLE_TRAPPED) {
					script trap after 5 ticks {
						nohalt;
						call spellCenteredSpriteEffect, 16;
					}
				}
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

void spellGreatDouse () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Vas An Flam@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				actor frame reach_1h;
				actor frame raise_1h;
				actor frame strike_1h;
			}

			var dousables = [SHAPE_TORCH_LIT, SHAPE_LIT_LAMP, SHAPE_LIGHTSOURCE_LIT, SHAPE_SCONCE_LIT];
			greatDouseIgnite(item, dousables);
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

void spellGreatIgnite () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Vas In Flam@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				actor frame raise_1h;
				actor frame reach_1h;
				actor frame strike_1h;
			}

			var ignitables = [SHAPE_TORCH, SHAPE_LAMPPOST, SHAPE_LIGHTSOURCE, SHAPE_SCONCE];
			greatDouseIgnite(item, ignitables);
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

void spellLight () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@In Lor@");
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

void spellLocate () {
	if (event == DOUBLECLICK) {
		item_say("@In Wis@");
		if (inMagicStorm()) {
			struct<Position> pos = get_object_position();
			var longi = ((pos.x - 0x3A5) / 10);
			var lat = ((pos.y - 0x46E) / 10);
			var longstr;
			var latstr;

			if (longi < 0) {
				longstr = " " + absoluteValueOf(longi) + " West";
			} else {
				longstr = " " + absoluteValueOf(longi) + " East";
			}
			if (lat < 0) {
				latstr = " " + absoluteValueOf(lat) + " North";
			} else {
				latstr = " " + absoluteValueOf(lat) + " South";
			}
			script item {
				nohalt;
				sfx 67;
				actor frame kneeling;
				actor frame standing;
				actor frame cast_up;
				wait 4;
				say latstr + longstr;
			}
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

void spellTranslate () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Rel Wis@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				actor frame raise_1h;
				actor frame strike_1h;
				actor frame strike_1h;
				actor frame standing;
			}
			AVATAR->set_item_flag(READ);
			script AVATAR after 10000 ticks {
				nohalt;
				finish;
				call spellClearFlag, READ;
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
