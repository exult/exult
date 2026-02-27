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
 *	Last Modified: 2006-03-19
 */

/* Reconstructed so we can add in fish-buying behaviour */

//Gordon's prices for buying and selling fish from the player
enum gordons_prices {
	GORDONS_BUY_PRICE		= 3,	//what Gordon pays for fresh fish
	GORDONS_BUY_OLD_PRICE	= 1,	//what Gordon pays for old fish
	GORDONS_SELL_PRICE		= 8,	//what Gordon sells fish and chips for
	GORDONS_ROD_PRICE		= 15	//what Gordon will sell his rod for
};

void Gordon object#(0x43A) () {
	var polite_title;
	var wearing_fellowship;
	//player clicks on Gordon
	if (event == DOUBLECLICK) {
		polite_title = getPoliteTitle();
		wearing_fellowship = UI_wearing_fellowship();

		if (UI_part_of_day() == NIGHT) {
			//Gordon is at/on his way to the fellowship meeting
			if (nearEachOther(item, BATLIN)) {
				item.say("Gordon is too involved in listening to the Fellowship meeting to hear you.*");
				return;
			} else if (gflags[BATLIN_MISSING]) {
				item.say("@Where oh where is Batlin? He is late for the meeting!@");
			} else {
				item.say("@Oh, my! I must leave immediately! I am late for the Fellowship meeting!@*");
				return;
			}
		} else if (!gflags[MET_GORDON]) {
			//Meeting Gordon for the first time
			item.say("You see a friendly face looking back at you.");
			gflags[MET_GORDON] = true;
		} else {
			//Meeting Gordon again
			item.say("@How art thou this fine day, ", polite_title,
			         "?@ asks Gordon.");
		}

		add(["name", "job", "bye"]);
		if (gflags[HIRED_BY_GORDON]) {
			add("sell fish");
			if (!PARTY->count_objects(SHAPE_FISHING_ROD, QUALITY_ANY, FRAME_ANY)) {
				add("fishing pole?");
			}
		} else if (gflags[ASKED_GORDON_ABOUT_FISHING]) {
			add("fishing");
		}

		//Start of regular conversation tree
		converse(0) {
			case "name" (remove):
				say("@My name is Gordon.@");

			case "job":
				say("@I sell fish and chips from my wagon.@");
				add(["fish and chips", "wagon"]);

			case "fish and chips" (remove):
				if (get_schedule_type() != TEND_SHOP) {
					say("@Come back later when I am open for business.@*");
					break;
				} else {
					say("@I have the best fish and chips thou shalt taste in all of Britannia. My price is only ",
					    GORDONS_SELL_PRICE, " gold coins per serving. Wouldst thou like to have some?@");

					if (askYesNo()) {
						if (chargeGold(GORDONS_SELL_PRICE)) {
							if (UI_add_party_items(1, SHAPE_FOOD, QUALITY_ANY, FRAME_FISH_AND_CHIPS, true)) {
								say("He hands you a plate.");
								say("@They are indeed the best fish and chips in all of Britannia.@");
							} else {
								//whoops, can't carry it
								say("@Thou art carrying too many things to take thy fish and chips from me!@");
								//Added: now he gives the bloody money back
								giveGold(GORDONS_SELL_PRICE);
							}
						} else {
							say("@Thou dost not have enough gold to get any fish and chips. Sorry!@");
						}
					} else {
						say("@Come back again when thou art hungry and I am sure thou shalt change thy mind.@");
					}
				}

			//Todo: actually give him a wagon, or else revise this
			//conversation thread
			case "wagon" (remove):
				say("@I just painted my wagon recently. It receives more attention. Business is much better now. I am saving my money to travel to Buccaneer's Den.@");
				add(["business", "Buccaneer's Den"]);

			case "business" (remove):
				say("@Ever since I became a member of The Fellowship, business has been increasing steadily. I have refined and improved the recipe for my fish batter and it has since become a favorite meal of nearly everyone in Britain. I have even served my fish and chips to Lord British himself.@");
				add(["Fellowship", "Lord British"]);

				//Added to support fish sales
				say("@In fact, I am so busy with my business now that I barely have time to go fishing anymore.@");
				if (!gflags[HIRED_BY_GORDON]) {
					add("fishing");
				}

			case "Lord British" (remove):
				say("@Thou dost know-- the bloke who wears a crown and acts like a king.@");

			case "Fellowship" (remove):
				if (wearing_fellowship) {
					say("@I am glad to see that thou art a member. Will I see thee at the next meeting?@");

					if (askYesNo()) {
						say("@Then I shall see thee at nine o'clock sharp!@");
					} else {
						say("@Thou shouldst apply thyself more stringently to the ways of The Fellowship! Our meeting is at nine o'clock. I can see thou dost certainly need to attend.@");
					}
				} else {
					askAboutFellowship();
				}

			//don't do it, you fool!
			case "philosophy" (remove):
				askAboutPhilosophy();

			case "Buccaneer's Den" (remove):
				say("@I wish to win some money at Buccaneer's Den. It is a pirate resort and they have a fabulous House of Games there.@");
				add(["pirate resort", "House of Games"]);

			case "pirate resort" (remove):
				say("@As I am certain thou knowest, Buccaneer's Den was once a den of thieves and villains. As such, it held a romantic appeal for those who longed for a taste of such an adventurous existence. I confess, I am one of those people. When thou dost spend thy life selling fish from a wagon, thou art in need of excitement. The pirates eventually realized just how much they are secretly envied, and so they have turned their island into a place of thrilling amusements.@");

			case "House of Games" (remove):
				say("@It is said they have several games of chance there! Gold can be won wagering on the outcome of a race of fine stallions.@");

			//guh? This never got used, since it was preempted by the
			//previous thread. Todo: move this into the previous thread!
			case "Fellowship" (remove):
				say("@I saw thee receive thy medallion. I can certainly say the Fellowship has done wonders for my life and I know it will be the same for thee as well.@ He gives thee a knowing grin.");

			case "bye":
				say("@Have a pleasant day, ", polite_title, ".@ *");
				break;


			//From this point on is added functionality
			case "fishing" (remove):

				say("@Aye, one of the reasons my fish and chips are so popular is that I use fresh fish. But I cannot mind the stall and go fishing at the same time! Lately I have been forced to buy day-old fish from Fred... 'tis not the same.@",
				    "~He thinks a moment. @You know, I would be willing to pay thee if thou wouldst bring me freshly-caught fish. Wouldst thou?@");

				if (askYesNo()) {
					say("@Excellent! I shall pay thee ", GORDONS_BUY_PRICE,
					    " gold coins per fish. There are some good fishing spots in the streams to the east.",
					    "~Remember, only the freshest fish will do.@");

					gflags[HIRED_BY_GORDON] = true;
					add("sell fish");
					if (!PARTY->count_objects(SHAPE_FISHING_ROD, QUALITY_ANY, FRAME_ANY)) {
						add("fishing pole?");
					}
				} else {
					//so we can ask him again next time
					gflags[ASKED_GORDON_ABOUT_FISHING] = true;
					say("@Oh well, maybe some other time then.@");
				}

			case "sell fish" (remove):
				if (get_schedule_type() != TEND_SHOP) {
					say("@Come back later when I am open for business.@*");
					break;
				} else {
					var num_fish = PARTY->count_objects(SHAPE_FOOD, QUALITY_FRESHFISH, FRAME_TROUT);
					var payment;
					if (num_fish > 0) {
						payment = num_fish * GORDONS_BUY_PRICE;

						say("@Oh, thou hast some fish to sell me?@ He checks them over.",
						    "~@Why, I see that thou hast ",
						    num_fish, " lovely fresh fish.@");

						if (num_fish > 1) {
							say("@At ",  GORDONS_BUY_PRICE,
							    " gold apiece, that comes to ",
							    payment, " gold.@");
						} else {
							say("@I shall pay thee ", payment, " gold for it.@");
						}

						if (giveGold(payment)) {
							UI_remove_party_items(num_fish, SHAPE_FOOD, QUALITY_FRESHFISH, FRAME_TROUT, true);
							say("He gives you the gold and takes the fish from you.",
							    "@Thank thee ", polite_title,
							    ", and do bring me more if thou catchest any.@");
						} else {
							say("@But thou art carrying too much for me to pay thee! Come back when thou art less burdened.@");
						}
					} else if (PARTY->count_objects(SHAPE_FOOD, QUALITY_ANY, FRAME_TROUT)) {
						//see if they have any old fish
						num_fish = PARTY->count_objects(SHAPE_FOOD, QUALITY_ANY, FRAME_TROUT);
						payment = num_fish * GORDONS_BUY_OLD_PRICE;

						say("@Oh, thou hast some fish to sell me?@ He checks them over.");
						if (num_fish > 1) {
							say("@Fie, thy fish are old and smelly! Didst I not tell thee that I only want fresh ones?");
							say("@I suppose they are better than nothing though. I shall give thee ",
							    payment, " gold for them, dost thou accept?@");
						} else {
							say("@Fie, this fish is old and smelly! Didst I not tell thee that I only want fresh ones?");
							say("@I suppose 'tis better than nothing though. I shall give thee ",
							    payment, " gold for it, dost thou accept?@");
						}

						if (askYesNo()) {
							if (giveGold(payment)) {
								UI_remove_party_items(num_fish, SHAPE_FOOD, QUALITY_ANY, FRAME_TROUT, true);
								say("He gives you the gold and takes the fish from you.",
								    "~@Thank thee ", polite_title,
								    ". Please bring me fresh ones next time, if thou catchest any.@");
							} else {
								say("@But thou art carrying too much for me to pay thee! Come back when thou art less burdened.@");
							}
						} else {
							say("@Suit thyself.@");
						}
					} else {
						say("@Aye, come back when thou hast fresh fish to sell me.@");
					}
				}

			case "fishing pole?" (remove):
				//player already has a rod (Note: this should no longer happen,
				//since every branch checks for the existence of a rod before
				//adding this conversation option)
				if (PARTY->count_objects(SHAPE_FISHING_ROD, QUALITY_ANY, FRAME_ANY)) {
					say("@Thou seem'st to have a sturdy fishing rod there. Try fishing in streams and pools, in spots where thou canst see plenty of fish.@");
				} else if (gflags[GOT_GORDONS_ROD] == true) {
					//player bought Gordon's rod already
					say("@But what hast thou done with the rod I gave thee? Lost it already? Well, I shan't part with another for thee then!@");
				} else if (wearing_fellowship) {
					//Fellowship members have it easy
					say("@Ah, thou hast no fishing pole? Well...I suppose I should be glad to lend my spare to a fellow member...",
					    "~Wouldst thou like to have it?@");
					if (askYesNo()) {
						if (UI_add_party_items(1, SHAPE_FISHING_ROD, QUALITY_ANY, FRAME_ANY, true)) {
							say("@Here thou art. Trust Thy Brother, as they say!@ He hands you a well-used fishing pole.");
							gflags[GOT_GORDONS_ROD] = true;
						} else {
							say("@Ahh, but thou art carrying too much! Return when thou hast relieved thy burden.@");
						}
					} else {
						say ("@Suit thyself. Ask me again if thou changest thy mind.@");
					}
				} else {
					//Everyone else has to pay
					say("@Ah, thou hast no fishing pole?@ He gets a calculating gleam in his eye.",
					    "~@I happen to have a spare...which I could sell to thee for only ",
					    GORDONS_ROD_PRICE,
					    " gold coins. It should pay for itself soon enough!",
						"~Wouldst thou like to have it?@");
					if (askYesNo()) {
						if (hasGold(GORDONS_ROD_PRICE)) {
							if (UI_add_party_items(1, SHAPE_FISHING_ROD, QUALITY_ANY, FRAME_ANY, true)) {
								say("@Here thou art. May it serve thee well.@ He hands you a well-used fishing pole.");
								chargeGold(GORDONS_ROD_PRICE);
								gflags[GOT_GORDONS_ROD] = true;
							} else{
								say("@Ahh, but thou art carrying too much! Return when thou hast relieved thy burden.@");
							}
						} else {
							say("@But thou dost not have enough gold!",
								"~Methinks thou shouldst consider joining the Fellowship, to better manage thine assets.@");
						}
					} else {
						say ("@Suit thyself. Ask me again if thou changest thy mind.@");
					}
				}
		}
	} else if (event == PROXIMITY) {
		//Player is nearby
		//Do his fish-selling spiel
		if (get_schedule_type() == TEND_SHOP) {
			//reorganised to make it easier to add barks
			var barks = [
				"@Fish 'n' chips!@",
				"@Hot fish 'n' chips!@",
				"@Fish 'n' chippies!@",
				"@Fish 'n' chips here!@"
			];
			var rand = UI_get_random(UI_get_array_size(barks));
			item_say(barks[rand]);
		} else {
			//do the usual schedule barks
			scheduleBarks(item);
		}
	}
}
