/*
Copyright (C) 2011-2025 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "ShortcutBar_gump.h"

#include "Gamemenu_gump.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "Notebook_gump.h"
#include "Text_button.h"
#include "actors.h"
#include "cheat.h"
#include "exult.h"
#include "exult_flx.h"
#include "fnames.h"
#include "game.h"
#include "gamewin.h"
#include "keyactions.h"
#include "party.h"
#include "shapeid.h"
#include "ucmachine.h"

#include <limits>

uint32 ShortcutBar_gump::eventType = std::numeric_limits<uint32>::max();

/*
 * some buttons should only be there or change appearance
 * when a certain item is in the party's inventory
 */
Game_object* is_party_item(
		int shnum,    // Desired shape.
		int frnum,    // Desired frame
		int qual      // Desired quality
) {
	Actor*    party[9];    // Get party.
	const int cnt = Game_window::get_instance()->get_party(party, 1);
	for (int i = 0; i < cnt; i++) {
		Actor*       person = party[i];
		Game_object* obj    = person->find_item(shnum, qual, frnum);
		if (obj) {
			return obj;
		}
	}
	return nullptr;
}

void ShortcutBar_gump::check_for_updates(int shnum) {
	if (shnum == 761
		|| (GAME_SI
			&& (shnum == 485
				|| shnum == 555))) {    // spellbook, keyring, jawbone
		has_changed = true;
	}
}

// add dirty region, if dirty
void ShortcutBar_gump::update_gump() {
	if (has_changed) {
		deleteButtons();
		createButtons();
		has_changed = false;
	}
}

// Hide the gump without destroying it
void ShortcutBar_gump::HideGump() {
	if (g_shortcutBar) {
		gumpman->remove_gump(
				g_shortcutBar);
	}
}

// Show the gump if it exists
void ShortcutBar_gump::ShowGump() {
	if (g_shortcutBar) {
		gumpman->add_gump(g_shortcutBar);
	}
}

// Check if the gump is currently visible
bool ShortcutBar_gump::Visible() {
	if (!g_shortcutBar) {
		return false;
	}

	for (auto it = gumpman->begin(); it != gumpman->end(); it++) {
		if (*it == g_shortcutBar) {
			return true;
		}
	}
	return false;
}

/*
 * To align button shapes vertically, we need to micro-manage the shapeOffsetY
 * values to shift shapes up or down.
 */
