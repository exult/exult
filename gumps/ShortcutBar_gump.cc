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
#include "gumpinf.h"
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
	// Data-driven: check if any shortcutbar entry monitors this shape
	const auto& icons = Gump_info::get_shortcutbar_icons();
	for (const auto& [slot, info] : icons) {
		auto check = [shnum](const Shortcutbar_icon_entry& e) {
			return e.valid && e.check_party_item
				   && e.shapefile_type == 1 && e.shape == shnum;
		};
		if (check(info.normal) || check(info.translucent)) {
			has_changed = true;
			return;
		}
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
		gumpman->remove_gump(g_shortcutBar);
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

	// Slot-to-type mapping (slot index matches gump_info.txt shortcutbar_icon)
	static const struct {
		ShortcutBarButtonItemType type;
		const char*               name;
	} slot_defs[] = {
			{SB_ITEM_DISK, "disk"},                      // slot 0
			{SB_ITEM_TOGGLE_COMBAT, "toggle combat"},    // slot 1
			{SB_ITEM_MAP, "map"},                        // slot 2
			{SB_ITEM_SPELLBOOK, "spellbook"},            // slot 3
			{SB_ITEM_BACKPACK, "backpack"},               // slot 4
			{SB_ITEM_KEY, "key"},                        // slot 5
			{SB_ITEM_NOTEBOOK, "notebook"},              // slot 6
			{SB_ITEM_TARGET, "target"},                  // slot 7
			{SB_ITEM_FEED, "feed"},                      // slot 8
			{SB_ITEM_JAWBONE, "jawbone"},                // slot 9
	};
	static constexpr int num_slot_defs
			= sizeof(slot_defs) / sizeof(slot_defs[0]);

	numButtons = 0;
	for (int slot = 0; slot < num_slot_defs; slot++) {
		const auto* icon = Gump_info::get_shortcutbar_icon(slot);
		if (!icon) {
			continue;
		}

		// Pick normal or translucent entry based on bar type
		const auto& entry = trlucent ? icon->translucent : icon->normal;
		if (!entry.valid) {
			continue;
		}
		if (entry.shapefile_type == -1) {
			continue;
		}

		ShapeFile sf = entry.get_shapefile();
		int                       shape = entry.shape;
		int                       frame = entry.frame;
		ShortcutBarButtonItemType type  = slot_defs[slot].type;
		const char*               name  = slot_defs[slot].name;
		bool force_translucent          = false;

		// Data-driven is_party_item check — always use the normal entry
		// to determine which shapes.vga item to look for, regardless
		// of which variant (normal/translucent) is being displayed.
		const auto& nentry     = icon->normal;
		Game_object* party_obj = nullptr;
		if (nentry.valid && nentry.check_party_item
			&& nentry.shapefile_type == 1) {
			party_obj = is_party_item(nentry.shape);
			if (!party_obj) {
				if (entry.fallback_vga >= 0) {
					// Use this variant's fallback icon (always shown, dimmed)
					sf    = entry.get_fallback_shapefile();
					shape = entry.fallback_shape;
					frame = entry.fallback_frame;
				} else if (gwin->sb_hide_missing_items()) {
					continue;
				} else {
					// No fallback — use translucent icon as dimmed indicator
					const auto& tentry = icon->translucent;
					if (tentry.valid && tentry.shapefile_type != -1) {
						sf    = tentry.get_shapefile();
						shape = tentry.shape;
						frame = tentry.frame;
					}
				}
				force_translucent = true;
			}
		}

		// Per-slot game logic overrides
		switch (slot) {
		case 1:    // combat - use extra_frame when in combat
			if (gwin->in_combat() && entry.extra_frame >= 0) {
				frame = entry.extra_frame;
			}
			break;
		case 5:    // key/keyring - SI keyring override
			if (GAME_SI && is_party_item(485)) {
				sf    = SF_SHORTCUTBAR_VGA;
				shape = 3;    // keyring shape in shortcutbar.vga
				frame = trlucent ? 1 : 0;
				type  = SB_ITEM_KEYRING;
				name  = "keyring";
			}
			break;
		case 9:    // jawbone - use actual game object frame
			if (party_obj && sf == SF_SHAPES_VGA) {
				frame = party_obj->get_framenum();
			}
			break;
		}

		buttonItems[numButtons].shapeId = new ShapeID(shape, frame, sf);
		buttonItems[numButtons].name    = name;
		buttonItems[numButtons].type    = type;
		buttonItems[numButtons].translucent = force_translucent;
		numButtons++;
	}

	const int barItemWidth = numButtons > 0 ? (320 / numButtons) : 320;

	for (int i = 0; i < numButtons; i++, x += barItemWidth) {
		Shape_frame* frame  = buttonItems[i].shapeId->get_shape();
		const int    dX     = frame->get_xleft() + (barItemWidth - frame->get_width()) / 2;
		const int    dY     = frame->get_yabove() + (height - frame->get_height()) / 2;
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
		: Gump(nullptr, placex, placey, EXULT_FLX_TRANSPARENTMENU_SHP, SF_EXULT_FLX) {
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
		Shape_frame* frame = item.shapeId->get_shape();
		sman->paint_shape(x, y, frame, item.translucent);
		// when the bar is on the game screen it may need an outline
		if (frame && frame->is_rle()
			&& gwin->get_outline_color() < NPIXCOLORS && starty >= 0) {
			sman->paint_outline(x, y, frame, gwin->get_outline_color());
		}
	}

	gwin->set_painted();
}

int ShortcutBar_gump::handle_event(SDL_Event* event) {
	Game_window* gwin          = Game_window::get_instance();
	static bool  handle_events = true;
	// When the Save/Load menu is open, or the notebook, don't handle events
	if (gumpman->modal_gump_mode() || gwin->get_usecode()->in_usecode() || g_waiting_for_click || Notebook_gump::get_instance()) {
		// do not register a mouse up event on notebook checkmark
		if (Notebook_gump::get_instance()) {
			handle_events = false;
		}
		return 0;
	}

	if ((event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_UP) && handle_events) {
		int x;
		int y;
		gwin->get_win()->screen_to_game(event->button.x, event->button.y, gwin->get_fastmouse(), x, y);
		Gump*        on_gump = gumpman->find_gump(x, y);
		Gump_button* button;
		if (x >= startx && x <= (locx + width) && y >= starty && y <= (starty + height)) {
			// do not register a mouse up event when closing a gump via
			// checkmark over the bar
			if (on_gump && (button = on_gump->on_button(x, y)) && button->is_checkmark()) {
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
	printf("Item %s is %sclicked\n", buttonItems[index].name, doubleClicked ? "double " : "");

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
			ActionCheatScreen(nullptr);    // cheat_screen
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
