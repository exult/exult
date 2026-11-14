# Shop Kit

## Overview

The shop kit is a ready-made shop for Black Gate and Serpent Isle mods. A mod includes one usecode file, `shop_kit.uc`, describes its wares in a flat array and calls `shopOpen` (or `shopOpenAfterTalk` from dialogue). The player sees an open book: the wares on the left page, each with its icon, name, stock and price, and the chosen ware on the right page with an enlarged preview, its description, price, stock, a quantity spinner, the total, the party's money and a Yes button next to a prompt such as "Buy 3 for 45 guilders?".

It is built only from the dynamic gump widgets: a source-1 [dynamic list](dynamic_lists.md), [dynamic texts](dynamic_texts.md), a list-bound [dynamic shape](dynamic_shapes.md), a [dynamic slider](dynamic_sliders.md) spinner and a [dynamic button](dynamic_buttons.md). The layout ships in Exult's own `data/bg/gump_info.txt` and `data/si/gump_info.txt`, so every BG or SI mod gets the shop with no data work, and a mod can still restyle it.

**Files:**
- `content/kits/shop/shop_kit.uc` — the kit
- `content/kits/shop/example_si.uc`, `content/kits/shop/example_bg.uc` — complete example shopkeepers
- `data/si/gump_info.txt` (gump 27), `data/bg/gump_info.txt` (gump 32) — the layout
- Installed to `$(datadir)/exult/kits/shop/` by `make install`

---

## Table of Contents

