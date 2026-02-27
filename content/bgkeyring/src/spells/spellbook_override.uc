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
 *	This header file overrides the spellbook's spells (most of them, anyway)
 *	to use the new versions. I preferred to do it this way for four reasons:
 *		(1) There is a clean algorythm by which NPC spells are cast;
 *		(2) It allows me to override specific spells and leave others;
 *		(3)	I can add new NPC-only spells without removing existing spells.
 *		(4) It is easier to add NPC-specific spells which only one NPC
 *			can cast (although I haven't made any such spells yet).
 *
 *	Presently, I haven't overriden Help (except for Multimap support), Recall
 *	and Armageddon -- I don't think that they are appropriate for NPCs, so I
 *	left them alone. I _have_ overriden Mark, but only so that is won't be
 *	possible to use it while inside the Shrine of the Codex.
 *	All other spells are overriden so that the Avatar and the NPCs play by the
 *	same rules.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

void spellAwaken_Override object#(0x640) () {
	spellAwaken(UI_click_on_item());
}
void spellWeather_Override object#(0x641) () {
	spellWeather();
}
void spellDouse_Override object#(0x642) () {
	spellDouse(UI_click_on_item());
}
void spellFireworks_Override object#(0x643) () {
	spellFireworks();
}
void spellGlimmer_Override object#(0x644) () {
	spellGlimmer();
}
void spellHelp_Override object#(0x645) () {
	if (event == DOUBLECLICK) {
		//If we came here from a doubleclick, forward to original:
		spellHelp_Override.original();
	} else if (event == SCRIPTED) {
		//We got here from a Usecode script, so we override it
		//for multi-map support:
		var pos = [0x03A8, 0x047A, 0x0, 0x0];
		PARTY->move_object(pos);
	}
}
void spellIgnite_Override object#(0x646) () {
	spellIgnite(UI_click_on_item());
}
void spellThunder_Override object#(0x647) () {
	spellThunder();
}

void spellCreateFood_Override object#(0x648) () {
	spellCreateFood();
}
void spellCure_Override object#(0x649) () {
	spellCure(UI_click_on_item());
}
void spellDetectTrap_Override object#(0x64A) () {
	spellDetectTrap();
}
void spellGreatDouse_Override object#(0x64B) () {
	spellGreatDouse();
}
void spellGreatIgnite_Override object#(0x64C) () {
	spellGreatIgnite();
}
void spellLight_Override object#(0x64D) () {
	spellLight();
}
void spellLocate_Override object#(0x64E) () {
	spellLocate();
}
void spellAwakenAll_Override object#(0x64F) () {
	spellAwakenAll();
}

void spellDestroyTrap_Override object#(0x650) () {
	spellDestroyTrap(UI_click_on_item());
}
void spellEnchant_Override object#(0x651) () {
	spellEnchant(UI_click_on_item());
}
void spellFireBlast_Override object#(0x652) () {
	spellFireBlast(UI_click_on_item());
}
void spellGreatLight_Override object#(0x653) () {
	spellGreatLight();
}
void spellMassCure_Override object#(0x654) () {
	spellMassCure();
}
void spellProtection_Override object#(0x655) () {
	spellProtection(UI_click_on_item());
}
void spellTelekinesis_Override object#(0x656) () {
	spellTelekinesis(UI_click_on_item());
}
void spellWizardEye_Override object#(0x657) () {
	spellWizardEye();
}

void spellCurse_Override object#(0x658) () {
	spellCurse(UI_click_on_item());
}
void spellHeal_Override object#(0x659) () {
	spellHeal(UI_click_on_item());
}
void spellSwarm_Override object#(0x65A) () {
	spellSwarm();
}
void spellProtectAll_Override object#(0x65B) () {
	spellProtectAll();
}
void spellParalyze_Override object#(0x65C) () {
	spellParalyze(UI_click_on_item());
}
void spellPeer_Override object#(0x65D) () {
	spellPeer();
}
void spellPoison_Override object#(0x65E) () {
	spellPoison(UI_click_on_item());
}
void spellSleep_Override object#(0x65F) () {
	spellSleep(UI_click_on_item());
}

