/*
 *  Gump_manager.cc - Object that manages all available gumps
 *
 *  Copyright (C) 2001-2024  The Exult Team
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

#include "Gump_manager.h"

#include "Actor_gump.h"
#include "Audio.h"
#include "Book_gump.h"
#include "CombatStats_gump.h"
#include "Configuration.h"
#include "Dynamic_container_gump.h"
#include "Face_stats.h"
#include "Gump.h"
#include "Jawbone_gump.h"
#include "Paperdoll_gump.h"
#include "Scroll_gump.h"
#include "ShortcutBar_gump.h"
#include "Sign_gump.h"
#include "Slider_gump.h"
#include "Spellbook_gump.h"
#include "Stats_gump.h"
#include "Yesno_gump.h"
#include "actors.h"
#include "exult.h"
#include "game.h"
#include "gamewin.h"
#include "gump_utils.h"
#include "items.h"
#include "jawbone.h"
#include "npcnear.h"
#include "spellbook.h"
#include "touchui.h"
#include "ucmachine.h"

#include <iostream>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#endif    // __GNUC__
static const SDL_MouseID EXSDL_TOUCH_MOUSEID = SDL_TOUCH_MOUSEID;
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

using std::cout;
using std::endl;

Gump_manager::Gump_manager() {
	std::string str;
	config->value("config/gameplay/right_click_closes_gumps", str, "yes");
	if (str == "no") {
		right_click_close = false;
	}
	config->set("config/gameplay/right_click_closes_gumps", str, true);

	config->value("config/gameplay/gumps_dont_pause_game", str, "no");
	dont_pause_game = str == "yes";
	config->set("config/gameplay/gumps_dont_pause_game", dont_pause_game ? "yes" : "no", true);
}

// Per-gump overlay layers are composited above the conversation layer (z 0)
// and below the mouse cursor (z 1<<20). Later gumps in the list get a higher z
// so they stack on top; the dragged gump is bumped above every other gump
// (see Dragging_info::paint, which uses z 1<<19).
namespace {
	constexpr int gump_layer_z_base            = 1 << 18;
	constexpr int gump_layer_text_tier_offset  = 1 << 16;
	constexpr int gump_layer_modal_tier_offset = 1 << 17;
	// Extra margin around a gump's reported bounds so outlines/badges are not
	// clipped by its layer buffer.
	constexpr int gump_layer_margin = 4;

	bool is_text_gump(const Gump* g) {
		return dynamic_cast<const Scroll_gump*>(g) != nullptr || dynamic_cast<const Sign_gump*>(g) != nullptr
			   || dynamic_cast<const Book_gump*>(g) != nullptr;
	}

	int z_tier_offset_for(const Gump* g) {
		if (g->is_modal()) {
			return gump_layer_modal_tier_offset;
		}
		if (is_text_gump(g)) {
			return gump_layer_text_tier_offset;
		}
		return 0;
	}

	// Pick the UI layer kind (and thus the config/video/ui/* size + scaler
	// override) for a gump's overlay layer: HUD gumps (shortcut bar,
	// face-stats), text gumps (scroll/sign/book), modal gumps, and ordinary
	// gumps each get their own.
	Image_window::UiLayerKind ui_kind_for(Gump* g, bool is_hud) {
		if (is_hud) {
			return Image_window::UiLayerHudGumps;
		}
		if (is_text_gump(g)) {
			return Image_window::UiLayerTextGumps;
		}
		if (g->is_modal()) {
			return Image_window::UiLayerModalGumps;
		}
		return Image_window::UiLayerGumps;
	}
}    // namespace

void Gump_manager::render_gump_to_layer(Gump* g, int z) {
	if (!g) {
		return;
	}
	const bool is_hud = dynamic_cast<ShortcutBar_gump*>(g) != nullptr || dynamic_cast<Face_stats*>(g) != nullptr;
	const int  parts  = is_hud ? g->hud_part_count() : 1;
	for (int part = 0; part < parts; ++part) {
		render_gump_part_to_layer(g, z, part, is_hud);
	}
	// If a previously-used second layer is no longer needed, hide it.
	if (parts < 2 && g->render_layer2 >= 0) {
		gwin->layer_set_visible(g->render_layer2, false);
	}
}

void Gump_manager::render_gump_part_to_layer(Gump* g, int z, int part, bool is_hud) {
	int&      layer  = (part == 0) ? g->render_layer : g->render_layer2;
	TileRect& bounds = (part == 0) ? g->layer_bounds : g->layer_bounds2;

	// Tight game-coordinate content rect for this part, then the padded buffer.
	TileRect content = (g->hud_part_count() > 1) ? g->hud_part_rect(part) : g->get_dirty();
	// An empty content rect means this part has nothing to draw (e.g. the
	// right-hand face-stats column when there are four or fewer party members);
	// hide its layer. This must be checked before enlarge(), which would inflate
	// a 0x0 rect into a non-empty buffer.
	if (content.w <= 0 || content.h <= 0) {
		if (layer >= 0) {
			gwin->layer_set_visible(layer, false);
		}
		return;
	}
	TileRect b = content;
	b.enlarge(gump_layer_margin);
	if (b.w <= 0 || b.h <= 0) {
		if (layer >= 0) {
			gwin->layer_set_visible(layer, false);
		}
		return;
	}
	// (Re)create the layer if missing or the bounds changed size.
	if (layer < 0 || bounds.w != b.w || bounds.h != b.h) {
		if (layer >= 0) {
			gwin->destroy_layer(layer);
			layer = -1;
		}
		layer = gwin->create_layer(b.w, b.h, 255, 0, z);
		if (layer < 0) {
			return;
		}
		gwin->layer_set_ui_kind(layer, ui_kind_for(g, is_hud));
	}
	bounds = b;
	gwin->layer_set_z(layer, z);

	Image_buffer8* lbuf = gwin->get_layer_ibuf(layer);
	if (!lbuf) {
		return;
	}
	// Clear the buffer to transparent and paint the gump 0-based (shifted by
	// -b.x,-b.y). Content outside this part's bounds is clipped away by the
	// buffer, so the two mode-3 groups each land in their own layer.
	lbuf->clear_clip();
	lbuf->fill8(255);    // Fully transparent.
	Image_buffer8* prev = gwin->push_render_target(lbuf);
	g->paint_shifted(-b.x, -b.y);
	gwin->pop_render_target(prev);

	Image_window* iwin = gwin->get_win();
	const float   f    = iwin->get_ui_scale_factor(ui_kind_for(g, is_hud));
	int           csx;
	int           csy;
	iwin->game_to_screen(b.x + b.w / 2, b.y + b.h / 2, false, csx, csy);
	float dw = static_cast<float>(b.w) * f;
	float dh = static_cast<float>(b.h) * f;
	float dx = static_cast<float>(csx) - dw / 2.0f;
	float dy = static_cast<float>(csy) - dh / 2.0f;
	// HUD gumps (shortcut bar, face-stats) follow the gumps size setting via
	// get_ui_scale_factor: Full = a fixed display size, Auto = the game area's
	// native size (matches the main game layer), 1/2/3 interpolate.
	if (is_hud) {
		const float dispw = static_cast<float>(iwin->get_display_width());
		const float disph = static_cast<float>(iwin->get_display_height());
		// Anchor detection in GAME coordinates (content vs the game area).
		const int gsx         = iwin->get_start_x();
		const int gsy         = iwin->get_start_y();
		const int gex         = iwin->get_end_x();
		const int gey         = iwin->get_end_y();
		const int lmg         = content.x - gsx;
		const int rmg         = gex - (content.x + content.w);
		const int tmg         = content.y - gsy;
		const int bmg         = gey - (content.y + content.h);
		const int tolg        = 8;    // game pixels
		const int xanc_forced = g->hud_part_xanchor(part);
		const int yanc_forced = g->hud_part_yanchor(part);
		int       xanc        = xanc_forced;
		int       yanc        = yanc_forced;
		if (xanc < 0) {
			const int d = lmg > rmg ? lmg - rmg : rmg - lmg;
			xanc        = (d <= tolg) ? 1 : (lmg < rmg ? 0 : 2);
		}
		if (yanc < 0) {
			const int d = tmg > bmg ? tmg - bmg : bmg - tmg;
			yanc        = (d <= tolg) ? 1 : (tmg < bmg ? 0 : 2);
		}
		if (xanc == 1) {
			dx = (dispw - dw) / 2.0f;    // centered on the display
		} else if (xanc == 0) {
			dx = static_cast<float>(b.x - gsx) * f;    // game-area left edge
		} else {
			dx = dispw - static_cast<float>(gex - b.x) * f;    // game-area right edge
		}
		if (yanc == 1) {
			dy = (disph - dh) / 2.0f;
		} else if (yanc == 0) {
			dy = static_cast<float>(b.y - gsy) * f;    // game-area top edge
		} else {
			dy = disph - static_cast<float>(gey - b.y) * f;    // game-area bottom edge
		}
	}
	if (g->is_modal() && !g->is_draggable()) {
		const float dispw = static_cast<float>(iwin->get_display_width());
		const float disph = static_cast<float>(iwin->get_display_height());
		if (dw <= dispw) {
			dx = dx < 0.0f ? 0.0f : (dx > dispw - dw ? dispw - dw : dx);
		} else {
			dx = (dispw - dw) / 2.0f;
		}
		if (dh <= disph) {
			dy = dy < 0.0f ? 0.0f : (dy > disph - dh ? disph - dh : dy);
		} else {
			dy = (disph - dh) / 2.0f;
		}
	}
	gwin->layer_set_dest(layer, static_cast<int>(dx), static_cast<int>(dy), static_cast<int>(dw), static_cast<int>(dh));
	gwin->layer_set_visible(layer, true);
	// The shortcut bar always gets the translucency index table. It only
	// affects pixels whose palette index is in the translucent/xform range
	// (the entry is non-zero there): those render with a FIXED alpha/colour
	// instead of the live, colour-cycling palette. This covers the fully
	// translucent bar as well as a single missing-item icon shown dimmed in
	// an otherwise solid bar. Normal opaque icon pixels have a zero table
	// entry and stay fully opaque, so the solid bar looks unchanged.
	ShortcutBar_gump* sb = dynamic_cast<ShortcutBar_gump*>(g);
	if (sb) {
		gwin->layer_set_index_argb(layer, Shape_manager::get_instance()->get_translucency_argb());
	} else {
		gwin->layer_set_index_argb(layer, nullptr);
	}
	gwin->layer_set_dirty(layer);
}

bool Gump_manager::map_game_to_gump(const Gump* g, int gx, int gy, int& lx, int& ly) const {
	if (!g || g->render_layer < 0 || !gwin->layer_is_visible(g->render_layer)) {
		lx = gx;
		ly = gy;
		return true;
	}
	int sx;
	int sy;
	gwin->get_win()->game_to_screen(gx, gy, false, sx, sy);
	int        llx;
	int        lly;
	const bool inside = gwin->screen_to_layer(g->render_layer, sx, sy, llx, lly);
	const int  lx0    = llx + g->layer_bounds.x;
	const int  ly0    = lly + g->layer_bounds.y;
	// Two-part HUD gump (Face_stats mode 3): if the point is not inside the
	// first part's layer, try the second part's layer before giving up.
	if (!inside && g->render_layer2 >= 0 && gwin->layer_is_visible(g->render_layer2)) {
		int        llx2;
		int        lly2;
		const bool inside2 = gwin->screen_to_layer(g->render_layer2, sx, sy, llx2, lly2);
		if (inside2) {
			lx = llx2 + g->layer_bounds2.x;
			ly = lly2 + g->layer_bounds2.y;
			return true;
		}
	}
	// Always output the (possibly extrapolated) gump-local coordinate so a
	// drag can continue past the gump's edge; the bool only reports whether
	// the point is actually within the gump's layer (used for hit-testing).
	lx = lx0;
	ly = ly0;
	return inside;
}

void Gump_manager::render_gumps_to_layer(bool modal) {
	// The dimmed backdrop behind modal gumps is drawn straight to the window
	// (it is full-screen and does not scale).
	if (modal && background) {
		background->paint();
	}
	int idx = 0;
	for (Gump_list* gmp = open_gumps; gmp; gmp = gmp->next, ++idx) {
		Gump* g = gmp->gump;
		if (g->is_modal() != modal) {
			continue;
		}
		if (g->uses_render_layer()) {
			render_gump_to_layer(g, gump_layer_z_base + z_tier_offset_for(g) + idx);
		} else {
			g->paint();
		}
	}
}

/*
 *  Showing gumps.
 */

