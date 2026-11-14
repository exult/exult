/*
 *  shop_kit.uc - a usecode shop for Exult mods (Black Gate and Serpent Isle).
 *
 *  The shop is a dynamic gump on the open book (SI gump 27, BG gump 32),
 *  laid out in Exult's own data/{bg,si}/gump_info.txt: a list of wares on
 *  the left page, the chosen ware with its price, quantity spinner, total
 *  and a Yes (buy/sell) button on the right.  See docs/shop_kit.md.
 *
 *  Include contract:
 *    - Declare the gump shape BEFORE including this file:
 *          const int SHOP_KIT_GUMP = 27;    // SI; 32 for BG
 *      (ucc has no #ifdef and usecode cannot ask which game runs, so the
 *      includer picks; a mod with its own layout names its own gump.)
 *    - Include it inside an #autonumber region; its functions are
 *      unnumbered:
 *          #autonumber 0x1000
 *          #include "shop_kit.uc"
 *    - Compile with -I <exult datadir>/kits/shop (or copy this file).
 *  All names start with shopKit / SHOP_KIT_ (the public calls with shop),
 *  and the kit declares its own constants, so it needs no other header.
 *
 *  Layout field ids (gump_info.txt, see the SHOP_KIT_FIELD_* constants)
 *  and widget callbacks: shape#(3840) list select, shape#(3841) list
 *  activate (double-click/Enter = buy), shape#(3842) quantity spinner,
 *  shape#(3843) the Yes button.  A mod restyling the shop in its patch
 *  gump_info.txt must keep these field ids and callback numbers.
 *
 *  Data (flat arrays, because usecode flattens nested array literals):
 *    wares:   7 entries per ware: shape, frame, quality (-359 = any),
 *             price, stock (-1 = unlimited), name ("" = the shape's own
 *             name), description
 *    offers:  5 entries per offer (selling): shape, frame, quality,
 *             price each, name
 *    options: currency shape (0 = 644, gold), currency name ("" =
 *             "gold"), haggle (0/1), callback (&function or 0), title
 *             ("" = "<NPC>'s wares")
 *
 *  Callback: var fn(ev, info), called on the shopkeeper NPC (item) with
 *  info = [index, shape, frame, quality, quantity, total].  For
 *  SHOP_KIT_OFFER / SHOP_KIT_SELL_OFFER it returns the agreed total;
 *  below 0 (-1), or 0 when the asking total info[6] was above 0, cancels
 *  ("No deal.").  For the other events the return value is ignored.  If the
 *  callback closes the shop (a conversation closes all gumps), the kit
 *  reopens it with the same rows, selection and quantity.
 *
 *  Intrinsics: only ones in both the BG and SI tables.  The list
 *  intrinsics (UI_set_list_row etc.) use provisional slots, see
 *  docs/dynamic_lists.md.
 */

// Engine constants, declared here so the kit needs no game header.
const int SHOP_KIT_AVATAR = -356;
const int SHOP_KIT_PARTY = -357;
const int SHOP_KIT_ANY = -359;

// The hidden "counter" bag in the shopkeeper's inventory that hosts the
// gump (a gump needs a container; NPCs themselves are not opened).
const int SHOP_KIT_COUNTER_SHAPE = 802;
const int SHOP_KIT_COUNTER_MARK = 233;
const int SHOP_KIT_GOLD = 644;
const int SHOP_KIT_MAX_QTY = 99;    // The spinner's max_val.

// Layout field ids (dynamic_texts, dynamic_shapes, dynamic_sliders,
// dynamic_lists in gump_info.txt).
const int SHOP_KIT_FIELD_TITLE = 1;      // text
const int SHOP_KIT_FIELD_LIST = 1;       // list (source 1)
const int SHOP_KIT_FIELD_NAME = 2;       // text
const int SHOP_KIT_FIELD_PREVIEW = 3;    // shape, bound to the list
const int SHOP_KIT_FIELD_DESC = 4;       // text
const int SHOP_KIT_FIELD_PRICE = 5;      // text
const int SHOP_KIT_FIELD_STOCK = 6;      // text
const int SHOP_KIT_FIELD_QTY = 7;        // text
const int SHOP_KIT_FIELD_SPINNER = 8;    // slider
const int SHOP_KIT_FIELD_TOTAL = 9;      // text
const int SHOP_KIT_FIELD_PURSE = 10;     // text
const int SHOP_KIT_FIELD_STATUS = 11;    // text
const int SHOP_KIT_FIELD_PROMPT = 12;    // text

