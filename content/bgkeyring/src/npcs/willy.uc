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

/* Willy the Baker in Britain. Reimplemented in preparation for more baking, and to fix a bug that prevented him from ever buying flour. */

const int FLOUR_SELL_PRICE = 4;	//what Willy will buy your flour for

extern void buyBread 0x946();
extern void sellBread 0x947();
extern void sellFlour 0x948();

void Willy object#(0x434) () {
	if (event == DOUBLECLICK) {
		var schedule = get_schedule_type();
		//Is carrying flour
		//This has now been fixed to recognise regular flour sacks: they'd screwed up the frame numbers
		//Will also recognise open flour sacks too
		//This requires modifying sellFlour(), as the same bug is present there
		var has_flour;
		if (PARTY->count_objects(SHAPE_KITCHEN_ITEM, QUALITY_ANY, FRAME_FLOURSACK_OPEN)
				|| PARTY->count_objects(SHAPE_KITCHEN_ITEM, QUALITY_ANY, FRAME_FLOURSACK)
				|| PARTY->count_objects(SHAPE_KITCHEN_ITEM, QUALITY_ANY, FRAME_FLOURSACK_2)) {
			has_flour = true;
		}

		/* Set up initial conversation options */

		add(["name", "job", "bye"]);
		if (gflags[JEANETTE_LOVES_WILLY]) {
			add("Jeanette");
		}
		if (gflags[HIRED_BY_WILLY]) {
			//has bread-baking quest
			add("made bread");
		}

		if (has_flour) {
			add("sell flour");
		}

		/* Start conversation */
		if(!gflags[MET_WILLY]) {
			item.say("You see a very clean-looking, portly young man who waves at you frantically.");
			gflags[MET_WILLY] = true;
		} else {
			item.say("@Ah, hello there! Good to see thee again!@ says Willy.");
		}

		//the 0 is simply to tell it to use all of the options I've added above,
		//rather than adding them the 'normal' way
		//this is because add() is easier for multiple options
		converse(0) {
			case "name" (remove):
			say("@My given name is Wilhelm, although no one calls me that. I prefer to be addressed as Willy. Thank thee very much.@");

			case "job":
				say("@I am the baker here in Britain and I make the sweetest bread thou hast ever tasted! ");
				if (schedule == BAKE) {
					say("@Hast thou had a chance to sample any of my bread yet?@");
					if (askYesNo()) {
						say("@Ah, then thou dost agree it is the sweetest, dost thou not?@");
						if (askYesNo()) {
							say("@Ha! Thou dost see, then? Everyone agrees! That should be proof enough!@");
							if (isNearby(SPARK)) {
								SPARK.say("@I want some!@*");
								item.say("@Here thou art, laddie.@ Willy hands Spark a pastry and the boy devours it in one gulp.*");
								SPARK.say("@Mmmmm! I say, ", getAvatarName(),
									", I think we need a lot of this for the road. We had best buy some, all right?@*");
								SPARK.hide();
							}
						} else {
							say("@Thou dost not?! Why, do not be ridiculous! Of course thou dost!@");
						}
					} else {
						say("@Then here, thou must have some!@ He tears a piece of bread off of one of several loaves he is carrying and stuffs it into your mouth. @There! Is it not the sweetest bread thou hast ever tasted? It is, is it not?!@ You chew as fast as you can in order to answer him.");
						if (askYesNo()) {
							say("He grabs your face by the cheeks and plants a big kiss on your forehead. @Thou art truly a person of good palate and refined taste!@");
						} else {
							say("Dejectedly Willy looks down at the loaf of bread he is carrying. He sniffs at it twice and tosses it out of sight.");
						}
					}
					add(["baker", "bread"]);
				} else {
					say("@Please come to the bakery when it is open in daytime hours and thou shalt sample some!@");
				}

			case "baker" (remove):
				say("He nods. @Yes, I am a baker and I have many secret recipes passed down to me by my father and mother. Why, there are even those who say I am a master baker!");
				say("@And there are those who call me a... doughnut,@ he says with a frown.");
				add(["secret recipes", "father and mother", "master baker", "doughnut"]);

			case "secret recipes" (remove):
				say("@Oh, dear. Do not tell me that thou art yet another person who is trying to pry one of my secret recipes out of me! If that is what thou art after then thou wilt just be disappointed!@");

			case "father and mother" (remove):
				say("Willy wipes away a tear. @Gone. Both of them. Gone to join mine ancestors in that great kitchen in the sky. I will never be able to cook as they did. Still I plod along, trying to keep the family name alive, and that is why I am a baker. But I suppose it is not the only reason.@");
				add("why");

			case "master baker" (remove):
				say("@Yes, many people tell me that. Now thou dost say it, too. If thou dost say so, then it must be true!@");
				say("Willy takes a bite of his own bread. @Mmmm. I -am- a master baker!@");

			case "doughnut" (remove):
				say("He gives you a long puzzled look. After a moment he takes one of his loaves of bread and swats you over the head with it.");

			case "why" (remove):
				say("@Actually, there is a very good reason why I am a baker.@");
				add("reason");

			case "reason" (remove):
				say("@Because the way to a woman's heart is through her stomach. Why, I have two women in love with me right now and I did not even have to pursue either one.@");
				add("two women");

			case "two women" (remove):
				say("He sighs. @If thou must know, their names are Jeanette and Gaye.@");
				add(["Jeanette", "Gaye"]);

			case "Jeanette" (remove):
				say("@Jeanette is a pleasant enough girl, but to be honest I cannot see myself with a tavern wench. She thinks I have not noticed how she feels about me. Frankly, I wish she would just leave me alone.@");

			case "Gaye" (remove):
				say("@Gaye, who runs the costume shoppe, is of more interest to me. But she is a Fellowship member and I have no wish to become one. I hope that it does not prevent us from courting.@");

			case "bread" (remove):
			//Fixed spelling mistake: renown->renowned
				say("@My bread is the finest in Britannia. It is renowned for both its pleasant taste and its very reasonable price. But it is a lot of work making enough to satisfy the constant demand for it. I need to hire someone to help me.@");
				add(["buy", "hire"]);

			case "buy" (remove):
				if (schedule == BAKE) {
					say("@I not only have bread for sale, but pastries, cakes and rolls as well. The most delicious baked goods thou couldst ever wish to pop into thy mouth! Wouldst thou like to buy some?@");
					if (askYesNo()) {
						buyBread();
					} else {
						say("@If thou wert a person of truly refined taste, thou wouldst buy some!@");
					}
				} else {
					say("@I am afraid the bakery is closed. Please come back during normal business hours.@");
				}

			case "hire" (remove):
				if (schedule == BAKE) {
					say("@Thou couldst work for me here in the shoppe making bread. Or I will buy sacks of flour from thee. Thou couldst buy them wholesale in Paws, and I will pay thee ",
						FLOUR_SELL_PRICE, " gold per sack.@");

					say("@Dost thou wish to work here in the shoppe for me?@ Willy asks hopefully.");
					if (askYesNo()) {
						say("@Excellent! Thou canst start work immediately! I shall pay thee 5 gold for every five loaves of bread thou dost make. All right?@");
						if (askYesNo()) {
							gflags[HIRED_BY_WILLY] = true;
							say("@First thou must make dough from the flour. Simply spread some flour out on the table, add some water to make it thick and, well, doughy. Then use the dough in the oven to bake it. Wait a bit, then-- voila! Thou dost have bread!@");
						} else {
							say("@Very well. But I warn thee that employment is hard to obtain in these times!@");
						}
					} else {
						say("@'Tis a pity thou art unavailable. Thou dost look like one who doth know their way around a kitchen.@");
					}
				} else {
					say("@I would be happy to talk with thee about employment at my shoppe during normal business hours.@");
				}

			case "made bread" (remove):
				sellBread();

			case "sell flour" (remove):
				sellFlour();

			case "bye":
				say("@Good day to thee, ", getPoliteTitle(),
					", and bon appetit!@*");
				return;
		}
	} else if (event == PROXIMITY) {
		if (get_schedule_type() == BAKE) {
			var barks = [
				"@Luscious bread!@",
				"@Delicious pastries!@",
				"@Bread fit for a king!@",
				"@Fresh pastries!@"
			];
			item_say(randomIndex(barks));
		} else {
			scheduleBarks(item);
		}
	}
}