bool Gump_manager::showing_gumps(bool no_pers) const {
	// If no gumps, or we do want to check for persistent, just check to see if
	// any exist
	if (!no_pers || !open_gumps) {
		return open_gumps != nullptr;
	}

	// If we don't want to check for persistend
	for (Gump_list* gump = open_gumps; gump; gump = gump->next) {
		if (!gump->gump->is_persistent()) {
			return true;
		}
	}

	return false;
}

/*
 *  Find the highest gump that the mouse cursor is on.
 *
 *  Output: ->gump, or null if none.
 */

Gump* Gump_manager::find_gump(
		int x, int y,    // Pos. on screen.
		bool pers        // Persistent?
) {
	Gump_list* gmp;
	Gump*      found = nullptr;    // We want last found in chain.
	for (gmp = open_gumps; gmp; gmp = gmp->next) {
		Gump* gump = gmp->gump;
		if (!gump) {
			continue;
		}
		// Each gump has its own (scaled) layer, so map the point through this
		// gump's layer to get the coordinate its shape was painted at.
		int lx;
		int ly;
		if (!map_game_to_gump(gump, x, y, lx, ly)) {
			continue;
		}
		if (gump->has_point(lx, ly) && (pers || !gump->is_persistent())) {
			found = gump;
		}
	}
	return found;
}

