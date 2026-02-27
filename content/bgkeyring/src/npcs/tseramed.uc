/*
 *
 *  Copyright (C) 2023  The Exult Team
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

void TseramedGenderedExchange (var make_gendered_remark, var party) {
	if (!make_gendered_remark) {
		return;
	}
	for (npc in party) {
		if (UI_get_npc_prop(npc, SEX_FLAG) == 1) {
			npc.say("@Take care with thy words, master woodsman.@");
			TSERAMED.say("@I do not mean this gracious company! Surely thou art among the elite of Britannia and a rare figure of a woman.@");
			npc.say("@Thy speech does me service. Alas! Too few are the women who learn skill in arms.@");
			npc->remove_npc_face();
			break;
		}
	}
}

void TseramedBattleQueries 0x8F3 (var party) {
	var got_reply = false;
	UI_push_answers();
	say("@Tell me something, if thou please. Many years have passed since I first learned my woodcraft. Such craft includes skill in arms, and I must know... Does the Avatar prefer mystical enchantment to overcome enemies, or physical strength and valor in arms?@");
	converse (["enchantment", "valor in arms", "bye"]) {
		case "enchantment":
			say("@I have suspected it! No skill have I in such deep matters, but perchance our speech might turn to enchantment when our quest is complete.@");
			gflags[TSERAMED_THINKS_MAGE] = true;
			break;

		case "valor in arms":
			say("@I have often suspected it! I am honored to travel with thee. I shall watch thee diligently, for surely thou art the greatest fighter who ever lived.@");
			say("@When our quest is complete we shall regale each other with our exploits. Tell me, dost thou prefer hand to hand combat or ranged weaponry?@");
			remove(["enchantment", "valor in arms"]);
			add(["hand to hand", "ranged weaponry"]);
			gflags[TSERAMED_THINKS_MAGE] = false;
			got_reply = false;

		case "hand to hand" (remove):
			var gender_remark = "and thou seemest man enough for such close work";
			var made_gendered_remark = false;
			if (UI_is_pc_female()) {
				gender_remark = "especially in women. The women of Britannia seldom have them";
				made_gendered_remark = true;
			}
			say("@Such weapons require strength and daring! I admire such qualities, ", gender_remark, ".@");
			say("@But my preferences run to the bow. An ancient weapon, and elegant, a fine bow of Yew may bring down game sooner than a sword.@");
			TseramedGenderedExchange(made_gendered_remark, party);
			break;

		case "ranged weaponry" (remove):
			say("@Such is also my choice. Few are my peers in the art of archery. A keen eye and steady hand are required, and that is rare in the men of this day. Even rarer in women. Sad, that the women of Britannia should be innocent of such art!@");
			TseramedGenderedExchange(true, party);
			break;

		case "bye":
			if (got_reply) {
				abort;
			}
			say("@Please, Avatar, I simply must know.@");
			got_reply = true;
	}
	UI_pop_answers();
	return;
}
