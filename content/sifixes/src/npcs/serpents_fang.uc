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
 *	Ardiniss gets a funeral when his body comes home, at which Tsandar makes
 *	peace with Isstanar, and Tsandar speaks once of the commendation. Until
 *	the Silver Seed is taken, Isstanar, Surok, Tsandar and Elissa also say
 *	one line as each stage of the orbs is reached (one orb, two or three,
 *	all four); in the original only the end of the quest is ever noticed.
 *
 *	The keep's people are shapes, not numbered NPCs; each marks "met" with
 *	its npc id (Elissa: 2 = still suspicious, 1 = trusts the Avatar). All of
 *	this happens before the original conversation, which then runs
 *	unchanged.
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

// Each NPC's slot in SERPENTS_FANG_NEWS (two flags each).
const int NEWS_ISSTANAR = 0;
const int NEWS_SUROK = 2;
const int NEWS_TSANDAR = 4;
const int NEWS_ELISSA = 6;

// How far the orbs have come: 1 = Yurel's orb, or one orb placed; 2 = two
// or three placed; 3 = all four. 0 = nothing yet, or the Seed is taken (the
// original conversations take over from there).
var serpentsFangOrbStage() {
	if (gflags[HAVE_SILVER_SEED] || gflags[PLANTED_SILVER_SEED]) {
		return 0;
	}
	var placed = 0;
	var orb_flags = [RED_ORB_PLACED, PURPLE_ORB_PLACED, BLUE_ORB_PLACED, YELLOW_ORB_PLACED];
	for (orb_flag in orb_flags) {
		if (gflags[orb_flag]) {
			placed += 1;
		}
	}
	if (placed >= 4) {
		return 3;
	}
	if (placed >= 2) {
		return 2;
	}
	if ((placed >= 1) || gflags[HAVE_PURPLE_ORB]) {
		return 1;
	}
	return 0;
}

// The last stage an NPC has spoken of.
var serpentsFangNewsLevel(var slot) {
	var level = 0;
	if (gflags[SERPENTS_FANG_NEWS + slot]) {
		level += 1;
	}
	if (gflags[SERPENTS_FANG_NEWS + slot + 1]) {
		level += 2;
	}
	return level;
}

void setSerpentsFangNewsLevel(var slot, var level) {
	gflags[SERPENTS_FANG_NEWS + slot] = ((level == 1) || (level == 3));
	gflags[SERPENTS_FANG_NEWS + slot + 1] = (level >= 2);
}

// The stage an NPC should speak of now, or 0; marks it spoken of.
var serpentsFangNewsDue(var slot) {
	var stage = serpentsFangOrbStage();
	if (stage > serpentsFangNewsLevel(slot)) {
		setSerpentsFangNewsLevel(slot, stage);
		return stage;
	}
	return 0;
}

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

// The pyre is lit: smoke over Surok, and the fire.
void lightArdinissPyre(var surok) {
	struct<Position> pos = surok->get_object_position();
	UI_sprite_effect(ANIMATION_BIG_CLOUDS, pos.x - 2, pos.y - 2, 0, 0, 0, -1);
	UI_play_sound_effect(0x51);
}

// Ardiniss is laid to rest; Tsandar makes peace with Isstanar. Surok's face
// is shown on entry and on return.
void ardinissFuneral(var surok, var body) {
	var title = getPoliteTitle();
	say("Surok sees what thou dost carry, and the colour leaves his face.");
	say("@Ardiniss... Thou hast brought him home.@");
	say(
		"@Wait here, " + title + ". I shall fetch the Commander, and Tsandar. Tsandar most of all.@");
	if (takeArdinissBody(body)) {
		say(
			"Surok takes a scroll from among Ardiniss's things and presses it into thy hands. @His last orders. He would want them to serve thee.@");
	}
	UI_remove_npc_face0();
	say("By nightfall a pyre stands in the courtyard. The whole garrison forms up around it.");
	UI_show_npc_face0(FACE_TSANDAR, 0);
	say(
		"@Ardiniss of the Order, Commander of this keep. He went where he would send no other man. That is the whole of it.@");
	UI_show_npc_face1(FACE_ISSTANAR, 0);
	say(
		"@I never served under him. I have served in his shadow every day since I came. Let it be lighter now.@");
	UI_remove_npc_face1();
	say("Tsandar sets the torch to the pyre. For a long time no one speaks.");
	lightArdinissPyre(surok);
	say("@Commander Isstanar. I have not made thy command an easy one. That ends tonight.@");
	UI_remove_npc_face0();
	UI_show_npc_face0(FACE_ISSTANAR, 0);
	say("@Then we are both in the Champion's debt, SubCommander.@");
	UI_remove_npc_face0();
	UI_show_npc_face0(FACE_SUROK, 0);
	say("@Loriel made the Belt of Strength for him, long ago. They are together now, I think.@");
}

