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

#ifndef MUI_KEYS_MODEL_H
#define MUI_KEYS_MODEL_H

#include "keys.h"

#include <string>
#include <vector>

/*
 *  The key bindings of the Input tab, without any UI. It reads the live
 *  KeyBinder (the built-in defaults, the user's keys file and a mod's
 *  patchkeys.txt, each line tagged with where it came from), keeps the
 *  user's edits as a pending set of bindings, and writes them as a complete
 *  keys file of its own: <SAVEGAME>/keys-<game>.txt, recognised by its first
 *  line. A keys file replaces the defaults, so the file is a full snapshot,
 *  not a list of changes. Nothing else is ever written: not the built-in
 *  defaults, not a mod's patchkeys.txt and not a keys file the editor did
 *  not write.
 *
 *  A row is one command: an action and its parameters ("walk_west" and
 *  "walk_west 1" are two rows, as are "inventory 0" and "inventory 1").
 *  One key can have only one command; an action can have many keys.
 */
class Mui_keys_model {
public:
	enum class Group {
		game,
		cheat,
		mapedit
	};

	struct Row {
		std::string id;        // Action in lower case, then "." and the parameters joined by "_".
		std::string action;    // Lower case.
		int         params[c_maxparams]{-1, -1, -1, -1};
		int         nparams = 0;
		std::string label;
		Group       group    = Group::game;
		bool        mod_keys = false;    // Has a key from a mod.
	};

	// A key shown on a row.
	struct Key {
		ExultKey key{SDLK_UNKNOWN, SDL_KMOD_NONE};
		bool     from_mod   = false;    // Read-only, from <PATCH>/patchkeys.txt.
		bool     overridden = false;    // The user's key, hidden by a mod's key.
	};

	enum class Check {
		ok,
		same_row,        // The row already has this key.
		taken_by_row,    // Another row has it (other_label says which).
		taken_by_mod,    // A mod binds it, and a mod always wins.
		unsupported      // Cannot be written in a keys file.
	};

	struct Check_result {
		Check       status = Check::ok;
		std::string other_label;
		std::string other_id;
	};

	enum class Apply_status {
		ok,
		nothing,           // No changes.
		needs_takeover,    // Another keys file is in use; ask first.
		refused,           // The editor's own path holds a file it did not write.
		failed             // Could not write.
	};

	struct Apply_result {
		Apply_status status = Apply_status::nothing;
		std::string  foreign;    // The other keys file (needs_takeover, refused).
		std::string  path;       // The editor's keys file.
	};

	Mui_keys_model();

	// Read the live bindings again; pending edits are dropped.
	void load();

	bool available() const {
		return !rows_.empty();
	}

	const std::vector<Row>& rows() const {
		return rows_;
	}

	const Row* find(const std::string& id) const;
	// Keys of a row: the user's, then a mod's.
	std::vector<Key> keys_of(const std::string& id) const;
	std::string      label_of(const std::string& id) const;

	// Pending edits differ from what is live.
	bool changed() const;
	bool row_changed(const std::string& id) const;
	// The pending set equals Exult's built-in keys.
	bool is_default() const;
	bool row_is_default(const std::string& id) const;

	Check_result check_key(const std::string& id, ExultKey key) const;
	// Replace one key of a row (old_key = SDLK_UNKNOWN adds a key). Any
	// other row that has the new key loses it.
	void set_key(const std::string& id, ExultKey old_key, ExultKey new_key);
	void unbind(const std::string& id);
	// Keys of other rows that Reset of this row would take over.
	std::vector<Check_result> reset_conflicts(const std::string& id, std::vector<ExultKey>* keys = nullptr) const;
	void                      reset_row(const std::string& id);
	void                      reset_all();
	void                      revert();

	// The text the editor would write for the pending set.
	std::string snapshot() const;
	// The editor's keys file (absolute path) and the keys file in use.
	std::string own_path() const;
	std::string configured_path() const;
	// Write the pending set to the editor's keys file, use it and reload the
	// bindings. takeover: replace another keys file in the configuration
	// (that file is not changed).
	Apply_result apply(bool takeover);

	// Everything that matches needle (lower case): section words, labels,
	// action names and key names.
	bool row_matches(const Row& r, const std::string& needle) const;
	bool section_matches(const std::string& needle) const;

	// Message text with %1, %2, %3 replaced.
	static std::string format(unsigned msg_id, const std::string& a = {}, const std::string& b = {}, const std::string& c = {});
	static std::string text(unsigned msg_id);
	static const char* marker();

	// Compare two sets of bindings (key to command) for the self test.
	static bool same_bindings(const KeyBinder& a, const KeyBinder& b);

private:
	struct Entry {
		ExultKey    key{SDLK_UNKNOWN, SDL_KMOD_NONE};
		std::string row;
		std::string action;
		int         params[c_maxparams]{-1, -1, -1, -1};
		int         nparams = 0;
		std::string comment;
		bool        hidden = false;
	};

	std::vector<Row>   rows_;
	std::vector<Entry> cur;     // The user's bindings, pending.
	std::vector<Entry> orig;    // The user's bindings as loaded.
	std::vector<Entry> defaults;
	std::vector<Entry> mod;    // A mod's effective bindings.

	static std::string        row_id(const std::string& action, const int* params, int nparams, int& trimmed);
	static Entry              make_entry(const KeyBinder::Binding& b);
	static std::vector<Entry> user_entries(const std::vector<KeyBinder::Binding>& all);
	static bool               same_set(const std::vector<Entry>& a, const std::vector<Entry>& b);
	static bool               same_command(const Entry& a, const Entry& b);

	static bool key_eq(ExultKey a, ExultKey b) {
		return a.key == b.key && a.mod == b.mod;
	}

	static std::vector<ExultKey> keys_in(const std::vector<Entry>& set, const std::string& id);
	const Entry*                 find_cur(ExultKey key) const;
	const Entry*                 find_mod(ExultKey key) const;
	void                         add_entry(const std::string& id, ExultKey key);
	std::string                  label_for(const Row& r) const;
};

#endif
