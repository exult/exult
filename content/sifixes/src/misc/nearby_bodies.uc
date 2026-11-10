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
 *	Finds the bodies of companions that Thoxa can raise, either carried by the
 *	party or lying near the Avatar. Used by Resurrect (0x8FE) and, through
 *	areThereBodiesNearby (0x8FC), by the Hourglass of Fate and Thoxa.
 *
 *	The original only looked for body shapes 400 and 414, but Wilfred's body
 *	is shape 402, so he could never be resurrected (issue #961). This version
 *	also looks for shape 402. Only bodies of joinable NPCs (and Cantra) are
 *	returned, as before, so other bodies of that shape are still ignored.
 *
 *	The original also returned the party member carrying a body along with
 *	the body itself; that entry was always filtered out below, so this version
 *	only collects the bodies.
 */
var getNearbyBodies 0x8FB () {
	var body_shapes = [SHAPE_BODY_400, SHAPE_BODY_402, SHAPE_BODY_414];
	var bodies = [];
	var shape;
	var member;
	var body;
	var party = UI_get_party_list();
	for (shape in body_shapes) {
		// Bodies carried by the party:
		for (member in party) {
			bodies = [bodies, member->get_cont_items(shape, QUALITY_ANY, FRAME_ANY)];
		}
		// Bodies lying near the Avatar:
		bodies = [bodies, AVATAR->find_nearby(shape, 80, 0)];
	}
	var resurrectables = [getJoinableNPCNumbers(), CANTRA];
	var found = [];
	for (body in bodies) {
		var npc = body->get_body_npc();
		if (getIndexForElement(npc, resurrectables)) {
			if (npc == DUPRE) {
				// As in the original: only Dupre's own body (shape 414,
				// frame 20) counts for him.
				if ((body->get_item_shape() == SHAPE_BODY_414) && (body->get_item_frame() == 20)) {
					found = [found, body];
				}
			} else {
				found = [found, body];
				if ((npc == SHAMINO) && gflags[BEATRIX_PROTECTION]) {
					SHAMINO->set_item_flag(SI_TOURNAMENT);
				}
			}
		}
	}
	return found;
}
