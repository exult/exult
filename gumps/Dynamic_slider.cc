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

#include "Dynamic_slider.h"

#include "Gump.h"
#include "Gump_button.h"
#include "contain.h"
#include "game.h"
#include "gamewin.h"
#include "gumpinf.h"
#include "ucmachine.h"

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include <climits>
#include <iostream>

/*
 *  Arrow button for the dynamic slider:
 */
class DynSlider_button : public Gump_button {
	bool is_dec;

public:
	DynSlider_button(Dynamic_slider* par, int px, int py, int shnum, bool dec)
			: Gump_button(par, shnum, px, py, SF_GUMPS_VGA), is_dec(dec) {}

	bool activate(MouseButton button) override {
		if (button != MouseButton::Left) {
			return false;
		}
		auto* slider = static_cast<Dynamic_slider*>(parent);
		if (is_dec) {
			slider->move_value(-1);
		} else {
			slider->move_value(1);
		}
		return true;
	}
};

Dynamic_slider::Dynamic_slider(Gump* par, const Dynamic_slider_def& def)
		: Gump_widget(par, -1, def.pos_x, def.pos_y, 0, SF_GUMPS_VGA), field_id(def.field_id), vertical(def.orientation == 0),
		  min_val(def.min_val), max_val(def.max_val), step_val(def.step > 0 ? def.step : 1), val(def.default_val),
		  usecode_fn(def.usecode_fn), thumb_pos(0), track_extent(def.extent >= 0 ? def.extent : 60), thumb_travel(0),
		  prev_drag(INT32_MIN), arrow_clicked(0), thumb(def.thumb_shape, 0, SF_GUMPS_VGA) {
	if (val < min_val) {
		val = min_val;
	} else if (val > max_val) {
		val = max_val;
	}

	compute_layout(def);

	// Compute usable travel distance: subtract the thumb's size along
	// the travel axis so it never overlaps the inc button at the end.
	Shape_frame* tf = thumb.get_shape();
	int          th = tf ? (vertical ? tf->get_height() : tf->get_width()) : 0;
	thumb_travel    = track_extent - th;
	if (thumb_travel < 0) {
		thumb_travel = 0;
	}

	update_thumb_from_val();
}

void Dynamic_slider::compute_layout(const Dynamic_slider_def& def) {
	// Get shape frames for sizing
	ShapeID dec_sid(def.dec_shape, 0, SF_GUMPS_VGA);
	auto*   dec_frame = dec_sid.get_shape();

	if (vertical) {
		// Vertical layout: dec (up) at top, track below it, inc (down) at bottom
		dec_bx      = 0;
		dec_by      = 0;
		int dec_h   = dec_frame ? dec_frame->get_height() : 8;
		track_start = dec_h;
		inc_bx      = 0;
		inc_by      = dec_h + track_extent;
	} else {
		// Horizontal: dec (left) at left, track after, inc (right) at right
		dec_bx      = 0;
		dec_by      = 0;
		int dec_w   = dec_frame ? dec_frame->get_width() : 8;
		track_start = dec_w;
		inc_bx      = dec_w + track_extent;
		inc_by      = 0;
	}

	// Adjust button positions: Gump_button coordinates are relative to parent widget
	// But for a widget that is itself inside a Gump, we need the buttons relative to us
	dec_btn = std::make_unique<DynSlider_button>(this, dec_bx, dec_by, def.dec_shape, true);
	inc_btn = std::make_unique<DynSlider_button>(this, inc_bx, inc_by, def.inc_shape, false);

	if (parent && (parent->get_debug_flags() & GUMP_DEBUG_CONSOLE)) {
		auto check_frames = [](Gump_button* btn, const char* name) {
			if (!btn) {
				return;
			}
			Shape_frame* f0 = btn->get_shape();
			btn->set_frame(1);
			Shape_frame* f1 = btn->get_shape();
			btn->set_frame(0);
			std::cerr << "[DynSlider] " << name << " shape=" << btn->get_shapenum() << " frame0=" << (f0 ? "ok" : "null")
					  << " frame1=" << (f1 ? "ok" : "null") << std::endl;
		};
		check_frames(dec_btn.get(), "dec");
		check_frames(inc_btn.get(), "inc");
	}
}

