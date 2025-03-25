/*
 *  utils.h - Common utility routines.
 *
 *  Copyright (C) 1998-1999  Jeffrey S. Freedman
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

#ifndef UTILS_H
#define UTILS_H

#include "common_types.h"

#include <dirent.h>

#include <cstring>
#include <functional>
#include <iosfwd>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

#ifndef ATTR_PRINTF
#	ifdef __GNUC__
#		define ATTR_PRINTF(x, y) __attribute__((format(printf, (x), (y))))
#	else
#		define ATTR_PRINTF(x, y)
#	endif
#endif

inline int ReadInt(std::istream& in, int def = 0) {
	int num;
	if (in.eof()) {
		return def;
	}
	in >> num;
	if (in.fail()) {
		return def;
	}
	in.ignore(std::numeric_limits<std::streamsize>::max(), '/');
	return num;
}

inline unsigned int ReadUInt(std::istream& in, int def = 0) {
	unsigned int num;
	if (in.eof()) {
		return def;
	}
	in >> num;
	if (in.fail()) {
		return def;
	}
	in.ignore(std::numeric_limits<std::streamsize>::max(), '/');
	return num;
}

inline void WriteInt(std::ostream& out, int num, bool final = false) {
	out << num;
	if (final) {
		out << std::endl;
	} else {
		out << '/';
	}
}

inline void WriteInt(std::ostream& out, unsigned int num, bool final = false) {
	out << num;
	if (final) {
		out << std::endl;
	} else {
		out << '/';
	}
}

inline std::string ReadStr(char*& eptr, int off = 1) {
	eptr += off;
	char*       pos = std::strchr(eptr, '/');
	std::string retval(eptr, pos - eptr);
	eptr = pos;
	return retval;
}

inline std::string ReadStr(std::istream& in) {
	std::string retval;
	std::getline(in, retval, '/');
	return retval;
}

inline void WriteStr(
		std::ostream& out, const std::string& str, bool final = false) {
	out << str;
	if (final) {
		out << std::endl;
	} else {
		out << '/';
	}
}

/*
 *  Get file size without undefined behavior.
 */
inline size_t get_file_size(std::istream& in) {
	const auto start = in.tellg();
	in.seekg(0);
	in.ignore(std::numeric_limits<std::streamsize>::max());
	const size_t len = in.gcount();
	in.seekg(start);
	return len;
}

// Sets factories for creating istreams/ostreams.  Intended to be called once
// during initialization before using any U7open...() calls and is not
// guaranteed to be thread-safe.
using U7IstreamFactory = std::function<std::unique_ptr<std::istream>(
		const char* s, std::ios_base::openmode mode)>;
using U7OstreamFactory = std::function<std::unique_ptr<std::ostream>(
		const char* s, std::ios_base::openmode mode)>;
void U7set_istream_factory(U7IstreamFactory factory);
void U7set_ostream_factory(U7OstreamFactory factory);

// Manually sets the home directory rather than trying to infer it from the
// environment. Intended to be called once during initialization before using
// U7open...() calls and is not guaranteed to be thread-safe.
void U7set_home(std::string home);

std::unique_ptr<std::istream> U7open_in(
		const char* fname,             // May be converted to upper-case.
		bool        is_text = false    // Should the file be opened in text mode
);

std::unique_ptr<std::ostream> U7open_out(
		const char* fname,             // May be converted to upper-case.
		bool        is_text = false    // Should the file be opened in text mode
);

std::unique_ptr<std::istream> U7open_static(
		const char* fname,     // May be converted to upper-case.
		bool        is_text    // Should file be opened in text mode
);
DIR* U7opendir(const char* fname    // May be converted to upper-case.
);
void U7remove(const char* fname);

bool U7exists(const char* fname);

inline bool U7exists(const std::string& fname) {
	return U7exists(fname.c_str());
}

int U7mkdir(const char* dirname, int mode);

#ifdef _WIN32
void redirect_output(const char* prefix = "std");
void cleanup_output(const char* prefix = "std");
#endif
void setup_data_dir(const std::string& data_path, const char* runpath);
void setup_program_paths();
#if defined(MACOSX) || defined(SDL_PLATFORM_IOS)
void setup_app_bundle_resource();
#endif

int U7chdir(const char* dirname);

void U7copy(const char* src, const char* dest);

bool is_system_path_defined(const std::string& path);
void store_system_paths();
void reset_system_paths();
void clear_system_path(const std::string& key);
void add_system_path(const std::string& key, const std::string& value);
void clone_system_path(const std::string& new_key, const std::string& old_key);
std::string get_system_path(const std::string& path);

#define BUNDLE_CHECK(x, y) \
	((is_system_path_defined("<BUNDLE>") && U7exists((x))) ? (x) : (y))

void        to_uppercase(std::string& str);
std::string to_uppercase(const std::string& str);

int    Log2(uint32 n);
uint32 msb32(uint32 x);
int    fgepow2(uint32 n);

char* newstrdup(const char* s);
char* Get_mapped_name(const char* from, int num, char* to);
int   Find_next_map(int start, int maxtry);

#endif /* _UTILS_H_ */
