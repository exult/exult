/*
 *  Copyright (C) 2000-2022  The Exult Team
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <cassert>
#include "common_types.h"
#include "XMLEntity.h"
#include <iostream>

using std::string;
using std::vector;
using std::ostream;
using std::endl;

static  string  close_tag(const string &s);

XMLnode::~XMLnode() {
	for (auto *node : nodelist)
		delete node;
}

const string    &XMLnode::reference(const string &h, bool &exists) {
	if (h.find('/') == string::npos) {
		// Must refer to me.
		if (id == h) {
			exists = true;
			return content;
		}
	} else {
		// Otherwise we want to split the string at the first /
		// then locate the branch to walk, and pass the rest
		// down.

		string k;
		k = h.substr(h.find('/') + 1);
		string k2 = k.substr(0, k.find('/'));
		for (auto *node : nodelist) {
			if (node->id == k2)
				return node->reference(k, exists);
		}
	}

	exists = false;
	static const std::string c_empty_string;
	return c_empty_string;
}


const XMLnode *XMLnode::subtree(const string &h) const {
	if (h.find('/') == string::npos) {
		// Must refer to me.
		if (id == h)
			return this;
	} else {
		// Otherwise we want to split the string at the first /
		// then locate the branch to walk, and pass the rest
		// down.

		string k;
		k = h.substr(h.find('/') + 1);
		string k2 = k.substr(0, k.find('/'));
		for (auto *node : nodelist) {
			if (node->id == k2)
				return node->subtree(k);
		}
	}

	return nullptr;
}


string  XMLnode::dump(int depth) {
	string s(depth, ' ');

	s += "<";
	s += id;
	s += ">";
	if (id[id.length() - 1] != '/') {
		for (auto *node : nodelist) {
			s += "\n";
			s += node->dump(depth + 2);
		}

		if (content.length()) {
			s += encode_entity(content);
		}
		if (id[0] == '?') {
			return s;
		}
		if (!content.length()) {
			s += "\n";
			s += string(depth, ' ');
		}

		if (!no_close) {
			s += "</";
			s += close_tag(id);
			s += ">";
		}
	}

	return s;
}

/* Output's a 'nicer' dump of the xmltree, one <tag> value </tag> per line
    the indent characters are specified by indentstr */
void XMLnode::dump(ostream &o, const string &indentstr, const unsigned int depth) const {
	// indent
	for (unsigned int i = 0; i < depth; i++)
		o << indentstr;

	// open tag
	o << '<' << id << '>';

	// if this tag has a closing tag...
	if (id[id.length() - 1] != '/') {
		// if we've got some subnodes, terminate this line...
		if (!nodelist.empty()) {
			o << endl;

			// ... then walk through them outputting them all ...
			for (auto* node : nodelist)
				node->dump(o, indentstr, depth + 1);
		}
		// ... else, if we have content in this output it.
		else if (content.length())
			o << ' ' << encode_entity(content) << ' ';

		// not a clue... it's in XMLnode::dump() so there must be a reason...
		if (id[0] == '?')
			return;

		// append a closing tag if there is one.
		if (!no_close) {
			// if we've got subnodes, we need to reindent
			if (!nodelist.empty())
				for (unsigned int i = 0; i < depth; i++)
					o << indentstr;

			o << "</" << close_tag(id) << '>';
		}
	}
	o << endl;
}

// This function does not make sense here. It should be in XMLEntity
void    XMLnode::xmlassign(const string &key, const string &value) {
	if (key.find('/') == string::npos) {
		// Must refer to me.
		if (id == key)
			content = value;
		else
			CERR("Walking the XML tree failed to create a final node.");
		return;
	}
	string k;
	k = key.substr(key.find('/') + 1);
	string k2 = k.substr(0, k.find('/'));
	for (auto* node : nodelist) {
		if (node->id == k2) {
			node->xmlassign(k, value);
			return;
		}
	}

	// No match, so create a new node and do recursion
	auto *t = new XMLnode(k2);
	nodelist.push_back(t);
	(*t).xmlassign(k, value);
}


void XMLnode::remove(const std::string &key, bool valueonly) {
	if (key.find('/') == string::npos) {
		// Must refer to me.
		if (id == key) {
			content = std::string();
			if (!valueonly) {
				for (auto* node : nodelist)
					delete node;
				nodelist.clear();
			}
			return;
		}
	} else {
		string k;
		k = key.substr(key.find('/') + 1);
		string k2 = k.substr(0, k.find('/'));
		for (auto it = nodelist.begin(); it != nodelist.end(); ++it) {
			XMLnode *node = (*it);
			if (node->id == k2) {
				node->remove(k, valueonly);
				if (node->content.empty() && node->nodelist.empty()) {
					nodelist.erase(it);
					delete node;
				}
				return;
			}
		}
	}

	CERR("Walking the XML tree failed to find the node.");
}