/*
 *  Test whether screen coordinates (mx, my) hit a slider arrow button.
 *  Returns -1 for dec, 1 for inc, 0 for neither.
 *  Uses slider-local coordinates so we only go through one parent level.
 */
int Dynamic_slider::hit_arrow(int mx, int my) const {
	int lx = mx, ly = my;
	screen_to_local(lx, ly);
	if (dec_btn) {
		Shape_frame* sf = dec_btn->get_shape();
		if (sf && sf->has_point(lx - dec_bx, ly - dec_by)) {
			return -1;
		}
	}
	if (inc_btn) {
		Shape_frame* sf = inc_btn->get_shape();
		if (sf && sf->has_point(lx - inc_bx, ly - inc_by)) {
			return 1;
		}
	}
	return 0;
}

void Dynamic_slider::update_thumb_from_val() {
	if (max_val <= min_val) {
		thumb_pos = 0;
		return;
	}
	thumb_pos = (val - min_val) * thumb_travel / (max_val - min_val);
	if (thumb_pos < 0) {
		thumb_pos = 0;
	} else if (thumb_pos > thumb_travel) {
		thumb_pos = thumb_travel;
	}
}

int Dynamic_slider::val_from_thumb() const {
	if (thumb_travel <= 0 || max_val <= min_val) {
		return min_val;
	}
	int delta = thumb_pos * (max_val - min_val) / thumb_travel;
	// Round to nearest step
	delta -= delta % step_val;
	return min_val + delta;
}

bool Dynamic_slider::call_usecode_callback() {
	if (usecode_fn < 0) {
		return false;
	}
	auto* gump = dynamic_cast<Gump*>(parent);
	if (!gump) {
		return false;
	}
	auto* cont = gump->get_container();
	if (!cont) {
		return false;
	}
	auto* ucm = Game_window::get_instance()->get_usecode();
	if (!ucm) {
		return false;
	}
	const int fn_id = ucm->get_shape_fun(usecode_fn);
	if (fn_id < 0) {
		return false;
	}
	ucm->call_usecode(fn_id, static_cast<Game_object*>(cont), Usecode_machine::double_click);
	// After usecode returns, `this` may have been destroyed
	// (e.g. close_all_gumps via show_npc_face).
	// Caller MUST NOT access member variables after a true return.
	return true;
}

Gump_widget* Dynamic_slider::clone(Gump* par) {
	Dynamic_slider_def def;
	def.field_id    = field_id;
	def.pos_x       = x;
	def.pos_y       = y;
	def.dec_shape   = dec_btn ? dec_btn->get_shapenum() : 0;
	def.inc_shape   = inc_btn ? inc_btn->get_shapenum() : 0;
	def.thumb_shape = thumb.get_shapenum();
	def.min_val     = min_val;
	def.max_val     = max_val;
	def.step        = step_val;
	def.default_val = val;
	def.extent      = track_extent;
	def.usecode_fn  = usecode_fn;
	def.orientation = vertical ? 0 : 1;
	return new Dynamic_slider(par, def);
}

void Dynamic_slider::set_val(int newval, bool recalc_thumb) {
	if (newval < min_val) {
		newval = min_val;
	}
	if (newval > max_val) {
		newval = max_val;
	}
	val = newval;
	if (recalc_thumb) {
		update_thumb_from_val();
	}
}

void Dynamic_slider::setselection(int newval) {
	set_val(newval);
}

void Dynamic_slider::move_value(int dir) {
	set_val(val + dir * step_val);
	gwin->add_dirty(get_rect());
	if (call_usecode_callback()) {
		return;    // `this` may be destroyed — do not touch members.
	}
}

