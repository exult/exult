/*	Copyright (C) 2024  The Exult Team
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*	The original Spinebreaker Grand Shrine Automaton had a bug where it would
 *	quote from the wrong book and expect the "right" answer.
 */

// Test of Order, refactored out of the Spinebreaker Grand Shrine Automaton.
void testOfOrder() {
	say("@Which book tells us, ~'The rules are the Forces. The rules are not a "
		"part of thee, they are thee and thou art the rules.'@*");
	say("@'Once those rules, or Forces, are one with thee, then, and only then,"
		" canst thou abide regardless of the circumstances.'@");
	var replies = [
		"Strength Though Ethics", "Guidelines of Life",
		"Tenets of Discipline"
	];
	if (gflags[READ_STRUCTURE_OF_ORDER]) {
		replies &= "The Structure of Order";
	} else {
		replies &= "The Joy of Order";
	}
	converse : nested (replies) {
	case "Strength Though Ethics":
		say("@An excellent book, one that can guide a disciple of Order to new "
			"consciousness. I would recommend that thou readest it. Then thou "
			"wouldst realize that the answer thou didst give was incorrect.@");
		break;

	case "Tenets of Discipline":
		say("@Shushuro was a great man indeed. If only mine hollow head could "
			"be so enlightened. But at least I realize my quote was not from "
			"that book's pages. Mayhap thou shouldst return to the city and "
			"examine thy books further.@");
		break;

	case "Guidelines of Life":
		say("@A masterpiece! Indeed, its words of wisdom could educate thee "
			"very well. Then thou wouldst be enlightened to learn that the "
			"book thou didst name doth not contain any such quote.@");
		say("@Mayhap thou dost need to study more at the Hierophant's  library."
			" 'Tis a wondrous place full of the wisdom of Order.@");
		break;

	case "The Joy of Order":
		say("@Curious, I have never heard of that book. Methinks that thou art "
			"trying to deceive me. Please, do us both a service and return "
			"thou to the library for further study.@");
		break;

	case "The Structure of Order" (remove):
		say("@Ah, so thou art a true acolyte of Order. I will permit thee to "
			"enter the Grand Shrine.@");
		UI_remove_npc_face0();
		var switches = find_nearby(SHAPE_SWITCH, 30, MASK_NONE);
		for (lever in switches) {
			switch (lever->get_item_quality()) {
			case 0:
				lever->set_item_quality(150);
				gflags[OPENED_SPINEBREAKER_DOOR] = true;
				UI_play_sound_effect(0x001E);
				changeSlidingDoorState(lever, 1);
				break;

			case 120:
				lever->set_item_quality(147);
				break;
			}
		}
		var closed_doors = find_nearby(SHAPE_METAL_WALL_NS, 20, MASK_NONE);
		for (wall in closed_doors) {
			wall->set_item_quality(147);
		}
		var open_walls = find_nearby(SHAPE_OPEN_METAL_WALL_NS, 20, MASK_NONE);
		for (wall in open_walls) {
			wall->set_item_quality(147);
			var lever = find_nearby(SHAPE_SWITCH, 20, MASK_NONE);
			if (lever->get_item_quality() == 147) {
				changeSlidingDoorState(lever, 0);
			}
		}
		delayedBark(METAL_MAN, "@Seek Order.@", 3);
		abort;
	}
	add(["Give me the test"]);
}

// Grand Shrine Automaton
void SpinebreakerGuard object#(0x4CA) () {
	if (get_item_flag(SI_ZOMBIE)) {
		partyAutomaton();
		abort;
	}
	if (event == DOUBLECLICK) {
		METAL_MAN->clear_item_say();
		AUTOMATON_FACE->show_npc_face0(0);
		if (!METAL_MAN->get_item_flag(MET)) {
			say("@Halt! Only a true follower of Order may pass this portal. "
				"Art thou such a disciple?@");
			add(["I follow Order", "I follow Chaos"]);
		} else if (gflags[OPENED_SPINEBREAKER_DOOR]) {
			say("@'Tis good to see thee again, scholar of Order.@");
			say("@I surmise thou didst come across with the others.@");
			add(["door locked", "others", "bye"]);
		} else {
			say("@Oh, 'tis only thee. I hope that thou hast become more "
				"learned in the ways of Order, lest thou wilt not be permitted "
				"to enter the grand shrine.@");
			add(["Give me the test", "grand shrine", "bye"]);
		}
		converse(0) {
		case "I follow Order":
			remove(["I follow Order", "I follow Chaos"]);
			say("@That may be, but I must test thy knowledge before I permit "
				"thee access to the grand shrine.@");
			add(["test", "grand shrine", "bye"]);

		case "test" (remove):
			say("@Though I doubt one such as thee would attempt to deceive "
				"me, many 'followers' of late do not appear to truly be "
				"devoted to Order.@");
			say("@In truth, I believe them to be impostors, though they did "
				"answer the question I put to them.@");
			add(["impostors", "question"]);

		case "question" (remove):
			say("@To insure that thou art a true student of Order, I will ask "
				"thee to complete a line from one of our greatest books and "
				"thou must name the book.@");
			say("@Only a devout student of Order would know the answer.@");
			add(["Give me the test"]);

		case "Give me the test" (remove):
			testOfOrder();

		case "grand shrine" (remove):
			say("@At the end of this corridor lies the grand shrine of Order. "
				"That is where the Hierophant performs the ceremony to open "
				"the Wall of Lights.@");

		case "impostors" (remove):
			say("@Though I should not speak such, they appeared to be, shall "
				"we say, less than intelligent. They seemed to have great "
				"difficulty with answering the most modest of my queries.@");
			say("@One of their number even threatened to tear me limb from "
				"limb if I failed to open this portal. But it was the large "
				"one who answered all the questions correctly. He alone was no "
				"dolt.@");
			add(["large one"]);

		case "others" (remove):
			say("@The others that I spoke about earlier. Or did I mention "
				"them? I thought that I mentioned that the 'disciples' of late "
				"do not appear to be truly devout.@*");
			say("@In truth, I believed them to be impostors, though they "
				"passed the test I put upon them.@");
			add(["impostors"]);

		case "large one" (remove):
			say("@He was an impressive man, in both size and intellect. He "
				"appeared to have the combined intellect that the rest of the "
				"party lacked.@*");
			say("@He answered my questions easily, though in truth I cannot "
				"remember what I asked of him.@");

		case "door locked" (remove):
			say("@I am truly sorry, but I can only operate these doors. I know "
				"nothing about any other doors in this place.@");

		case "I follow Chaos" (remove):
			say("@Then thou shalt not pass.@");
			delayedBark(METAL_MAN, "@Duty.@", 3);
			METAL_MAN->set_schedule_type(PATROL);
			break;

		case "bye":
			UI_remove_npc_face0();
			UI_remove_npc_face1();
			delayedBark(AVATAR, "@I have heard enough.@", 0);
			delayedBark(METAL_MAN, "@Always follow Order.@", 3);
			METAL_MAN->set_schedule_type(PATROL);
			break;
		}
	}
}
