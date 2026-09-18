/*
 *  drag.cc - Dragging objects in Game_window.
 *
 *  Copyright (C) 2000-2025  The Exult Team
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
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "drag.h"

#include "Audio.h"
#include "Dynamic_container_gump.h"
#include "Gump.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "actors.h"
#include "barge.h"
#include "cheat.h"
#include "chunks.h"
#include "effects.h"
#include "gamemap.h"
#include "gamerend.h"
#include "gamewin.h"
#include "gumpinf.h"
#include "ignore_unused_variable_warning.h"
#include "mouse.h"
#include "paths.h"
#include "ucmachine.h"

#include <iostream> /* Debugging */

using std::cout;
using std::endl;

/*
 *  Create for a given (newly created) object.
 */
Dragging_info::Dragging_info(Game_object_shared newobj    // Object NOT in world.  This is
														  //   dropped, or deleted.
							 )
		: obj(std::move(newobj)), is_new(true), gump(nullptr), button(nullptr), mouse_widget(nullptr), widget_gump(nullptr),
		  old_pos(-1, -1, -1), old_foot(0, 0, 0, 0), old_lift(-1), quantity(obj->get_quantity()), readied_index(-1), mousex(-1),
		  mousey(-1), paintx(-1000), painty(-1000), mouse_shape(Mouse::mouse()->get_shape()), rect(0, 0, 0, 0), okay(true),
		  possible_theft(false) {
	rect = gwin->get_shape_rect(obj.get());
	rect.enlarge(8);    // Make a little bigger.
}

/*
 *  Begin a possible drag.
 */

Dragging_info::Dragging_info(
		int x, int y    // Mouse position.
		)
		: obj(nullptr), is_new(false), gump(nullptr), button(nullptr), mouse_widget(nullptr), widget_gump(nullptr),
		  old_pos(-1, -1, -1), old_foot(0, 0, 0, 0), old_lift(-1), quantity(0), readied_index(-1), mousex(x), mousey(y),
		  paintx(-1000), painty(-1000), mouse_shape(Mouse::mouse()->get_shape()), rect(0, 0, 0, 0), okay(false),
		  possible_theft(false) {
	// First see if it's a gump.
	gump   = gumpman->find_gump(x, y);
	int gx = x;
	int gy = y;
	gumpman->map_game_to_gump(gump, x, y, gx, gy);
	Game_object* to_drag = nullptr;
	// Debug: log click chain for dynamic gumps
	auto* dyn_gump = gump ? dynamic_cast<Dynamic_container_gump*>(gump) : nullptr;
	if (dyn_gump && (dyn_gump->get_debug_flags() & GUMP_DEBUG_CONSOLE)) {
		std::cerr << "[Drag] Click at screen (" << x << "," << y << ")"
				  << " -> find_gump: " << (gump ? "FOUND" : "null") << " (shape " << (gump ? gump->get_shapenum() : -1) << ")"
				  << std::endl;
	}
	if (gump) {
		to_drag = gump->find_object(gx, gy);
		if (to_drag) {
			// Save location info.
			gump->get_shape_location(to_drag, paintx, painty);
			old_pos = Tile_coord(to_drag->get_tx(), to_drag->get_ty(), 0);
			if (Main_actor* act = gwin->get_main_actor()) {
				old_lift = act->get_lift();
			}
		} else if ((button = gump->on_button(gx, gy)) != nullptr) {
			if (dyn_gump && (dyn_gump->get_debug_flags() & GUMP_DEBUG_CONSOLE)) {
				std::cerr << "[Drag] on_button -> FOUND button shape=" << button->get_shapenum() << " pos=(" << button->get_x()
						  << "," << button->get_y() << ")" << std::endl;
			}
			// Remember the owning gump so the release can be hit-tested in the
			// gump's own (scaled) layer coordinates.
			widget_gump = gump;
			gump        = nullptr;
			if (!button->is_draggable()) {
				return;
			}
			button->push(Gump::MouseButton::Left);
			// Pushed button, so make noise.
			if (!button->is_checkmark()) {
				Audio::get_ptr()->play_sound_effect(Audio::game_sfx(73));
			}
			gwin->set_painted();
		} else if ((mouse_widget = gump->forward_mouse_down(gx, gy, Gump::MouseButton::Left)) != nullptr) {
			// A widget (e.g. slider thumb/track) captured the click.
			if (dyn_gump && (dyn_gump->get_debug_flags() & GUMP_DEBUG_CONSOLE)) {
				std::cerr << "[Drag] forward_mouse_down -> widget captured click" << std::endl;
			}
			widget_gump = gump;
			gump        = nullptr;
		} else if (gump->is_draggable()) {
			if (dyn_gump && (dyn_gump->get_debug_flags() & GUMP_DEBUG_CONSOLE)) {
				std::cerr << "[Drag] on_button -> null (no button found at click pos), dragging gump instead" << std::endl;
			}
			// Dragging whole gump.
			paintx = gump->get_x();
			painty = gump->get_y();
			cout << "(x,y) rel. to gump is (" << (x - paintx) << ", " << (y - painty) << ")" << endl;
		} else {    // the gump isn't draggable
			return;
		}
	} else if (x > 0 && y > 0 && x < gwin->get_width() && y < gwin->get_height()) {    // Not found in gump?
		to_drag = gwin->find_object(x, y);
		if (!to_drag) {
			return;
		}
		// Get coord. where painted.
		gwin->get_shape_location(to_drag, paintx, painty);
		old_pos  = to_drag->get_tile();
		old_foot = to_drag->get_footprint();
	}
	if (to_drag) {
		quantity = to_drag->get_quantity();
		// Save original lift.
		old_lift = to_drag->get_outermost()->get_lift();
		obj      = to_drag->shared_from_this();
	}
	okay = true;
}

