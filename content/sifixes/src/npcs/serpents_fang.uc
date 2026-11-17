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
 *	Serpent's Fang: Surok's promise and Commander Ardiniss's body.
 *
 *	Surok (0x3B2) says that Isstanar would reward the Avatar for destroying
 *	Aram-Dol, and asks for Ardiniss's body to be brought home. Neither
 *	Isstanar (0x3B1) nor anyone else ever checks for either.
 *
 *	Now Isstanar pays the reward (gold and a sealed commendation) once
 *	Aram-Dol is dealt with (see misc/aram_dol.uc), and Surok reminds the
 *	Avatar of it until then. Surok takes Ardiniss's body (400 frame 13, from
 *	(2518,3048)) when it is carried or laid near him, gives back what was in
 *	it (his note, 707 quality 124, may still be needed at the force walls)
 *	and turns away any other body. The note is used up at the force walls
 *	(0x603), so his body counts with or without it.
 *
 *	The keep's people are shapes, not numbered NPCs; each marks "met" with
 *	its npc id. All of this happens before the original conversation, which
 *	then runs unchanged.
 */
const int ARDINISS_NOTE_QUALITY = 124;
const int ARDINISS_BODY_FRAME = 13;

// Isstanar's sealed commendation; items/scroll.uc gives it its text.
const int COMMENDATION_QUALITY = 184;
const int ARAM_DOL_REWARD_GOLD = 500;

// A stranger's body this close to Surok is one he comments on.
const int BODY_SEARCH_RADIUS = 8;

// Ardiniss's body laid this close counts as brought to him. Wider, because
// a room full of bodies can put his further off.
const int ARDINISS_SEARCH_RADIUS = 16;

var holdsArdinissNote(var body) {
	return body->count_objects(SHAPE_BROWN_SCROLL, ARDINISS_NOTE_QUALITY, FRAME_ANY) > 0;
}

// Bodies of one shape carried by the party or lying within radius of Surok.
var bodiesNearSurok(var surok, var shape, var radius) {
	var bodies = [];
	var party = UI_get_party_list();
	for (member in party) {
		bodies = [bodies, member->get_cont_items(shape, QUALITY_ANY, FRAME_ANY)];
	}
	return [bodies, surok->find_nearby(shape, radius, 0)];
}

// Ardiniss's body: the one holding his note or, once the note is used up,
// the one with his frame.
var findArdinissBody(var surok) {
	var bodies = bodiesNearSurok(surok, SHAPE_BODY_400, ARDINISS_SEARCH_RADIUS);
	for (body in bodies) {
		if (body && holdsArdinissNote(body)) {
			return body;
		}
	}
	for (body in bodies) {
		if (body && (body->get_item_frame() == ARDINISS_BODY_FRAME)) {
			return body;
		}
	}
	return 0;
}

// Any body at all, so Surok can say it is not Ardiniss.
var anyBodyNearSurok(var surok) {
	var shapes = [SHAPE_BODY_400, SHAPE_BODY_402, SHAPE_BODY_414, SHAPE_BODY_762, SHAPE_BODY_778,
		SHAPE_BODY_892];
	for (shape in shapes) {
		var bodies = bodiesNearSurok(surok, shape, BODY_SEARCH_RADIUS);
		for (body in bodies) {
			if (body) {
				return true;
			}
		}
	}
	return false;
}

// Surok takes the body. What was in it goes to the Avatar (or at his feet
// if he cannot hold it). Returns whether the note was among it.
var takeArdinissBody(var body) {
	var had_note = holdsArdinissNote(body);
	var contents = body->get_cont_items(SHAPE_ANY, QUALITY_ANY, FRAME_ANY);
	for (obj in contents) {
		if (obj->get_container() == body) {
			obj->set_last_created();
			if (!AVATAR->give_last_created()) {
				UI_update_last_created(AVATAR->get_object_position());
			}
		}
	}
	body->remove_item();
	gflags[ARDINISS_BROUGHT_HOME] = true;
	return had_note;
}

void giveIsstanarReward() {
	UI_add_party_items(ARAM_DOL_REWARD_GOLD, SHAPE_GOLD_COIN, QUALITY_ANY, 0, true);
	giveItemsToPartyMember(AVATAR, 1, SHAPE_SCROLL, COMMENDATION_QUALITY, 0, 0, true);
	gflags[ISSTANAR_REWARDED_ARAM_DOL] = true;
}

void Isstanar shape#(0x3B1) () {
	if ((event != STARTED_TALKING) || !get_npc_id()) {
		Isstanar.original();
		return;
	}
	if (aramDolDealtWith() && !gflags[ISSTANAR_REWARDED_ARAM_DOL]) {
		var title = getPoliteTitle();
		UI_show_npc_face0(FACE_ISSTANAR, 0);
		if (gflags[ARAM_DOL_SLAIN]) {
			say(
				"@Word hath come from the eastern watch, " + title + ". The dead in Aram-Dol's tunnels walk no more. Thou hast destroyed the liche.@");
		} else {
			say(
				"@Thou hast carried the blue orb out of Aram-Dol's lair, " + title + ", and lived. Few in this keep could say as much.@");
		}
		say(
			"@Surok promised thee a reward in my name. He promiseth too freely, but in this he was right.@");
		giveIsstanarReward();
		say(
			"@Five hundred in gold, and my commendation under seal. Any soldier of Order who seeth it will know what thou hast done for Serpent's Fang.@");
		UI_remove_npc_face0();
	}
	Isstanar.original();
}

void Surok shape#(0x3B2) () {
	if ((event != STARTED_TALKING) || !get_npc_id()) {
		Surok.original();
		return;
	}
	var title = getPoliteTitle();
	var body = 0;
	var stranger = false;
	if (!gflags[ARDINISS_BROUGHT_HOME]) {
		body = findArdinissBody(item);
		if (!body) {
			stranger = anyBodyNearSurok(item);
		}
	}
	var nudge = aramDolDealtWith() && !gflags[ISSTANAR_REWARDED_ARAM_DOL];
	if (body || stranger || nudge) {
		UI_show_npc_face0(FACE_SUROK, 0);
		if (body) {
			say(
				"@Ardiniss... Thou hast brought him home. I shall see that he is laid to rest as befits the Commander of this keep. I thank thee, " + title + ".@");
			if (takeArdinissBody(body)) {
				say(
					"Surok takes a scroll from among Ardiniss's things and presses it into thy hands. @His last orders. He would want them to serve thee.@");
			}
		} else if (stranger) {
			say(
				"@That is not Ardiniss, " + title + ". He went to the southwest outpost and never came back. If he lieth anywhere, he lieth there. If thou hast found him, lay him here beside me, or carry him to me.@");
		}
		if (nudge) {
			say(
				"@They say the tunnels to the east have gone quiet. If that is thy doing, go to Isstanar. I promised thee a reward in his name.@");
		}
		UI_remove_npc_face0();
	}
	Surok.original();
}