void spellConjure_Override object#(0x660) () {
	spellConjure();
}
void spellLightning_Override object#(0x661) () {
	spellLightning(UI_click_on_item());
}
void spellMark_Override object#(0x662) () {
	var target;
	struct<Position> pos;
	var in_codex_shrine;

	if (event == DOUBLECLICK) {
		//Stop Avatar's usecode:
		halt_scheduled();
		//Prompt for a target:
		target = UI_click_on_item();
		//For Codex Quest: of the Virtue Stone of Spirituality has been
		//reattuned to the Shrine of Spirituality, clear the flag if the
		//player decides to cast Mark on it. This allows the Codex to restore
		//the stone once again if needed.
		if ((target->get_item_frame() == 7) && gflags[ATTUNED_SPIRITUALITY_STONE]) {
			gflags[ATTUNED_SPIRITUALITY_STONE] = false;
		}
		if (target->get_map_num() == 0) {
			//Get the stone's position:
			pos = target->get_object_position();

			//Check to see if the STONE is inside the area of the Shrine of the Codex:
			if (!UI_is_pc_inside() && (pos.x >= 0xA50) && (pos.y >= 0xABC) && (pos.x <= 0xAE0) && (pos.y <= 0xB1D)) {
				in_codex_shrine = true;
			}
			if ((pos.x >= 0xA80) && (pos.y >= 0xADC) && (pos.x <= 0xABB) && (pos.y <= 0xB0D)) {
				in_codex_shrine = true;
			}
		}

		if (!in_codex_shrine) {
			//The stone is not inside the area guarded by the Guardians;
			//forward to original usecode:
			//Whoever invented this intrinsic deserves commendation:
			target->set_intercept_item();
			spellMark_Override.original();
		} else {
			//The stone is in the forbidden zone, so the spell fails:
			item_say("@Kal Por Ylem@");
			script item {
				actor frame cast_up;
				actor frame cast_out;
				actor frame strike_2h;
				call spellFails;
			}
		}
	}
}
void spellMassCurse_Override object#(0x663) () {
	spellMassCurse();
}
/*
spellRecall_Override object#(0x667) () {
	item->spellRecall_Override.original();
}
*/
void spellReveal_Override object#(0x665) () {
	spellReveal();
}
void spellSeance_Override object#(0x666) () {
	spellSeance();
}
void spellUnlockMagic_Override object#(0x667) () {
	spellUnlockMagic(UI_click_on_item());
}

void spellCharm_Override object#(0x668) () {
	spellCharm(UI_click_on_item());
}
void spellDance_Override object#(0x669) () {
	spellDance();
}
void spellDispelField_Override object#(0x66A) () {
	spellDispelField(UI_click_on_item());
}
void spellExplosion_Override object#(0x66B) () {
	spellExplosion(UI_click_on_item());
}
void spellGreatHeal_Override object#(0x66C) () {
	spellGreatHeal(UI_click_on_item());
}
void spellInvisibility_Override object#(0x66D) () {
	spellInvisibility(UI_click_on_item());
}
void spellFireField_Override object#(0x66E) () {
	spellFireField(UI_click_on_item());
}
void spellMassSleep_Override object#(0x66F) () {
	spellMassSleep();
}

void spellCauseFear_Override object#(0x670) () {
	spellCauseFear();
}
void spellClone_Override object#(0x671) () {
	spellClone(UI_click_on_item());
}
void spellFireRing_Override object#(0x672) () {
	spellFireRing(UI_click_on_item());
}
void spellFlameStrike_Override object#(0x673) () {
	spellFlameStrike();
}
void spellMagicStorm_Override object#(0x674) () {
	spellMagicStorm();
}
void spellPoisonField_Override object#(0x675) () {
	spellPoisonField(UI_click_on_item());
}
void spellSleepField_Override object#(0x676) () {
	spellSleepField(UI_click_on_item());
}
void spellTremor_Override object#(0x677) () {
	spellTremor();
}

void spellCreateGold_Override object#(0x678) () {
	spellCreateGold(UI_click_on_item());
}
void spellDeathBolt_Override object#(0x679) () {
	spellDeathBolt(UI_click_on_item());
}
void spellDelayedBlast_Override object#(0x67A) () {
	spellDelayedBlast(UI_click_on_item());
}
void spellEnergyField_Override object#(0x67B) () {
	spellEnergyField(UI_click_on_item());
}
void spellEnergyMist_Override object#(0x67C) () {
	spellEnergyMist(UI_click_on_item());
}
void spellMassCharm_Override object#(0x67D) () {
	spellMassCharm();
}
void spellMassMight_Override object#(0x67E) () {
	spellMassMight();
}
void spellRestoration_Override object#(0x67F) () {
	spellRestoration();
}

/*
void spellArmaggedon_Override object#(0x680) () {
	item->spellArmaggedon_Override.original();
}
*/
void spellDeathVortex_Override object#(0x681) () {
	spellDeathVortex(UI_click_on_item());
}
void spellMassDeath_Override object#(0x682) () {
	spellMassDeath();
}
void spellInvisibilityAll_Override object#(0x683) () {
	spellInvisibilityAll();
}
void spellResurrect_Override object#(0x684) () {
	spellResurrect(UI_click_on_item());
}
void spellSummon_Override object#(0x685) () {
	spellSummon();
}
void spellSwordStrike_Override object#(0x686) () {
	spellSwordStrike(UI_click_on_item());
}
void spellTimeStop_Override object#(0x687) () {
	spellTimeStop();
}