1. [Design](#design)
2. [Layout](#layout)
3. [Installing and Including](#installing-and-including)
4. [API Reference](#api-reference)
5. [Callbacks and Events](#callbacks-and-events)
6. [Selling](#selling)
7. [Example Shopkeeper](#example-shopkeeper)
8. [Engine Additions](#engine-additions)
9. [Known Limits](#known-limits)

---

## Design

- **The open book (SI gump 27, BG gump 32).** Both games have the same art, 307×148 with the hotspot at the top-left, and each page holds about 125 pixels: room for a list on the left and a details panel on the right. The scroll is too narrow for both. No game uses the book as a container, and `Book_gump` / `Text_gump` never call `Gump::set_object_area()`, so real books get no widgets and stay as they are.
- **The games' own buttons.** Buy (or Sell) is the Yes button (SI 52, BG 70). Close is U7's red checkmark, which every container gump has. In the original windows it sits outside the frame, on a coloured strip that is part of each container's art. The book has no strip, so the shop's checkmark sits on Exult's own checkmark tab (the green one on Exult's menus, from `exult.flx`), outside the book's bottom-left corner in the 27-pixel margin the book art leaves free on the left. A double-click or Enter on a row also buys.
- **The layout ships with Exult.** The field ids and callback numbers live in one place, and a mod's patch `gump_info.txt` overrides the entries per gump shape (see [Restyling](#restyling)).
- **A hidden counter bag hosts the gump.** A dynamic gump needs a container, and NPCs themselves cannot be opened. The kit finds or creates a bag (shape 802, quality 233) in the shopkeeper's inventory and opens the book on it. It is reused on every visit. It is never in the world, where it would be visible, takeable and could be deleted with its chunk, and never in the Avatar, where its weight would distort the carry check. The book's empty `container_area` (see [Engine Additions](#engine-additions)) keeps it empty.
- **Flat arrays.** Usecode flattens nested array literals, so wares, offers and options are flat arrays with a fixed stride, and the kit sets list rows one at a time.
- **Transactions use the engine's own checks.** Goods are placed with `UI_add_cont_items` on each party member in turn, which applies the weight and volume limits. Whatever is left over is taken back and the purchase fails with nothing spilt on the ground. Payment uses `UI_remove_party_items`.
- **No new intrinsic slots.** The kit uses only intrinsics present in both the BG and SI tables, plus three small engine changes.

---

## Layout

Coordinates are relative to the book's top-left corner. SI uses gump 27 and the Yes button 52; BG uses gump 32 and the Yes button 70, with the same coordinates.

| Field | Entry (SI) | Shows |
|---|---|---|
| `container_area` | `:27/0/0/0/0` | Empty: a panel that holds no objects |
| `checkmark_pos` | `:27/25/135/2/1` | Close, on a tab outside the book's bottom-left corner (the last field asks for the tab) |
| text 1 | `:27/1/97/9/2/2` | Shop title, centred on the left page |
| list 1 | `:27/1/35/21/110/9/11/2/1/-1/-1/-1/3840/3841/-1/1` | Wares: icon, "Name (stock)", price on the right, "1-9 of N" |
| text 2 | `:27/2/235/9/2/2` | Ware name, centred on the right page |
| shape 3 | `:27/3/180/20/0/0/0/1/110/40` | Preview bound to list 1, boxed and enlarged |
| text 4 | `:27/4/178/61/4` | Description (one line) |
| text 5 | `:27/5/178/70/4` | "Price: 15 guilders each" |
| text 6 | `:27/6/178/79/4` | "In stock: 12" / "Plenty" (selling: "You have: 4") |
| text 7 | `:27/7/178/88/2` | "Quantity: 3" |
| slider 8 | `:27/8/262/96/17/16/-1/1/99/1/1/0/3842/1` | Quantity, a horizontal spinner |
| text 9 | `:27/9/178/100/2` | "Total: 45 guilders" |
| text 10 | `:27/10/178/111/4` | "You have 120 guilders" |
| text 11 | `:27/11/235/120/4/2` | Status: "You cannot afford that." / "Thank you." |
| button | `:27/52/296/140/0/1/-1/3843/0/-1/-1` | Yes = Buy (or Sell) |
| text 12 | `:27/12/272/132/4/1` | Prompt "Buy 3 for 45 guilders?" |

Callback numbers are decimal: 3840 = list select, 3841 = list activate (buy), 3842 = quantity, 3843 = Yes. Font 2 is small black and font 4 is tiny.

**Keys:** Up/Down move through the list; Left/Right change the quantity; Enter buys. The mouse wheel over the list scrolls it, and anywhere else on the gump it changes the quantity.

### Restyling

A mod restyles the shop in its own patch `gump_info.txt`: an entry for gump 27 (or 32) in a section replaces that section's shipped entries for the gump. Move the texts, change fonts, or use other button art, but keep the field ids and callback numbers, which the kit relies on. The spinner (slider 8) must stay: the kit reads it to tell whether the shop is open.

A mod can also put the whole layout on another gump shape and declare that shape as `SHOP_KIT_GUMP`.

---

## Installing and Including

`make install` copies the kit and both examples to `$(datadir)/exult/kits/shop/`. A mod can compile against that folder, or copy `shop_kit.uc` into its own sources.

```usecode
#game "serpentisle"

// Must come before the include: the book in SI (32 in BG).
const int SHOP_KIT_GUMP = 27;

// The kit's functions are unnumbered, so include it inside #autonumber.
#autonumber 0x1000
#include "shop_kit.uc"
```

```
ucc -I /path/to/exult/data/kits/shop -o usecode mymod.uc
```

ucc has no `#ifdef` and usecode cannot ask which game is running, so the includer picks the gump. All kit names start with `shopKit` / `SHOP_KIT_` (the public calls with `shop`), and the kit declares its own constants for -356 (Avatar), -357 (party) and -359 (any), so it does not clash with SI Fixes, the Keyring mod or other headers. Its widget callbacks are `shape#(3840)` to `shape#(3843)`; shape numbers from 0x400 go through ucc's symbol table, and no BG or SI shape reaches 3840.

---

## API Reference

### Data

**Wares** (buying), 7 entries per ware:

| Offset | Entry | Notes |
|---|---|---|
| 1 | shape | |
| 2 | frame | |
| 3 | quality | -359 = any |
| 4 | price | Per item, in the currency |
| 5 | stock | -1 = unlimited; 0 = sold out (the row is disabled) |
| 6 | name | "" = the shape's own name in the game |
| 7 | description | One line |

**Offers** (selling), 5 entries per offer: shape, frame, quality, price each, name.

**Options**, 5 entries:

| Offset | Entry | Default |
|---|---|---|
| 1 | Currency shape | 0 = 644 (gold coins) |
| 2 | Currency name | "" = "gold" |
| 3 | Haggle | 0 = off, 1 = call the callback with an offer before each sale |
| 4 | Callback | `&function`, or 0 = none |
| 5 | Title | "" = "\<NPC\>'s wares" (selling: "Selling to \<NPC\>") |

The `SHOP_KIT_WARE_*`, `SHOP_KIT_OFFER_*` and `SHOP_KIT_OPT_*` constants name the offsets.

### Calls

| Call | Returns | Description |
|---|---|---|
| `shopOpen(npc, wares, options)` | 1/0 | Opens the shop now |
| `shopOpenSell(npc, offers, options)` | 1/0 | Opens a shop that buys from the party |
| `shopOpenAfterTalk(npc, wares, options)` | — | Opens the shop 2 ticks later, from a `nohalt` script on the Avatar (so a later script on the Avatar does not cancel it). Use it from dialogue: the conversation closes all gumps when it ends |
| `shopOpenSellAfterTalk(npc, offers, options)` | — | The same for selling |
| `shopSelect(i)` | 1/0 | Selects row `i` (0 for a disabled or missing row); the quantity goes back to 1 |
| `shopSetQuantity(n)` | quantity | Sets the quantity, clamped to 1..min(stock, 99) (selling: what the party holds) |
| `shopBuy()` | result | Buys (or sells) the selected row at the current quantity |
| `shopClose()` | — | Closes the shop's gump |
| `shopIsOpen()` | 1/0 | Whether the shop's gump is open |
| `shopGetWares()` | array | The wares with the stock as it is now (keep it in a `static var`) |
| `shopGetQuantity()` | quantity | |
| `shopGetTotal()` | total | Price × quantity of the selection |

The widget callbacks call these too, so a test drives exactly what a click does:

| Callback | Does |
|---|---|
| `shape#(3840)` list select | `shopSelect(UI_get_list_selection(item, 1))` |
| `shape#(3841)` list activate | `shopBuy()` |
| `shape#(3842)` spinner | `shopSetQuantity(UI_get_slider_value(item, 8))` |
| `shape#(3843)` Yes | `shopBuy()` |

### Result and Event Codes

| Constant | Value | Meaning |
|---|---|---|
| `SHOP_KIT_BOUGHT` | 1 | Bought (result and event) |
| `SHOP_KIT_SOLD` | 2 | Sold (result and event) |
| `SHOP_KIT_OFFER` | 3 | Event: haggle over a purchase |
| `SHOP_KIT_SELL_OFFER` | 4 | Event: haggle over a sale |
| `SHOP_KIT_NO_MONEY` | 5 | The party cannot pay |
| `SHOP_KIT_TOO_HEAVY` | 6 | The goods (or the coins, selling) do not fit; nothing changed |
| `SHOP_KIT_SOLD_OUT` | 7 | The ware is sold out |
| `SHOP_KIT_NOTHING` | 8 | No selection, the party lacks the goods, the haggle was cancelled, or the shop is closed |
| `SHOP_KIT_BUSY` | 9 | A purchase is already running (a callback called `shopBuy`) |

### A Purchase, Step by Step

1. A row must be selected and enabled, and the quantity no more than the stock, else `SOLD_OUT` / `NOTHING`.
2. The party's money, `UI_count_objects(-357, currency, -359, -359)`, must cover the total, else `NO_MONEY`.
3. With haggling on, the callback gets `SHOP_KIT_OFFER` and returns the agreed total. A total below 0, or 0 when the asking total was above 0, cancels: `NOTHING`, status "No deal.". The money is checked again.
4. The goods go to the party members in order with `UI_add_cont_items`. If any are left over, the placed ones are removed again: `TOO_HEAVY`.
5. The total is paid with `UI_remove_party_items`.
6. The stock goes down. At 0 the row shows "sold out" and is disabled, and the selection moves to the next enabled row. The callback gets `SHOP_KIT_BOUGHT`.

---

## Callbacks and Events

The callback is a `var` function with two parameters, called on the shopkeeper (so `item` is the NPC):

```usecode
var myShopEvent(ev, info) {
    // info = [index, shape, frame, quality, quantity, total]
    if (ev == SHOP_KIT_OFFER) {
        return info[6];    // The agreed total; -1 cancels.
    }
    return info[6];        // Ignored for other events.
}
```

It must always return a value. For `SHOP_KIT_OFFER` and `SHOP_KIT_SELL_OFFER` the return value is the agreed total, and `info[6]` is the asking total. A return below 0 (use -1) cancels. So does 0 when the asking total was above 0, so a stray 0 never gives the goods away or takes them for nothing. A free ware (asking total 0) still goes through at 0. A cancelled haggle returns `SHOP_KIT_NOTHING` and shows "No deal.". For `SHOP_KIT_BOUGHT` and `SHOP_KIT_SOLD` the return value is ignored.

**Conversations and the reopen rule.** A callback may want to start a conversation, for example to haggle for real. Showing an NPC's face closes all gumps, so after every callback the kit checks whether the shop was open and is now closed, and if so reopens it with the same rows, selection and quantity. The headless test checks this with a callback that calls `UI_show_npc_face(npc, 0)` then `UI_remove_npc_face(npc)`: the shop is closed inside the callback, the purchase completes and the shop is open again with the same rows and selection. A full dialogue with answers (`converse`) inside a callback was not run headless, so test that path in your mod before relying on it. Opening the shop from dialogue with `shopOpenAfterTalk` is the tested route.

**Busy guard.** While a purchase runs, `shopBuy` returns `SHOP_KIT_BUSY`, so a callback cannot start a second one.

---

## Selling

`shopOpenSell(npc, offers, options)` opens the same book in sell mode. Each row is one kind of goods the shopkeeper buys, showing how many the party holds; rows the party has none of are disabled. The Yes button sells:

1. The party must hold the quantity, else `NOTHING`.
2. With haggling on, the callback gets `SHOP_KIT_SELL_OFFER` and returns the agreed total; the same rule as buying cancels it (below 0, or 0 when the asking total was above 0).
3. The party is paid first: the coins go to the party members in order. If they do not all fit, the coins placed are taken back and the result is `TOO_HEAVY`, with the goods never touched.
4. Then the goods are removed with `UI_remove_party_items`. If that fails, the coins are taken back with `UI_remove_party_items` and the result is `NOTHING`.
5. The callback gets `SHOP_KIT_SOLD`.

Because the goods are removed only after the coins fit, a failed sale leaves them exactly as they were: the kit never rebuilds goods with a guessed quality or frame.

---

## Example Shopkeeper

`content/kits/shop/example_si.uc`, compiled on every build (its BG twin, `example_bg.uc`, uses gump 32 and gold):

```usecode
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
```

The stock lives in a global `static var`, which Exult saves with the game, and is refreshed from `shopGetWares()` after each purchase. Names left as "" come from the game ("torch", "bedroll").

---

## Engine Additions

The kit needed three small engine changes and no new intrinsics:

- **E1. An empty `container_area` makes a panel.** A `container_area` with a width or height of 0 or less still makes a dynamic gump, but `Dynamic_container_gump::add()` refuses every drop that no source-0 list takes, so a dragged item goes back where it came from. Without it, an item dropped on the shop would vanish into the hidden counter. Existing entries all have positive sizes and are unchanged. See [dynamic_gumps.md](dynamic_gumps.md).
- **E2. The widget intrinsics find a nested container's own gump.** `UI_set_gump_text`, `UI_set_gump_shape`, `UI_set_slider_value`, `UI_get_slider_value` and `UI_set_gump_text_font` look up the gump showing the item, then its owner's, as before, and then the gump whose container is the item. Before, they failed for a dynamic gump on a container inside something else: the counter in an NPC, or a dynamic bag in the backpack. Calls that worked before find the same widget. See [dynamic_sliders.md](dynamic_sliders.md), [dynamic_texts.md](dynamic_texts.md) and [dynamic_shapes.md](dynamic_shapes.md).
- **E3. A checkmark on a tab.** `checkmark_pos` takes an optional fifth field. With `1`, `Checkmark_button` paints Exult's checkmark tab (`exult.flx` `checkmark_background`) centred behind the checkmark, and `Gump::paint()` paints it before the gump's shape, so the book covers the tab's right 3 pixels and the tab seems to hang out from behind the cover. The tab is part of the gump for `has_point()` and `get_rect()`: clicking it drags the shop, and only the checkmark closes it. Entries without the field are unchanged. See [dynamic_gumps.md](dynamic_gumps.md#checkmark_pos).

`UI_set_slider_value` runs no callback, so the kit clamps the spinner to the stock in its callback without looping.

---

## Known Limits

- **One line of description.** Text fields do not wrap; keep descriptions short.
- **99 per purchase.** The spinner's maximum is 99 in the layout; the kit clamps lower to the stock.
- **Closing has no callback.** Esc and the checkmark close the gump with no usecode, as on every container gump. The shop's state stays until the next `shopOpen`.
- **The counter bag stays in the NPC.** It is empty and reused, but it is a real object in the shopkeeper's inventory (shape 802, quality 233). Usecode that empties an NPC's inventory removes it; the kit makes a new one next time.
- **Keys go to the focused gump only.** There is no keyboard focus across gumps.
- **One shop at a time.** Opening a shop closes another shopkeeper's open shop.
- **Dialogue inside a callback is not fully tested.** The reopen rule was tested headless with `UI_show_npc_face`/`UI_remove_npc_face`; a callback running a `converse` loop with answers was not.
- **A 0 offer cancels a priced ware.** A haggle callback cannot give a priced ware away (or take goods for nothing) by returning 0; for a gift, use a free ware (price 0) or give the item from usecode.
- **Rolling back coins on a failed sale takes any coins.** If the goods cannot be removed after the party was paid, the kit takes back the same number of the currency shape with any quality and frame.