// Ware entries (stride 7).
const int SHOP_KIT_WARE_SIZE = 7;
const int SHOP_KIT_WARE_SHAPE = 1;
const int SHOP_KIT_WARE_FRAME = 2;
const int SHOP_KIT_WARE_QUALITY = 3;
const int SHOP_KIT_WARE_PRICE = 4;
const int SHOP_KIT_WARE_STOCK = 5;
const int SHOP_KIT_WARE_NAME = 6;
const int SHOP_KIT_WARE_DESC = 7;

// Offer entries for selling (stride 5).
const int SHOP_KIT_OFFER_SIZE = 5;
const int SHOP_KIT_OFFER_SHAPE = 1;
const int SHOP_KIT_OFFER_FRAME = 2;
const int SHOP_KIT_OFFER_QUALITY = 3;
const int SHOP_KIT_OFFER_PRICE = 4;
const int SHOP_KIT_OFFER_NAME = 5;

// Options (5 entries).
const int SHOP_KIT_OPT_CURRENCY = 1;
const int SHOP_KIT_OPT_CURRENCY_NAME = 2;
const int SHOP_KIT_OPT_HAGGLE = 3;
const int SHOP_KIT_OPT_CALLBACK = 4;
const int SHOP_KIT_OPT_TITLE = 5;

// Results of shopBuy() and callback events.
const int SHOP_KIT_BOUGHT = 1;
const int SHOP_KIT_SOLD = 2;
const int SHOP_KIT_OFFER = 3;
const int SHOP_KIT_SELL_OFFER = 4;
const int SHOP_KIT_NO_MONEY = 5;
const int SHOP_KIT_TOO_HEAVY = 6;
const int SHOP_KIT_SOLD_OUT = 7;
const int SHOP_KIT_NOTHING = 8;
const int SHOP_KIT_BUSY = 9;

// Modes.
const int SHOP_KIT_MODE_BUY = 1;
const int SHOP_KIT_MODE_SELL = 2;

// State of the shop on screen (one at a time).
static var shopKitNpc;
static var shopKitCounter;
static var shopKitMode;
static var shopKitItems;      // Wares or offers.
static var shopKitNames;      // Display name per item.
static var shopKitOptions;
static var shopKitSel;
static var shopKitQty;
static var shopKitStatus;
static var shopKitBusy;
// Arguments for the deferred open (shopOpenAfterTalk).
static var shopKitPendNpc;
static var shopKitPendItems;
static var shopKitPendOptions;
static var shopKitPendMode;

/*
 *  Helpers.
 */

// Find or create the shopkeeper's counter bag.  Output: the bag or 0.
var shopKitCounterOf(npc) {
	if (!npc) {
		return 0;
	}
	var counter = UI_find_object(npc, SHOP_KIT_COUNTER_SHAPE, SHOP_KIT_COUNTER_MARK, SHOP_KIT_ANY);
	if (counter) {
		return counter;
	}
	counter = UI_create_new_object(SHOP_KIT_COUNTER_SHAPE);
	if (!counter) {
		return 0;
	}
	UI_set_item_quality(counter, SHOP_KIT_COUNTER_MARK);
	if (!UI_give_last_created(npc)) {
		UI_remove_item(counter);
		return 0;
	}
	return counter;
}

// The name a shape shows in the game, read from a temporary object.
var shopKitName(shape, frame) {
	var obj = UI_create_new_object(shape);
	if (!obj) {
		return "";
	}
	if (frame >= 0) {
		UI_set_item_frame(obj, frame);
	}
	var name = UI_get_npc_name(obj);
	UI_remove_item(obj);
	return name;
}

var shopKitOpt(n) {
	if (UI_get_array_size(shopKitOptions) < n) {
		return 0;
	}
	return shopKitOptions[n];
}