void Dynamic_slider::paint() {
	if (parent && (parent->get_debug_flags() & GUMP_DEBUG_CONSOLE)) {
		int dbgx = x, dbgy = y;
		parent->local_to_screen(dbgx, dbgy);
		std::cerr << "[DynSlider] field_id=" << field_id << " local=(" << x << "," << y << ")"
				  << " screen=(" << dbgx << "," << dbgy << ")"
				  << " vertical=" << vertical << std::endl
				  << std::flush;
	}
	// Paint arrow buttons (frame 1 when pressed, frame 0 otherwise).
	// Safety: only use frame 1 if it actually exists in the shape;
	// otherwise keep frame 0 so the button doesn't vanish.
	if (dec_btn) {
		int sx = dec_bx, sy = dec_by;
		local_to_screen(sx, sy);
		if (arrow_clicked == -1) {
			dec_btn->set_frame(1);
			if (!dec_btn->get_shape()) {
				dec_btn->set_frame(0);
			}
		}
		dec_btn->paint_shape(sx, sy);
		dec_btn->set_frame(0);
	}
	if (inc_btn) {
		int sx = inc_bx, sy = inc_by;
		local_to_screen(sx, sy);
		if (arrow_clicked == 1) {
			inc_btn->set_frame(1);
			if (!inc_btn->get_shape()) {
				inc_btn->set_frame(0);
			}
		}
		inc_btn->paint_shape(sx, sy);
		inc_btn->set_frame(0);
	}

	// Paint thumb
	int tx, ty;
	if (vertical) {
		tx = x;
		ty = y + track_start + thumb_pos;
	} else {
		tx = x + track_start + thumb_pos;
		ty = y;
	}
	if (parent) {
		parent->local_to_screen(tx, ty);
	}
	if (thumb.get_shape()) {
		thumb.paint_shape(tx, ty);
	}
}

bool Dynamic_slider::run() {
	if (arrow_clicked != 0) {
		if (next_auto_push_time == 0) {
			next_auto_push_time = SDL_GetTicks() + 500;
		} else if (SDL_GetTicks() >= next_auto_push_time) {
			next_auto_push_time += 50;
			move_value(arrow_clicked);
			return true;
		}
	} else {
		next_auto_push_time = 0;
	}
	return false;
}

TileRect Dynamic_slider::get_rect() const {
	TileRect r;
	if (vertical) {
		int inc_h = 8;
		if (inc_btn) {
			auto* sf = inc_btn->get_shape();
			if (sf) {
				inc_h = sf->get_height();
			}
		}
		int   w  = 0;
		auto* tf = thumb.get_shape();
		if (tf) {
			w = tf->get_width();
		}
		// Fall back to arrow widths when no thumb (e.g. spinner).
		if (w <= 0) {
			if (dec_btn) {
				auto* sf = dec_btn->get_shape();
				if (sf) {
					w = sf->get_width();
				}
			}
			if (w <= 0) {
				w = 16;
			}
		}
		r = TileRect(0, 0, w, track_start + track_extent + inc_h);
	} else {
		int inc_w = 8;
		if (inc_btn) {
			auto* sf = inc_btn->get_shape();
			if (sf) {
				inc_w = sf->get_width();
			}
		}
		int   h  = 0;
		auto* tf = thumb.get_shape();
		if (tf) {
			h = tf->get_height();
		}
		// Fall back to arrow heights when no thumb (e.g. spinner).
		if (h <= 0) {
			if (dec_btn) {
				auto* sf = dec_btn->get_shape();
				if (sf) {
					h = sf->get_height();
				}
			}
			if (h <= 0) {
				h = 16;
			}
		}
		r = TileRect(0, 0, track_start + track_extent + inc_w, h);
	}
	local_to_screen(r.x, r.y);
	return r;
}