void ShortcutBar_gump::createButtons() {
	startx = gwin->get_win()->get_start_x();
	resx   = gwin->get_win()->get_full_width();
	gamex  = gwin->get_game_width();
	starty = gwin->get_win()->get_start_y();
	resy   = gwin->get_win()->get_full_height();
	gamey  = gwin->get_game_height();
	for (auto& buttonItem : buttonItems) {
		buttonItem.translucent = false;
	}
	int x = (gamex - 320) / 2;

	memset(buttonItems, 0, sizeof(buttonItems));
	const bool trlucent = gwin->get_shortcutbar_type() == 1 && starty >= 0;
	// disk
	buttonItems[0].shapeId = new ShapeID(
			EXULT_FLX_SB_DISK_SHP, trlucent ? 1 : 0, SF_EXULT_FLX);
	buttonItems[0].name = "disk";
	buttonItems[0].type = SB_ITEM_DISK;

	// peace/combat
	if (gwin->in_combat()) {
		buttonItems[1].shapeId = new ShapeID(
				EXULT_FLX_SB_COMBAT_SHP, trlucent ? 3 : 2, SF_EXULT_FLX);
	} else {
		buttonItems[1].shapeId = new ShapeID(
				EXULT_FLX_SB_COMBAT_SHP, trlucent ? 1 : 0, SF_EXULT_FLX);
	}
	buttonItems[1].name = "toggle combat";
	buttonItems[1].type = SB_ITEM_TOGGLE_COMBAT;

	// map
	if (trlucent) {
		if (GAME_SI) {
			buttonItems[2].shapeId
					= new ShapeID(EXULT_FLX_SB_MAPS_SHP, 1, SF_EXULT_FLX);
		} else {
			buttonItems[2].shapeId
					= new ShapeID(EXULT_FLX_SB_MAPS_SHP, 0, SF_EXULT_FLX);
		}
	} else {
		buttonItems[2].shapeId = new ShapeID(178, 0, SF_SHAPES_VGA);
	}
	buttonItems[2].name = "map";
	buttonItems[2].type = SB_ITEM_MAP;

	// spellbook
	if (GAME_SI) {
		if (is_party_item(761)) {
			if (trlucent) {
				buttonItems[3].shapeId = new ShapeID(
						EXULT_FLX_SB_SPELLBOOK_SHP, 2, SF_EXULT_FLX);
			} else {
				buttonItems[3].shapeId = new ShapeID(761, 0, SF_SHAPES_VGA);
			}
		} else {
			if (gwin->sb_hide_missing_items()) {
				buttonItems[3].shapeId = new ShapeID(
						EXULT_FLX_TRANSPARENTMENU_SHP, 0, SF_EXULT_FLX);
			} else {
				buttonItems[3].shapeId = new ShapeID(
						EXULT_FLX_SB_SPELLBOOK_SHP, 3, SF_EXULT_FLX);
				buttonItems[3].translucent = true;
			}
		}
	} else {
		if (is_party_item(761)) {
			if (trlucent) {
				buttonItems[3].shapeId = new ShapeID(
						EXULT_FLX_SB_SPELLBOOK_SHP, 0, SF_EXULT_FLX);
			} else {
				buttonItems[3].shapeId = new ShapeID(761, 0, SF_SHAPES_VGA);
			}
		} else {
			if (gwin->sb_hide_missing_items()) {
				buttonItems[3].shapeId = new ShapeID(
						EXULT_FLX_TRANSPARENTMENU_SHP, 0, SF_EXULT_FLX);
			} else {
				buttonItems[3].shapeId = new ShapeID(
						EXULT_FLX_SB_SPELLBOOK_SHP, 1, SF_EXULT_FLX);
				buttonItems[3].translucent = true;
			}
		}
	}
	buttonItems[3].name = "spellbook";
	buttonItems[3].type = SB_ITEM_SPELLBOOK;

	// backpack
	if (trlucent) {
		buttonItems[4].shapeId
				= new ShapeID(EXULT_FLX_SB_BACKPACK_SHP, 0, SF_EXULT_FLX);
	} else {
		buttonItems[4].shapeId = new ShapeID(801, 0, SF_SHAPES_VGA);
	}
	buttonItems[4].name = "backpack";
	buttonItems[4].type = SB_ITEM_BACKPACK;

	// key/keyring
	if (GAME_SI && is_party_item(485)) {
		buttonItems[5].shapeId = new ShapeID(
				EXULT_FLX_SB_KEYRING_SHP, trlucent ? 1 : 0, SF_EXULT_FLX);
		buttonItems[5].name = "keyring";
		buttonItems[5].type = SB_ITEM_KEYRING;
	} else {
		if (trlucent) {
			buttonItems[5].shapeId
					= new ShapeID(EXULT_FLX_SB_KEY_SHP, 0, SF_EXULT_FLX);
		} else {
			buttonItems[5].shapeId = new ShapeID(641, 28, SF_SHAPES_VGA);
		}
		buttonItems[5].name = "key";
		buttonItems[5].type = SB_ITEM_KEY;
	}

	// notebook
	if (trlucent) {
		buttonItems[6].shapeId
				= new ShapeID(EXULT_FLX_SB_NOTEBOOK_SHP, 0, SF_EXULT_FLX);
	} else {
		buttonItems[6].shapeId = new ShapeID(642, 7, SF_SHAPES_VGA);
	}
	buttonItems[6].name = "notebook";
	buttonItems[6].type = SB_ITEM_NOTEBOOK;

	// target
	buttonItems[7].shapeId = new ShapeID(
			EXULT_FLX_SB_TARGET_SHP, trlucent ? 1 : 0, SF_EXULT_FLX);
	buttonItems[7].name = "target";
	buttonItems[7].type = SB_ITEM_TARGET;

	// feed
	if (trlucent) {
		buttonItems[8].shapeId
				= new ShapeID(EXULT_FLX_SB_FOOD_SHP, 0, SF_EXULT_FLX);
	} else if (GAME_SI) {
		buttonItems[8].shapeId = new ShapeID(23, 3, SF_GUMPS_VGA);
	} else {
		buttonItems[8].shapeId = new ShapeID(28, 3, SF_GUMPS_VGA);
	}
	buttonItems[8].name = "feed";
	buttonItems[8].type = SB_ITEM_FEED;

	// jawbone
	if (GAME_SI) {
		Game_object* jawbone;
		if ((jawbone = is_party_item(555))) {
			if (trlucent) {
				buttonItems[9].shapeId = new ShapeID(
						EXULT_FLX_SB_JAWBONE_SHP, 0, SF_EXULT_FLX);
			} else {
				buttonItems[9].shapeId = new ShapeID(
						555, jawbone->get_framenum(), SF_SHAPES_VGA);
			}
		} else {
			if (gwin->sb_hide_missing_items()) {
				buttonItems[9].shapeId = new ShapeID(
						EXULT_FLX_TRANSPARENTMENU_SHP, 0, SF_EXULT_FLX);
			} else {
				buttonItems[9].shapeId = new ShapeID(
						EXULT_FLX_SB_JAWBONE_SHP, 1, SF_EXULT_FLX);
				buttonItems[9].translucent = true;
			}
		}
		buttonItems[9].name = "jawbone";
		buttonItems[9].type = SB_ITEM_JAWBONE;

		numButtons = 10;
	} else {
		numButtons = 9;
	}

	const int barItemWidth = (320 / numButtons);

	for (int i = 0; i < numButtons; i++, x += barItemWidth) {
		Shape_frame* frame = buttonItems[i].shapeId->get_shape();
		const int    dX
				= frame->get_xleft() + (barItemWidth - frame->get_width()) / 2;
		const int dY = frame->get_yabove() + (height - frame->get_height()) / 2;
		buttonItems[i].mx   = x + dX;
		buttonItems[i].my   = starty + dY;
		buttonItems[i].rect = TileRect(x, starty, barItemWidth, height);
		// this is safe to do since it only effects certain palette colors
		// which will be color cycling otherwise
		if (trlucent) {
			buttonItems[i].translucent = true;
		}
	}
}