/*
 *  Clean up: free the dragged-object overlay layer.
 */
Dragging_info::~Dragging_info() {
	free_item_layer();
}

/*
 *  First motion.
 *
 *  Output: false if failed.
 */

bool Dragging_info::start(
		int x, int y    // Mouse position.
) {
	const int deltax = abs(x - mousex);
	const int deltay = abs(y - mousey);
	if (deltax <= 2 && deltay <= 2) {
		return false;    // Wait for greater motion.
	}
	if (obj) {
		// Don't want to move walls.
		if (!cheat.in_hack_mover() && !obj->is_dragable() && !obj->get_owner()) {
			Mouse::mouse()->flash_shape(Mouse::tooheavy);
			Audio::get_ptr()->play_sound_effect(Audio::game_sfx(76));
			obj  = nullptr;
			gump = nullptr;
			okay = false;
			return false;
		}
		Game_object* owner = obj->get_outermost();
		if (owner == obj.get()) {
			if (!cheat.in_hack_mover() && !Fast_pathfinder_client::is_grabable(gwin->get_main_actor(), obj.get())) {
				Mouse::mouse()->flash_shape(Mouse::redx);
				obj  = nullptr;
				okay = false;
				return false;
			}
		}
	}
	Mouse::mouse()->set_shape(Mouse::hand);
	// Remove text, so that we don't potentially paint the object under and
	// the mouse pointer over it.
	gwin->get_effects()->remove_text_effects();
	// Store original pos. on screen.
	rect = gump ? (obj ? gump->get_shape_rect(obj.get()) : gump->get_dirty()) : gwin->get_shape_rect(obj.get());
	if (gump) {    // Remove from actual position.
		if (obj) {
			int gx = mousex;
			int gy = mousey;
			gumpman->map_game_to_gump(gump, mousex, mousey, gx, gy);
			// Dragged items are painted in raw game coordinates. When the source
			// gump is scaled in a layer, the grab point (gx,gy) is in mapped
			// gump coordinates, so convert the anchor to raw game coordinates.
			const int anchor_dx          = gx - paintx;
			const int anchor_dy          = gy - painty;
			paintx                       = mousex - anchor_dx;
			painty                       = mousey - anchor_dy;
			Container_game_object* owner = gump->get_cont_or_actor(gx, gy);
			// Get the object
			Game_object* owner_obj  = gump->get_owner()->get_outermost();
			Main_actor*  main_actor = gwin->get_main_actor();
			// Check the range
			if (!cheat.in_hack_mover() && !Fast_pathfinder_client::is_grabable(main_actor, owner_obj)) {
				obj  = nullptr;
				gump = nullptr;
				okay = false;
				Mouse::mouse()->flash_shape(Mouse::outofrange);
				return false;
			}
			if (owner) {
				readied_index = owner->find_readied(obj.get());
			}
			gump->remove(obj.get());
		} else {
			gumpman->remove_gump(gump);
		}
	} else {
		Game_object_shared keep;
		obj->remove_this(&keep);    // This SHOULD work (jsf 21-12-01).
	}

	// include bbox in rect if bbox renderimg is enabled
	if (obj && gwin->get_render()->get_bbox_index() != -1) {
		const auto& info  = obj->get_info();
		int         bbx_w = (info.get_3d_xtiles(obj->get_framenum()) * c_tilesize) + (info.get_3d_height() * c_tilesize / 2) + 1;
		int         bbx_h = (info.get_3d_ytiles(obj->get_framenum()) * c_tilesize) + (info.get_3d_height() * c_tilesize / 2) + 1;

		TileRect bbox_rect(rect.x - bbx_w, rect.y - bbx_h, bbx_w + 1, bbx_h + 1);
		rect = rect.add(bbox_rect);
	}
	// Make a little bigger.
	// rect.enlarge(c_tilesize + obj ? 0 : c_tilesize/2);
	rect.enlarge(deltax > deltay ? deltax : deltay);

	TileRect crect = gwin->clip_to_win(rect);
	gwin->paint(crect);    // Paint over obj's. area.
	return true;
}

