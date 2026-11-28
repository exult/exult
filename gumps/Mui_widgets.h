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

#ifndef MUI_WIDGETS_H
#define MUI_WIDGETS_H

#include "Mui_painter.h"
#include "Mui_theme.h"
#include "rect.h"

#include <SDL3/SDL_keycode.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

class Mui_window;

/*
 *  A small retained widget toolkit for the themed menus, independent of
 *  Gump_widget. Coordinates are layer (display) pixels. Sizes come from the
 *  theme metrics times the UI scale. The vocabulary (id, text, font, shape,
 *  value) follows docs/dynamic_gumps.md.
 */
class Mui_widget {
public:
	explicit Mui_widget(std::string id_ = {}) : id(std::move(id_)) {}

	virtual ~Mui_widget() = default;

	Mui_widget(const Mui_widget&)            = delete;
	Mui_widget& operator=(const Mui_widget&) = delete;

	std::string id;
	TileRect    rect{0, 0, 0, 0};
	bool        visible   = true;
	bool        enabled   = true;
	bool        focusable = false;
	bool        stretch   = false;    // Take spare width in a horizontal box.
	bool        fill_w    = true;     // Use the full width in a vertical box.
	std::string help;                 // One-line help shown in the footer.
	Mui_widget* parent           = nullptr;
	int         surface_override = -1;    // Palette index of the surface below, if known.

	// Preferred size for the available width.
	virtual void measure(Mui_painter& p, int avail_w, int& w, int& h);

	// Place children after rect was set.
	virtual void layout(Mui_painter& p) {
		ignore_unused(p);
	}

	virtual void paint(Mui_painter& p) {
		ignore_unused(p);
	}

	// Input. Return true when handled.
	virtual bool on_mouse_down(int x, int y, int button) {
		ignore_unused(x, y, button);
		return false;
	}

	virtual bool on_mouse_up(int x, int y, int button, bool inside) {
		ignore_unused(x, y, button, inside);
		return false;
	}

	virtual bool on_mouse_move(int x, int y, bool held) {
		ignore_unused(x, y, held);
		return false;
	}

	virtual bool on_wheel(int x, int y, int dy) {
		ignore_unused(x, y, dy);
		return false;
	}

	virtual bool on_key(SDL_Keycode key, int unicode) {
		ignore_unused(key, unicode);
		return false;
	}

	virtual bool on_text(const char* text) {
		ignore_unused(text);
		return false;
	}

	// Enter/Space on the focused widget.
	virtual bool activate() {
		return false;
	}

	virtual bool wants_text() const {
		return false;
	}

	// Called when the widget loses keyboard focus.
	virtual void on_blur() {}

	// Tree access.
	virtual Mui_widget* hit(int x, int y);
	virtual void        collect(std::vector<Mui_widget*>& out);
	virtual Mui_widget* find(const std::string& want);
	// Sub-part rects for the test driver ("<id>.<part>").
	virtual bool find_part(const std::string& want, TileRect& r);

	virtual void attach(Mui_window* w) {
		win = w;
	}

	// Values for the test driver and generic code.
	virtual std::string value_text() const {
		return {};
	}

	virtual bool select_index(int index) {
		ignore_unused(index);
		return false;
	}

	Mui_window* window() const {
		return win;
	}

	void invalidate() const;
	bool is_hover() const;
	bool is_focus() const;
	bool is_pressed() const;

	bool is_live() const {    // Visible and enabled up the tree.
		return visible && enabled && (!parent || parent->is_live());
	}

	// Palette index of the surface this widget sits on (for text blending).
	virtual uint8 surface(const Mui_painter& p) const;

protected:
	Mui_window* win = nullptr;

	template <typename... Ts>
	static void ignore_unused(const Ts&...) {}
};

// Vertical or horizontal stack of children.
class Mui_box : public Mui_widget {
public:
	enum class Dir {
		vertical,
		horizontal
	};

	explicit Mui_box(Dir d = Dir::vertical, std::string id_ = {}) : Mui_widget(std::move(id_)), dir(d) {}

	Dir        dir;
	float      gap    = -1;       // Reference px; -1 = theme gap.
	float      pad    = 0;        // Reference px around the children.
	bool       panel  = false;    // Paint a panel background.
	bool       centre = false;    // Centre children on the main axis.
	Mui_colour bg     = Mui_colour::panel;

	template <typename T, typename... Args>
	T* add(Args&&... args) {
		auto* w = new T(std::forward<Args>(args)...);
		add_widget(std::unique_ptr<Mui_widget>(w));
		return w;
	}

	Mui_widget* add_widget(std::unique_ptr<Mui_widget> w);
	void        clear();

	const std::vector<std::unique_ptr<Mui_widget>>& get_children() const {
		return children;
	}