/*
 *  Find gump containing a given object.
 */

Gump* Gump_manager::find_gump(const Game_object* obj) {
	// Get container object is in.
	const Game_object* owner = obj->get_owner();
	if (!owner) {
		// obj has no owner — check if obj itself is a container whose
		// gump is open (e.g. usecode intrinsics called with the
		// container object directly, as in Dynamic_container_gump).
		for (Gump_list* gmp = open_gumps; gmp; gmp = gmp->next) {
			if (gmp->gump->get_container() == obj) {
				return gmp->gump;
			}
		}
		Gump* dragged = gwin->get_dragging_gump();
		if (dragged && dragged->get_container() == obj) {
			return dragged;
		}
		return nullptr;
	}
	// Look for container's gump.
	for (Gump_list* gmp = open_gumps; gmp; gmp = gmp->next) {
		if (gmp->gump->get_container() == owner) {
			return gmp->gump;
		}
	}

	Gump* dragged = gwin->get_dragging_gump();
	if (dragged && dragged->get_container() == owner) {
		return dragged;
	}

	return nullptr;
}

/*
 *  Find gump with a given owner & shapenum.
 */

Gump* Gump_manager::find_gump(
		const Game_object* owner,
		int                shapenum    // May be c_any_shapenum
) {
	Gump_list* gmp;    // See if already open.
	for (gmp = open_gumps; gmp; gmp = gmp->next) {
		if (gmp->gump->get_owner() == owner && (shapenum == c_any_shapenum || gmp->gump->get_shapenum() == shapenum)) {
			return gmp->gump;
		}
	}

	Gump* dragged = gwin->get_dragging_gump();
	if (dragged && dragged->get_owner() == owner && (shapenum == c_any_shapenum || dragged->get_shapenum() == shapenum)) {
		return dragged;
	}

	return nullptr;
}