bool Dynamic_slider::mouse_down(int mx, int my, MouseButton button) {
	if (button != MouseButton::Left) {
		return Gump_widget::mouse_down(mx, my, button);
	}

	// Check arrow buttons using slider-local coordinates
	int arrow = hit_arrow(mx, my);
	if (parent && (parent->get_debug_flags() & GUMP_DEBUG_CONSOLE)) {
		int lx = mx, ly = my;
		screen_to_local(lx, ly);
		std::cerr << "[DynSlider::mouse_down] screen=(" << mx << "," << my << ") local=(" << lx << "," << ly
				  << ") hit_arrow=" << arrow << std::endl;
	}
	if (arrow != 0) {
		arrow_clicked = arrow;
		move_value(arrow);
		return true;
	}

	// Check if on thumb
	int lx = mx, ly = my;
	screen_to_local(lx, ly);

	Shape_frame* tf = thumb.get_shape();
	if (tf) {
		int tx, ty;
		if (vertical) {
			tx = 0;
			ty = track_start + thumb_pos;
		} else {
			tx = track_start + thumb_pos;
			ty = 0;
		}
		if (tf->has_point(lx - tx, ly - ty)) {
			prev_drag = vertical ? my : mx;
			return true;
		}
	}

	// Check if click is on the track — jump thumb to position
	if (vertical) {
		int   track_w = 16;
		auto* tw      = thumb.get_shape();
		if (tw) {
			track_w = tw->get_width();
		}
		if (lx >= 0 && lx < track_w && ly >= track_start && ly <= track_start + track_extent) {
			thumb_pos = ly - track_start;
			if (thumb_pos > thumb_travel) {
				thumb_pos = thumb_travel;
			}
			val = val_from_thumb();
			gwin->add_dirty(get_rect());
			call_usecode_callback();
			// `this` may be destroyed — just return.
			return true;
		}
	} else {
		int   track_h = 16;
		auto* th      = thumb.get_shape();
		if (th) {
			track_h = th->get_height();
		}
		if (ly >= 0 && ly < track_h && lx >= track_start && lx <= track_start + track_extent) {
			thumb_pos = lx - track_start;
			if (thumb_pos > thumb_travel) {
				thumb_pos = thumb_travel;
			}
			val = val_from_thumb();
			gwin->add_dirty(get_rect());
			call_usecode_callback();
			// `this` may be destroyed — just return.
			return true;
		}
	}

	return Gump_widget::mouse_down(mx, my, button);
}

bool Dynamic_slider::mouse_up(int mx, int my, MouseButton button) {
	ignore_unused_variable_warning(mx, my);
	if (button != MouseButton::Left) {
		return Gump_widget::mouse_up(mx, my, button);
	}

	if (arrow_clicked != 0) {
		arrow_clicked       = 0;
		next_auto_push_time = 0;
		gwin->add_dirty(get_rect());
		return true;
	}

	if (is_dragging()) {
		prev_drag = INT32_MIN;
		set_val(val);    // Snap thumb to correct position
		gwin->add_dirty(get_rect());
		return true;
	}

	return Gump_widget::mouse_up(mx, my, button);
}

bool Dynamic_slider::mouse_drag(int mx, int my) {
	if (!is_dragging()) {
		return Gump_widget::mouse_drag(mx, my);
	}

	int cur   = vertical ? my : mx;
	int delta = cur - prev_drag;
	prev_drag = cur;

	thumb_pos += delta;
	if (thumb_pos < 0) {
		thumb_pos = 0;
	} else if (thumb_pos > thumb_travel) {
		thumb_pos = thumb_travel;
	}

	int newval = val_from_thumb();
	if (newval != val) {
		val = newval;
		gwin->add_dirty(get_rect());
		if (call_usecode_callback()) {
			return true;    // `this` may be destroyed.
		}
		return true;
	}

	gwin->add_dirty(get_rect());
	return true;
}

bool Dynamic_slider::key_down(SDL_Keycode chr, SDL_Keycode unicode) {
	ignore_unused_variable_warning(unicode);
	if (vertical) {
		switch (chr) {
		case SDLK_UP:
			move_value(-1);
			return true;
		case SDLK_DOWN:
			move_value(1);
			return true;
		default:
			break;
		}
	} else {
		switch (chr) {
		case SDLK_LEFT:
			move_value(-1);
			return true;
		case SDLK_RIGHT:
			move_value(1);
			return true;
		default:
			break;
		}
	}
	return false;
}

bool Dynamic_slider::mousewheel_up(int mx, int my) {
	ignore_unused_variable_warning(mx, my);
	move_value(-1);
	return true;
}

bool Dynamic_slider::mousewheel_down(int mx, int my) {
	ignore_unused_variable_warning(mx, my);
	move_value(1);
	return true;
}

Gump_widget* Dynamic_slider::Input_first() {
	if (arrow_clicked != 0 || is_dragging()) {
		return this;
	}
	return nullptr;
}