	void        measure(Mui_painter& p, int avail_w, int& w, int& h) override;
	void        layout(Mui_painter& p) override;
	void        paint(Mui_painter& p) override;
	Mui_widget* hit(int x, int y) override;
	void        collect(std::vector<Mui_widget*>& out) override;
	Mui_widget* find(const std::string& want) override;
	bool        find_part(const std::string& want, TileRect& r) override;
	void        attach(Mui_window* w) override;
	uint8       surface(const Mui_painter& p) const override;

protected:
	std::vector<std::unique_ptr<Mui_widget>> children;
	std::vector<int>                         sizes;    // Measured main-axis sizes.
	int                                      gap_px(const Mui_painter& p) const;
};

class Mui_label : public Mui_widget {
public:
	Mui_label(
			std::string text_, Mui_font_role role_ = Mui_font_role::body, Mui_colour colour_ = Mui_colour::text,
			bool wrap_ = false);

	std::string   text;
	Mui_font_role role;
	Mui_colour    colour;
	bool          wrap;
	Mui_align     align = Mui_align::left;

	void measure(Mui_painter& p, int avail_w, int& w, int& h) override;
	void paint(Mui_painter& p) override;

	std::string value_text() const override {
		return text;
	}
};

class Mui_section_header : public Mui_widget {
public:
	explicit Mui_section_header(std::string text_) : text(std::move(text_)) {}

	std::string text;
	void        measure(Mui_painter& p, int avail_w, int& w, int& h) override;
	void        paint(Mui_painter& p) override;
};

class Mui_badge : public Mui_widget {
public:
	enum class Kind {
		now,
		restart,
		changed
	};

	Mui_badge(Kind k, std::string text_) : kind(k), text(std::move(text_)) {
		fill_w = false;
	}

	Kind        kind;
	std::string text;
	void        measure(Mui_painter& p, int avail_w, int& w, int& h) override;
	void        paint(Mui_painter& p) override;
};

class Mui_button : public Mui_widget {
public:
	enum class Style {
		primary,
		secondary,
		destructive,
		flat    // Borderless (title bar close, list actions).
	};

	Mui_button(std::string id_, std::string text_, Style s = Style::secondary, std::function<void()> click = {});

	std::string           text;
	Style                 style;
	Mui_icon              icon     = Mui_icon::check;
	bool                  has_icon = false;
	float                 min_w    = 0;                   // Reference px.
	Mui_colour            flat_ink = Mui_colour::text;    // Ink of a flat button at rest.
	std::function<void()> on_click;

	void measure(Mui_painter& p, int avail_w, int& w, int& h) override;
	void paint(Mui_painter& p) override;
	bool on_mouse_down(int x, int y, int button) override;
	bool on_mouse_up(int x, int y, int button, bool inside) override;
	bool activate() override;

	std::string value_text() const override {
		return text;
	}
};

class Mui_toggle : public Mui_widget {
public:
	Mui_toggle(std::string id_, bool value_, std::function<void(bool)> change = {});

	bool                      value;
	std::function<void(bool)> on_change;

	void measure(Mui_painter& p, int avail_w, int& w, int& h) override;
	void paint(Mui_painter& p) override;
	bool on_mouse_down(int x, int y, int button) override;
	bool on_mouse_up(int x, int y, int button, bool inside) override;
	bool on_key(SDL_Keycode key, int unicode) override;
	bool activate() override;
	void set(bool v);

	std::string value_text() const override {
		return value ? "1" : "0";
	}

	bool select_index(int index) override {
		set(index != 0);
		return true;
	}
};

class Mui_segmented : public Mui_widget {
public:
	Mui_segmented(std::string id_, std::vector<std::string> options_, int index_, std::function<void(int)> change = {});

	std::vector<std::string> options;
	std::vector<std::string> keys;    // Optional part names for find_part().
	int                      index;
	std::function<void(int)> on_change;

	void        measure(Mui_painter& p, int avail_w, int& w, int& h) override;
	void        paint(Mui_painter& p) override;
	bool        on_mouse_down(int x, int y, int button) override;
	bool        on_mouse_up(int x, int y, int button, bool inside) override;
	bool        on_key(SDL_Keycode key, int unicode) override;
	bool        activate() override;
	bool        find_part(const std::string& want, TileRect& r) override;
	void        set(int i);
	std::string value_text() const override;

	bool select_index(int i) override {
		set(i);
		return true;
	}

private:
	TileRect segment_rect(int i) const;
	int      press = -1;
};

class Mui_list : public Mui_widget {
public:
	Mui_list(std::string id_, std::vector<std::string> rows_, int sel = -1);

