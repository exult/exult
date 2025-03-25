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
#	include <config.h>
#endif

#include "Configuration.h"

#include "exceptions.h"
#include "ignore_unused_variable_warning.h"
#include "utils.h"

#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>

using std::atoi;
using std::endl;
using std::ostream;
using std::perror;
using std::string;
#ifdef _WIN32
using std::cerr;
#endif
using std::isspace;

#define TRACE_CONF 0

#if TRACE_CONF
#	define CTRACE(X) cerr << X << endl
#else
#	define CTRACE(X)
#endif

void Configuration::value(
		const string& key, string& ret, const std::string& defaultvalue) const {
	const XMLnode* sub = xmltree->subtree(key);
	if (sub) {
		ret = sub->value();
	} else {
		ret = defaultvalue;
	}
}

void Configuration::value(
		const string& key, bool& ret, bool defaultvalue) const {
	const XMLnode* sub = xmltree->subtree(key);
	if (sub) {
		ret = (to_uppercase(sub->value()) == "YES");
	} else {
		ret = defaultvalue;
	}
}

void Configuration::value(const string& key, int& ret, int defaultvalue) const {
	const XMLnode* sub = xmltree->subtree(key);
	if (sub) {
		ret = atoi(sub->value().c_str());
	} else {
		ret = defaultvalue;
	}
}

bool Configuration::key_exists(const string& key) const {
	const XMLnode* sub = xmltree->subtree(key);
	return sub != nullptr;
}

void Configuration::set(
		const string& key, const string& value, bool write_out) {
	// Break k up into '/' separated elements.
	// start advancing walk, one element at a time, creating nodes
	// as needed.
	// At the end of that, walk is the target node, and we
	// can set the value.

	// We must also properly encode the value before writing it out.
	// Must remember that.

	xmltree->xmlassign(key, value);
	if (write_out) {
		write_back();
	}
}

void Configuration::set(const char* key, const char* value, bool write_out) {
	const string k(key);
	const string v(value);
	set(k, v, write_out);
}

void Configuration::set(const char* key, const string& value, bool write_out) {
	const string k(key);
	set(k, value, write_out);
}

void Configuration::set(const char* key, int value, bool write_out) {
	const string k(key);
	const string v = std::to_string(value);
	set(k, v, write_out);
}

void Configuration::set(const string& key, int value, bool write_out) {
	const string v = std::to_string(value);
	set(key, v, write_out);
}

void Configuration::remove(const string& key, bool write_out) {
	xmltree->remove(key, true);
	if (write_out) {
		write_back();
	}
}

bool Configuration::read_config_string(const string& s) {
	const string& sbuf(s);
	size_t        nn = 0;
	while (isspace(static_cast<char>(s[nn]))) {
		++nn;
	}

	assert(s[nn] == '<');
	++nn;

	xmltree->xmlparse(sbuf, nn);
	is_file = false;
	return true;
}

static inline bool is_path_absolute(const string& path) {
#ifdef _WIN32
	const auto is_win32_abs_path = [](const string& path) {
		return (path.find(".\\") == 0) || (path.find("..\\") == 0)
			   || (path[0] == '\\')
			   || (std::isalpha(path[0]) && path[1] == ':'
				   && (path[2] == '/' || path[2] == '\\'));
	};
#else
	const auto is_win32_abs_path = [](const string& path) {
		ignore_unused_variable_warning(path);
		return false;
	};
#endif

	return (path.find("./") == 0) || (path.find("../") == 0) || (path[0] == '/')
		   || is_win32_abs_path(path);
}

