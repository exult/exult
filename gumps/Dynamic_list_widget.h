/*
Copyright (C) 2026 The Exult Team

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

#ifndef DYNAMIC_LIST_WIDGET_H
#define DYNAMIC_LIST_WIDGET_H

#include "Gump_button.h"
#include "gumpinf.h"
#include "shapeid.h"

#include <memory>
#include <string>
#include <vector>

class Dynamic_slider;
class Game_object;

/*
 *  A data-driven scrolling list on a gump, configured via the
 *  dynamic_lists section of gump_info.txt.  Rows come from the gump's
 *  container (source 0: icon, name, quantity; the rows are the objects
 *  themselves, so they drag in and out) or from usecode (source 1).
 *  Scrolling uses a Dynamic_slider owned by the list.
 *
 *  It is a Gump_button so that Gump::on_button() finds it over the rows:
 *  a click selects, a double-click activates, and a double-click on a
 *  row never closes the gump.
 */
class Dynamic_list_widget : public Gump_button {
public:
	// Row flags (Row::flags).
	enum Row_flags {
		row_flag_disabled = 0x01    // Drawn shaded; cannot be selected, activated or dragged
	};

	struct Row {
		std::string text;          // Left column (source 0: the object's name)
		std::string right;         // Right-aligned column (source 0: quantity)
		int         shape = -1;    // shapes.vga icon (source 1; -1 = none)
		int         frame = 0;     // Icon frame
		int         flags = 0;     // Row_flags
		int         font  = -1;    // fonts.vga font (-1 = the list's)
	};

	Dynamic_list_widget(Gump* par, const Dynamic_list_def& def);
	~Dynamic_list_widget() override;

	Gump_widget* clone(Gump* par) override;

	void     paint() override;
	bool     run() override;
	TileRect get_rect() const override;

	Gump_button* on_button(int mx, int my) override;
	bool         push(MouseButton button) override;
	void         unpush(MouseButton button) override;
	bool         activate(MouseButton button) override;
	void         double_clicked(int x, int y) override;

	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;
	bool mouse_drag(int mx, int my) override;
	bool key_down(SDL_Keycode chr, SDL_Keycode unicode) override;
	bool mousewheel_up(int mx, int my) override;
	bool mousewheel_down(int mx, int my) override;

	Gump_widget* Input_first() override;

	int get_field_id() const {
		return def.field_id;
	}

	// Rows from the container's contents?
	bool shows_contents() const {
		return def.source == 0;
	}

	// Rebuild source-0 rows if the contents changed.  Never runs usecode.
	void sync_rows();
	int  get_count();
	// Selected row, 1-based; 0 = none.
	int get_selection();
	// Select a row (1-based; 0 clears) and scroll it into view.  No
	// callback.  Returns false for a missing or disabled row.
	bool set_selection(int index);
	// Source 1: replace all rows.  Clears the selection, scrolls to the top.
	int set_rows(std::vector<Row> newrows);
	// Set one row (1-based).  'given' is how many of the fields text,
	// right, shape, frame, flags, font were supplied.  Source 1: replaces
	// row 1..n, or appends at n+1.  Source 0: sets the right column, flags
	// and font of row 1..n, until the rows are rebuilt.
	bool set_row(int index, const Row& row, int given);
	// Source 0: the object at a row (1-based; 0 = the selected row).
	Game_object* get_object(int index);

	// Queries for the hosting gump (screen coordinates).
	Game_object* object_at(int mx, int my);    // Enabled source-0 row's object
	bool         row_location(const Game_object* obj, int& ox, int& oy) const;
	bool         is_drop_target(int mx, int my) const;    // On the rows
	void         select_object(Game_object* obj, bool fire);
	void         object_removed(Game_object* obj);
	// What a bound Dynamic_shape_widget shows: the selected object
	// (source 0) or the selected row's icon (source 1); null if none.
	const ShapeID* get_preview();

	// Scrolling.
	void line_up();
	void line_down();
	void page_up();
	void page_down();
	void scroll_to(int newfirst);

	int get_first() const {
		return first;
	}

	int get_rows() const {
		return def.rows;
	}

	// Darken a screen rectangle to 70%, 85% or 95%, keeping to the
	// palette's own tones.
	static void paint_shade(int w, int h, int sx, int sy, int pct);
	// Name as shown in a row: without a leading "<quantity> ", capitalised.
	static std::string display_name(const Game_object* obj);
	// Shorten a string to fit a width, at a word boundary where it can.
	static std::string fit_text(int fontnum, const std::string& s, int w);

private:
	Dynamic_list_def                def;
	std::unique_ptr<Dynamic_slider> slider;
	bool                            builtin_controls;    // Exult's own arrows and slider
	int                             first = 0;           // First visible row

	// Source 1 rows, or source 0 rows (with their objects).
	std::vector<Row>          rows;
	std::vector<Game_object*> objs;    // Source 0 only, parallel to rows

	// Signature of source-0 rows: rebuilt only when it changes.
	struct Row_sig {
		Game_object* obj;
		int          shape, frame, quantity;

		bool operator==(const Row_sig& o) const {
			return obj == o.obj && shape == o.shape && frame == o.frame && quantity == o.quantity;
		}
	};

	std::vector<Row_sig> signature;
	bool                 rows_pending = true;    // rows_fn due (once on open)

	// Selection.  Source 1: sel_index.  Source 0: sel_obj, only
	// dereferenced once found among objs; sel_hint is the row to select
	// if it goes away; sel_follow keeps a row selected while there are rows.
	int          sel_index  = -1;
	Game_object* sel_obj    = nullptr;
	int          sel_hint   = 0;
	bool         sel_follow = true;

	int  pressed_row    = -1;       // Row under the last on_button()
	bool select_pending = false;    // select_fn due (activate() or run())
	bool focused        = false;    // Last list clicked on this gump (keys)

	ShapeID preview_sid;     // Source 1: the selected row's icon
	int     groove_y = 0;    // Built-in controls: the track, rel. to the list
	int     groove_h = 0;

	int  selected_row();    // 0-based, -1 = none (after sync_rows)
	bool row_disabled(int index) const;
	bool select_row(int index);    // 0-based; true if changed
	void make_focused();
	bool other_list_focused() const;
	int  row_under(int mx, int my) const;    // 0-based or -1, no sync
	void update_slider();
	void dirty();
	bool call_fn(int fn);    // true if usecode ran: `this` may be gone
	bool activate_selected();
	bool move_selection(int dir);
	int  text_x() const;
	bool has_icons() const;
	void paint_row(int index, int sx, int sy);
};

#endif