	std::vector<std::string> rows;
	int                      selected;
	int                      scroll       = 0;    // First visible row.
	int                      visible_rows = 6;    // Preferred height in rows.
	float                    row_ref      = 0;    // Reference px; 0 = theme row_h.
	std::function<void(int)> on_select;
	std::function<void(int)> on_activate;
	// Optional custom row painter.
	std::function<void(Mui_painter&, int, const TileRect&, bool)> paint_row;

	void        measure(Mui_painter& p, int avail_w, int& w, int& h) override;
	void        paint(Mui_painter& p) override;
	bool        on_mouse_down(int x, int y, int button) override;
	bool        on_mouse_up(int x, int y, int button, bool inside) override;
	bool        on_mouse_move(int x, int y, bool held) override;
	bool        on_wheel(int x, int y, int dy) override;
	bool        on_key(SDL_Keycode key, int unicode) override;
	bool        activate() override;
	bool        find_part(const std::string& want, TileRect& r) override;
	void        set_selected(int i, bool notify = true);
	std::string value_text() const override;

	bool select_index(int i) override {
		set_selected(i);
		return true;
	}

	int row_height() const {
		return row_px;
	}

private:
	int  row_px      = 0;
	int  press_row   = -1;
	int  drag_y      = -1;
	int  drag_scroll = 0;
	int  rows_shown() const;
	void clamp_scroll();
	void ensure_visible(int i);
};

class Mui_dropdown : public Mui_widget {
public:
	Mui_dropdown(std::string id_, std::vector<std::string> options_, int index_, std::function<void(int)> change = {});

	std::vector<std::string> options;
	int                      index;
	std::function<void(int)> on_change;

	void        measure(Mui_painter& p, int avail_w, int& w, int& h) override;
	void        paint(Mui_painter& p) override;
	bool        on_mouse_down(int x, int y, int button) override;
	bool        on_mouse_up(int x, int y, int button, bool inside) override;
	bool        on_key(SDL_Keycode key, int unicode) override;
	bool        activate() override;
	void        set(int i);
	void        open_popup();
	std::string value_text() const override;

	bool select_index(int i) override {
		set(i);
		return true;
	}
};

class Mui_slider : public Mui_widget {
public:
	Mui_slider(std::string id_, int min_, int max_, int step_, int value_, std::function<void(int)> change = {});

	int                             minv, maxv, step, value;
	std::function<void(int)>        on_change;
	std::function<std::string(int)> format;    // Value text; default the number.

	void measure(Mui_painter& p, int avail_w, int& w, int& h) override;
	void paint(Mui_painter& p) override;
	bool on_mouse_down(int x, int y, int button) override;
	bool on_mouse_up(int x, int y, int button, bool inside) override;
	bool on_mouse_move(int x, int y, bool held) override;
	bool on_key(SDL_Keycode key, int unicode) override;
	bool on_wheel(int x, int y, int dy) override;
	void set(int v);

	std::string value_text() const override {
		return std::to_string(value);
	}

	bool select_index(int i) override {
		set(i);
		return true;
	}

private:
	TileRect track_rect(const Mui_painter* p = nullptr) const;
	int      value_w  = 0;
	int      knob_px  = 0;
	bool     dragging = false;
	void     set_from_x(int x);
};

class Mui_number_field : public Mui_widget {
public:
	Mui_number_field(std::string id_, int min_, int max_, int step_, int value_, std::function<void(int)> change = {});

	int                      minv, maxv, step, value;
	std::function<void(int)> on_change;

	void measure(Mui_painter& p, int avail_w, int& w, int& h) override;
	void paint(Mui_painter& p) override;
	bool on_mouse_down(int x, int y, int button) override;
	bool on_mouse_up(int x, int y, int button, bool inside) override;
	bool on_key(SDL_Keycode key, int unicode) override;
	bool on_wheel(int x, int y, int dy) override;
	void on_blur() override;
	void set(int v);

	std::string value_text() const override {
		return std::to_string(value);
	}

	bool select_index(int i) override {
		set(i);
		return true;
	}

private:
	std::string typed;        // Digits typed but not yet committed.
	int         press = 0;    // -1 minus, +1 plus.
	TileRect    minus_rect() const;
	TileRect    plus_rect() const;
	void        commit();
};

class Mui_text_field : public Mui_widget {
public:
	Mui_text_field(
			std::string id_, std::string text_, std::string placeholder_ = {}, std::function<void(const std::string&)> change = {});

	std::string                             text;
	std::string                             placeholder;
	bool                                    search_icon = false;
	size_t                                  max_len     = 80;
	std::function<void(const std::string&)> on_change;
	std::function<void(const std::string&)> on_submit;

