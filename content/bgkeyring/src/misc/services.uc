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
 *	This source file contains functions used in general services (selling of
 *	items, spells and healing).
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */


enum selling_result {
	NO_QUANTITY							= 0,
	SELL_SUCCEEDED						= 1,
	CANNOT_CARRY						= 2,
	NOT_ENOUGH_GOLD						= 3
};

enum healing_type {
	USE_HERBS							= 1,
	USE_BANDAGES						= 2,
	USE_POTIONS							= 3,
	USE_MAGIC							= 4
};

enum healing_prices {
	SERVICE_NONE						= 0,
	SERVICE_CURE						= 1,
	SERVICE_HEAL						= 2,
	SERVICE_RESURRECT					= 3
};

var giveItemsToParty (var quantity, var shapenum, var quality, var framenum, var flag, var unknown) {
	var startcount = PARTY->count_objects(shapenum, quality, framenum);
	var itemconts = UI_add_party_items(quantity, shapenum, quality, framenum, flag);

	if (UI_get_array_size(itemconts) == 1) {
		return [0, 0];
	} else {
		var finalcount = PARTY->count_objects(shapenum, quality, framenum);
		var delta = finalcount - startcount;

		var companions = removeFromArrayByIndex(itemconts, 1);

		if (companions) {
			if (quantity == 1) {
				say("@Thou art so encumbered! Perhaps one of thy friends might be willing to carry this?@");
			} else {
				say("@Thou art so encumbered! Let me give some of this to thy friends.@");
			}
			var msgs = ["@I'll help carry it.@", "@I will carry that.@", "@I will carry some.@", "@Let me help with that.@"];

			for (npc in companions) {
				interjectIfPossible(UI_get_npc_number(npc), msgs[UI_get_random(UI_get_array_size(msgs))]);
			}
		}

		if (delta > 0) {
			if (quantity == 1) {
				say("@Since thou art so encumbered, I shall set this upon the ground.@");
			} else if (delta == 1) {
				say("@Since not one of you can hold another, I shall place this last remaining one upon the ground.@");
			} else {
				say("@Since thou art so encumbered, I shall place the remaining ",
				    delta, " of these upon the ground.@");
			}
			struct<Position> pos = AVATAR->get_object_position();
			var groundobjs = shapenum->create_new_object();
			groundobjs->set_item_frame(framenum);
			groundobjs->set_item_quality(quality);
			if (delta > 1) {
				if (groundobjs->set_item_quantity(delta) == 0) {
					UI_update_last_created(pos);
					while (delta > 1) {
						groundobjs = shapenum->create_new_object();
						groundobjs->set_item_frame(framenum);
						groundobjs->set_item_quality(quality);
						UI_update_last_created(pos);
						delta -= 1;
					}
				} else {
					UI_update_last_created(pos);
				}
			} else {
				UI_update_last_created(AVATAR->get_object_position());
			}
		}
		return itemconts;
	}
}

var sellAmountToPartyExt (var shapenum, var framenum, var base_quant, var price, var max_quant, var min_quant, var money_shape) {
	var amount;
	var finalquant;

	//Default to standard gold coins
	if (!money_shape) {
		money_shape = SHAPE_GOLD;
	}
	var partygold = PARTY->count_objects(money_shape, QUALITY_ANY, FRAME_ANY);

	if (max_quant == 0) {
		amount = 1;
	} else {
		amount = UI_input_numeric_value(min_quant, max_quant, 1, 1);
	}
	if (price > 1) {
		finalquant = price * amount;
	} else {
		finalquant = amount;
	}
	if (amount == 0) {
		return NO_QUANTITY;
	} else {
		if (partygold >= (base_quant * amount)) {
			UI_remove_party_items((base_quant * amount), money_shape, QUALITY_ANY, SHAPE_ANY, true);
			giveItemsToParty(finalquant, shapenum, QUALITY_ANY, framenum, false, true);
			return SELL_SUCCEEDED;
		} else {
			return NOT_ENOUGH_GOLD;
		}
	}
}

var promptForPayment (var price, var dialog) {
	if (price > 0) {
		say(dialog[2], price, dialog[3]);
		if (askYesNo()) {
			if (!hasGold(price)) {
				say(dialog[4]);
				return false;
			} else {
				chargeGold(price);
				return true;
			}
		} else {
			say(dialog[5]);
			return false;
		}
	}
	return true;
}

static var service_healing_targets;
static var service_healing_spell;