void ShortcutBar_gump::deleteButtons() {
	for (int i = 0; i < numButtons; i++) {
		delete buttonItems[i].shapeId;
		buttonItems[i].shapeId = nullptr;
	}
	startx = 0;
	resx   = 0;
	gamex  = 0;
	starty = 0;
	resy   = 0;
	gamey  = 0;
}

/*
 * Construct a shortcut bar gump at the top of screen.
 * Also register it to gump manager.
 * This gump is persistent, not draggable.
 * There must be only one shortcut bar in the game.
 */
ShortcutBar_gump::ShortcutBar_gump(int placex, int placey)
		: Gump(nullptr, placex, placey, EXULT_FLX_TRANSPARENTMENU_SHP,
			   SF_EXULT_FLX) {
	/*static bool init = false;
	assert(init == 0); // Protect against re-entry
	init = true;*/

	if (ShortcutBar_gump::eventType == std::numeric_limits<uint32>::max()) {
		ShortcutBar_gump::eventType = SDL_RegisterEvents(1);
	}

	resx   = gwin->get_win()->get_full_width();
	width  = resx;
	height = 25;
	locx   = placex;
	locy   = placey;
	for (auto& buttonItem : buttonItems) {
		buttonItem.pushed = false;
	}
	createButtons();
	gumpman->add_gump(this);
	has_changed = true;
}