var shopKitCurrency() {
	var cur = shopKitOpt(SHOP_KIT_OPT_CURRENCY);
	if (!cur) {
		return SHOP_KIT_GOLD;
	}
	return cur;
}

var shopKitCurrencyName() {
	var name = shopKitOpt(SHOP_KIT_OPT_CURRENCY_NAME);
	if (!name || name == "") {
		return "gold";
	}
	return name;
}

var shopKitStride() {
	if (shopKitMode == SHOP_KIT_MODE_SELL) {
		return SHOP_KIT_OFFER_SIZE;
	}
	return SHOP_KIT_WARE_SIZE;
}

var shopKitCount() {
	return UI_get_array_size(shopKitItems) / shopKitStride();
}

// Entry off (1-based) of item i; shape, frame, quality and price have
// the same offsets in wares and offers.
var shopKitField(i, off) {
	return shopKitItems[(i - 1) * shopKitStride() + off];
}

var shopKitNameField(i) {
	if (shopKitMode == SHOP_KIT_MODE_SELL) {
		return shopKitField(i, SHOP_KIT_OFFER_NAME);
	}
	return shopKitField(i, SHOP_KIT_WARE_NAME);
}

var shopKitDesc(i) {
	if (shopKitMode == SHOP_KIT_MODE_SELL) {
		return "";
	}
	return shopKitField(i, SHOP_KIT_WARE_DESC);
}

// How many the party holds of item i.
var shopKitPartyCount(i) {
	return UI_count_objects(
			SHOP_KIT_PARTY, shopKitField(i, SHOP_KIT_WARE_SHAPE), shopKitField(i, SHOP_KIT_WARE_QUALITY),
			shopKitField(i, SHOP_KIT_WARE_FRAME));
}

// Buying: the stock (-1 = unlimited).  Selling: what the party holds.
var shopKitAvail(i) {
	if (shopKitMode == SHOP_KIT_MODE_SELL) {
		return shopKitPartyCount(i);
	}
	return shopKitField(i, SHOP_KIT_WARE_STOCK);
}

var shopKitEnabled(i) {
	if (i < 1 || i > shopKitCount()) {
		return 0;
	}
	return shopKitAvail(i) != 0;
}

var shopKitMaxQty(i) {
	var avail = shopKitAvail(i);
	if (avail < 0 || avail > SHOP_KIT_MAX_QTY) {
		return SHOP_KIT_MAX_QTY;
	}
	if (avail < 1) {
		return 1;
	}
	return avail;
}

var shopKitPartyGold() {
	return UI_count_objects(SHOP_KIT_PARTY, shopKitCurrency(), SHOP_KIT_ANY, SHOP_KIT_ANY);
}

// Place qty of a shape with the party, member by member, using the
// engine's own weight and volume checks.  If anything is left over, take
// back what was placed.  Output: 1 if all were placed, else 0.
var shopKitGive(shape, qty, quality, frame) {
	if (quality == SHOP_KIT_ANY) {
		quality = 0;
	}
	if (frame < 0) {
		frame = 0;
	}
	var party = UI_get_party_list();
	var placed = [];
	var left = qty;
	var i = 1;
	var cnt = UI_get_array_size(party);
	while (i <= cnt) {
		var took = 0;
		if (left > 0) {
			var before = left;
			left = UI_add_cont_items(party[i], left, shape, quality, frame, 0);
			took = before - left;
		}
		placed = [placed, took];
		i = i + 1;
	}
	if (left <= 0) {
		return 1;
	}
	i = 1;
	while (i <= cnt) {
		if (placed[i] > 0) {
			UI_remove_cont_items(party[i], placed[i], shape, quality, frame);
		}
		i = i + 1;
	}
	return 0;
}