/*
 *  Mouse was moved while dragging.
 *
 *  Output: true iff movement started/continued.
 */

bool Dragging_info::moved(
		int x, int y    // Mouse pos. in window.
) {
	// Forward drag to a widget that captured the initial click.
	if (mouse_widget) {
		int wx = x;
		int wy = y;
		gumpman->map_game_to_gump(widget_gump, x, y, wx, wy);
		mouse_widget->mouse_drag(wx, wy);
		return true;
	}
	if (!obj && !gump) {
		return false;
	}
	if (rect.w == 0) {
		if (!start(x, y)) {
			return false;
		}
	} else {
		gwin->add_dirty(gwin->clip_to_win(rect));
	}
	gwin->set_painted();
	const int deltax = x - mousex;
	const int deltay = y - mousey;
	mousex           = x;
	mousey           = y;
	// Shift to new position.
	rect.shift(deltax, deltay);
	paintx += deltax;
	painty += deltay;
	if (gump && !obj) {    // Dragging a gump?
		gump->set_pos(paintx, painty);
	}
	gwin->add_dirty(gwin->clip_to_win(rect));
	return true;
}

// The dragged item's layer sits directly beneath the mouse-pointer layer.
static const int item_layer_z = (1 << 20) - 1;

/*
 *  Destroy the dragged-object overlay layer (if any).
 */
void Dragging_info::free_item_layer() {
	// Only touch the window if it still exists (at shutdown it may be gone).
	if (item_layer >= 0 && Game_window::get_instance() == gwin) {
		gwin->destroy_layer(item_layer);
	}
	item_layer   = -1;
	item_layer_w = 0;
	item_layer_h = 0;
}

/*
 *  Render the dragged object into its own layer, scaled and placed to
 *  match the mouse-pointer layer.
 */