/*
 *  Add a gump to the end of a chain.
 */

void Gump_manager::add_gump(Gump* gump) {
	auto* g = new Gump_list(gump);

	set_kbd_focus(gump);
	if (!open_gumps) {
		open_gumps = g;    // First one.
	} else {
		Gump_list* last = open_gumps;
		while (last->next) {
			last = last->next;
		}
		last->next = g;
	}
	if (!gump->is_persistent()) {    // Count 'gump mode' gumps.
		// And pause the game, if we want it
		non_persistent_count++;
		if (!dont_pause_game) {
			gwin->get_tqueue()->pause(Game::get_ticks());
		}
	}
}

/*
 *  Close a gump and delete it
 */

bool Gump_manager::close_gump(Gump* gump) {
	const bool ret     = remove_gump(gump);
	Gump*      dragged = gwin->get_dragging_gump();
	if (dragged == gump) {
		gwin->stop_dragging();
	}
	delete gump;
	if (touchui != nullptr && non_persistent_count == 0) {
		touchui->showGameControls();
	}
	return ret;
}

/*
 *  Remove a gump from the chain
 */

bool Gump_manager::remove_gump(Gump* gump) {
	if (gump == kbd_focus) {
		set_kbd_focus(nullptr);
	}
	if (open_gumps) {
		if (open_gumps->gump == gump) {
			Gump_list* p = open_gumps->next;
			delete open_gumps;
			open_gumps = p;
		} else {
			Gump_list* p = open_gumps;    // Find prev. to this.
			while (p->next != nullptr && p->next->gump != gump) {
				p = p->next;
			}

			if (p->next) {
				Gump_list* g = p->next->next;
				delete p->next;
				p->next = g;
			} else {
				return true;
			}
		}
		if (!gump->is_persistent()) {    // Count 'gump mode' gumps.
			// And resume queue if last.
			// Gets messed up upon 'load'.
			if (non_persistent_count > 0) {
				non_persistent_count--;
			}
			if (!dont_pause_game) {
				gwin->get_tqueue()->resume(Game::get_ticks());
			}
		}
	}

	return false;
}

/*
 *  Show a gump.
 */

