/*
 *  Copyright (C) 2000-2022
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef XMLENTITY_H
#define XMLENTITY_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

std::string encode_entity(const std::string& s);

class XMLnode {
	using PXMLnode = std::unique_ptr<XMLnode>;
	std::string           id;
	std::string           content;
	std::vector<PXMLnode> nodelist;
	bool                  no_close = false;

public:
	XMLnode() = default;

	explicit XMLnode(std::string_view i) : id(i) {}

	XMLnode(const XMLnode&) = delete;
	XMLnode(XMLnode&&)      = default;
	~XMLnode()              = default;

	XMLnode&         operator=(const XMLnode&) = delete;
	XMLnode&         operator=(XMLnode&&)      = default;
	std::string_view reference(std::string_view, bool&) const;
	const XMLnode*   subtree(std::string_view) const;

	std::string_view value() const {
		return content;
	}

	using KeyType     = std::pair<std::string, std::string>;
	using KeyTypeList = std::vector<KeyType>;

	bool searchpairs(
			KeyTypeList& ktl, std::string_view basekey,
			std::string_view currkey, const unsigned int pos);
	void selectpairs(KeyTypeList& ktl, std::string_view currkey);

	std::string dump(int depth = 0);
	void        dump(
				   std::ostream& o, std::string_view indentstr,
				   const unsigned int depth = 0) const;

	void xmlassign(std::string_view key, std::string_view value);
	void xmlparse(std::string_view s, std::size_t& pos);

	void listkeys(
			std::string_view, std::vector<std::string>&,
			bool longformat = true) const;

	void remove(std::string_view key, bool valueonly);
};

#endif