void Dragging_info::paint_obj_to_layer() {
	Shape_frame* frame = obj->get_shape();
	if (!frame) {
		return;
	}
	const int xleft  = frame->get_xleft();
	const int yabove = frame->get_yabove();
	const int w      = frame->get_width();
	const int h      = frame->get_height();
	if (w <= 0 || h <= 0) {
		return;
	}
	Image_window* iwin = gwin->get_win();
	// (Re)create the layer if missing or the shape size changed.
	if (item_layer < 0 || item_layer_w != w || item_layer_h != h) {
		if (item_layer >= 0) {
			gwin->destroy_layer(item_layer);
			item_layer = -1;
		}
		item_layer = gwin->create_layer("item", w, h, 255, 0, item_layer_z);
		if (item_layer < 0) {
			return;
		}
		// Derive scale/placement settings from the mouse-pointer layer.
		gwin->layer_set_ui_kind(item_layer, Image_window::UiLayerMousePointer);
		item_layer_w = w;
		item_layer_h = h;
	}
	gwin->layer_set_z(item_layer, item_layer_z);

	Image_buffer8* lbuf = gwin->get_layer_ibuf(item_layer);
	if (!lbuf) {
		return;
	}
	lbuf->clear_clip();
	lbuf->fill8(255);    // Fully transparent.
	Image_buffer8* prev = gwin->push_render_target(lbuf);
	// Paint the shape at its origin within the layer buffer.
	if (obj->get_flag(Obj_flags::invisible)) {
		obj->paint_invisible(xleft, yabove);
	} else {
		obj->paint_shape(xleft, yabove);
	}
	gwin->pop_render_target(prev);
	gwin->layer_set_dirty(item_layer);

	// Size the dragged item to match the mouse pointer, but ENLARGE ONLY: never
	// shrink it below its native (game -> screen) size. So compute the item's
	// native world scale, then grow each axis up to the pointer scale if the
	// pointer is larger; if the pointer would shrink it, keep it native.
	float     sx    = 1.0f;
	float     sy    = 1.0f;
	const int gamew = iwin->get_game_width();
	const int gameh = iwin->get_game_height();
	if (gamew > 0 && gameh > 0) {
		int gx0;
		int gy0;
		int gx1;
		int gy1;
		iwin->game_to_screen(0, 0, false, gx0, gy0);
		iwin->game_to_screen(gamew, gameh, false, gx1, gy1);
		sx = static_cast<float>(gx1 - gx0) / static_cast<float>(gamew);
		sy = static_cast<float>(gy1 - gy0) / static_cast<float>(gameh);
		if (sx <= 0.0f) {
			sx = 1.0f;
		}
		if (sy <= 0.0f) {
			sy = 1.0f;
		}
	}
	if (Mouse::mouse()) {
		float px = sx;
		float py = sy;
		Mouse::mouse()->get_pointer_scale(px, py);
		if (px > sx) {
			sx = px;    // Enlarge to the pointer size.
		}
		if (py > sy) {
			sy = py;
		}
	}
	int cx;
	int cy;
	iwin->game_to_screen(mousex, mousey, false, cx, cy);
	const float ox = static_cast<float>(cx) + static_cast<float>(paintx - mousex) * sx;
	const float oy = static_cast<float>(cy) + static_cast<float>(painty - mousey) * sy;
	const int   dx = static_cast<int>(ox - xleft * sx);
	const int   dy = static_cast<int>(oy - yabove * sy);
	gwin->layer_set_dest(item_layer, dx, dy, static_cast<int>(w * sx), static_cast<int>(h * sy));
	gwin->layer_set_visible(item_layer, true);
}

/*
 *  Paint object being moved.
 */

void Dragging_info::paint() {
	if (!rect.w) {    // Not moved enough yet?
		return;
	}
	if (obj) {
		const int bbox = gwin->get_render()->get_bbox_index();
		// In map-edit mode the dragged item is not promoted to its own layer.
		// Once the drop has started also revert back so the item paints below
		// a possible quantity slider gump.
		if (bbox != -1 || cheat.in_map_editor() || dropping) {
			// Legacy direct paint into the main buffer.
			free_item_layer();
			if (obj->get_flag(Obj_flags::invisible)) {
				obj->paint_invisible(paintx, painty);
			} else {
				// paint bbox back
				if (bbox != -1) {
					obj->get_info().paint_bbox(
							paintx, painty, obj->get_framenum(), Game_window::get_instance()->get_win()->get_ib8(), bbox, 2);
				}
				obj->paint_shape(paintx, painty);
				// paint bbox front
				if (bbox != -1) {
					obj->get_info().paint_bbox(
							paintx, painty, obj->get_framenum(), Game_window::get_instance()->get_win()->get_ib8(), bbox, 1);
				}
			}
			return;
		}
		// Normal case: render the dragged object into its own overlay layer.
		paint_obj_to_layer();
	} else if (gump) {
		// The dragged gump lives outside the open-gump list, so render it into
		// its own overlay layer (bumped above the other gumps) so it scales
		// exactly like it does when settled - no snap on drop.
		gumpman->render_gump_to_layer(gump, 1 << 19);
	}
}

/*
 *  Mouse was released, so drop object.
 *      Return true iff the dropping mouseclick has been handled.
 *      (by buttonpress, drag)
 */