void Gump_manager::add_gump(
		Game_object* obj,         // Object gump represents.
		int          shapenum,    // Shape # in 'gumps.vga'.
		bool         actorgump    // If showing an actor's gump
) {
	bool paperdoll = false;

	// overide for paperdolls
	if (actorgump && (sman->can_use_paperdolls() && sman->are_paperdolls_enabled())) {
		paperdoll = true;
	}

	Gump* dragged = gwin->get_dragging_gump();

	// If we are dragging the same, just return
	if (dragged && dragged->get_owner() == obj && dragged->get_shapenum() == shapenum) {
		return;
	}

	static int cnt = 0;    // For staggering them.
	Gump_list* gmp;        // See if already open.
	for (gmp = open_gumps; gmp; gmp = gmp->next) {
		if (gmp->gump->get_owner() == obj && gmp->gump->get_shapenum() == shapenum) {
			break;
		}
	}

	if (gmp) {    // Found it?
		// Move it to end.
		Gump* gump = gmp->gump;
		if (gmp->next) {
			remove_gump(gump);
			add_gump(gump);
		} else {
			set_kbd_focus(gump);
		}
		gwin->paint();
		return;
	}

	int x = (1 + cnt) * gwin->get_width() / 10;
	int y = (1 + cnt) * gwin->get_height() / 10;

	const ShapeID s_id(shapenum, 0, paperdoll ? SF_PAPERDOL_VGA : SF_GUMPS_VGA);
	Shape_frame*  shape = s_id.get_shape();

	// Keep the initial (non-saved) position fully on-screen.
	{
		Image_window* iwin  = gwin->get_win();
		const float   f     = iwin->get_ui_scale_factor(Image_window::UiLayerGumps);
		const int     gleft = x - shape->get_xleft();
		const int     gtop  = y - shape->get_yabove();
		const int     gw    = shape->get_width();
		const int     gh    = shape->get_height();
		int           csx;
		int           csy;
		iwin->game_to_screen(gleft + gw / 2, gtop + gh / 2, false, csx, csy);
		const float dw    = static_cast<float>(gw) * f;
		const float dh    = static_cast<float>(gh) * f;
		const float dispw = static_cast<float>(iwin->get_display_width());
		const float disph = static_cast<float>(iwin->get_display_height());
		float       ncsx  = static_cast<float>(csx);
		float       ncsy  = static_cast<float>(csy);
		if (dw <= dispw) {
			ncsx = ncsx < dw / 2.0f ? dw / 2.0f : (ncsx > dispw - dw / 2.0f ? dispw - dw / 2.0f : ncsx);
		} else {
			ncsx = dispw / 2.0f;    // Wider than the display: centre it.
		}
		if (dh <= disph) {
			ncsy = ncsy < dh / 2.0f ? dh / 2.0f : (ncsy > disph - dh / 2.0f ? disph - dh / 2.0f : ncsy);
		} else {
			ncsy = disph / 2.0f;
		}
		if (static_cast<int>(ncsx) != csx || static_cast<int>(ncsy) != csy) {
			// Convert the clamped centre back to game coords and shift x,y by
			// that game-space delta so the scaled gump lands on-screen.
			int ngx;
			int ngy;
			int ogx;
			int ogy;
			iwin->screen_to_game(static_cast<int>(ncsx), static_cast<int>(ncsy), false, ngx, ngy);
			iwin->screen_to_game(csx, csy, false, ogx, ogy);
			x += ngx - ogx;
			y += ngy - ogy;
			cnt = 0;    // Restart the stagger since we wrapped back on-screen.
		}
	}

	Gump* new_gump = nullptr;
	if (obj) {
		Actor* npc = obj->as_actor();
		if (npc && paperdoll) {
			new_gump = new Paperdoll_gump(npc, x, y, npc->get_npc_num());
		} else if (npc && actorgump) {
			new_gump = new Actor_gump(npc, x, y, shapenum);
		} else if (shapenum == game->get_shape("gumps/statsdisplay")) {
			new_gump = Stats_gump::create(obj, x, y);
		} else if (shapenum == game->get_shape("gumps/spellbook")) {
			new_gump = new Spellbook_gump(static_cast<Spellbook_object*>(obj));
		} else if (shapenum == game->get_shape("gumps/jawbone")) {
			new_gump = new Jawbone_gump(static_cast<Jawbone_object*>(obj), x, y);
		} else if (shapenum == game->get_shape("gumps/spell_scroll")) {
			new_gump = new Spellscroll_gump(obj);
		}
		// If we have an object, we can force a container gump.
		if (!new_gump && obj->as_container()) {
			// Check if this gump shape has container_area in gump_info.txt
			if (Dynamic_container_gump::has_config(shapenum)) {
				new_gump = new Dynamic_container_gump(obj->as_container(), x, y, shapenum);
			} else {
				new_gump = new Container_gump(obj->as_container(), x, y, shapenum);
			}
		}
	} else if (
			Game::get_game_type() == SERPENT_ISLE && shapenum >= game->get_shape("gumps/cstats/1")
			&& shapenum <= game->get_shape("gumps/cstats/6")) {
		new_gump = new CombatStats_gump(x, y);
	}

	if (!new_gump) {
		// We failed; so bail out (we did nothing but waste time)
		CERR("Failed to create gump: " << obj << ", " << shapenum << ", " << actorgump);
		return;
	}

	// Paint new one last.
	add_gump(new_gump);

	// Center dynamic gumps — the cascading position and saved-position
	// restore can place these larger gumps offscreen.
	if (dynamic_cast<Dynamic_container_gump*>(new_gump)) {
		new_gump->set_pos();
	}

	// For Dynamic_container_gumps, fire the gump's shape usecode with
	// double_click so that on-open initialisation functions can run.
	if (dynamic_cast<Dynamic_container_gump*>(new_gump)) {
		auto*     ucm = gwin->get_usecode();
		const int fn  = ucm->get_shape_fun(shapenum);
		ucm->call_usecode(fn, obj, Usecode_machine::double_click);
	}
	if (touchui != nullptr && !gumps_dont_pause_game()) {
		touchui->hideGameControls();
	}
	if (++cnt == 8) {
		cnt = 0;
	}
	const int sfx = Audio::game_sfx(14);
	Audio::get_ptr()->play_sound_effect(sfx);    // The weird noise.
	gwin->paint();                               // Show everything.
}

/*
 *  End gump mode.
 */

void Gump_manager::close_all_gumps(bool pers) {
	bool removed = false;

	Gump_list* prev = nullptr;
	Gump_list* next = open_gumps;

	while (next) {    // Remove all gumps.
		Gump_list* gump = next;
		next            = gump->next;

		// Don't delete if persistant or modal.
		if ((!gump->gump->is_persistent() || pers) && !gump->gump->is_modal()) {
			if (!gump->gump->is_persistent()) {
				gwin->get_tqueue()->resume(Game::get_ticks());
			}
			if (prev) {
				prev->next = gump->next;
			} else {
				open_gumps = gump->next;
			}
			delete gump->gump;
			delete gump;
			removed = true;
		} else {
			prev = gump;
		}
	}
	non_persistent_count = 0;
	set_kbd_focus(nullptr);
	gwin->get_npc_prox()->wait(4);    // Delay "barking" for 4 secs.
	if (removed) {
		gwin->paint();
	}
	if (touchui != nullptr && !modal_gump_count && non_persistent_count == 0 && !gwin->is_in_exult_menu()) {
		touchui->showGameControls();
	}
}

/*
 *  Set the keyboard focus to a given gump.
 */

void Gump_manager::set_kbd_focus(Gump* gump    // May be nullptr.
) {
	if (gump && gump->can_handle_kbd()) {
		kbd_focus = gump;
	} else {
		kbd_focus = nullptr;
	}
}

/*
 *  Handle a double-click.
 */

