/*
 *  example_si.uc - a Serpent Isle shopkeeper using the shop kit.
 *
 *  Compile (the build does this to check the kit):
 *      ucc -I <exult datadir>/kits/shop -o usecode example_si.uc
 *  Then give an NPC the function exampleSiShopkeeper as its usecode
 *  (in Exult Studio) and talk to them.  See docs/shop_kit.md.
 */
#game "serpentisle"

// The kit's gump: the open book in SI.  Must come before the include.
const int SHOP_KIT_GUMP = 27;

#autonumber 0x1000
#include "shop_kit.uc"

const int EXAMPLE_SI_GUILDER = 952;
const int EXAMPLE_SI_FOOD = 377;
const int EXAMPLE_SI_TORCH = 595;
const int EXAMPLE_SI_BEDROLL = 583;
const int EXAMPLE_SI_BANDAGE = 827;

// The wares, with the stock kept between visits.
static var exampleSiStock;

var exampleSiWares() {
	if (!exampleSiStock) {
		// shape, frame, quality, price, stock, name, description
		exampleSiStock = [
			EXAMPLE_SI_FOOD,    0, SHOP_KIT_ANY,  3, 12, "bread", "A fresh loaf, still warm.",
			EXAMPLE_SI_TORCH,   0, SHOP_KIT_ANY,  5, -1, "",      "Burns for hours.",
			EXAMPLE_SI_BEDROLL, 0, SHOP_KIT_ANY, 25,  2, "",      "For nights under the stars.",
			EXAMPLE_SI_BANDAGE, 0, SHOP_KIT_ANY,  8,  6, "",      "Clean linen for wounds."
		];
	}
	return exampleSiStock;
}

// What the shopkeeper buys: shape, frame, quality, price each, name.
var exampleSiOffers() {
	return [EXAMPLE_SI_TORCH, 0, SHOP_KIT_ANY, 2, "", EXAMPLE_SI_BANDAGE, 0, SHOP_KIT_ANY, 3, ""];
}

// The shop's callback, called on the shopkeeper (item).
// info = [index, shape, frame, quality, quantity, total].
var exampleSiShopEvent(ev, info) {
	if (ev == SHOP_KIT_OFFER) {
		// Haggling: a tenth off for five or more.  Return the agreed total
		// (-1, or 0 for a priced ware, cancels).  If a conversation here
		// closes the shop, the kit reopens it afterwards (tested headless
		// with UI_show_npc_face/UI_remove_npc_face, not with a full
		// dialogue).
		var total = info[6];
		if (info[5] >= 5) {
			total = total - total / 10;
			UI_item_say(item, "For thee, a better price.");
		}
		return total;
	}
	if (ev == SHOP_KIT_BOUGHT) {
		exampleSiStock = shopGetWares();
		UI_item_say(item, "A fine choice!");
	} else if (ev == SHOP_KIT_SOLD) {
		UI_item_say(item, "I shall find a buyer for these.");
	}
	return info[6];
}

var exampleSiOptions() {
	// currency, currency name, haggle, callback, title
	return [EXAMPLE_SI_GUILDER, "guilders", 1, &exampleSiShopEvent, ""];
}

void exampleSiShopkeeper object#() () {
	if (event != 1) {
		return;
	}
	item.say("@Welcome, traveller. Wouldst thou buy, or sell?@");
	converse (["buy", "sell", "bye"]) {
	case "buy":
		say("@Have a look at my wares.@");
		shopOpenAfterTalk(item, exampleSiWares(), exampleSiOptions());
		break;
	case "sell":
		say("@Show me what thou hast.@");
		shopOpenSellAfterTalk(item, exampleSiOffers(), exampleSiOptions());
		break;
	case "bye":
		say("@Fare thee well.@");
		break;
	}
}
