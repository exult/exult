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

#ifndef DYNAMIC_SLIDER_H
#define DYNAMIC_SLIDER_H

#include "Gump_widget.h"
#include "shapeid.h"

#include <functional>
#include <memory>
#include <utility>

class Gump_button;
struct Dynamic_slider_def;

/*
 *  A data-driven slider on a gump, configured via gump_info.txt.
 *  Supports both vertical and horizontal orientations.
 *  Value can be read/set via usecode intrinsics.
 */
class Dynamic_slider : public Gump_widget {
	int  field_id;    // Unique ID for usecode to address this slider
	bool vertical;    // true = vertical, false = horizontal
	int  min_val, max_val;
	int  step_val;
	int  val;           // Current value
	int  usecode_fn;    // Usecode function to call on change (-1 = none)

	// Set by C++ hosts only (see Dynamic_slider_def).
	ShapeFile shapefile;
	int       track_pad;
	int       thumb_inset;
	bool      top_left;
	bool      hide_idle_thumb;

	// C++ change hook; replaces the usecode callback when set.
	std::function<void(int)> on_change;

	// Geometry (relative to widget position)
	int thumb_pos;        // Current thumb offset along track (pixels)
	int track_extent;     // Size of sliding region in pixels
	int thumb_travel;     // Usable travel = track_extent - thumb_height
	int prev_drag;        // Previous mouse coord during drag (INT_MIN = not dragging)
	int arrow_clicked;    // -1=dec pressed, 0=none, 1=inc pressed

	ShapeID                      thumb;
	std::unique_ptr<Gump_button> dec_btn;    // Decrement (up/left)
	std::unique_ptr<Gump_button> inc_btn;    // Increment (down/right)

	// Button positions (relative to widget)
	int dec_bx, dec_by;
	int inc_bx, inc_by;
	int track_start;    // Pixel offset where track begins (after dec button)

	uint64_t next_auto_push_time = 0;

	void compute_layout(const Dynamic_slider_def& def);
	void update_thumb_from_val();
	int  val_from_thumb() const;
	bool call_usecode_callback();
	int  hit_arrow(int mx, int my) const;    // -1=dec, 1=inc, 0=none
	bool thumb_shown() const;
	int  cross_extent() const;                    // Size across the travel axis
	void thumb_origin(int& tx, int& ty) const;    // Thumb hotspot, slider-local

public:
	// Part of the slider under a point (see hit_part()).
	enum class Part {
		none,
		dec,
		inc,
		thumb,
		before,    // Track before the thumb
		after      // Track after the thumb
	};

	Dynamic_slider(Gump_Base* par, const Dynamic_slider_def& def);

	Gump_widget* clone(Gump* par) override;

	void paint() override;
	bool run() override;

	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;
	bool mouse_drag(int mx, int my) override;
	bool key_down(SDL_Keycode chr, SDL_Keycode unicode) override;
	bool mousewheel_up(int mx, int my) override;
	bool mousewheel_down(int mx, int my) override;

	TileRect get_rect() const override;

	Gump_widget* Input_first() override;

	int get_field_id() const {
		return field_id;
	}

	int getselection() const override {
		return val;
	}

	void setselection(int newval) override;

	void move_value(int dir);
	void set_val(int newval, bool recalc_thumb = true);

	// Change the range; clamps the value and moves the thumb.
	void set_range(int newmin, int newmax);

	int get_min() const {
		return min_val;
	}

	int get_max() const {
		return max_val;
	}

	// Call this instead of the usecode function when the value changes.
	void set_on_change(std::function<void(int)> fn) {
		on_change = std::move(fn);
	}

	Part hit_part(int mx, int my) const;

	bool is_dragging() const {
		return prev_drag != INT32_MIN;
	}
};

#endif