bool Dragging_info::drop(
		int x, int y,    // Mouse pos.
		bool moved       // has mouse moved from starting pos?
) {
	bool handled = moved;
	Mouse::mouse()->set_shape(mouse_shape);
	// The drop is finishing: stop promoting the item to its high-z overlay
	// layer and free it now, so anything shown during the drop (e.g. the
	// quantity slider) is not covered by the dragged item.
	dropping = true;
	free_item_layer();
	if (mouse_widget) {
		// Release a widget that captured the initial click (e.g. slider).
		int wx = x;
		int wy = y;
		gumpman->map_game_to_gump(widget_gump, x, y, wx, wy);
		mouse_widget->mouse_up(wx, wy, Gump::MouseButton::Left);
		mouse_widget = nullptr;
		widget_gump  = nullptr;
		gwin->paint();
		return true;
	}
	if (button) {
		button->unpush(Gump::MouseButton::Left);
		int bx = x;
		int by = y;
		gumpman->map_game_to_gump(widget_gump, x, y, bx, by);
		const bool release_hit = button->on_button(bx, by);
		if (release_hit) {
			// Clicked on button.
			button->activate(Gump::MouseButton::Left);
		}
		handled = true;
	} else if (!obj) {    // Only dragging a gump?
		if (!gump) {
			return handled;
		}
		if (!moved) {    // A click just raises it to the top.
			gumpman->remove_gump(gump);
		}
		gumpman->add_gump(gump);
	} else if (!moved) {    // For now, if not moved, leave it.
		return handled;
	} else if (!drop(x, y)) {    // Drop it.
		put_back();              // Wasn't (all) moved.
	}
	obj  = nullptr;    // Clear so we don't paint them.
	gump = nullptr;
	free_item_layer();    // Object returns to the main layer.
	gwin->paint();
	return handled;
}

/*
 *  Check weight.
 *
 *  Output: false if too heavy, with mouse flashed.
 */

static bool Check_weight(
		Game_window* gwin, Game_object* to_drop,
		Game_object* owner    // Who the new owner will be.
) {
	ignore_unused_variable_warning(gwin);
	if (cheat.in_hack_mover()) {    // hack-mover  -> no weight checking
		return true;
	}

	if (!owner) {
		return true;
	}
	owner = owner->get_outermost();
	if (!owner->get_flag(Obj_flags::in_party)) {
		return true;    // Not a party member, so okay.
	}
	const int wt = owner->get_weight() + to_drop->get_weight();
	if (wt / 10 > owner->get_max_weight()) {
		Mouse::mouse()->flash_shape(Mouse::tooheavy);
		Audio::get_ptr()->play_sound_effect(Audio::game_sfx(76));
		return false;
	}
	return true;
}

/*
 *  Put back object where it came from.
 */

void Dragging_info::put_back() {
	if (gump) {    // Put back remaining/orig. piece.
		// And don't check for volume!
		// Restore saved vals.
		obj->set_shape_pos(old_pos.tx, old_pos.ty);
		// 1st try with dont_check==false so usecode gets called.
		if (!gump->add(obj.get(), -2, -2, -2, -2, false)) {
			gump->add(obj.get(), -2, -2, -2, -2, true);
		}
	} else if (is_new) {
		obj->set_invalid();    // It's not in the world.
		obj->remove_this();
	} else {    // Normal object.  Put it back.
		obj->move(old_pos);
	}
	obj    = nullptr;    // Just to be safe.
	is_new = false;
}

/*
 *  Drop object on a gump.
 *
 *  Output: False if not (all) of object was dropped.
 */

