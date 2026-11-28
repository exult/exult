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

#ifndef MUI_CONFIG_EDITOR_H
#define MUI_CONFIG_EDITOR_H

#include "singles.h"

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

class Mui_box;
class Mui_window;
struct Mui_config_key;

/*
 *  The Advanced tab of the themed Options window: every exult.cfg key in
 *  a section tree, with its full path, description, a control for its type
 *  (from data/mui_config_keys.txt via Mui_config_meta), "Applies now" or
 *  "Needs restart", a "Changed from default" marker and Reset to default.
 *  Keys the table does not know are listed under "Other" with a text
 *  field. Edits are held until apply(), which writes only the edited keys
 *  (config->set per key, one write_back) and refreshes the engine for keys
 *  that apply at once.
 *
 *  Widget ids: adv.filter, adv.count, adv.sections (rows = visible tree
 *  nodes), and per key adv.<path> (control), adv.row.<path>,
 *  adv.<path>.reset/.changed/.now/.restart/.notset/.error/.path/.desc.
 */
class Mui_config_editor : public Game_singletons {
public:
	struct Entry {
		std::string           path;
		const Mui_config_key* meta    = nullptr;    // nullptr = unknown key.
		bool                  present = false;      // Set in exult.cfg.
		std::string           live;                 // Value in exult.cfg.
		std::string           section;              // Tree node it is listed under.
	};

	// rebuild(focus_id) rebuilds the window's widgets later.
	Mui_config_editor(Mui_window& win, std::function<void(const std::string&)> rebuild);

	// Read the live configuration again (pending edits are kept).
	void reload();
	void build(Mui_box& body);

	bool changed() const {
		return !pending.empty();
	}

	bool has_errors() const {
		return !errors.empty();
	}

	// Validate and write the pending edits. Returns false (and writes
	// nothing) if a value is invalid; wrote is set if a key was written.
	bool apply(bool& wrote);
	void revert();

	// Keys whose path or description contains needle (lower case).
	std::vector<const Entry*> search(const std::string& needle) const;
	// Show a key: select its section, clear the filter, highlight its row.
	void reveal(const std::string& path);

	static std::string control_id(const std::string& path) {
		return "adv." + path;
	}

	std::string description(const Entry& e) const;

private:
	struct Node {
		std::string path;    // "config/gameplay"; other_node for "Other".
		std::string label;
		int         depth        = 1;
		bool        has_children = false;
		int         keys         = 0;    // Keys listed directly under it.
	};

	Mui_window&                             win;
	std::function<void(const std::string&)> rebuild;
	std::vector<Entry>                      entries;    // Sorted by path.
	std::vector<Node>                       nodes;      // Whole tree, in order.
	std::map<std::string, std::string>      pending;    // path -> new value.
	std::map<std::string, std::string>      errors;     // path -> message.
	std::set<std::string>                   expanded;
	std::string                             section;
	std::string                             filter;
	std::string                             highlight;

	std::vector<const Node*> visible_nodes() const;
	std::string              value_of(const Entry& e) const;
	void                     set_pending(const Entry& e, const std::string& value, bool now);
	void                     select_section(const std::string& path);
	void                     build_tree(Mui_box& col);
	void                     build_keys(Mui_box& col);
	void                     build_row(Mui_box& col, const Entry& e);
	void                     add_control(Mui_box& line, const Entry& e);
	void                     apply_now(const std::string& path, const std::string& value);
};

#endif