//Reimplemented to fix a bug that prevented this ever working
void sellFlour 0x948() {
	UI_push_answers();
	item.say("@Excellent! Dost thou have some flour for me?@");

	if (askYesNo()) {
		say("@Very good! Let me see how many sacks thou dost have...@");

		var num_sack1 = PARTY->count_objects(SHAPE_KITCHEN_ITEM, QUALITY_ANY, FRAME_FLOURSACK_OPEN);
		var num_sack2 = PARTY->count_objects(SHAPE_KITCHEN_ITEM, QUALITY_ANY, FRAME_FLOURSACK);
		var num_sack3 = PARTY->count_objects(SHAPE_KITCHEN_ITEM, QUALITY_ANY, FRAME_FLOURSACK_2);

		var total_sacks = num_sack1 + num_sack2 + num_sack3;
		var payment = total_sacks * FLOUR_SELL_PRICE;

		if (!total_sacks) {
			say("@But thou dost not have a single one in thy possession! Art thou trying to trick me? Get out of my shoppe!@*");
			item.hide();
			return;
		} else {
			say("@Beautiful flour! ", total_sacks,
				"! That means I owe thee ", payment,
				" gold. Here thou art! I shall take the flour from thee now!@");

			if (giveGold(payment)) {
				UI_remove_party_items(num_sack1, SHAPE_KITCHEN_ITEM, QUALITY_ANY, FRAME_FLOURSACK_OPEN, true);
				UI_remove_party_items(num_sack2, SHAPE_KITCHEN_ITEM, QUALITY_ANY, FRAME_FLOURSACK, true);
				UI_remove_party_items(num_sack3, SHAPE_KITCHEN_ITEM, QUALITY_ANY, FRAME_FLOURSACK_2, true);

				say("@Come back and work for me at any time!@*");
				return;
			} else {
				say("@If thou dost travel in a lighter fashion, thou wouldst have hands to take my gold!@");
			}
		}
	} else {
		say("@No? Then thou art a -loaf-er! Ha ha ha!@");
	}
	UI_pop_answers();
}


