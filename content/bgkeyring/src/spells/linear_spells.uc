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
 *	linear spells. The exception is the 'Help' spell.
 *
 *	There is also a new spell in the list: 'Detect Charges'.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

/*
	Linear Spells

	extern void spellAwaken (struct<ObjPos> target);
	extern void spellDouse (struct<ObjPos> target);
	extern void spellFireworks ();
	extern void spellGlimmer ();
	extern void spellIgnite (struct<ObjPos> target);
	extern void spellThunder ();
	extern void spellWeather ();
	extern void spellDetectCharges (struct<ObjPos> target);
*/

enum linear_spells {
	SPELL_AWAKEN					= 0,
	SPELL_DOUSE						= 1,
	SPELL_FIREWORKS					= 2,
	SPELL_GLIMMER					= 3,
	SPELL_IGNITE					= 4,
	SPELL_THUNDER					= 5,
	SPELL_WEATHER					= 6,
	SPELL_DETECT_CHARGES			= 7			//NPC-only spell
};

void spellAwaken (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		var dir = direction_from(target);
		item_say("@An Zu@");
		if (inMagicStorm() && (target.obj != 0)) {
			script item {
				nohalt;
				face dir;
				sfx 64;
				actor frame raise_1h;
				actor frame strike_1h;
			}
			script target after 5 ticks {
				nohalt;
				call spellAwakenEffect;
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
	}
}

void spellDouse (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		//struct<ObjPos> target = UI_click_on_item();
		var dir = direction_from(target);
		item_say("@An Flam@");
		if (inMagicStorm()) {
			set_to_attack(target, SHAPE_DOUSE);
			script item {
				nohalt;
				face dir;
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
		var target_shape = get_item_shape();
		if (target_shape in [SHAPE_TORCH_LIT, SHAPE_LIT_LAMP, SHAPE_LIGHTSOURCE_LIT, SHAPE_SCONCE_LIT]) {
			script item {
				nohalt;
				call get_usecode_fun(), DOUBLECLICK;
			}
			UI_play_sound_effect(46);
		} else {
			flashBlocked(60);
		}
	}
}

void spellFireworks () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Bet Ort@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				actor frame cast_out;
				actor frame cast_up;
				sfx 36;
				call spellOffCenterSpriteEffect, 12;
			}
		} else {
			script item {
				nohalt;
				actor frame cast_out;
				actor frame cast_up;
				call spellFails;
			}
		}
	}
}

void spellGlimmer () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Bet Lor@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				sfx 68;
				actor frame raise_1h;
				actor frame strike_1h;
				call spellCauseLight, 110;
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

void spellIgnite (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		//struct<ObjPos> target = UI_click_on_item();
		var dir = direction_from(target);
		item_say("@In Flam@");
		if (inMagicStorm()) {
			set_to_attack(target, SHAPE_IGNITE);
			script item {
				nohalt;
				face dir;
				actor frame cast_up;
				actor frame cast_out;
				actor frame strike_2h;
				attack;
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
	} else if (event == WEAPON) {
		var target_shape = get_item_shape();
		if (target_shape in [SHAPE_TORCH, SHAPE_LAMPPOST, SHAPE_LIGHTSOURCE, SHAPE_SCONCE]) {
			script item {
				nohalt;
				call get_usecode_fun(), DOUBLECLICK;
			}
		} else {
			flashBlocked(60);
		}
	}
}

void spellThunder () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Vas Kal@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				actor frame reach_2h;
				actor frame strike_2h;
				sfx 62;
			}
		} else {
			script item {
				nohalt;
				actor frame reach_2h;
				actor frame strike_2h;
				call spellFails;
			}
		}
	}
}

void spellWeather () {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		item_say("@Rel Hur@");
		if (inMagicStorm()) {
			script item {
				nohalt;
				actor frame raise_1h;
				actor frame strike_1h;
				sfx 68;
			}
			var weather_array = [0, 1, 2];
			if (UI_get_weather() == 0) {
				UI_set_weather(weather_array[UI_die_roll(2, 3)]);
			} else if (UI_get_weather() != 3) {
				UI_set_weather(0);
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

void spellDetectCharges (struct<ObjPos> target) {
	if (event == DOUBLECLICK) {
		halt_scheduled();
		//struct<ObjPos> target = UI_click_on_item();
		var target_shape = target->get_item_shape();
		var dir = direction_from(target);
		item_say("@Wis Ort@");
		if (inMagicStorm() && (target_shape in [SHAPE_LIGHTNING_WAND, SHAPE_FIRE_WAND, SHAPE_FIREDOOM_STAFF])) {
			var bark = "@" + target->get_item_quality() + " charges left@";
			script item {
				nohalt;
				sfx 67;
				actor frame cast_up;
				actor frame cast_out;
				actor frame ready;
				wait 4;
				say bark;
			}
		} else {
			script item {
				nohalt;
				actor frame cast_up;
				actor frame cast_out;
				actor frame ready;
				call spellFails;
			}
		}
	}
}