ShortcutBar_gump::~ShortcutBar_gump() {
	deleteButtons();
	gwin->set_all_dirty();
}

void ShortcutBar_gump::paint() {
	Game_window*   gwin = Game_window::get_instance();
	Shape_manager* sman = Shape_manager::get_instance();

	Gump::paint();

	for (int i = 0; i < numButtons; i++) {
		const ShortcutBarButtonItem& item = buttonItems[i];
		const int                    x    = locx + item.mx;
		const int                    y    = locy + item.my;
		sman->paint_shape(x, y, item.shapeId->get_shape(), item.translucent);
		// when the bar is on the game screen it may need an outline
		if (gwin->get_outline_color() < NPIXCOLORS && starty >= 0) {
			sman->paint_outline(
					x, y, item.shapeId->get_shape(), gwin->get_outline_color());
		}
	}

	gwin->set_painted();
}

int ShortcutBar_gump::handle_event(SDL_Event* event) {
	Game_window* gwin          = Game_window::get_instance();
	static bool  handle_events = true;
	// When the Save/Load menu is open, or the notebook, don't handle events
	if (gumpman->modal_gump_mode() || gwin->get_usecode()->in_usecode()
		|| g_waiting_for_click || Notebook_gump::get_instance()) {
		// do not register a mouse up event on notebook checkmark
		if (Notebook_gump::get_instance()) {
			handle_events = false;
		}
		return 0;
	}

	if ((event->type == SDL_EVENT_MOUSE_BUTTON_DOWN
		 || event->type == SDL_EVENT_MOUSE_BUTTON_UP)
		&& handle_events) {
		int x;
		int y;
		gwin->get_win()->screen_to_game(
				event->button.x, event->button.y, gwin->get_fastmouse(), x, y);
		Gump*        on_gump = gumpman->find_gump(x, y);
		Gump_button* button;
		if (x >= startx && x <= (locx + width) && y >= starty
			&& y <= (starty + height)) {
			// do not register a mouse up event when closing a gump via
			// checkmark over the bar
			if (on_gump && (button = on_gump->on_button(x, y))
				&& button->is_checkmark()) {
				handle_events = false;
				return 0;
			} else if (on_gump) {
				// do not click "through" a gump
				return 0;
			}
			if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
				sdl_mouse_down(event, x, y);
			} else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
				sdl_mouse_up(event, x, y);
			}
			return 1;
		}
	} else {
		handle_events = true;
		return 0;
	}
	return 0;
}

void ShortcutBar_gump::sdl_mouse_down(SDL_Event* event, int mx, int my) {
	ignore_unused_variable_warning(event);
	for (int i = 0; i < numButtons; i++) {
		if (buttonItems[i].rect.has_point(mx, my)) {
			buttonItems[i].pushed = true;
		}
	}
}

/*
 * Runs on timer thread. Should never directly access anything in main thread.
 * Just push an event to main thread so that our global shortcut bar instance
 * can catch it.
 */
static Uint32 didMouseUp(void* param, SDL_TimerID timerID, Uint32 interval) {
	ignore_unused_variable_warning(timerID, interval);
	SDL_Event event;
	SDL_zero(event);
	event.type       = ShortcutBar_gump::eventType;
	event.user.code  = ShortcutBar_gump::SHORTCUT_BAR_MOUSE_UP;
	event.user.data1 = param;
	event.user.data2 = nullptr;
	SDL_PushEvent(&event);
	return 0;
}

/*
 * Runs on main thread.
 */
void ShortcutBar_gump::handleMouseUp(SDL_Event& event) {
	if (event.user.code != ShortcutBar_gump::SHORTCUT_BAR_MOUSE_UP) {
		return;
	}
	sintptr button;
	std::memcpy(&button, &event.user.data1, sizeof(sintptr));
	if (button >= 0 && button < numButtons) {
		onItemClicked(button, false);
		if (timerId != 0) {
			SDL_RemoveTimer(timerId);
			timerId = 0;
		}
	}
}