bool Dragging_info::drop_on_gump(
		int x, int y,            // Mouse position.
		Game_object* to_drop,    // == obj if whole thing.
		Gump*        on_gump     // Gump to drop it on.
) {
	Game_object* owner_obj = on_gump->get_owner();
	if (owner_obj) {
		owner_obj = owner_obj->get_outermost();
	}
	Main_actor* main_actor = gwin->get_main_actor();
	// always red X and ding when putting into itself
	if (owner_obj == obj.get()) {
		Mouse::mouse()->flash_shape(Mouse::redx);
		Audio::get_ptr()->play_sound_effect(Audio::game_sfx(76));
		return false;
	}
	// Check the range
	if (owner_obj && !cheat.in_hack_mover() && !Fast_pathfinder_client::is_grabable(main_actor, owner_obj)) {
		// Object was not grabable
		Mouse::mouse()->flash_shape(Mouse::outofrange);
		return false;
	}
	int gx = x;
	int gy = y;
	gumpman->map_game_to_gump(on_gump, x, y, gx, gy);
	if (!Check_weight(gwin, to_drop, on_gump->get_cont_or_actor(gx, gy))) {
		return false;
	}
	if (on_gump != gump) {    // Not moving within same gump?
		possible_theft = true;
	}
	// Add, and allow to combine.
	if (!on_gump->add(to_drop, gx, gy, paintx, painty, false, true)) {
		// Failed.
		if (to_drop != obj.get()) {
			// Watch for partial drop.
			const int nq = to_drop->get_quantity();
			if (nq < quantity) {
				obj->modify_quantity(quantity - nq);
			}
		}
		Mouse::mouse()->flash_shape(Mouse::wontfit);
		return false;
	}
	return true;
}

/*
 *  See if there's something blocking an object at a given point.
 */

static bool Is_inaccessible(Game_window* gwin, Game_object* obj, int x, int y) {
	Game_object* block = gwin->find_object(x, y);
	return block && block != obj && !block->is_dragable();
}

/*
 *  Drop object onto the map.
 *
 *  Output: False if not (all) of object was dropped.
 */

bool Dragging_info::drop_on_map(
		int x, int y,           // Mouse position.
		Game_object* to_drop    // == obj if whole thing.
) {
	// Attempting to drop off screen?
	if (x < 0 || y < 0 || x >= gwin->get_width() || y >= gwin->get_height()) {
		Mouse::mouse()->flash_shape(Mouse::redx);
		Audio::get_ptr()->play_sound_effect(Audio::game_sfx(76));
		return false;
	}

	int       max_lift = cheat.in_hack_mover() ? 255 : gwin->get_main_actor()->get_lift() + 5;
	const int skip     = gwin->get_render_skip_lift();
	if (max_lift >= skip) {    // Don't drop where we cannot see.
		max_lift = skip - 1;
	}
	// Drop where we last painted it.
	int posx = paintx;
	int posy = painty;
	if (posx == -1000) {    // Unless we never painted.
		posx = x;
		posy = y;
	}
	int lift;
	// Was it dropped on something?
	Game_object* found   = gwin->find_object(x, y);
	int          dropped = 0;    // 1 when dropped.
	if (found && found != obj.get()) {
		if (!Check_weight(gwin, to_drop, found)) {
			return false;
		}
		if (found->drop(to_drop)) {
			dropped        = 1;
			possible_theft = true;
		}
		// Try to place on 'found'.
		else if ((lift = found->get_lift() + found->get_info().get_3d_height()) <= max_lift) {
			dropped = gwin->drop_at_lift(to_drop, posx, posy, lift);
		} else if (
				// Object is too tall to drop on.
				(lift = found->get_info().get_3d_height()) > max_lift) {
			Mouse::mouse()->flash_shape(Mouse::redx);
			Audio::get_ptr()->play_sound_effect(Audio::game_sfx(76));
			return false;
		} else {
			// Too high.
			Mouse::mouse()->flash_shape(Mouse::blocked);
			Audio::get_ptr()->play_sound_effect(Audio::game_sfx(76));
			return false;
		}
	}
	// Find where to drop it, but stop if
	//   it will end up hidden (-1).
	for (lift = old_lift; !dropped && lift <= max_lift; lift++) {
		dropped = gwin->drop_at_lift(to_drop, posx, posy, lift);
	}

	if (dropped <= 0) {
		Mouse::mouse()->flash_shape(Mouse::redx);
		Audio::get_ptr()->play_sound_effect(Audio::game_sfx(76));
		return false;
	}
	// Moved more than 2 tiles.
	if (!gump && !possible_theft && to_drop->get_tile().distance(old_pos) > 2) {
		possible_theft = true;
	}
	return true;
}

/*
 *  Drop at given position.
 *  ++++++NOTE:  Potential problems here with 'to_drop' being deleted by
 *      call to add().  Probably add() should provide feedback if obj.
 *      is combined with another.
 *
 *  Output: False if put_back() should be called.
 */