// The list row for item i: [text, right text, shape, frame, flags].
var shopKitRow(i) {
	var name = shopKitNames[i];
	var shape = shopKitField(i, SHOP_KIT_WARE_SHAPE);
	var frame = shopKitField(i, SHOP_KIT_WARE_FRAME);
	var price = shopKitField(i, SHOP_KIT_WARE_PRICE);
	var avail = shopKitAvail(i);
	if (frame < 0) {
		frame = 0;
	}
	if (shopKitMode == SHOP_KIT_MODE_SELL) {
		var flags = 0;
		if (avail <= 0) {
			flags = 1;
		}
		return [name + " (" + avail + ")", price, shape, frame, flags];
	}
	if (avail == 0) {
		return [name, "sold out", shape, frame, 1];
	}
	if (avail < 0) {
		return [name, price, shape, frame, 0];
	}
	return [name + " (" + avail + ")", price, shape, frame, 0];
}

// Rows one at a time: a row literal inside a rows literal would flatten.
void shopKitFill() {
	UI_set_list_rows(shopKitCounter, SHOP_KIT_FIELD_LIST, []);
	var n = shopKitCount();
	var i = 1;
	while (i <= n) {
		UI_set_list_row(shopKitCounter, SHOP_KIT_FIELD_LIST, i, shopKitRow(i));
		i = i + 1;
	}
}

// The enabled row nearest after (or at) prefer, wrapping; 0 if none.
var shopKitPick(prefer) {
	var n = shopKitCount();
	if (prefer < 1) {
		prefer = 1;
	}
	var k = 0;
	while (k < n) {
		var i = (prefer - 1 + k) % n + 1;
		if (shopKitEnabled(i)) {
			return i;
		}
		k = k + 1;
	}
	return 0;
}

var shopKitTotal() {
	if (!shopKitSel) {
		return 0;
	}
	return shopKitField(shopKitSel, SHOP_KIT_WARE_PRICE) * shopKitQty;
}

void shopKitText(fid, text) {
	UI_set_gump_text(shopKitCounter, fid, text);
}

// All texts, the spinner and the prompt.
void shopKitRefresh() {
	var cur = shopKitCurrencyName();
	var title = shopKitOpt(SHOP_KIT_OPT_TITLE);
	if (!title || title == "") {
		if (shopKitMode == SHOP_KIT_MODE_SELL) {
			title = "Selling to " + UI_get_npc_name(shopKitNpc);
		} else {
			title = UI_get_npc_name(shopKitNpc) + "'s wares";
		}
	}
	shopKitText(SHOP_KIT_FIELD_TITLE, title);
	shopKitText(SHOP_KIT_FIELD_PURSE, "You have " + shopKitPartyGold() + " " + cur);
	var status = shopKitStatus;
	if (!shopKitCount()) {
		if (shopKitMode == SHOP_KIT_MODE_SELL) {
			status = "Nothing wanted.";
		} else {
			status = "Nothing for sale.";
		}
	}
	if (!status) {
		status = "";
	}
	shopKitText(SHOP_KIT_FIELD_STATUS, status);
	UI_set_slider_value(shopKitCounter, SHOP_KIT_FIELD_SPINNER, shopKitQty);
	var i = shopKitSel;
	if (!i) {
		shopKitText(SHOP_KIT_FIELD_NAME, "");
		shopKitText(SHOP_KIT_FIELD_DESC, "");
		shopKitText(SHOP_KIT_FIELD_PRICE, "");
		shopKitText(SHOP_KIT_FIELD_STOCK, "");
		shopKitText(SHOP_KIT_FIELD_QTY, "");
		shopKitText(SHOP_KIT_FIELD_TOTAL, "");
		shopKitText(SHOP_KIT_FIELD_PROMPT, "");
		return;
	}
	var price = shopKitField(i, SHOP_KIT_WARE_PRICE);
	var avail = shopKitAvail(i);
	var total = shopKitTotal();
	shopKitText(SHOP_KIT_FIELD_NAME, shopKitNames[i]);
	shopKitText(SHOP_KIT_FIELD_DESC, shopKitDesc(i));
	shopKitText(SHOP_KIT_FIELD_PRICE, "Price: " + price + " " + cur + " each");
	if (shopKitMode == SHOP_KIT_MODE_SELL) {
		shopKitText(SHOP_KIT_FIELD_STOCK, "You have: " + avail);
	} else if (avail < 0) {
		shopKitText(SHOP_KIT_FIELD_STOCK, "Plenty");
	} else {
		shopKitText(SHOP_KIT_FIELD_STOCK, "In stock: " + avail);
	}
	shopKitText(SHOP_KIT_FIELD_QTY, "Quantity: " + shopKitQty);
	shopKitText(SHOP_KIT_FIELD_TOTAL, "Total: " + total + " " + cur);
	if (shopKitMode == SHOP_KIT_MODE_SELL) {
		shopKitText(SHOP_KIT_FIELD_PROMPT, "Sell " + shopKitQty + " for " + total + " " + cur + "?");
	} else {
		shopKitText(SHOP_KIT_FIELD_PROMPT, "Buy " + shopKitQty + " for " + total + " " + cur + "?");
	}
}

