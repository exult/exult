/*
 *  example_bg.uc - a Black Gate shopkeeper using the shop kit.
 *
 *  Compile (the build does this to check the kit):
 *      ucc -I <exult datadir>/kits/shop -o usecode example_bg.uc
 *  Then give an NPC the function exampleBgShopkeeper as its usecode
 *  (in Exult Studio) and talk to them.  See docs/shop_kit.md.
 */
#game "blackgate"

// The kit's gump: the open book in BG.  Must come before the include.
const int SHOP_KIT_GUMP = 32;

#autonumber 0x1000
#include "shop_kit.uc"

const int EXAMPLE_BG_GOLD = 644;
const int EXAMPLE_BG_FOOD = 377;
const int EXAMPLE_BG_TORCH = 595;
const int EXAMPLE_BG_BEDROLL = 583;
const int EXAMPLE_BG_ARROWS = 722;

// The wares, with the stock kept between visits.
static var exampleBgStock;

var exampleBgWares() {
	if (!exampleBgStock) {
		// shape, frame, quality, price, stock, name, description
		exampleBgStock = [
			EXAMPLE_BG_FOOD,    0, SHOP_KIT_ANY,  2, 12, "bread", "A fresh loaf, still warm.",
			EXAMPLE_BG_TORCH,   0, SHOP_KIT_ANY,  4, -1, "",      "Burns for hours.",
			EXAMPLE_BG_BEDROLL, 0, SHOP_KIT_ANY, 20,  2, "",      "For nights under the stars.",
			EXAMPLE_BG_ARROWS,  0, SHOP_KIT_ANY,  1, 60, "",      "Fletched with goose feathers."
		];
	}
	return exampleBgStock;
}

// What the shopkeeper buys: shape, frame, quality, price each, name.
var exampleBgOffers() {
	return [EXAMPLE_BG_TORCH, 0, SHOP_KIT_ANY, 1, "", EXAMPLE_BG_ARROWS, 0, SHOP_KIT_ANY, 1, ""];
}

// The shop's callback, called on the shopkeeper (item).
// info = [index, shape, frame, quality, quantity, total].
var exampleBgShopEvent(ev, info) {
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
		exampleBgStock = shopGetWares();
		UI_item_say(item, "A fine choice!");
	} else if (ev == SHOP_KIT_SOLD) {
		UI_item_say(item, "I shall find a buyer for these.");
	}
	return info[6];
}

var exampleBgOptions() {
	// currency, currency name, haggle, callback, title
	return [EXAMPLE_BG_GOLD, "gold", 1, &exampleBgShopEvent, ""];
}

void exampleBgShopkeeper object#() () {
	if (event != 1) {
		return;
	}
	item.say("@Welcome, friend. Wouldst thou buy, or sell?@");
	converse (["buy", "sell", "bye"]) {
	case "buy":
		say("@Have a look at my wares.@");
		shopOpenAfterTalk(item, exampleBgWares(), exampleBgOptions());
		break;
	case "sell":
		say("@Show me what thou hast.@");
		shopOpenSellAfterTalk(item, exampleBgOffers(), exampleBgOptions());
		break;
	case "bye":
		say("@Fare thee well.@");
		break;
	}
}