bool Gump_manager::double_clicked(
		int x, int y,    // Coords in window.
		Game_object*& obj) {
	Gump* gump = find_gump(x, y);

	if (gump) {
		// If avatar cannot act, a double-click will only close gumps, and
		// nothing else.
		if (!gwin->main_actor_can_act()) {
			if (gwin->get_double_click_closes_gumps()) {
				gump->close();
				gwin->paint();
			}
			return true;
		}
		int gx = x;
		int gy = y;
		map_game_to_gump(gump, x, y, gx, gy);
		// Find object in gump.
		obj = gump->find_object(gx, gy);
		if (!obj) {    // Maybe it's a spell.
			Gump_button* btn = gump->on_button(gx, gy);
			if (btn) {
				btn->double_clicked(gx, gy);
			} else if (gwin->get_double_click_closes_gumps()) {
				gump->close();
				gwin->paint();
			}
		}
		return true;
	}

	return false;
}

/*
 *  Send kbd. event to gump that has focus.
 *  Output: true if handled here.
 */
bool Gump_manager::handle_kbd_event(void* ev) {
	return kbd_focus ? kbd_focus->handle_kbd_event(ev) : false;
}

/*
 *  Forward a mouse wheel event to the topmost gump under the cursor.
 *  Output: true if a gump handled the event.
 */
bool Gump_manager::handle_mouse_wheel(float y, float x, int mx, int my) {
	ignore_unused_variable_warning(x);
	Gump* gump = find_gump(mx, my);
	if (!gump) {
		return false;
	}
	int gx = mx;
	int gy = my;
	map_game_to_gump(gump, mx, my, gx, gy);
	if (y > 0) {
		gump->mousewheel_up(gx, gy);
	} else if (y < 0) {
		gump->mousewheel_down(gx, gy);
	}
	// Always consume the event when cursor is over a gump so the
	// game-world cheat-scroll never fires through an open gump.
	return true;
}

/*
 *  Update the gumps
 */
void Gump_manager::update_gumps() {
	for (Gump_list* gmp = open_gumps; gmp; gmp = gmp->next) {
		gmp->gump->update_gump();
	}
}

/*
 *  Paint the gumps
 */
void Gump_manager::paint(bool modal) {
	if (!open_gumps && !(modal && background)) {
		return;
	}
	render_gumps_to_layer(modal);
}

/*
 *  Verify user wants to quit.
 *
 *  Output: true to quit.
 */
bool Gump_manager::okay_to_quit(Paintable* paint) {
	// Prevent reentering this function
	static bool inthis = false;
	if (inthis) {
		return false;
	}
	inthis = true;
	if (Yesno_gump::ask(GumpStrings::Doyoureallywanttoquit_(), paint)) {
		quitting_time = QUIT_TIME_YES;
	}
	inthis = false;
	return quitting_time != QUIT_TIME_NO;
}

static Gump_Base::MouseButton SDL_MouseButton_to_Gump(Uint8 sdlbutton) {
	switch (sdlbutton) {
	case 1:
		return Gump_Base::MouseButton::Left;
	case 2:
		return Gump_Base::MouseButton::Middle;
	case 3:
		return Gump_Base::MouseButton::Right;
	}
	return Gump_Base::MouseButton::Unknown;
}