// Open the counter's gump and show the current state.  Output: 1/0.
var shopKitShow() {
	if (!shopKitCounter || !UI_open_gump(shopKitCounter, SHOP_KIT_GUMP)) {
		return 0;
	}
	shopKitFill();
	shopKitSel = shopKitPick(shopKitSel);
	if (shopKitSel) {
		UI_set_list_selection(shopKitCounter, SHOP_KIT_FIELD_LIST, shopKitSel);
		if (shopKitQty > shopKitMaxQty(shopKitSel)) {
			shopKitQty = shopKitMaxQty(shopKitSel);
		}
	}
	if (shopKitQty < 1) {
		shopKitQty = 1;
	}
	shopKitRefresh();
	return 1;
}

var shopKitStart(npc, items, options, mode) {
	var counter = shopKitCounterOf(npc);
	if (!counter) {
		return 0;
	}
	if (shopKitCounter && shopKitCounter != counter) {
		UI_close_gump2(shopKitCounter);
	}
	shopKitNpc = npc;
	shopKitCounter = counter;
	shopKitMode = mode;
	shopKitItems = items;
	shopKitOptions = options;
	shopKitSel = 0;
	shopKitQty = 1;
	shopKitStatus = "";
	// Names once per opening (temporary objects for "" names).
	shopKitNames = [];
	var n = shopKitCount();
	var i = 1;
	while (i <= n) {
		var name = shopKitNameField(i);
		if (!name || name == "") {
			name = shopKitName(shopKitField(i, SHOP_KIT_WARE_SHAPE), shopKitField(i, SHOP_KIT_WARE_FRAME));
		}
		shopKitNames = [shopKitNames, name];
		i = i + 1;
	}
	return shopKitShow();
}

var shopKitInfo(total) {
	var i = shopKitSel;
	return [i, shopKitField(i, SHOP_KIT_WARE_SHAPE), shopKitField(i, SHOP_KIT_WARE_FRAME),
			shopKitField(i, SHOP_KIT_WARE_QUALITY), shopKitQty, total];
}

/*
 *  Public calls.
 */

// 1 if the shop's gump is open.
var shopIsOpen() {
	if (!shopKitCounter) {
		return 0;
	}
	// The spinner's minimum is 1, so 0 means no gump.
	return UI_get_slider_value(shopKitCounter, SHOP_KIT_FIELD_SPINNER) > 0;
}

// Call the shop's callback on the NPC; reopen the shop if the callback
// closed it (a conversation closes all gumps).  Output: its return value
// (info[6], the total, without a callback).
var shopKitCall(ev, info) {
	var cb = shopKitOpt(SHOP_KIT_OPT_CALLBACK);
	if (!cb) {
		return info[6];
	}
	var was_open = shopIsOpen();
	var ret = shopKitNpc->(*cb)(ev, info);
	if (was_open && !shopIsOpen()) {
		shopKitShow();
	}
	return ret;
}

var shopOpen(npc, wares, options) {
	return shopKitStart(npc, wares, options, SHOP_KIT_MODE_BUY);
}

var shopOpenSell(npc, offers, options) {
	return shopKitStart(npc, offers, options, SHOP_KIT_MODE_SELL);
}

// Runs from a script on the Avatar, after the conversation has ended.
void shopKitDeferredOpen object#() () {
	shopKitStart(shopKitPendNpc, shopKitPendItems, shopKitPendOptions, shopKitPendMode);
}

