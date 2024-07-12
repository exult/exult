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
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <iostream>

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

char safe_tolower(char c) {
	return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

bool iequals(std::string_view lhs, std::string_view rhs) {
	return std::equal(
			lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
			[](char left, char right) {
				return safe_tolower(left) == safe_tolower(right);
			});
}

void Configuration::value(
		std::string_view key, string& ret,
		std::string_view defaultvalue) const {
	const XMLnode* sub = xmltree->subtree(key);
	if (sub != nullptr) {
		ret = sub->value();
	} else {
		ret = defaultvalue;
	}
}

void Configuration::value(
		std::string_view key, bool& ret, bool defaultvalue) const {
	const XMLnode* sub = xmltree->subtree(key);
	if (sub != nullptr) {
		ret = iequals(sub->value(), "yes");
	} else {
		ret = defaultvalue;
	}
}

void Configuration::value(
		std::string_view key, int& ret, int defaultvalue) const {
	const XMLnode* sub = xmltree->subtree(key);
	if (sub != nullptr) {
		const auto& value = sub->value();
		const auto* start = value.data();
		const auto* end   = std::next(start, value.size());
		auto [p, ec]      = std::from_chars(start, end, ret);
		if (ec != std::errc()) {
			ret = defaultvalue;
		}
	} else {
		ret = defaultvalue;
	}
}

bool Configuration::key_exists(std::string_view key) const {
	const XMLnode* sub = xmltree->subtree(key);
	return sub != nullptr;
}

void Configuration::set(
		std::string_view key, std::string_view value, bool write_out) {
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

void Configuration::set(std::string_view key, int value, bool write_out) {
	const string v = std::to_string(value);
	set(key, v, write_out);
}

void Configuration::remove(std::string_view key, bool write_out) {
	xmltree->remove(key, true);
	if (write_out) {
		write_back();
	}
}

bool Configuration::read_config_string(std::string_view s) {
	size_t nn = 0;
	while (isspace(static_cast<unsigned char>(s[nn])) != 0) {
		++nn;
	}

	assert(s[nn] == '<');
	++nn;

	xmltree->xmlparse(s, nn);
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
		const std::string& input_filename, std::string_view root) {
	CTRACE("Configuration::read_config_file");

	string fname = input_filename;
	// Don't frob the filename if it starts with a dot and
	// a slash or with two dots and a slash.
	// Or if it's not a relative path.
	if (!is_path_absolute(get_system_path(input_filename))) {
#if (defined(XWIN) || defined(MACOSX) || defined(_WIN32) \
	 || defined(__IPHONEOS__))
		fname = "<CONFIG>/";
#	if (defined(XWIN) && !defined(MACOSX) && !defined(__IPHONEOS__))
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
		if (fname != input_filename && U7exists(input_filename)
			&& get_system_path("<HOME>") != ".") {
			if (!U7exists(fname)) {
				cerr << "Warning: configuration file '" << input_filename
					 << "' is being copied to file '" << fname
					 << "' and will no longer be used." << endl;
				try {
					const size_t pos = fname.find_last_of("/\\");
					if (pos != string::npos) {
						// First, try to make the directory.
						const std::string path = fname.substr(0, pos);
						U7mkdir(path, 0755);
					}
					U7copy(input_filename, fname);
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
		std::string_view input_filename, std::string_view root) {
	filename = input_filename;

	CTRACE("Configuration::read_abs_config_file");

	clear(root);

	is_file = true;    // set to file, even if file not found

	std::unique_ptr<std::istream> pIfile;
	try {
		pIfile = U7open_in(filename, true);
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

	CTRACE("Configuration::read_config_file - file read");

	read_config_string(sbuf);

	is_file = true;
	return true;
}

string Configuration::dump() {
	return xmltree->dump();
}

ostream& Configuration::dump(ostream& o, std::string_view indentstr) {
	xmltree->dump(o, indentstr);
	return o;
}

void Configuration::write_back() {
	if (!is_file) {
		return;    // Don't write back if not from a file
	}

	std::unique_ptr<std::ostream> pOfile;
	try {
		pOfile = U7open_out(filename, true);
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
		std::string_view key, bool longformat) const {
	std::vector<string> vs;
	if (const XMLnode* sub = xmltree->subtree(key); sub != nullptr) {
		sub->listkeys(key, vs, longformat);
	}

	return vs;
}

void Configuration::clear(std::string_view new_root) {
	CTRACE("Configuration::clear");

	xmltree.reset();
	CTRACE("Configuration::clear - xmltree deleted");
	if (!new_root.empty()) {
		rootname = new_root;
	}
	CTRACE("Configuration::clear - new root specified");
	xmltree = std::make_unique<XMLnode>(rootname);
	CTRACE("Configuration::clear - fin");
}

void Configuration::getsubkeys(KeyTypeList& ktl, std::string_view basekey) {
	xmltree->searchpairs(ktl, basekey, "", 0);
}