bool Gump_manager::handle_modal_gump_event(Modal_gump* gump, SDL_Event& event) {
	//  Game_window *gwin = Game_window::get_instance();
	// int scale_factor = gwin->get_fastmouse() ? 1
	//          : gwin->get_win()->get_scale();
	static bool rightclick;

	int           gx;
	int           gy;
	int           rgx;
	int           rgy;
	SDL_Keycode   keysym_unicode = 0;
	SDL_Keycode   chr            = 0;
	SDL_Renderer* renderer       = SDL_GetRenderer(gwin->get_win()->get_screen_window());

	switch (event.type) {
	case SDL_EVENT_FINGER_DOWN: {
		SDL_ConvertEventToRenderCoordinates(renderer, &event);
		if ((!Mouse::use_touch_input) && (event.tfinger.fingerID != 0)) {
			Mouse::use_touch_input = true;
			gwin->set_painted();
		}
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		SDL_ConvertEventToRenderCoordinates(renderer, &event);
		gwin->get_win()->screen_to_game(event.button.x, event.button.y, gwin->get_fastmouse(), gx, gy);
		map_game_to_gump(gump, gx, gy, rgx, rgy);

#ifdef DEBUG
		cout << "(x,y) rel. to gump is (" << (gx - gump->get_x()) << ", " << (gy - gump->get_y()) << ")" << endl;
#endif
		if (g_shortcutBar && g_shortcutBar->handle_event(&event)) {
			break;
		}
		if (event.button.button == 1) {
			gump->mouse_down(rgx, rgy, SDL_MouseButton_to_Gump(event.button.button));
			// If this began a modal drag, keep the drag anchor in the same
			// coordinate space as mouse motion (raw game coords) to avoid the
			// initial one-frame jump.
			gump->sync_drag_anchor(gx, gy);
		} else if (event.button.button == 2) {
			if (!gump->mouse_down(rgx, rgy, SDL_MouseButton_to_Gump(event.button.button)) && gwin->get_mouse3rd()) {
				gump->key_down(SDLK_RETURN, SDLK_RETURN);
			}
		} else if (event.button.button == 3) {
			rightclick = true;
			gump->mouse_down(rgx, rgy, SDL_MouseButton_to_Gump(event.button.button));
		} else {
			gump->mouse_down(rgx, rgy, SDL_MouseButton_to_Gump(event.button.button));
		}
		break;
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		SDL_ConvertEventToRenderCoordinates(renderer, &event);
		gwin->get_win()->screen_to_game(event.button.x, event.button.y, gwin->get_fastmouse(), gx, gy);
		map_game_to_gump(gump, gx, gy, rgx, rgy);
		if (g_shortcutBar && g_shortcutBar->handle_event(&event)) {
			break;
		}
		const bool raw_drag_coords = (event.button.button == 1 && gump->is_dragging());
		if (event.button.button != 3) {
			if (raw_drag_coords) {
				gump->mouse_up(gx, gy, SDL_MouseButton_to_Gump(event.button.button));
			} else {
				gump->mouse_up(rgx, rgy, SDL_MouseButton_to_Gump(event.button.button));
			}
		} else if (rightclick) {
			rightclick = false;
			if (!gump->mouse_up(rgx, rgy, SDL_MouseButton_to_Gump(event.button.button)) && gumpman->can_right_click_close()) {
				return false;
			}
		}
		break;
	}
	case SDL_EVENT_FINGER_MOTION: {
		SDL_ConvertEventToRenderCoordinates(renderer, &event);
		gwin->get_win()->screen_to_game(event.button.x, event.button.y, gwin->get_fastmouse(), gx, gy);
		map_game_to_gump(gump, gx, gy, rgx, rgy);
		static int   numFingers = 0;
		SDL_Finger** fingers    = SDL_GetTouchFingers(event.tfinger.touchID, &numFingers);
		if (fingers) {
			SDL_free(fingers);
		}
		if (numFingers > 1) {
			if (event.tfinger.dy < 0) {
				if (!gump->mouse_down(rgx, rgy, SDL_MouseButton_to_Gump(event.button.button))) {
					int wx;
					int wy;
					map_game_to_gump(gump, Mouse::mouse()->get_mousex(), Mouse::mouse()->get_mousey(), wx, wy);
					gump->mousewheel_up(wx, wy);
				}
			} else if (event.tfinger.dy > 0) {
				if (!gump->mouse_down(rgx, rgy, SDL_MouseButton_to_Gump(event.button.button))) {
					int wx;
					int wy;
					map_game_to_gump(gump, Mouse::mouse()->get_mousex(), Mouse::mouse()->get_mousey(), wx, wy);
					gump->mousewheel_down(wx, wy);
				}
			}
		}
		break;
	}
	// Mousewheel scrolling with SDL2.
	case SDL_EVENT_MOUSE_WHEEL: {
		int wx;
		int wy;
		map_game_to_gump(gump, Mouse::mouse()->get_mousex(), Mouse::mouse()->get_mousey(), wx, wy);
		if (event.wheel.y > 0) {
			gump->mousewheel_up(wx, wy);
		} else if (event.wheel.y < 0) {
			gump->mousewheel_down(wx, wy);
		}
		break;
	}
	case SDL_EVENT_MOUSE_MOTION:
		if (Mouse::use_touch_input && event.motion.which != EXSDL_TOUCH_MOUSEID) {
			Mouse::use_touch_input = false;
		}
		SDL_ConvertEventToRenderCoordinates(renderer, &event);
		gwin->get_win()->screen_to_game(event.motion.x, event.motion.y, gwin->get_fastmouse(), gx, gy);
		map_game_to_gump(gump, gx, gy, rgx, rgy);

		Mouse::mouse()->move(gx, gy);
		Mouse::mouse_update = true;
		// Dragging with left button?
		if (event.motion.state & SDL_BUTTON_LMASK) {
			if (gump->is_dragging()) {
				gump->mouse_drag(gx, gy);
			} else {
				gump->mouse_drag(rgx, rgy);
			}
		}
		break;
	case SDL_EVENT_QUIT:
		if (okay_to_quit()) {
			return false;
		}
		break;
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_TEXT_INPUT:
		Translate_keyboard(event, chr, keysym_unicode, true);
		{
			if ((chr == SDLK_S) && (event.key.mod & SDL_KMOD_ALT) && (event.key.mod & SDL_KMOD_CTRL)) {
				make_screenshot(true);
				return true;
			}
			// Alt-x for quit
			if ((chr == SDLK_X) && ((event.key.mod & SDL_KMOD_ALT) || event.key.mod & SDL_KMOD_GUI)) {
				if (okay_to_quit()) {
					return false;
				}
			}

			bool handled = gump->key_down(chr, keysym_unicode);
			// we'll allow the gump to handle escape first
			// before closing the gump
			if (!handled) {
				if (chr == SDLK_ESCAPE) {
					return false;
				}
			}
		}
		break;
	default:
		if (event.type == TouchUI::eventType) {
			if (event.user.code == TouchUI::EVENT_CODE_TEXT_INPUT) {
				if (event.user.data1 != nullptr) {
					const char* text = static_cast<const char*>(event.user.data1);
					if (text) {
						gump->text_input(text);
					}
					free(event.user.data1);
				}
			}
		}
		break;
	}
	return true;
}

/*
 *  Handle a modal gump, like the range slider or the save box, until
 *  the gump self-destructs.
 *
 *  Output: false if user hit ESC.
 */

