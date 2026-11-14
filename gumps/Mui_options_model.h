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

#ifndef MUI_OPTIONS_MODEL_H
#define MUI_OPTIONS_MODEL_H

#include <map>
#include <memory>
#include <string>
#include <vector>

class Gump_button;
class Modal_gump;
class VideoOptions_gump;

/*
 *  One page of the themed Options window, backed by the classic option gump
 *  of that page used as a headless model. The classic gump is constructed
 *  (so its load_settings() runs) but never shown: values are read from its
 *  buttons, changed through the buttons' own toggle callbacks and applied
 *  with the gump's own save_settings(), so every setting keeps exactly the
 *  classic behaviour and config keys.
 */
class Mui_options_model {
public:
	enum class Tab {
		video,
		audio,
		engine,
		display,
		input,
		ui,
		count
	};

	enum class Kind {
		choice,    // A toggle button with a list of choices.
		action     // A button that opens a classic sub-screen.
	};

	enum class Apply {
		now,       // Takes effect when applied.
		restart    // Takes effect the next time Exult (or a game) starts.
	};

	struct Row {
		std::string id;             // Stable setting id, e.g. "doubleclick".
		int         button = -1;    // Classic button id.
		unsigned    label  = 0;     // exultmsg id of the classic label.
		unsigned    help   = 0;     // exultmsg id of the one-line help.
		std::string keys;           // Config keys written, space separated.
		Apply       apply   = Apply::now;
		Kind        kind    = Kind::choice;
		bool        pending = false;    // Action edits state applied later.
		// Live state, refreshed from the classic gump.
		bool                     visible = false;
		bool                     indent  = false;    // Sub-setting of the row above.
		std::vector<std::string> choices;
		int                      value = -1;
	};

	explicit Mui_options_model(Tab t);
	~Mui_options_model();

	Mui_options_model(const Mui_options_model&)            = delete;
	Mui_options_model& operator=(const Mui_options_model&) = delete;

	Tab get_tab() const {
		return which;
	}

	const std::vector<Row>& rows() const {
		return table;
	}

	const Row*  find(const std::string& id) const;
	std::string label(const Row& r) const;
	std::string help(const Row& r) const;
	// Change a choice row. Returns true when the set of rows or their
	// choices changed (the classic gump rebuilt its buttons).
	bool set_value(const std::string& id, int index);
	// Run an action row (opens a classic sub-screen).
	void activate(const std::string& id);
	// Save through the classic gump, then re-read.
	void apply();
	// Throw away unapplied changes (recreates the classic gump).
	void revert();
	// Changes since the window opened or the last apply.
	bool changed() const;
	bool row_changed(const Row& r) const;
	// Value of a row at the baseline (-1 if unknown).
	int baseline_value(const std::string& id) const;

	static const char* tab_key(Tab t);

private:
	Tab                         which;
	std::unique_ptr<Modal_gump> gump;
	std::vector<Row>            table;
	std::map<std::string, int>  baseline;
	bool                        touched = false;

	void                    create();
	static void             add_game_areas(VideoOptions_gump& g);
	void                    refresh();
	void                    mark_baseline();
	Gump_button*            button(int id) const;
	std::string             signature() const;
	static std::vector<Row> build_table(Tab t);
};

#endif