void ShortcutBar_gump::sdl_mouse_up(SDL_Event* event, int mx, int my) {
	ignore_unused_variable_warning(event);
	int i;

	for (i = 0; i < numButtons; i++) {
		if (buttonItems[i].rect.has_point(mx, my)) {
			break;
		}
	}

	if (i < numButtons) {
		/*
		 * Button i is hit.
		 * Cancel the previous mouse up timer
		 */
		if (timerId) {
			SDL_RemoveTimer(timerId);
			timerId = SDL_TimerID{};
		}

		/*
		 * For every double click,
		 * there are usually two clicks:
		 *    MOUSEDOWN MOUSEUP MOUSEDOWN MOUSEUP
		 * Therefore when we get the first MOUSEUP, we
		 * have no idea if we are going to get another one.
		 * So we delay the handler.
		 */
		if (event->button.clicks >= 2) {
			onItemClicked(i, true);
		} else {
			sintptr button_id = i;
			void*   data;
			std::memcpy(&data, &button_id, sizeof(sintptr));
			timerId = SDL_AddTimer(500 /*ms delay*/, didMouseUp, data);
		}
	}

	for (i = 0; i < numButtons; i++) {
		buttonItems[i].pushed = false;
	}
}

void ShortcutBar_gump::onItemClicked(int index, bool doubleClicked) {
	printf("Item %s is %sclicked\n", buttonItems[index].name,
		   doubleClicked ? "double " : "");

	switch (buttonItems[index].type) {
	case SB_ITEM_DISK: {
		if (doubleClicked) {
			ActionFileGump(nullptr);    // save_restore
		} else {
			ActionCloseOrMenu(nullptr);    // close_or_menu
		}
		break;
	}
	case SB_ITEM_BACKPACK: {
		const int j = -1;
		ActionInventory(&j);    // inventory
		break;
	}
	case SB_ITEM_SPELLBOOK: {
		gwin->activate_item(761);    // useitem 761
		break;
	}
	case SB_ITEM_NOTEBOOK: {
		if (doubleClicked && cheat()) {
			cheat.cheat_screen();    // cheat_screen
		} else if (!doubleClicked) {
			ActionNotebook(nullptr);    // notebook
		}
		break;
	}
	case SB_ITEM_KEY: {
		if (doubleClicked) {             // Lockpicks
			gwin->activate_item(627);    // useitem 627
		} else {
			ActionTryKeys(nullptr);    // try_keys
		}
		break;
	}
	case SB_ITEM_KEYRING: {
		if (doubleClicked) {             // Lockpicks
			gwin->activate_item(627);    // useitem 627
		} else {
			gwin->activate_item(485);    // useitem 485
		}
		break;
	}
	case SB_ITEM_MAP: {
		if (doubleClicked && cheat()) {
			cheat.map_teleport();    // map_teleport
		} else if (!doubleClicked) {
			gwin->activate_item(178, 0);    // useitem 178, frame 0
		}
		break;
	}
	case SB_ITEM_TOGGLE_COMBAT: {
		ActionCombat(nullptr);    // toggle_combat
		break;
	}
	case SB_ITEM_TARGET: {
		if (doubleClicked && cheat()) {
			ActionTeleportTargetMode(nullptr);    // target_mode_teleport
		} else if (!doubleClicked) {
			ActionTarget(nullptr);    // target_mode
		}
		break;
	}
	case SB_ITEM_JAWBONE: {
		gwin->activate_item(555);    // useitem 555 #SI only
		break;
	}
	case SB_ITEM_FEED: {
		if (doubleClicked) {
			ActionUseHealingItems(nullptr);    // use_healing_items
		} else if (GAME_SI) {
			int params[2];
			params[0] = 1557;
			params[1] = 0;
			ActionCallUsecode(params);    // call_usecode 1557 0
		} else {
			ActionUseFood(nullptr);    // usefood
		}
		break;
	}
	default: {
		break;
	}
	}
}