void serviceMultipleSpellHealing object#() () {
	if (event == SCRIPTED) {
		var spellname = service_healing_spell;
		var circle = getSpellCircle(spellname);
		var spell = getIndexForSpell(circle, spellname);
		var targetlist = service_healing_targets;

		if (isTargetedSpell(circle, spell)) {
			var target = targetlist[1];
			targetlist = removeFromArray(targetlist, target);
			target->set_intercept_item();
		} else {
			targetlist = [];
		}

		if (targetlist) {
			script item after 8 ticks {
				nohalt;
				call serviceMultipleSpellHealing, SCRIPTED;
			}
		} else {
			service_healing_spell = "";
		}

		service_healing_targets = targetlist;

		npcCastSpellDialog(item,
			spellname,
			spellitemGetTalkCast(get_npc_number()));
	}
}

var getServiceHealingDialog (var npcnum) {
	switch (npcnum) {
		case REYNA:
			return [
				"@What is thy need?@",
				"@I must charge thee ", " gold. Is this price agreeable?@",
				"@I am sorry, " + getPoliteTitle() + ", but thou dost not have enough gold. Perhaps, I will be able to aid thee next time.@",
				"@Then I cannot help thee, " + getPoliteTitle() + ". I am truly sorry, but my fees are set.@",
				"@Who needs to be ", "?@",
				"@None of you seem to be wounded.@~~She appears pleased.",
				"@None of you seem to be poisoned.@~~She appears pleased.",
				"@I am sorry, but thou hast not presented anyone to me who requires mine assistance. If there is someone who truly needs my skills, I must have a closer look.@",
				"@Art thou sure? Some of thy friends still need to be healed.@",
				"@Art thou sure? Some of thy friends still appear to be poisoned.@",
				"@Art thou sure? Some of thy friends still appear to require mine aid.@",
				"@I am glad of that, " + getPoliteTitle() + ". I am happy to help those in need, but I would be far happier if there were never a need!@"
			];
		case CSIL:
			return [
				"@Which of my services dost thou have need of?@",
				"@My price is ", " gold. Is this price agreeable?@",
				"@Tsk, tsk. Thou dost not have enough gold for the service. I do hope I may help thee another day.@",
				"@Then thou must look elsewhere for that service.@",
				"@Who dost thou wish to be ", "?@",
				"@It pleases me that none of you are wounded.@",
				"@I am pleased that none of you are poisoned.@",
				"@I do not see anyone who needs resurrecting. I must be able to see the body to resurrect. If thou art carrying thy friend, pray lay them on the ground so that I may attend to them.@",
				"@Art thou sure? Some of thy friends still need to be healed.@",
				"@Art thou sure? Some of thy friends still appear to be poisoned.@",
				"@Art thou sure? Some of thy friends still appear to require mine aid.@",
				"@Very well. It pleases me that thou art healthy.@"
			];
		case ELAD:
			return [
				"@Which of my services dost thou have need of?@",
				"@My price is ", " gold. Dost thou agree?@",
				"@Thou dost not have that much gold! Mayhaps thou couldst return with more and purchase the service then.@",
				"@Then thou must look elsewhere for that service.@",
				"@Whom dost thou wish to have ", "?@",
				"@Well, I do not see anyone wounded here.@ He laughs.",
				"@Well, no one here is poisoned, it seems.@ He laughs.",
				"@Well, I do not seem to see anyone who needs mine assistance. Unless thou art carrying someone in thy packs....@ He laughs.",
				"@Art thou sure? Some of thy friends still need to be healed.@",
				"@Art thou sure? Some of thy friends still appear to be poisoned.@",
				"@Art thou sure? Some of thy friends still appear to require mine aid.@",
				"@Though I want thy business, I am pleased to see my services are not needed!@"
			];
		case LEIGH:
			return [
				"@Which of my services dost thou have need of?@",
				"@My price is ", " gold. Art thou interested?@",
				"@Thou dost not have enough gold! Mayhaps thou couldst return when thou hast more.@",
				"@Then thou must go elsewhere.@",
				"@Who dost thou wish to have ", "?@",
				"@None of you seem to be wounded.@~~She appears pleased.",
				"@None of you seem to be poisoned.@~~She appears pleased.",
				"@There seems to be no one who needs such assistance. Perhaps, if I have overlooked anyone, thou couldst set him or her before me.@",
				"@Art thou sure? Some of thy friends still need to be healed.@",
				"@Art thou sure? Some of thy friends still appear to be poisoned.@",
				"@Art thou sure? Some of thy friends still appear to require mine aid.@",
				"@Excellent, thou art uninjured!@"
			];
		case INMANILEM:
			return [
				"@To need which of my services?@",
				"@To charge ", " gold. To still want my services?@",
				"@To have not that much gold! To perhaps return with more and purchase the service then.@",
				"@To be sorry. To look elsewhere for that service then.@",
				"@To want to ", " whom?@",
				"@To not see anyone in need of healing.@",
				"@To not see anyone poisoned.@",
				"@To not see anyone who is in need of resurrection. To have to see the body to save the spirit. To lay your companion on the ground so that I may return them to this world.@",
				"@To ask if thou art sure. To see that some of thy friends still need to be healed.@",
				"@To ask if thou art sure. To see that some of thy friends still appear to be poisoned.@",
				"@To ask if thou art sure. To see that some of thy friends still appear to require mine aid.@",
				"@To have no need for my healing.@"
			];
		case CHANTU:
			return [
				"@Which of my services dost thou need?@",
				"@My price is ", " gold. Is this satisfactory?@",
				"@Thou dost not have any gold. I am truly sorry. I cannot help thee until thou canst provide the proper fee.@ Chantu bows respectfully.",
				"@Then I am truly sorry. I must charge what I must charge. We do not live in prosperous times.@",
				"@Who dost thou wish to be ", "?@",
				"@I apologize, " + getPoliteTitle() + ", but I do not see anyone who is in need of healing.@",
				"@I apologize, " + getPoliteTitle() + ", but I do not see anyone who needs to be cured of poison.@",
				"@I apologize, " + getPoliteTitle() + ", but I do not see anyone who is in need of resurrection. I must be able to see the body to save the spirit. If thou art carrying thy misfortunate friend, pray lay them on the ground so that I may return them to this world.@",
				"@Some of thy friends still need to be healed; I will be here shouldst thou reconsider.@",
				"@Some of thy friends still appear to be poisoned; I will be here shouldst thou reconsider.@",
				"@Some of thy friends still appear to require mine aid; I will be here shouldst thou reconsider.@",
				"@So thou art healthy? 'Tis good news. If thou dost need my services in the future, do not hesitate to return.@"
			];
		case JAANA:
			return [
				"@Which of my services dost thou have need of?@",
				"@My price is ", " gold. Is this price agreeable?@",
				"@Thou dost not have that much gold! Mayhaps thou couldst return with more and purchase the service then.@",
				"@Then thou must look elsewhere for that service.@",
				"@Who dost thou wish to be ", "?@",
				"@I do not see anyone who is in need of healing.@",
				"@I do not see anyone who needs to be cured of poison.@",
				"@I do believe I am going blind. I do not see anyone who is in need of resurrection. Art thou fooling me again, or art thou hiding the injured one? I must be able to see the person to help them. If thou art carrying thy friend in thy pack, pray lay them on the ground so that I may perform my duties as thou hast requested.@",
				"@Art thou sure? I believe that some of thy friends still need to be healed.@",
				"@Art thou sure? I believe that some of thy friends still appear to be poisoned.@",
				"@Art thou sure? I believe that some of thy friends still appear to require mine aid.@",
				"@Avatar! Thou dost tell me to prepare to heal and then thou dost tell me 'Nobody'! Is this thine idea of a joke? Healing is a serious business!@"
			];
		case LAURIANNA:
			return [
				"@Which mode of healing dost thou wish?@",
				"", "",
				"",
				"",
				"@Who dost thou wish to be ", "?@",
				"@None of you seem to be wounded.@~~She appears pleased.",
				"@None of you seem to be poisoned.@~~She appears pleased.",
				"@I am sorry, but thou hast not presented anyone to me who requires mine assistance. If there is someone who truly needs my skills, I must have a closer look.@",
				"@Art thou sure? Some of thy friends still need to be healed.@",
				"@Art thou sure? Some of thy friends still appear to be poisoned.@",
				"@Art thou sure? Some of thy friends still appear to require mine aid.@",
				"@I am glad that thou art not in need of healing!@"
			];
		case LORD_BRITISH:
			return [
				"@Of which service dost thou have need?@",
				"", "",
				"",
				"",
				"@Who dost thou wish to be ", "?@",
				"@I do apologize, " + getAvatarName() + ", but I do not see anyone who must be healed.@",
				"@I do apologize, " + getAvatarName() + ", but I do not see anyone who must be cured of poison.@",
				"@I do apologize, " + getAvatarName() + ", but I do not see anyone who must be resurrected. I must be able to see the body. If thou art carrying thine unlucky companion, please lay them on the ground.@",
				"@Art thou sure? I can see that some of thy friends still need to be healed.@",
				"@Art thou sure? I can see that some of thy friends still appear to be poisoned.@",
				"@Art thou sure? I can see that some of thy friends still appear to require mine aid.@",
				"@'Tis good to hear that thou art well. Do not hesitate to come and see me if thou dost need healing of any kind.@"
			];
	}
}

