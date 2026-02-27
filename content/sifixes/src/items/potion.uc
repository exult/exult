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
 *	I did this so the companions can refuse Blue potions while on Spinebreaker
 *	Mountains. It also is a good learning exercise...
 */

extern void healByAmount 0x976 (var target, var amount);

enum potions {
	SLEEP_POTION = 0,
	HEALING_POTION = 1,
	CURING_POTION = 2,
	POISION_POTION = 3,
	AWAKENING_POTION = 4,
	PROTECTION_POTION = 5,
	LIGHT_POTION = 6,
	INVISIBILITY_POTION = 7,
	MANA_POTION = 8,
	WARMTH_POTION = 9
};

void Potion shape#(0x154) () {
	var itemframe;
	struct<ObjPos> target;

	if (event == DOUBLECLICK) {
		itemframe = get_item_frame();
		target = UI_click_on_item();

		UI_play_sound_effect2(0x24, item);
		if (target->is_npc()) {
			UI_play_sound_effect(0x39);
			if (itemframe == SLEEP_POTION) {
				var npcnum = target->get_npc_number();
				if ((getAvatarLocationID() == SPINEBREAKER_MOUNTAINS) &&
				    ((npcnum == IOLO) || (npcnum == SHAMINO) || (npcnum == DUPRE))) {
					npcnum.say("@But we are so close to Batlin now, Avatar! I don't want to miss all the action!@");
					abort;
				}
				target->set_item_flag(ASLEEP);
			} else if (itemframe == HEALING_POTION) {
				healByAmount(target, UI_die_roll(3, 12));
			} else if (itemframe == CURING_POTION) {
				target->clear_item_flag(POISONED);
				target->clear_item_flag(PARALYZED);
				target->clear_item_flag(ASLEEP);
				target->clear_item_flag(CHARMED);
				target->clear_item_flag(CURSED);
			} else if (itemframe == POISION_POTION) {
				target->set_item_flag(POISONED);
			} else if (itemframe == AWAKENING_POTION) {
				target->clear_item_flag(ASLEEP);
			} else if (itemframe == PROTECTION_POTION) {
				target->set_item_flag(PROTECTION);
			} else if (itemframe == LIGHT_POTION) {
				UI_cause_light(200);
			} else if (itemframe == INVISIBILITY_POTION) {
				target->set_item_flag(INVISIBLE);
			} else if (itemframe == MANA_POTION) {
				if (target == AVATAR->get_npc_object()) {
					var mana = AVATAR->get_npc_prop(MANA);
					var recover = UI_get_random(10);
					if ((mana + recover) > 31) {
						recover = (31 - mana);
					}
					AVATAR->set_npc_prop(MANA, recover);
				}
			} else if (itemframe == WARMTH_POTION) {
				target->set_temperature(0);
			} else if (itemframe >= 10) {
				partyUtters(1, "@What is this!@", 0, false);
				abort;
			}
			remove_item();
			abort;
		} else {
			struct<Position> pos = [target.x, target.y, target.z];
			partyUtters(1, "@Please, " + getPoliteTitle() + ", waste them not!@", 0, false);
			var spill = UI_create_new_object(SHAPE_SPILL);
			if (spill) {
				spill->set_item_flag(TEMPORARY);
				spill->set_item_frame(UI_die_roll(4, 23));
				UI_update_last_created(pos);
			}
			remove_item();
		}
	}
}