//Willy's bread-purchasing function. Reimplemented to allow him to buy
//other kinds of bread. Honestly, this is the most stupidly overcomplex
//implementation I have ever seen.
const int BREAD_SELL_PRICE = 1;
const int BREAD_LOTS = 5;
void sellBread 0x947() {
	UI_push_answers();
	say("@Excellent! Dost thou have some loaves for me?@");
	if (askYesNo()) {
		say("@Very good! Let me see how many thou dost have...@");

		//the kinds of bread that Willy accepts
		var bread_frames = [FRAME_BREAD, FRAME_ROLLS, FRAME_BAGUETTE];

		//find all food items within 25 units of Willy
		var nearby_food = WILLY->find_nearby(SHAPE_FOOD, 25, 0);
		var party = UI_get_party_list();

		//say("Nearby food: " + nearby_food->get_array_size());

		//go through the party and find all the food items they are carrying
		for (npc in party) {
			nearby_food = [nearby_food, npc->get_cont_items(SHAPE_FOOD, QUALITY_ANY, FRAME_ANY)];
		}

		//now, go through all *those* food items to find the ones that are actually bread
		var bread = [];
		for (food in nearby_food) {
			if (food->get_item_flag(OKAY_TO_TAKE)
					&& (food->get_item_frame() in bread_frames)
					&& food->get_distance(WILLY) <= 25) {
				bread = [bread, food];
			}
		}

		//Finally, give the player the money and take the bread
		var num_bread = bread->get_array_size();
		//Willy buys bread in lots of 5
		var chargeable_bread = (num_bread / BREAD_LOTS) * BREAD_LOTS;

		if (chargeable_bread == 0) {
			say("@Thou have not made enough bread to be worthy of any payment at all.@");
		} else {
			var payment = BREAD_SELL_PRICE * chargeable_bread;
			say("@Scrumptious! ", num_bread, " loaves! That means I owe thee ",
				payment, " gold.");

			if (giveGold(payment)) {
				say("@Here thou art! I shall take the loaves from thee now!");

				var count = 1;
				while (count < num_bread) {
					var food = bread[count];
					if (food->get_container()) {
						//Item is in the party's inventory, delete it
						//FFS, can we stop using subtractQuantity() already
						subtractQuantity(food);
					} else {
						//Go through all the ones on the ground, making them belong to Willy
						food->clear_item_flag(OKAY_TO_TAKE);
					}
					count += 1;
				}
				say("@Come back and work for me at any time!@");
				return;
			} else {
				say("@But thou art too overburdened! If thou dost travel in a lighter fashion, thou wouldst have hands to take my gold!@");
			}
		}
	} else {
		say("@No? What hast thou been doing? -Loaf-ing around? Ha ha ha!@");
	}

	UI_pop_answers();
	return;
}