void    XMLnode::listkeys(const string &key, vector<string> &vs, bool longformat) const {
	string s(key);
	s += "/";

	for (auto *node : nodelist) {
		if (!longformat)
			vs.push_back(node->id);
		else
			vs.push_back(s + node->id);
	}
}

string  encode_entity(const string &s) {
	string  ret;

	for (char ch : s) {
		switch (ch) {
		case '<':
			ret += "&lt;";
			break;
		case '>':
			ret += "&gt;";
			break;
		case '"':
			ret += "&quot;";
			break;
		case '\'':
			ret += "&apos;";
			break;
		case '&':
			ret += "&amp;";
			break;
		default:
			ret += ch;
		}
	}
	return ret;
}

static  string  decode_entity(const string &s, std::size_t &pos) {
	std::size_t         old_pos = pos;
	string::size_type   entity_name_len = s.find_first_of("; \t\r\n", pos) - pos - 1;

	/* Call me paranoid... but I don't think having an end-of-line or similar
	    inside a &...; expression is 'good', valid though it may be. */
	assert(s[pos + entity_name_len + 1] == ';');

	string              entity_name = s.substr(pos + 1, entity_name_len);

	pos += entity_name_len + 2;

	// std::cout << "DECODE: " << entity_name << endl;

	if (entity_name == "amp")  return string("&");
	else if (entity_name == "apos") return string("'");
	else if (entity_name == "quot") return string("\"");
	else if (entity_name == "lt")   return string("<");
	else if (entity_name == "gt")   return string(">");

	return s.substr(old_pos, entity_name_len + 2);
}

static  string  close_tag(const string &s) {
	if (s.find(' ') == string::npos)
		return s;

	return s.substr(0, s.find(' '));
}


static  void    trim(string &s) {
	// Clean off leading whitespace
	while (s.length() && s[0] <= 32) {
		s = s.substr(1);
	}
	// Clean off trailing whitespace
	while (s.length() && s[s.length() - 1] <= 32) {
		s.erase(s.length() - 1);
	}
}

void    XMLnode::xmlparse(const string &s, std::size_t &pos) {
	bool    intag = true;

	id = "";
	while (pos < s.length()) {
		switch (s[pos]) {
		case '<': {
			// New tag?
			if (s[pos + 1] == '/') {
				// No. Close tag.
				while (s[pos] != '>')
					pos++;
				++pos;
				trim(content);
				return;
			}
			auto *t = new XMLnode;
			++pos;
			t->xmlparse(s, pos);
			nodelist.push_back(t);
			break;
		}
		case '>':
			// End of tag
			if (s[pos - 1] == '/') {
				if (s[pos - 2] == '<') {
					++pos;
					return; // An empty tag
				} else {
					++pos;
					no_close = true;
					return;
				}
			} else if ((id[0] == '!') && (id[1] == '-') && (id[2] == '-')) {
				++pos;
				no_close = true;
				return;
			}
			++pos;
			intag = false;
			if (s[pos] < 32)
				++pos;
			break;
		case '&':
			content += decode_entity(s, pos);
			break;
		default:
			if (intag)
				id += s[pos++];
			else
				content += s[pos++];
		}
	}
	trim(content);
}

/* Returns a list of key->value pairs that are found under the provided 'basekey'.
    Ignores comments (<!-- ... --> and doesn't return them.
    Returns true if search is 'finished' */
bool XMLnode::searchpairs(KeyTypeList &ktl, const string &basekey, const string &currkey, const unsigned int pos) {
	/* If our 'current key' is longer then the key we're serching for
	    we've obviously gone too deep in this branch, and we won't find
	    it here. */
	if ((currkey.size() <= basekey.size()) && (id[0] != '!')) {
		/* If we've found it, return every key->value pair under this key,
		    then return true, since we've found the key we were looking for. */
		if (basekey == currkey + id) {
			for (auto *node : nodelist)
				if (node->id[0] != '!')
					node->selectpairs(ktl, "");
			return true;
		}
		/* Else, keep searching for the key under it's subnodes */
		else
			for (auto* node : nodelist)
				if (node->searchpairs(ktl, basekey, currkey + id + '/', pos))
					return true;
	}
	return false;
}

/* Just adds every key->value pair under the this node to the ktl */
void XMLnode::selectpairs(KeyTypeList &ktl, const std::string &currkey) {
	ktl.push_back(KeyType(currkey + id, content));

	for (auto* node : nodelist)
		node->selectpairs(ktl, currkey + id + '/');
}