bool Configuration::read_config_file(
		const string& input_filename, const string& root) {
	string fname;

	CTRACE("Configuration::read_config_file");

	fname = input_filename;
	// Don't frob the filename if it starts with a dot and
	// a slash or with two dots and a slash.
	// Or if it's not a relative path.
	if (!is_path_absolute(get_system_path(input_filename))) {
#if (defined(XWIN) || defined(MACOSX) || defined(_WIN32) \
	 || defined(SDL_PLATFORM_IOS))
		fname = "<CONFIG>/";
#	if (defined(XWIN) && !defined(MACOSX) && !defined(SDL_PLATFORM_IOS))
		fname += ".";
#	endif
		fname += input_filename;
#else
		// For now, just read file from current directory
		fname = input_filename;
#endif

#if defined(_WIN32)
		// Note: this first check misses some cases of path equality, but it
		// does eliminates some spurious warnings.
		if (fname != input_filename && U7exists(input_filename.c_str())
			&& get_system_path("<HOME>") != ".") {
			if (!U7exists(fname.c_str())) {
				cerr << "Warning: configuration file '" << input_filename
					 << "' is being copied to file '" << fname
					 << "' and will no longer be used." << endl;
				try {
					const size_t pos = fname.find_last_of("/\\");
					if (pos != string::npos) {
						// First, try to make the directory.
						const std::string path = fname.substr(0, pos);
						U7mkdir(path.c_str(), 0755);
					}
					U7copy(input_filename.c_str(), fname.c_str());
				} catch (exult_exception& /*e*/) {
					cerr << "File copy FAILED. Old settings will be lost"
						 << endl;
				}
			} else {
				cerr << "Warning: configuration file '" << input_filename
					 << "' is being ignored in favor of file '" << fname << "'."
					 << endl;
			}
		}
#endif    // _WIN32
	}
	return read_abs_config_file(fname, root);
}

// read config from file, without pre-processing the filename
bool Configuration::read_abs_config_file(
		const string& input_filename, const string& root) {
	filename = input_filename;

	CTRACE("Configuration::read_abs_config_file");

	clear(root);

	is_file = true;    // set to file, even if file not found

	std::unique_ptr<std::istream> pIfile;
	try {
		pIfile = U7open_in(filename.c_str(), true);
	} catch (exult_exception&) {
		// configuration file not found
		return false;
	}
	auto& ifile = *pIfile;

	if (ifile.fail()) {
		return false;
	}

	string sbuf;
	string line;
	// copies the entire contents of the input file into sbuf
	getline(ifile, line);
	while (ifile.good()) {
		sbuf += line + "\n";
		getline(ifile, line);
	}
	// empty file just do nothing
	if (sbuf.empty()) {
		CTRACE("Configuration::read_config_file - no data read");
		return false;
	}
	CTRACE("Configuration::read_config_file - file read");
	read_config_string(sbuf);

	is_file = true;
	return true;
}

string Configuration::dump() {
	return xmltree->dump();
}

ostream& Configuration::dump(ostream& o, const string& indentstr) {
	xmltree->dump(o, indentstr);
	return o;
}

void Configuration::write_back() {
	if (!is_file) {
		return;    // Don't write back if not from a file
	}

	std::unique_ptr<std::ostream> pOfile;
	try {
		pOfile = U7open_out(filename.c_str(), true);
	} catch (const file_open_exception&) {
		perror("Failed to write configuration file");
		return;
	}
	auto& ofile = *pOfile;
	if (ofile.fail()) {
		perror("Failed to write configuration file");
	}
	ofile << dump() << endl;
}

std::vector<string> Configuration::listkeys(
		const string& key, bool longformat) {
	std::vector<string> vs;
	const XMLnode*      sub = xmltree->subtree(key);
	if (sub) {
		sub->listkeys(key, vs, longformat);
	}

	return vs;
}

std::vector<string> Configuration::listkeys(const char* key, bool longformat) {
	const string s(key);
	return listkeys(s, longformat);
}

void Configuration::clear(const string& new_root) {
	CTRACE("Configuration::clear");

	delete xmltree;
	CTRACE("Configuration::clear - xmltree deleted");
	if (!new_root.empty()) {
		rootname = new_root;
	}
	CTRACE("Configuration::clear - new root specified");
	xmltree = new XMLnode(rootname);
	CTRACE("Configuration::clear - fin");
}

void Configuration::getsubkeys(KeyTypeList& ktl, const string& basekey) {
	xmltree->searchpairs(ktl, basekey, string(), 0);
}
