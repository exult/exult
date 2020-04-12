/*
 *  Copyright (C) 2000-2013  The Exult Team
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

#include <string>
#include <vector>

std::string encode_entity(const std::string &s);

class   XMLnode {
protected:
	std::string             id;
	std::string             content;
	std::vector<XMLnode *>  nodelist;
	bool                    no_close = false;

public:
	XMLnode() = default;
	XMLnode(const std::string &i) : id(i)
	{  }
	XMLnode(const XMLnode &) = delete;
	XMLnode(XMLnode &&) = default;
	~XMLnode();

	XMLnode &operator=(const XMLnode &) = delete;
	XMLnode &operator=(XMLnode &&) = default;
	const std::string &reference(const std::string &, bool &);
	const XMLnode *subtree(const std::string &) const;

	const std::string &value() const {
		return content;
	}

	using KeyType = std::pair<std::string, std::string>;
	using KeyTypeList = std::vector<KeyType>;

	bool searchpairs(KeyTypeList &ktl, const std::string &basekey, const std::string &currkey, const unsigned int pos);
	void selectpairs(KeyTypeList &ktl, const std::string &currkey);

	std::string dump(int depth = 0);
	void dump(std::ostream &o, const std::string &indentstr, const unsigned int depth = 0) const;

	void    xmlassign(const std::string &key, const std::string &value);
	void    xmlparse(const std::string &s, std::size_t &pos);

	void    listkeys(const std::string &, std::vector<std::string> &, bool longformat = true) const;

	void    remove(const std::string &key, bool valueonly);

};

#endif
