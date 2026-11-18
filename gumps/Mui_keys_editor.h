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

#ifndef MUI_KEYS_EDITOR_H
#define MUI_KEYS_EDITOR_H

#include "Mui_keys_model.h"

#include <functional>
#include <set>
#include <string>
#include <vector>

class Mui_box;
class Mui_window;

/*
 *  The "Key bindings" section at the end of the Input tab of the themed
 *  Options window: every bindable action with its keys. Click a key (or
 *  Add key) and press the new key; Esc cancels. A key already used by
 *  another action asks before it is moved; a key a mod binds is refused
 *  (a mod's patchkeys.txt loads last and always wins). Unbind and Reset
 *  work per action, Reset all keys on the whole set. Changes are held
 *  until the window's Apply, which writes the editor's own keys file and
 *  loads it at once (Mui_keys_model).
 *
 *  Widget ids: keys.header, keys.note, keys.file, keys.filter,
 *  keys.reset_all, keys.status, keys.group.<game|cheat|mapedit>, and per
 *  action keys.row.<id> (a box) with .label, .key.<n>, .add, .unbind,
 *  .reset, .notset, .mod, .overridden, .changed, .now; keys.capture while
 *  a key is being chosen. <id> is the action in lower case, then "." and
 *  its parameters joined by "_" (walk_west, walk_west.1, inventory.0).
 */
class Mui_keys_editor {
public:
	// rebuild(focus_id) rebuilds the window's widgets later.
	Mui_keys_editor(Mui_window& win, std::function<void(const std::string&)> rebuild);

	// False before a game is running (no key bindings to edit).
	bool available() const {
		return model.available();
	}

	void build(Mui_box& body);

	bool changed() const {
		return model.changed();
	}

	// Write the pending set. May ask first (another keys file in use); then
	// the changes stay pending until the question is answered.
	bool apply(bool& wrote);
	void revert();
	// Read the live bindings again unless edits are pending.
	void reload();

	// Rows that match needle (lower case); section is set if the needle
	// names the section itself (keys, rebind, shortcut ...).
	std::vector<std::string> search(const std::string& needle, bool& section) const;

	std::string row_label(const std::string& id) const {
		return model.label_of(id);
	}

	// Show a row: expand its group, clear the filter, highlight it.
	void reveal(const std::string& id);

	// Id of the control that takes the focus on a revealed row.
	static std::string focus_id(const std::string& id) {
		return "keys.row." + id + ".add";
	}

	Mui_keys_model& get_model() {
		return model;
	}

private:
	Mui_window&                             win;
	std::function<void(const std::string&)> rebuild;
	Mui_keys_model                          model;
	std::set<int>                           expanded;    // Mui_keys_model::Group values.
	std::string                             filter;
	std::string                             highlight;
	std::string                             status;
	bool                                    status_error = false;
	std::string                             takeover;    // Keys file to ask about.
	// The key being chosen: a row, and which of its keys (-1 = a new one).
	std::string capture_row;
	int         capture_index = -1;

	void               build_row(Mui_box& col, const Mui_keys_model::Row& r);
	void               start_capture(const std::string& id, int index);
	void               end_capture();
	void               captured(const std::string& id, int index, ExultKey key);
	void               say(const std::string& text, bool error);
	void               report(const Mui_keys_model::Apply_result& r, bool& wrote);
	void               ask_takeover(const std::string& foreign);
	void               ask_now();
	static std::string key_text(ExultKey key);
};

#endif
