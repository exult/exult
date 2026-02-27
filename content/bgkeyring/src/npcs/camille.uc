/*
 *
 *  Copyright (C) 2006  Alun Bestor/The Exult Team
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
 *	Author: Marzo Junior (reorganizing/updating code by Alun Bestor)
 *	Last Modified: 2006-06-16
 */

/* Camille, the widow of Paws: Reimplemented to let you do the good deed of giving her Thurston's payment for the flour. */
void Camille object#(0x4B1) () {
	if (event == DOUBLECLICK) {
		//After Feridwyn has confronted the avatar with "proof" of Tobias' guilt, Camille will come running to talk
		if (gflags[TOBIAS_ACCUSED] && !gflags[HEARD_CAMILLES_DEFENSE]) {
			item.say("@Avatar! My son Tobias has been wrongly accused! He is no thief! And I cannot believe a vial of venom was found in his possession. I truly believe it was planted there! Please -- I beg thee! Please clear my son's name. He has done no wrong!");
			say("@I know my son Tobias has suffered for not having a father. I have tried my best on mine own to raise him well, but this farm requires so much work that I fear I do not have enough time to devote to him. But I know in mine heart that my son is not a thief.@*");
			say("@Might I suggest that thou speak with Morfin again. He may have recognized signs of usage of this foul substance in other members of the village.@");

			//set her back to wandering around
			UI_set_schedule_type(item, LOITER);
			//don't repeat this conversation again
			gflags[HEARD_CAMILLES_DEFENSE] = true;
			return;
		}

		add(["name", "job", "bye"]);

		if (gflags[HEARD_ABOUT_PAWS_THEFT] && !gflags[SOLVED_PAWS_THEFT]) {
			add("thief");
		}
		if (gflags[TOBIAS_ACCUSED] && !gflags[SOLVED_PAWS_THEFT]) {
			add("Feridwyn");
		}
		if (gflags[SOLVED_PAWS_THEFT]) {
			add("Tobias cleared");
		}

		//Added: Allows you to give her Thurston's payment for the wheat
		if (gflags[DELIVERED_CAMILLES_WHEAT] && !gflags[GAVE_CAMILLE_PAYMENT] && hasGold(10)) {
			add("Thurston's payment");
		}

		//first time meeting
		if (!gflags[MET_CAMILLE]) {
			item.say("You see a farm woman. She rubs her hands, which are covered with dirt and lines drawn by toil.");
			say("@My dreams have become reality. Thou art the Avatar, art thou not? I recognized thee immediately!@");
			gflags[MET_CAMILLE] = true;
		} else {
			item.say("@How art thou, ", getPoliteTitle(), "?@ Camille asks.");
		}

		converse(0) {
			case "name" (remove):
				say("@My name is Camille, Avatar. It is an honor to meet thee.@");

			case "job":
				say("@I run a small farm here in Paws with my son, Tobias. I am a widow.@");
				add(["Paws", "Tobias", "farm"]);

			case "farm" (remove):
				say("@I grow a few crops. Especially carrots and wheat.@");
				add("carrots");
				if (!gflags[GOT_CAMILLES_WHEAT]) {
					//adjusted: it used to be that the entire farm thread would be hidden once you had accepted the grain
					add("wheat");
				}

			case "carrots" (remove):
				say("@I believe my carrots are especially tasty. Wouldst thou like to purchase some? They would only cost thee one gold for three.@");

				if (askYesNo()) {
					say("@How many dost thou desire?@");
					var num_carrots = UI_input_numeric_value(3, 30, 3, 3);
					var carrot_payment = num_carrots / 3;
					if (hasGold(carrot_payment)) {
						if (UI_add_party_items(num_carrots, SHAPE_FOOD, QUALITY_ANY, FRAME_CARROTS, true)) {
							// Collect the payment
							chargeGold(carrot_payment);
							say("@I am sure thou wilt love them.@");
						} else {
							say("@Thou must first lighten thy load. Then I can give thee some delicious carrots.@");
						}
					} else {
						//Man, she's such a sweetie!
						say("@I am sorry, Avatar.@ She shakes her head sadly. @Thou dost not have the gold to be able to taste them.@",
						    "~~She stares at you for a moment, obviously thoughtful. Lowering her voice, she says,",
							"~~@Go ahead, Avatar, take one.@");
						if (UI_add_party_items(1, SHAPE_FOOD, QUALITY_ANY, FRAME_CARROTS, true)) {
							say("Smiling gently, she hands you a carrot.");
						} else {
							say("@Thou art carrying too much...@ She seems truly disappointed.");
						}
					}
				} else {
					say("@If that is thy wish, Avatar, but they are quite good!@");
				}

			case "wheat" (remove):
				say("@That reminds me. This package needs to be taken to the mill today. If thou canst deliver it for me, Thurston will pay thee for it. Wilt thou?@");
				if (askYesNo()) {
					if (UI_add_party_items(1, SHAPE_WHEAT, QUALITY_ANY, FRAME_ANY, true)) {
						say("@Be sure and take this to Thurston, the mill owner. He shall pay thee for thy trouble.@");
						gflags[GOT_CAMILLES_WHEAT] = true;
					} else {
						say("@Thou art carrying too much! Go put something down and I will give it to thee then.@");
					}
				} else {
					say("@I understand that thou art busy on thy quest, Avatar.@");
				}

			case "Paws" (remove):
				say("@Life is hard here in Paws. It is a town of poor people with all the ills that poverty brings. At least The Fellowship brings us some relief.@");
				add(["ills", "Fellowship"]);

			case "Tobias" (remove):
				if (gflags[TOBIAS_ACCUSED]) {
					say("@I know my son. I know that he is growing up unhappy. But I cannot believe that he would steal things.@");
				}

				say("@He is basically a good boy. He works hard and misses his father.@");

			case "Fellowship" (remove):
				say("@I am not sure whether I trust The Fellowship. It has undoubtedly done some good things in this world so it cannot be all bad. Or, at least, the people in it cannot be all bad.@");

			case "ills" (remove):
				say("@Recently, our town has been plagued by a thief.@");
				add("thief");

			case "thief" (remove):
				if (!gflags[TOBIAS_ACCUSED]) {
					say("@Some silver serpent venom was stolen from the merchant Morfin who operates the slaughterhouse.@");
					gflags[HEARD_ABOUT_PAWS_THEFT] = true;
				} else {
					say("@I do not care what Feridwyn says! My son is no thief!@");
				}

			case "Feridwyn" (remove):
				say("@That man Feridwyn knows that I do not trust The Fellowship, and for that he considers me his personal enemy. I do not know why he would seek to attack me through my son but he must not be allowed to succeed.@");

			case "Tobias cleared" (remove):
				say("You tell Camille how you discovered that Garritt was really the thief and that her son Tobias has been cleared. @I want to thank thee for finding the thief in our town and clearing my son's name. It does mine heart good to see that the Avatar has returned to us once again and that thou dost care enough about the people of Britannia to help solve our local troubles here in Paws. Again Avatar, I thank thee.@");

			case "bye":
				say("@Pleasant journey, Avatar.@*");
				return;

			case "Thurston's payment" (remove):
				if (chargeGold(10)) {
					gflags[GAVE_CAMILLE_PAYMENT] = true;
					say("You hand her ten gold pieces, telling her that it is Thurston's payment for the wheat. She looks down at the gold in her palm and begins to protest -- so you gently close her fingers over the money.");

					if (isNearby(TOBIAS)) {
						TOBIAS.say("@Oh come on, mother, take it! Thou canst not go inflicting thy charity on everyone else and not expect someone to do the same to thee!@");
						TOBIAS.hide();
					}
					say("You see a cloud lift from her brow as her guilt gives way to reluctant happiness, and she sighs and looks sheepish.",
					    "~@Oh, I do thank thee Avatar! I shall take Tobias to lunch at the Salty Dog!@");
					say("@I am sorry that I have not the good grace to accept thy charity as it should be taken...",
					    "~@I suppose we have worked to support ourselves for so long that it is sometimes hard to accept the gift of help from others. But I thank thee kindly, and so does Tobias!@ She smiles gratefully.");
				} else {
					AVATAR.say("You open your mouth to bring up Thurston's payment for the wheat, then realise that you do not have enough money left to give her. You decide to wait until your purse is feeling healthier.");
					AVATAR.hide();
				}
		}
	} else if (event == PROXIMITY) {
		scheduleBarks(item);
	}
}