var getServiceHealingPricelist (var npcnum) {
	switch (npcnum) {
		case REYNA:
			if (gflags[GAVE_REYNA_FLOWERS]) {
				return [5, 15, 200];
			} else {
				return [10, 30, 400];
			}
		case JAANA:
			if ((npcnum->get_npc_object() in UI_get_party_list()) ||
				(npcnum->get_schedule_type() == WAIT)) {
				return 0;
			} else {
				return [15, 30, 400];
			}
		case LAURIANNA:
		case LORD_BRITISH:
			return 0;
		case CSIL:
			return [30, 40, 450];
		case ELAD:
			return [10, 25, 425];
		case LEIGH:
			return [8, 25, 385];
		case INMANILEM:
			return [10, 25, 430];
		case CHANTU:
			return [15, 30, 400];
	}
}

void serviceHeal () {
	UI_push_answers();

	var npcnum = get_npc_number();
	// Ensure proper initialization:
	var npcind = spellitemGetNPCIndex(npcnum);
	var dialog = getServiceHealingDialog(npcnum);
	var price_list = getServiceHealingPricelist(npcnum);
	var magicheal = (!get_item_flag(NECROMANCER) && (get_item_flag(MAGE_CLASS) || get_item_flag(BARD_CLASS)));

	var choices = ["none"];
	var healing_spells = [];

	if (magicheal) {
		healing_spells = getLeveledSpellList(item,
				get_item_flag(HEALER),
				["Cure", "Mass cure", "Heal", "Great heal", "Restoration", "Resurrect", "Mass resurrect"],
				[1, 2, 3, 5, 7, 8, 8],
				global_spells_unknown[npcind]);
		if (("Cure" in healing_spells) || ("Mass cure" in healing_spells)) {
			choices << "curing";
		}
		if (("Heal" in healing_spells) || ("Great heal" in healing_spells) || ("Restoration" in healing_spells)) {
			choices << "healing";
		}
		if (("Resurrect" in healing_spells) || ("Mass resurrect" in healing_spells)) {
			choices << "resurrection";
		}
	} else {
		choices << ["curing", "healing"];
	}

	say(dialog[1]);
	var reply = chooseFromMenu2([choices]) - 1;
	if (reply) {
		var targets = [];
		var bodynpcs = [];
		if (reply == SERVICE_HEAL) {
			targets = filterListByRelHits(UI_get_party_list(), 4);
		} else if (reply == SERVICE_CURE) {
			targets = filterListByFlag(UI_get_party_list(), POISONED, true);
		} else if (reply == SERVICE_RESURRECT) {
			var bodyshapes = [SHAPE_BODIES_1, SHAPE_BODIES_2, SHAPE_LARGE_BODIES, SHAPE_NEW_BODIES];
			var bodies = [];
			for (shnum in bodyshapes) {
				bodies = [bodies, find_nearby(shnum, 25, MASK_NONE)];
			}
			for (body in bodies) {
				if (body->get_body_npc()) {
					targets << body;
					bodynpcs << body->get_body_npc();
				}
			}
		}

		var numtargets = UI_get_array_size(targets);
		if (numtargets == 0) {
			if (reply == SERVICE_HEAL) {
				say(dialog[8]);
			} else if (reply == SERVICE_CURE) {
				say(dialog[9]);
			} else if (reply == SERVICE_RESURRECT) {
				say(dialog[10]);
			}
		} else {
			numtargets = UI_get_array_size(targets);
			var namelist;
			if (reply == SERVICE_RESURRECT) {
				namelist = bodynpcs->get_npc_name();
			} else {
				namelist = targets->get_npc_name();
			}
			var choice = 1;
			var heal_everyone = true;
			if (numtargets > 1 && price_list) {
				if (reply == SERVICE_HEAL) {
					say(dialog[6], "healed", dialog[7]);
				} else if (reply == SERVICE_CURE) {
					say(dialog[6], "cured of poison", dialog[7]);
				} else if (reply == SERVICE_RESURRECT) {
					say(dialog[6], "resurrected", dialog[7]);
				}
				choice = chooseFromMenu2(["Nobody", "Everyone", namelist]) - 1;
				heal_everyone = (choice == 1);
				if (choice > 1) {
					choice -= 1;
				}
			} else if (price_list) {
				heal_everyone = false;
			}

			if (!choice) {
				targets = 0;
				if (reply == SERVICE_HEAL) {
					say(dialog[11]);
				} else if (reply == SERVICE_CURE) {
					say(dialog[12]);
				} else if (reply == SERVICE_RESURRECT) {
					say(dialog[13]);
				}
			} else if (price_list) {
				var price = price_list[reply];
				if (heal_everyone && price) {
					if (magicheal) {
						if (((reply == SERVICE_HEAL) && ("Restoration" in healing_spells)) ||
							((reply == SERVICE_CURE) && ("Mass cure" in healing_spells)) ||
							((reply == SERVICE_RESURRECT) && ("Mass resurrect" in healing_spells))) {
							price = (price * 5)/2;
						} else {
							price *= numtargets;
						}
					} else {
						price *= numtargets;
					}
				} else {
					targets = [targets[choice]];
					numtargets = 1;
				}
				if (!promptForPayment(price, dialog)) {
					targets = 0;
				}
			}

			if (targets) {
				if (magicheal) {
					var spell = 0;
					//Try magic healing first
					if (reply == SERVICE_HEAL) {
						if (numtargets > 1 && ("Restoration" in healing_spells)) {
							service_healing_spell = "Restoration";
						} else if ("Great heal" in healing_spells) {
							service_healing_spell = "Great heal";
						}
					} else if (reply == SERVICE_CURE) {
						if (numtargets > 1 && ("Mass cure" in healing_spells)) {
							service_healing_spell = "Mass cure";
						} else if ("Cure" in healing_spells) {
							service_healing_spell = "Cure";
						}
					} else if (reply == SERVICE_RESURRECT) {
						if (numtargets > 1 && ("Mass resurrect" in healing_spells)) {
							//Should only happen for LB and Laurianna
							service_healing_spell = "Mass resurrect";
						} else if ("Resurrect" in healing_spells) {
							service_healing_spell = "Resurrect";
						}
					}
					if (service_healing_spell) {
						service_healing_targets = targets;
						script item call serviceMultipleSpellHealing, SCRIPTED;
						abort;
					}
				}
				//Normal healing. Magical healers fallback to it if they
				//don't know enough spells.
				if (reply == SERVICE_HEAL) {
					var verbs = [" dresses ", " tends to "];
					var msgs = [
						"'s wounds with bandages soaked in an infusion of Ginseng. The wounds heal quickly afterwards.@",
						"'s wounds and feeds the patient a yellow potion, which immediately closes the wounds.@",
						"'s wounds by applying a herbal unguent upon them. Not long afterwards, the wounds are fully healed.@"
					];
					while (numtargets) {
						var currtarget = targets[numtargets];
						say("@"+ get_npc_name() + verbs[UI_get_random(UI_get_array_size(verbs))] + currtarget->get_npc_name() + msgs[UI_get_random(UI_get_array_size(msgs))]);
						var str = currtarget->get_npc_prop(STRENGTH);
						var hps = currtarget->get_npc_prop(HEALTH);
						currtarget->set_npc_prop(HEALTH, str - hps);
						numtargets -= 1;
					}
				} else if (reply == SERVICE_CURE) {
					var verbs = [" feeds ", " gives "];
					var msgs = [
						" an infusion of Ginseng, which nullifies the potion after a while.@",
						" a red potion, which cancels the poison almost immeditatelly.@",
						" a herbal antidote. After a while, the poison is gone.@"
					];
					while (numtargets) {
						var currtarget = targets[numtargets];
						say("@"+ get_npc_name() + verbs[UI_get_random(UI_get_array_size(verbs))] + currtarget->get_npc_name() + msgs[UI_get_random(UI_get_array_size(msgs))]);
						currtarget->clear_item_flag(POISONED);
						numtargets -= 1;
					}
				} else if (reply == SERVICE_RESURRECT) {
					say("@This message should never appear. Warn the programmer: there is a bug with resurrection and normal healing.@");
				}
			}
		}
	} else {
		say(dialog[14]);
	}
	UI_pop_answers();
}
