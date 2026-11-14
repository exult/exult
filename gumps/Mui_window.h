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

#ifndef MUI_WINDOW_H
#define MUI_WINDOW_H

#include "Modal_gump.h"
#include "Mui_theme.h"
#include "Mui_widgets.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

/*
 *  Shell of a themed menu screen: a modal gump that paints itself into its
 *  own display-resolution layer (UiLayerMenus) over a translucent scrim.
 *  It draws the frame, title bar, tabs (top or left, per theme), the body
 *  built by the subclass, and a footer with the help line, the Ultima/Modern
 *  theme switch and the page buttons. Dropdown lists and dialogs are
 *  overlays inside the window, not new modal loops.
 */
class Mui_window : public Modal_gump {
public:
	explicit Mui_window(std::string title_);
	~Mui_window() override;

	// Gump interface.
	bool uses_render_layer() const override {
		return false;
	}

	bool wants_display_coords() const override {
		return true;
	}

	bool     display_to_local(int sx, int sy, int& lx, int& ly) const override;
	void     set_layer_z_hint(int z) override;
	void     paint() override;
	bool     run() override;
	void     close() override;
	bool     has_point(int x, int y) const override;
	TileRect get_rect() const override;
	bool     mouse_down(int mx, int my, MouseButton button) override;
	bool     mouse_up(int mx, int my, MouseButton button) override;
	bool     mouse_drag(int mx, int my) override;
	bool     mousewheel_up(int mx, int my) override;
	bool     mousewheel_down(int mx, int my) override;
	bool     key_down(SDL_Keycode chr, SDL_Keycode unicode) override;
	bool     text_input(const char* text) override;

	// Widget services.
	const Mui_theme& theme() const {
		return *theme_ptr;
	}

	float get_scale() const {
		return scale;
	}

	void set_dirty() {
		dirty = true;
	}

	bool is_hover(const Mui_widget* w) const {
		return w && w == hover;
	}

	bool is_focus(const Mui_widget* w) const {
		return w && w == focus;
	}

	bool is_pressed(const Mui_widget* w) const {
		return w && w == pressed;
	}

	void set_focus(Mui_widget* w);

	Mui_widget* get_focus() const {
		return focus;
	}

	std::string current_help() const;
	// Open a popup (e.g. a dropdown list) anchored below the owner.
	void open_popup(Mui_widget* owner, std::unique_ptr<Mui_widget> content, const TileRect& r);
	void close_overlay();

	bool has_overlay() const {
		return !overlays.empty();
	}

	// Confirmation and text prompt dialogs (overlays).
	void show_confirm(const std::string& msg, bool destructive, std::function<void(bool)> done);
	void show_prompt(const std::string& msg, const std::string& initial, std::function<void(bool, const std::string&)> done);

	// Switch the menu style (writes config/gameplay/menu_style) and relayout.
	void switch_theme(const std::string& name);
	// Close the window (returns from do_modal_gump).
	void finish();
	// Run fn at the next safe point (outside widget callbacks), e.g. to
	// rebuild the widget tree or close an overlay.
	void post(std::function<void()> fn);

	// Test driver access.
	Mui_widget* find(const std::string& id);
	bool        widget_rect(const std::string& id, TileRect& r);

	int get_layer() const {
		return layer;
	}

	void force_paint();

	// Mouse capture for drags (scrollbars, sliders).
	Mui_widget* get_pressed() const {
		return pressed;
	}

protected:
	std::string title;
	// Build the body; called again whenever the theme or size changes.
	virtual void build(Mui_box& body) = 0;

	// Optional tabs. Tab change rebuilds the body.
	virtual std::vector<std::string> tab_labels() const {
		return {};
	}

	// Optional stable names of the tabs (test driver parts "mui.tabs.<key>").
	virtual std::vector<std::string> tab_keys() const {
		return {};
	}

	virtual void on_tab(int index) {
		tab_index = index;
	}

	// Extra footer buttons (right side).
	virtual void build_footer(Mui_box& footer) {
		ignore_unused_variable_warning(footer);
	}

	// Optional widgets in the title bar, right-aligned before the close
	// button (e.g. a search field). Width in reference pixels.
	virtual void build_title(Mui_box& bar) {
		ignore_unused_variable_warning(bar);
	}

	virtual float title_bar_width() const {
		return 320;
	}

	virtual void on_cancel() {
		finish();
	}

	virtual void on_apply() {}

	// Preferred window size in reference pixels; height 0 = fit content.
	virtual void preferred_size(float& w, float& h) const {
		w = 1120;
		h = 660;
	}

	// Hint shown in the help line when nothing is hovered or focused.
	virtual std::string default_help() const;

	int  tab_index = 0;
	void rebuild();    // Rebuild all widgets later (keeps tab and focus id).

	// Lay the widgets out again after a content size changed (keeps the
	// widgets, focus and scroll position).
	void relayout() {
		layout_all();
	}

	// Rebuild later and then focus the widget with this id.
	void rebuild(const std::string& focus_id);

private:
	std::shared_ptr<Mui_theme>       theme_ptr;
	float                            scale      = 1;
	int                              layer      = -1;
	int                              scrim      = -1;
	int                              layer_w    = 0;
	int                              layer_h    = 0;
	int                              z_hint     = 0;
	bool                             dirty      = true;
	bool                             need_build = true;
	TileRect                         frame_rect{0, 0, 0, 0};    // Whole window.
	TileRect                         title_rect{0, 0, 0, 0};
	TileRect                         tabs_rect{0, 0, 0, 0};
	TileRect                         body_rect{0, 0, 0, 0};
	TileRect                         help_rect{0, 0, 0, 0};
	TileRect                         footer_rect{0, 0, 0, 0};
	std::unique_ptr<Mui_button>      close_button;
	std::unique_ptr<Mui_tabs>        tabs;
	std::unique_ptr<Mui_scroll_view> body;
	std::unique_ptr<Mui_help_line>   help_line;
	std::unique_ptr<Mui_box>         footer;
	std::unique_ptr<Mui_box>         title_bar;
	Mui_box*                         body_box = nullptr;

	struct Overlay {
		std::unique_ptr<Mui_widget> root;
		Mui_widget*                 owner  = nullptr;
		bool                        dialog = false;
		std::string                 prev_focus;
	};

	std::vector<Overlay> overlays;
	Mui_widget*          hover         = nullptr;
	Mui_widget*          focus         = nullptr;
	Mui_widget*          pressed       = nullptr;
	bool                 text_input_on = false;
	uint32               caret_ticks   = 0;

	std::vector<std::function<void()>> pending;

	void process_pending();
	void rebuild_now();
	bool ensure_layers();
	void destroy_layers();
	void hide_layers();
	void layout_all();
	void paint_frame(Mui_painter& p);
	void paint_overlay(Mui_painter& p, Overlay& o);
	// Widget roots in hit order (topmost first).
	std::vector<Mui_widget*> roots() const;
	std::vector<Mui_widget*> focus_chain() const;
	Mui_widget*              hit_widget(int x, int y) const;
	void                     move_focus(int dir);
	void                     clear_pointers();
	void                     update_text_input();
	std::string              focus_id_for_rebuild;
	std::string              requested_focus;
};

#endif
