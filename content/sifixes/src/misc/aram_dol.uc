/*
 *
 *  Copyright (C) 2026  The Exult Team
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
 */

/*
 *	Aram-Dol, the liche east of Serpent's Fang.
 *
 *	Surok tells the Avatar that Isstanar would reward him for destroying
 *	Aram-Dol, but nothing ever notices the liche's death. He is not in
 *	npc.dat: a usecode egg (0x769) creates him (shape 981) at
 *	(3015,2572,1), and shape 981 has no usecode. Exult only runs a monster's
 *	DEATH usecode when it carries SI_TOURNAMENT, so the egg now tags him and
 *	his shape function sets ARAM_DOL_SLAIN when he dies. A tournament
 *	death gives no experience, so the shape function credits the killer
 *	with what the engine would have given for him.
 *
 *	The blue orb (450 frame 10) lies loose in his lair. For games whose egg
 *	hatched before this version, carrying or placing that orb counts as
 *	having dealt with him.
 */

// Creates Aram-Dol; tag him so his death runs our shape function.
void AramDolEgg object#(0x769) () {
	AramDolEgg.original();
	if (event != EGG) {
		return;
	}
	var liches = UI_find_nearby(item, SHAPE_LICHE, 20, MASK_NPC2);
	for (liche in liches) {
		liche->set_item_flag(SI_TOURNAMENT);
	}
}

// The part of a liche's kill experience that does not depend on its stats:
// 32 from the liche's entry in monsters.dat and 1 for its one immunity.
// Aram-Dol only carries keys, which add nothing.
const int ARAM_DOL_BASE_EXPERIENCE = 33;

// Experience for killing Aram-Dol, worked out the way the engine does for
// any monster (actors.cc, Actor::reduce_health).
var aramDolExperience(var liche) {
	if (liche->get_item_flag(ASLEEP)) {
		return 0;
	}
	var xp = liche->get_npc_prop(STRENGTH) + liche->get_npc_prop(COMBAT);
	xp = xp + ((liche->get_npc_prop(DEXTERITY) + 1) / 3);
	xp = xp + (liche->get_npc_prop(INTELLIGENCE) / 5);
	xp = (xp + ARAM_DOL_BASE_EXPERIENCE) / 2;
	return xp;
}

// Whoever struck the killing blow; the engine records them as the
// oppressor (0 is the Avatar) before running the DEATH usecode.
var aramDolKiller(var liche) {
	var oppressor = liche->get_oppressor();
	if (oppressor == 0) {
		return AVATAR;
	}
	var killer = (0 - oppressor)->get_npc_object();
	if (!killer) {
		return AVATAR;
	}
	return killer;
}

// Shape 981 has no usecode in the original. Only a tagged Aram-Dol gets
// here on DEATH; every other event does nothing, as before.
void AramDol shape#(0x3D5) () {
	if ((event == DEATH) && get_item_flag(SI_TOURNAMENT)) {
		gflags[ARAM_DOL_SLAIN] = true;
		clear_item_flag(SI_TOURNAMENT);
		// The engine gives no experience for a tournament death, and our
		// kill below has no attacker, so credit the killer here. Usecode
		// experience is halved by the engine, hence the doubling.
		var xp = aramDolExperience(item);
		if (xp > 0) {
			aramDolKiller(item)->set_npc_prop(EXPERIENCE, xp * 2);
		}
		reduce_health(500, NORMAL_DAMAGE);
	}
}

// Aram-Dol is dead, or (for older games) his blue orb is out of his lair.
var aramDolDealtWith() {
	if (gflags[ARAM_DOL_SLAIN] || gflags[BLUE_ORB_PLACED]) {
		return true;
	}
	return PARTY->count_objects(SHAPE_ORB, QUALITY_ANY, 10) > 0;
}