void shopKitDefer(npc, items, options, mode) {
	shopKitPendNpc = npc;
	shopKitPendItems = items;
	shopKitPendOptions = options;
	shopKitPendMode = mode;
	script SHOP_KIT_AVATAR after 2 ticks {
		// A later script on the Avatar must not cancel this one.
		nohalt;
		call shopKitDeferredOpen;
	}
}

// From dialogue: the conversation closes all gumps when it ends, so open
// the shop 2 ticks later.
void shopOpenAfterTalk(npc, wares, options) {
	shopKitDefer(npc, wares, options, SHOP_KIT_MODE_BUY);
}

void shopOpenSellAfterTalk(npc, offers, options) {
	shopKitDefer(npc, offers, options, SHOP_KIT_MODE_SELL);
}

// Select row i (does not run select_fn).  Output: 1/0.
var shopSelect(i) {
	if (!shopIsOpen() || !shopKitEnabled(i)) {
		return 0;
	}
	shopKitSel = i;
	shopKitQty = 1;
	shopKitStatus = "";
	UI_set_list_selection(shopKitCounter, SHOP_KIT_FIELD_LIST, i);
	shopKitRefresh();
	return 1;
}

// Set the quantity, clamped to 1..min(stock, 99).  Output: the quantity.
var shopSetQuantity(n) {
	var max = SHOP_KIT_MAX_QTY;
	if (shopKitSel) {
		max = shopKitMaxQty(shopKitSel);
	}
	if (n > max) {
		n = max;
	}
	if (n < 1) {
		n = 1;
	}
	shopKitQty = n;
	if (shopIsOpen()) {
		shopKitRefresh();
	}
	return n;
}

var shopGetQuantity() {
	return shopKitQty;
}

var shopGetTotal() {
	return shopKitTotal();
}

// The wares (or offers) with the stock as it is now.
var shopGetWares() {
	return shopKitItems;
}

void shopClose() {
	if (shopKitCounter) {
		UI_close_gump2(shopKitCounter);
	}
}

// After a sale: rows, selection (the next enabled row if this one ran
// out) and quantity.
void shopKitAfterSale(status) {
	shopKitStatus = status;
	shopKitFill();
	var sel = shopKitPick(shopKitSel);
	if (sel != shopKitSel) {
		shopKitQty = 1;
	}
	shopKitSel = sel;
	if (sel) {
		UI_set_list_selection(shopKitCounter, SHOP_KIT_FIELD_LIST, sel);
		if (shopKitQty > shopKitMaxQty(sel)) {
			shopKitQty = shopKitMaxQty(sel);
		}
	}
	shopKitRefresh();
}

var shopKitFailed(code, status) {
	shopKitStatus = status;
	shopKitRefresh();
	return code;
}

// A haggle reply that cancels: below 0, or 0 for a priced ware (a
// callback that returns nothing must not give the goods away).
var shopKitNoDeal(total, asking) {
	return total < 0 || (total == 0 && asking > 0);
}

// Buying: stock, purse, then (with haggling) the OFFER callback's agreed
// total; then place the goods and take the coins.
var shopKitBuy() {
	var i = shopKitSel;
	if (!i) {
		return shopKitFailed(SHOP_KIT_NOTHING, "");
	}
	var stock = shopKitField(i, SHOP_KIT_WARE_STOCK);
	if (stock == 0 || (stock > 0 && shopKitQty > stock)) {
		return shopKitFailed(SHOP_KIT_SOLD_OUT, "That is sold out.");
	}
	var total = shopKitTotal();
	if (shopKitPartyGold() < total) {
		return shopKitFailed(SHOP_KIT_NO_MONEY, "You cannot afford that.");
	}
	if (shopKitOpt(SHOP_KIT_OPT_HAGGLE)) {
		var asking = total;
		total = shopKitCall(SHOP_KIT_OFFER, shopKitInfo(asking));
		if (shopKitNoDeal(total, asking)) {
			return shopKitFailed(SHOP_KIT_NOTHING, "No deal.");
		}
		if (shopKitPartyGold() < total) {
			return shopKitFailed(SHOP_KIT_NO_MONEY, "You cannot afford that.");
		}
	}
	var shape = shopKitField(i, SHOP_KIT_WARE_SHAPE);
	var frame = shopKitField(i, SHOP_KIT_WARE_FRAME);
	var quality = shopKitField(i, SHOP_KIT_WARE_QUALITY);
	var qty = shopKitQty;
	if (!shopKitGive(shape, qty, quality, frame)) {
		return shopKitFailed(SHOP_KIT_TOO_HEAVY, "You cannot carry that.");
	}
	if (total > 0) {
		UI_remove_party_items(total, shopKitCurrency(), SHOP_KIT_ANY, SHOP_KIT_ANY);
	}
	if (stock > 0) {
		shopKitItems[(i - 1) * SHOP_KIT_WARE_SIZE + SHOP_KIT_WARE_STOCK] = stock - qty;
	}
	var info = shopKitInfo(total);
	shopKitAfterSale("Thank you.");
	shopKitCall(SHOP_KIT_BOUGHT, info);
	return SHOP_KIT_BOUGHT;
}

