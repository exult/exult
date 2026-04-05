/*
Copyright (C) 2025 The Exult Team

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

#include "Dynamic_button.h"

#include "Audio.h"
#include "Gump.h"
#include "contain.h"
#include "gamewin.h"
#include "gumpinf.h"
#include "ucmachine.h"

Dynamic_button::Dynamic_button(Gump* par, const Dynamic_button_def& def)
		: Gump_button(par, def.button_shape, def.pos_x, def.pos_y, SF_GUMPS_VGA), default_frame(def.default_frame),
		  clicked_frame(def.clicked_frame), sound_fx(def.sound_fx), usecode_fn(def.usecode_fn), action_type(def.action_type),
		  visibility_flag(def.visibility_flag), usecode_param(def.usecode_param), toggled(false) {
	set_frame(default_frame);
}

Gump_widget* Dynamic_button::clone(Gump* par) {
	Dynamic_button_def def;
	def.button_shape    = get_shapenum();
	def.pos_x           = x;
	def.pos_y           = y;
	def.default_frame   = default_frame;
	def.clicked_frame   = clicked_frame;
	def.sound_fx        = sound_fx;
	def.usecode_fn      = usecode_fn;
	def.action_type     = action_type;
	def.visibility_flag = visibility_flag;
	def.usecode_param   = usecode_param;
	return new Dynamic_button(par, def);
}

bool Dynamic_button::is_visible() const {
	if (visibility_flag < 0) {
		return true;    // Always visible
	}
	auto* ucm = Game_window::get_instance()->get_usecode();
	return ucm && ucm->get_global_flag_bool(visibility_flag);
}

void Dynamic_button::paint() {
	if (!is_visible()) {
		return;
	}
	// Determine which frame to draw.
	const int paint_frame = (action_type == 2 && toggled) ? clicked_frame : is_pushed() ? clicked_frame : default_frame;

	int sx = 0, sy = 0;
	local_to_screen(sx, sy);

	// Save/restore frame so the resting frame (default_frame) stays current
	// outside of painting.  This prevents on_widget() from checking
	// hit-tests against the clicked_frame whose shape origin may differ.
	// Compensate for origin differences between default and paint frames.
	// If the clicked frame has a different xleft/yabove than the default
	// frame, adjust the paint position so both frames render at the same
	// screen location.
	const int prev_frame = get_framenum();
	if (paint_frame != default_frame) {
		set_frame(default_frame);
		Shape_frame* f0 = get_shape();
		set_frame(paint_frame);
		Shape_frame* f1 = get_shape();
		if (f0 && f1) {
			sx += f1->get_xleft() - f0->get_xleft();
			sy += f1->get_yabove() - f0->get_yabove();
		}
	} else {
		set_frame(paint_frame);
	}
	paint_shape(sx, sy);
	set_frame(prev_frame);
}

bool Dynamic_button::activate(MouseButton button) {
	if (button != MouseButton::Left) {
		return false;
	}
	if (!is_visible()) {
		return false;
	}

	// Play sound effect
	if (sound_fx >= 0) {
		Audio::get_ptr()->play_sound_effect(Audio::game_sfx(sound_fx));
	}

	// Toggle state for toggle buttons
	if (action_type == 2) {
		toggled = !toggled;
	}

	// Call usecode function
	if (usecode_fn >= 0) {
		auto* gwin = Game_window::get_instance();
		auto* ucm  = gwin->get_usecode();
		if (ucm) {
			// Always fire as double_click — consistent with
			// Dynamic_slider which also uses double_click.
			// Use the gump's container as the item context
			auto*        par  = dynamic_cast<Gump*>(parent);
			Game_object* item = par ? par->get_container() : nullptr;
			// Resolve shape number to function ID the same way world
			// double-clicks do (Game_object::get_usecode): use get_shape_fun()
			// which consults the usecode symbol table when available (mods
			// compiled with #autonumber) and falls back to the old-style
			// formula (0xC00 + shape_num) otherwise.
			const int fn_id = ucm->get_shape_fun(usecode_fn);
			if (fn_id >= 0) {
				ucm->call_usecode(fn_id, item, Usecode_machine::double_click);
			}
			// After usecode returns, `this` may have been
			// destroyed (e.g. close_all_gumps via show_npc_face).
			// Do NOT access member variables — just return.
			return true;
		}
	}

	// Post-action (no usecode ran)
	if (action_type == 1) {
		parent->close();
	}

	return true;
}