void Isstanar shape#(0x3B1) () {
	if ((event != STARTED_TALKING) || !get_npc_id()) {
		Isstanar.original();
		return;
	}
	var title = getPoliteTitle();
	var news = serpentsFangNewsDue(NEWS_ISSTANAR);
	var reward = aramDolDealtWith() && !gflags[ISSTANAR_REWARDED_ARAM_DOL];
	if (news || reward) {
		UI_show_npc_face0(FACE_ISSTANAR, 0);
		if (news == 1) {
			say(
				"@I am told thou hast begun to gather the orbs. Perhaps the Hierophant chose better than I feared.@");
		} else if (news == 2) {
			say(
				"@More of the orbs restored to their places, and thou art still alive. My doubts grow thin, " + title + ".@");
		} else if (news == 3) {
			say(
				"@All four orbs in their places. The Seed lieth within thy reach. I was wrong to doubt thee.@");
		}
		if (reward) {
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
		}
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
	var news = serpentsFangNewsDue(NEWS_SUROK);
	if (body || stranger || nudge || news) {
		UI_show_npc_face0(FACE_SUROK, 0);
		if (body) {
			ardinissFuneral(item, body);
		} else if (stranger) {
			say(
				"@That is not Ardiniss, " + title + ". He went to the southwest outpost and never came back. If he lieth anywhere, he lieth there. If thou hast found him, lay him here beside me, or carry him to me.@");
		}
		if (nudge) {
			say(
				"@They say the tunnels to the east have gone quiet. If that is thy doing, go to Isstanar. I promised thee a reward in his name.@");
		}
		if (news == 1) {
			say(
				"@The soldiers talk of nothing but thee and the orbs. Do take care. I would rather not mend thee twice.@");
		} else if (news == 2) {
			say(
				"@More of the orbs restored! I have not seen this garrison so cheerful since Ardiniss led it.@");
		} else if (news == 3) {
			say("@All four orbs! The Seed itself, then. Go carefully, and come back to me whole.@");
		}
		UI_remove_npc_face0();
	}
	Surok.original();
}

// Tsandar's one word on the commendation, once Isstanar has given it.
var tsandarPraiseDue() {
	return gflags[ISSTANAR_REWARDED_ARAM_DOL] && !gflags[TSANDAR_PRAISED_AVATAR];
}

void Tsandar shape#(0x3B3) () {
	if ((event != STARTED_TALKING) || !get_npc_id()) {
		Tsandar.original();
		return;
	}
	var title = getPoliteTitle();
	var praise = tsandarPraiseDue();
	var news = serpentsFangNewsDue(NEWS_TSANDAR);
	if (praise || news) {
		UI_show_npc_face0(FACE_TSANDAR, 0);
		if (praise) {
			say(
				"@So the Commander hath given thee his seal. Ardiniss would have given it thee too.@");
			say("@That liche cost us good soldiers. Thou hast done well, " + title + ".@");
			gflags[TSANDAR_PRAISED_AVATAR] = true;
		}
		if (news == 1) {
			say("@An orb returned. That is a start, " + title + ". Only a start.@");
		} else if (news == 2) {
			say(
				"@Thou hast won back more of the orbs than this garrison hath in nine months. I do not say it gladly. But I say it.@");
		} else if (news == 3) {
			say("@All four. I would not have believed it. Ardiniss would have.@");
		}
		UI_remove_npc_face0();
	}
	Tsandar.original();
}

void Elissa shape#(0x3F7) () {
	// Only once she trusts the Avatar (npc id 1; 2 = still suspicious).
	if ((event != STARTED_TALKING) || (get_npc_id() != 1)) {
		Elissa.original();
		return;
	}
	var title = getPoliteTitle();
	var news = serpentsFangNewsDue(NEWS_ELISSA);
	if (news) {
		UI_show_npc_face0(FACE_ELISSA, 0);
		if (news == 1) {
			say("@I have heard of thine orb, " + title + ". Perhaps thou art no spy after all.@");
		} else if (news == 2) {
			say(
				"@Thou hast been busy, " + title + ". The mages speak of thee with something like respect.@");
		} else if (news == 3) {
			say("@All four orbs restored. The Order oweth thee a debt it will not easily repay.@");
		}
		UI_remove_npc_face0();
	}
	Elissa.original();
}