	void measure(Mui_painter& p, int avail_w, int& w, int& h) override;
	void paint(Mui_painter& p) override;
	bool on_mouse_down(int x, int y, int button) override;
	bool on_key(SDL_Keycode key, int unicode) override;
	bool on_text(const char* t) override;

	bool wants_text() const override {
		return true;
	}

	void set_text(const std::string& t, bool notify = true);

	std::string value_text() const override {
		return text;
	}

private:
	size_t cursor = std::string::npos;    // npos = end.
	bool   insert(const std::string& s);
};

// Grid of cards painted by a callback.
class Mui_grid : public Mui_widget {
public:
	Mui_grid(std::string id_, int count_, float card_w_ref, float card_h_ref);

	int                                                           count;
	float                                                         card_w, card_h;    // Reference px.
	int                                                           selected = -1;
	std::function<void(Mui_painter&, int, const TileRect&, bool)> paint_card;
	std::function<void(int)>                                      on_select;
	std::function<void(int)>                                      on_activate;

	void measure(Mui_painter& p, int avail_w, int& w, int& h) override;
	void paint(Mui_painter& p) override;
	bool on_mouse_down(int x, int y, int button) override;
	bool on_mouse_up(int x, int y, int button, bool inside) override;
	bool on_key(SDL_Keycode key, int unicode) override;
	bool activate() override;
	bool find_part(const std::string& want, TileRect& r) override;
	void set_selected(int i, bool notify = true);

	std::string value_text() const override {
		return std::to_string(selected);
	}

	bool select_index(int i) override {
		set_selected(i);
		return true;
	}

	TileRect card_rect(int i) const;

private:
	int cols = 1, cw = 0, ch = 0, gapx = 0;
	int press = -1;
};

class Mui_tabs : public Mui_widget {
public:
	Mui_tabs(std::string id_, std::vector<std::string> labels_, int index_, bool vertical_, std::function<void(int)> change = {});

	std::vector<std::string> labels;
	std::vector<std::string> keys;    // Optional part names.
	int                      index;
	bool                     vertical;
	std::function<void(int)> on_change;

	void        measure(Mui_painter& p, int avail_w, int& w, int& h) override;
	void        paint(Mui_painter& p) override;
	bool        on_mouse_down(int x, int y, int button) override;
	bool        on_mouse_up(int x, int y, int button, bool inside) override;
	bool        on_key(SDL_Keycode key, int unicode) override;
	bool        find_part(const std::string& want, TileRect& r) override;
	void        set(int i);
	std::string value_text() const override;

	bool select_index(int i) override {
		set(i);
		return true;
	}

	TileRect tab_rect(int i) const;

private:
	std::vector<int> widths;
	int              tab_px = 0;
	int              press  = -1;
};

// Vertical scrolling container with a scrollbar.
class Mui_scroll_view : public Mui_widget {
public:
	explicit Mui_scroll_view(std::unique_ptr<Mui_widget> child_, std::string id_ = {});

	int scroll = 0;    // Pixels.

	Mui_widget* get_child() const {
		return child.get();
	}

	void        measure(Mui_painter& p, int avail_w, int& w, int& h) override;
	void        layout(Mui_painter& p) override;
	void        paint(Mui_painter& p) override;
	Mui_widget* hit(int x, int y) override;
	void        collect(std::vector<Mui_widget*>& out) override;
	Mui_widget* find(const std::string& want) override;
	bool        find_part(const std::string& want, TileRect& r) override;
	void        attach(Mui_window* w) override;
	bool        on_wheel(int x, int y, int dy) override;
	bool        on_mouse_down(int x, int y, int button) override;
	bool        on_mouse_up(int x, int y, int button, bool inside) override;
	bool        on_mouse_move(int x, int y, bool held) override;
	uint8       surface(const Mui_painter& p) const override;
	void        ensure_visible(const TileRect& r);
	void        set_scroll(int s);

private:
	std::unique_ptr<Mui_widget> child;
	int                         content_h = 0;
	int                         bar_w     = 0;
	int                         drag_y    = -1;
	int                         drag_s    = 0;
	TileRect                    thumb_rect() const;
	int                         content_w() const;
	void                        place_child();
};

// Shows the help of the hovered or focused widget.
class Mui_help_line : public Mui_widget {
public:
	Mui_help_line() : Mui_widget("mui.help") {}

	void        measure(Mui_painter& p, int avail_w, int& w, int& h) override;
	void        paint(Mui_painter& p) override;
	std::string value_text() const override;
};

// Fixed-size gap.
class Mui_spacer : public Mui_widget {
public:
	explicit Mui_spacer(float ref_h, bool stretch_ = false) : h_ref(ref_h) {
		stretch = stretch_;
	}

	float h_ref;
	void  measure(Mui_painter& p, int avail_w, int& w, int& h) override;
};

#endif
