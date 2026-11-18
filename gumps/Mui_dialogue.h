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

#ifndef MUI_DIALOGUE_H
#define MUI_DIALOGUE_H

#include "Mui_theme.h"
#include "rect.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

class Mui_painter;
class Mui_text_font;

/*
 *  The themed conversation view. Not a gump: Conversation owns it (null in
 *  the classic style) and pushes copies of its state (faces, answers,
 *  text) into it. It paints into two full-display layers below every gump
 *  (panel, text and answers at z 0; portraits at z 1 with translucency)
 *  and runs its own wait loops for "continue" and for answer choices.
 */
class Mui_dialogue {
public:
	struct Face {
		int         shape    = -1;
		int         frame    = 0;
		bool        used     = false;
		bool        drawable = false;    // Has a face frame and is not hidden.
		bool        large    = false;    // Guardian, serpents.
		std::string name;                // Speaker label, may be empty.
	};

	enum class Wait {
		none,
		page,    // Between pages of one text.
		cont,    // Click to continue after the last page.
		choice
	};

	// One line of the scroll-back history: a said text or a chosen answer.
	struct Entry {
		std::string speaker;    // Label, may be empty.
		std::string text;
		bool        choice = false;
	};

	// Resolve config/gameplay/dialogue_style (follow -> menu_style). Sets
	// theme to the theme name, or "classic"; false for the classic view.
	static bool resolve_style(std::string& theme);

	explicit Mui_dialogue(std::shared_ptr<Mui_theme> theme);
	~Mui_dialogue();
	Mui_dialogue(const Mui_dialogue&)            = delete;
	Mui_dialogue& operator=(const Mui_dialogue&) = delete;

	void set_faces(const std::array<Face, 2>& faces, int active);
	void set_answers(const std::vector<std::string>& list);
	void show_choices(bool on);
	// Page the text into the panel, waiting between pages; the last page
	// stays shown.
	void say(const std::string& text, bool large);
	void wait_continue();
	// Index of the chosen answer, or -2 for Escape / right click.
	int  choose();
	void hide();
	void paint();
	// History of the session: Conversation adds the chosen answers.
	void add_choice(const std::string& speaker, const std::string& text);
	void clear_history();

	// Test support.
	const std::string& theme_name() const {
		return theme->name;
	}

	const std::string& current_page() const {
		return page_text;
	}

	const std::vector<std::string>& page_log() const {
		return log;
	}

	Wait waiting() const {
		return wait;
	}

	const std::vector<Entry>& get_history() const {
		return history;
	}

	bool history_open() const {
		return hist_open;
	}

	// Scroll offset of the open history and its largest value.
	int history_scroll() const {
		return hist_scroll;
	}

	int history_scroll_max() const {
		return hist_max_scroll();
	}

	// Label of the speaker of the shown text (may be empty).
	std::string speaker() const;
	// Layer rectangle of "answer.N", "continue", "text", "face.0", "face.1",
	// "panel", "history.close", "history.thumb", "history.track".
	bool part_rect(const std::string& part, TileRect& r);

	int get_layer() const {
		return layer;
	}

private:
	struct Layout {
		TileRect panel;
		TileRect inner;
		TileRect cards[2];
		bool     card_shown[2] = {false, false};
		TileRect plate;
		TileRect text;
		TileRect answers;    // Visible answer area (when choosing).
		TileRect footer;
		TileRect cue;
		TileRect large_face;
		int      line_h    = 1;
		int      lines     = 3;
		int      cols      = 1;
		int      rows      = 0;
		int      btn_h     = 1;
		int      gap       = 1;
		int      content_h = 0;    // Height of all answer rows.
		int      bar_w     = 0;
	};

	// History mode: the panel grown to 70 % of the display.
	struct Hist_line {
		std::string text;
		int         y      = 0;    // Top, relative to the content.
		bool        label  = false;
		bool        choice = false;
	};

	struct Hist_layout {
		TileRect               panel;
		TileRect               header;
		TileRect               close;
		TileRect               view;     // Visible entries.
		TileRect               track;    // Scrollbar.
		TileRect               thumb;
		TileRect               footer;
		int                    line_h    = 1;
		int                    label_h   = 1;
		int                    content_h = 0;
		std::vector<Hist_line> lines;
	};

	std::shared_ptr<Mui_theme> theme;
	int                        layer      = -1;
	int                        face_layer = -1;
	int                        layer_w    = 0;
	int                        layer_h    = 0;
	float                      scale      = 1;
	float                      text_size  = 1;    // Body text size factor.
	bool                       shown      = false;
	bool                       dirty      = true;
	std::array<Face, 2>        faces;
	int                        active = 0;
	std::vector<std::string>   answers;
	bool                       choosing = false;
	int                        focus    = 0;
	int                        hover    = -1;
	int                        scroll   = 0;
	Wait                       wait     = Wait::none;
	// The shown text and who said it.
	std::vector<std::string> page_lines;
	std::string              page_text;
	bool                     text_large = false;
	int                      text_slot  = -1;
	int                      text_shape = -1;
	std::vector<std::string> log;    // Every page shown (test driver only).
	Layout                   lay;
	// Scroll-back history.
	std::vector<Entry> history;
	bool               hist_open   = false;
	int                hist_scroll = 0;
	bool               hist_drag   = false;
	int                hist_grab   = 0;    // Pointer offset into the thumb.
	Hist_layout        hl;
	// Auto-advance: milliseconds of reading time per character, 0 = off.
	int advance_ms = 0;

	bool                                  ensure_layers();
	void                                  destroy_layers();
	const Mui_text_font*                  body_font() const;
	void                                  layout(Mui_painter& p);
	std::vector<std::vector<std::string>> paginate(const std::string& text, const Mui_text_font* font) const;
	void                                  show_page(const std::vector<std::string>& lines);
	TileRect                              answer_rect(int i) const;
	int                                   answer_at(int x, int y) const;
	void                                  set_focus(int i);
	void                                  scroll_by(int dy);
	void                                  paint_frame(Mui_painter& p, const TileRect& panel);
	void                                  paint_panel(Mui_painter& p);
	void                                  paint_card(Mui_painter& p, int slot);
	void                                  paint_answers(Mui_painter& p);
	void                                  paint_faces(Mui_painter& p);
	void                                  layout_history(Mui_painter& p);
	void                                  paint_history(Mui_painter& p);
	void                                  open_history();
	void                                  close_history();
	void                                  hist_scroll_to(int y);
	int                                   hist_max_scroll() const;
	// Handle an input while the history is open; true if it was used.
	bool hist_key(int key);
	int  wait_input(Wait mode);
	bool to_layer(int sx, int sy, int& lx, int& ly) const;
};

#endif