bool Dragging_info::drop(
		int x, int y    // Mouse position.
) {
	// Get orig. loc. info.
	const int          oldcx   = old_pos.tx / c_tiles_per_chunk;
	const int          oldcy   = old_pos.ty / c_tiles_per_chunk;
	Game_object*       to_drop = obj.get();    // If quantity, split it off.
	Game_object_shared to_drop_shared;
	// Being liberal about taking stuff:
	const bool okay_to_move = to_drop->get_flag(Obj_flags::okay_to_take);
	const int  old_top      = old_pos.tz + obj->get_info().get_3d_height();
	// First see if it's a gump.
	Gump* on_gump = gumpman->find_gump(x, y);
	// Don't prompt if within same gump
	// or if alternate drop is enabled (ctrl inverts).

	const bool* keystate = SDL_GetKeyboardState(nullptr);
	const bool  drop     = (keystate[SDL_SCANCODE_LCTRL] || keystate[SDL_SCANCODE_RCTRL]) ? gwin->get_alternate_drop()
																						  : !gwin->get_alternate_drop();
	const bool  temp     = obj->get_flag(Obj_flags::is_temporary);

	bool escaped = false;
	if (quantity > 1 && (!on_gump || on_gump != gump) && drop) {
		quantity = gumpman->prompt_for_number(0, quantity, 1, quantity, nullptr, &escaped);
	}

	if (quantity <= 0 || escaped) {
		return false;
	}
	if (quantity < obj->get_quantity()) {
		// Need to drop a copy.
		to_drop_shared = gmap->create_ireg_object(obj->get_shapenum(), obj->get_framenum());
		to_drop        = to_drop_shared.get();
		to_drop->modify_quantity(quantity - 1);
		if (okay_to_move) {    // Make sure copy is okay to take.
			to_drop->set_flag(Obj_flags::okay_to_take);
		}
		if (temp) {
			to_drop->set_flag(Obj_flags::is_temporary);
		}
	}
	// Drop it.
	if (!(on_gump ? drop_on_gump(x, y, to_drop, on_gump) : drop_on_map(x, y, to_drop))) {
		return false;
	}
	// Make a 'dropped' sound.
	Audio::get_ptr()->play_sound_effect(Audio::game_sfx(74));
	if (!gump) {    // Do eggs where it came from.
		gmap->get_chunk(oldcx, oldcy)->activate_eggs(obj.get(), old_pos.tx, old_pos.ty, old_pos.tz, old_pos.tx, old_pos.ty);
	}
	// Special:  BlackSword in SI.
	else if (readied_index >= 0 && obj->get_shapenum() == 806) {
		// Do 'unreadied' usecode.
		int gx = x;
		int gy = y;
		gumpman->map_game_to_gump(gump, x, y, gx, gy);
		gump->get_cont_or_actor(gx, gy)->call_readied_usecode(readied_index, obj.get(), Usecode_machine::unreadied);
	}
	// On a barge?
	Barge_object* barge = gwin->get_moving_barge();
	if (barge) {
		barge->set_to_gather();    // Refigure what's on barge.
	}
	// Check for theft.
	if (!okay_to_move && !cheat.in_hack_mover() && possible_theft && !gwin->is_in_dungeon()) {
		gwin->theft();
	}
	if (to_drop == obj.get()) {    // Whole thing?
		// Watch for stuff on top of it.
		if (old_foot.w > 0) {
			Map_chunk::gravity(old_foot, old_top);
		}
		return true;    // All done.
	}
	// Subtract quantity moved.
	obj->modify_quantity(-quantity);
	return false;    // Put back the rest.
}

/*
 *  Begin a possible drag when the mouse button is depressed.  Also detect
 *  if the 'close' checkmark on a gump is being depressed.
 *
 *  Output: true iff object selected for dragging
 */

bool Game_window::start_dragging(
		int x, int y    // Position in window.
) {
	delete dragging;
	dragging = new Dragging_info(x, y);
	if (dragging->okay) {
		return true;    // Success, so far.
	}
	delete dragging;
	dragging = nullptr;
	return false;
}

/*
 *  Mouse moved while dragging.
 */