// Selling: count, then (with haggling) the SELL_OFFER callback's agreed
// total; then pay the coins and only then take the goods, so a failure
// rolls back exactly (no goods rebuilt with a guessed quality or frame).
var shopKitSell() {
	var i = shopKitSel;
	if (!i) {
		return shopKitFailed(SHOP_KIT_NOTHING, "");
	}
	var have = shopKitPartyCount(i);
	if (have < shopKitQty) {
		return shopKitFailed(SHOP_KIT_NOTHING, "You do not have that many.");
	}
	var total = shopKitTotal();
	if (shopKitOpt(SHOP_KIT_OPT_HAGGLE)) {
		var asking = total;
		total = shopKitCall(SHOP_KIT_SELL_OFFER, shopKitInfo(asking));
		if (shopKitNoDeal(total, asking)) {
			return shopKitFailed(SHOP_KIT_NOTHING, "No deal.");
		}
	}
	var shape = shopKitField(i, SHOP_KIT_OFFER_SHAPE);
	var frame = shopKitField(i, SHOP_KIT_OFFER_FRAME);
	var quality = shopKitField(i, SHOP_KIT_OFFER_QUALITY);
	var qty = shopKitQty;
	// Pay first: shopKitGive takes back what it placed if the coins do
	// not all fit, so the goods are never touched on TOO_HEAVY.
	if (total > 0 && !shopKitGive(shopKitCurrency(), total, SHOP_KIT_ANY, 0)) {
		return shopKitFailed(SHOP_KIT_TOO_HEAVY, "You cannot carry the coins.");
	}
	if (!UI_remove_party_items(qty, shape, quality, frame)) {
		// The party no longer has that many: take the coins back.
		if (total > 0) {
			UI_remove_party_items(total, shopKitCurrency(), SHOP_KIT_ANY, SHOP_KIT_ANY);
		}
		return shopKitFailed(SHOP_KIT_NOTHING, "You do not have that many.");
	}
	var info = shopKitInfo(total);
	shopKitAfterSale("Thank you.");
	shopKitCall(SHOP_KIT_SOLD, info);
	return SHOP_KIT_SOLD;
}

// Buy (or sell, in a shop opened with shopOpenSell) the selected row at
// the current quantity.  Output: a SHOP_KIT_* result code.
var shopBuy() {
	if (shopKitBusy) {
		return SHOP_KIT_BUSY;
	}
	if (!shopIsOpen()) {
		return SHOP_KIT_NOTHING;
	}
	shopKitBusy = 1;
	var ret;
	if (shopKitMode == SHOP_KIT_MODE_SELL) {
		ret = shopKitSell();
	} else {
		ret = shopKitBuy();
	}
	shopKitBusy = 0;
	return ret;
}

/*
 *  Widget callbacks (item = the counter bag).
 */

void shopKitOnSelect shape#(3840) () {
	shopSelect(UI_get_list_selection(item, SHOP_KIT_FIELD_LIST));
}

void shopKitOnActivate shape#(3841) () {
	shopBuy();
}

void shopKitOnQuantity shape#(3842) () {
	shopSetQuantity(UI_get_slider_value(item, SHOP_KIT_FIELD_SPINNER));
}

void shopKitOnYes shape#(3843) () {
	shopBuy();
}