bool Gump_manager::do_modal_gump(
		Modal_gump*         gump,     // What the user interacts with.
		Mouse::Mouse_shapes shape,    // Mouse shape to use.
		Paintable*          paint     // Paint this over everything else.
) {
	modal_gump_count++;

	//  Game_window *gwin = Game_window::get_instance();

	// maybe make this selective? it's nice for menus, but annoying for sliders
	//  gwin->end_gump_mode();

	// Pause the game
	gwin->get_tqueue()->pause(SDL_GetTicks());

	const Mouse::Mouse_shapes saveshape = Mouse::mouse()->get_shape();
	if (shape != Mouse::dontchange) {
		Mouse::mouse()->set_shape(shape);
	}
	bool escaped = false;
	background   = dynamic_cast<BackgroundPaintable*>(paint);
	if (background) {
		paint = nullptr;
	}
	add_gump(gump);
	gump->run();
	gwin->paint();    // Show everything now.
	if (paint) {
		paint->paint();
	}
	Mouse::mouse()->show();
	gwin->show();
	if (touchui != nullptr) {
		touchui->hideGameControls();
	}
	do {
		Delay();                   // Wait a fraction of a second.
		Mouse::mouse()->hide();    // Turn off mouse.
		Mouse::mouse_update = false;
		SDL_Event event;
		bool      got_event = false;
		while (!escaped && !gump->is_done() && SDL_PollEvent(&event)) {
			escaped   = !handle_modal_gump_event(gump, event);
			got_event = true;
		}

		// A layer-backed gump is redrawn from scratch each frame, so any event
		// that might have changed it must trigger a full repaint. The gump's
		// own paint() calls (made while handling the event) otherwise land
		// directly in the game window as an unscaled duplicate while the scaled
		// overlay layer stays stale (looking like the click "did nothing").
		const bool ran = gump->run();
		if (ran || gwin->is_dirty() || got_event) {
			gwin->paint();    // Paint each cycle.
			if (paint) {
				paint->paint();
			}
		}
		gwin->rotatecolours();
		Mouse::mouse()->show();       // Re-display mouse.
		if (!gwin->show() &&          // Blit to screen if necessary.
			Mouse::mouse_update) {    // If not, did mouse change?
			Mouse::mouse()->blit_dirty();
		}
	} while (!gump->is_done() && !escaped && quitting_time == QUIT_TIME_NO);
	Mouse::mouse()->hide();
	remove_gump(gump);
	Mouse::mouse()->set_shape(saveshape);
	// Leave mouse off.
	gwin->paint();
	gwin->show(true);
	// Resume the game
	gwin->get_tqueue()->resume(SDL_GetTicks());

	modal_gump_count--;
	if (touchui != nullptr) {
		if (!gwin->is_in_exult_menu()) {
			touchui->showButtonControls();
		}
		if ((non_persistent_count == 0 || gumpman->gumps_dont_pause_game()) && !modal_gump_count && !gwin->is_in_exult_menu()
			&& !ucmachine->get_num_faces_on_screen()) {
			touchui->showGameControls();
		}
	}
	background = nullptr;
	return !escaped;
}

/*
 *  Prompt for a numeric value using a slider.
 *
 *  Output: Value,
 *
 *  Set escaped to a pointer to bool to allow the user to escape the prompt.
 *  If so, the bool will be set to true if the user escaped, false otherwise.
 */

int Gump_manager::prompt_for_number(
		int minval, int maxval,    // Range.
		int        step,
		int        defval,    // Default to start with.
		Paintable* paint,     // Should be the conversation.
		bool*      escaped    // If non-null, allow user to escape and will be set indicating if user escaped
) {
	auto*      slider = new Slider_gump(minval, maxval, step, defval, escaped != nullptr);
	const bool ok     = do_modal_gump(slider, Mouse::hand, paint);
	if (escaped) {
		*escaped = !ok;
	}
	const int ret = slider->get_val();
	delete slider;
	return ret;
}

/*
 *  Show a number.
 */

void Gump_manager::paint_num(
		int                   num,
		int                   x,    // Coord. of right edge of #.
		int                   y,    // Coord. of top of #.
		std::shared_ptr<Font> font) {
	//  Shape_manager *sman = Shape_manager::get_instance();
	char buf[20];
	snprintf(buf, sizeof(buf), "%d", num);
	if (font == nullptr) {
		font = sman->get_font(2);
	}
	sman->paint_text(font, buf, x - font->get_text_width(buf), y);
}

/*
 *
 */
void Gump_manager::set_gumps_dont_pause_game(bool p) {
	// Don't do anything if they are the same
	if (dont_pause_game == p) {
		return;
	}

	dont_pause_game = p;

	// If pausing enabled, we need to go through and pause each gump
	if (!dont_pause_game) {
		for (Gump_list* gump = open_gumps; gump; gump = gump->next) {
			if (!gump->gump->is_persistent()) {
				gwin->get_tqueue()->pause(Game::get_ticks());
			}
		}
	}
	// Otherwise we need to go through and resume each gump :-)
	else {
		for (Gump_list* gump = open_gumps; gump; gump = gump->next) {
			if (!gump->gump->is_persistent()) {
				gwin->get_tqueue()->resume(Game::get_ticks());
			}
		}
	}
}