bool Game_window::drag(
		int x, int y    // Mouse position in window.
) {
	return dragging ? dragging->moved(x, y) : false;
}

/*
 *  Mouse was released, so drop object.
 *      Return true iff the dropping mouseclick has been handled.
 *      (by buttonpress, drag)
 *  Output: MUST set dragging = nullptr.
 */

bool Game_window::drop_dragged(
		int x, int y,    // Mouse pos.
		bool moved       // has mouse moved from starting pos?
) {
	if (!dragging) {
		return false;
	}
	const bool handled = dragging->drop(x, y, moved);
	delete dragging;
	dragging = nullptr;
	return handled;
}

void Game_window::stop_dragging() {
	delete dragging;
	dragging = nullptr;
}

/*
 *  Try to drop at a given lift.  Note:  None of the drag state variables
 *  may be used here, as it's also called from the outside.
 *
 *  Output: 1 if successful.
 *      0 if blocked
 *      -1 if it would end up hidden by a non-moveable object.
 */

int Game_window::drop_at_lift(
		Game_object* to_drop, int x, int y,    // Pixel coord. in window.
		int at_lift) {
	x += at_lift * 4 - 1;    // Take lift into account, round.
	y += at_lift * 4 - 1;
	const int         tx    = (scrolltx + x / c_tilesize) % c_num_tiles;
	const int         ty    = (scrollty + y / c_tilesize) % c_num_tiles;
	const int         cx    = tx / c_tiles_per_chunk;
	const int         cy    = ty / c_tiles_per_chunk;
	Map_chunk*        chunk = map->get_chunk(cx, cy);
	int               lift;    // Can we put it here?
	const Shape_info& info   = to_drop->get_info();
	const int         frame  = to_drop->get_framenum();
	const int         xtiles = info.get_3d_xtiles(frame);
	const int         ytiles = info.get_3d_ytiles(frame);
	int               max_drop;
	int               move_flags;
	if (cheat.in_hack_mover()) {
		max_drop = at_lift - cheat.get_edit_lift();
		//		max_drop = max_drop < 0 ? 0 : max_drop;
		if (max_drop < 0) {    // Below lift we're editing?
			return 0;
		}
		move_flags = MOVE_WALK | MOVE_MAPEDIT;
	} else {
		// Allow drop of 5;
		max_drop   = 5;
		move_flags = MOVE_WALK;
	}
	if (Map_chunk::is_blocked(
				info.get_3d_height(), at_lift, tx - xtiles + 1, ty - ytiles + 1, xtiles, ytiles, lift, move_flags, max_drop)) {
		return 0;
	}

	// Needs to be reachable, except when it is just lower, so the object can
	// fall down.
	if (!cheat.in_hack_mover()) {
		bool grabbable = Fast_pathfinder_client::is_grabable(main_actor, Tile_coord(tx, ty, lift));
		if (!grabbable && main_actor->get_lift() > lift) {
			grabbable = Fast_pathfinder_client::is_grabable(main_actor, Tile_coord(tx, ty, main_actor->get_lift()));
		}
		if (!grabbable) {
			return 0;
		}
	}

	to_drop->set_invalid();
	to_drop->move(tx, ty, lift);
	const TileRect rect = get_shape_rect(to_drop);
	// Avoid dropping behind walls.
	if (Is_inaccessible(this, to_drop, rect.x + 2, rect.y + 2) && Is_inaccessible(this, to_drop, rect.x + rect.w - 3, rect.y + 2)
		&& Is_inaccessible(this, to_drop, rect.x + 2, rect.y + rect.h - 3)
		&& Is_inaccessible(this, to_drop, rect.x + rect.w - 3, rect.y + rect.h - 3)
		&& Is_inaccessible(this, to_drop, rect.x + (rect.w >> 1), rect.y + (rect.h >> 1))) {
		Game_object_shared keep;
		to_drop->remove_this(&keep);
		return -1;
	}
#ifdef DEBUG
	cout << "Dropping object at (" << tx << ", " << ty << ", " << lift << ")" << endl;
#endif
	// On an egg?
	chunk->activate_eggs(to_drop, tx, ty, lift, tx, ty);

	if (to_drop == main_actor) {
		center_view(to_drop->get_tile());
		paint();
	}
	return 1;
}
