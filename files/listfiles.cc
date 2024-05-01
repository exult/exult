/*
Copyright (C) 2001-2022 The Exult Team

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

#include <type_traits>
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "listfiles.h"
#include "utils.h"

#include <cstring>
#include <iostream>
#include <string>

// Need iOS >= 13 and NDK >= 22 for std::filesystem
#include <filesystem>
#include <regex>

using std::string;

// TODO: If SDL ever adds directory traversal to rwops, update U7ListFiles() to
// use it.

namespace fs      = std::filesystem;
using string_type = fs::path::string_type;
using value_type  = fs::path::value_type;
using regex_t     = std::basic_regex<value_type>;

static std::error_code U7ListFilesImp(
		const fs::path& source, const regex_t& regex, FileList& files) {
	// In here, we convert the path directly to a std::string_view if it was a
	// std::string, or we reinterpret the data as a char* if it was a
	// std::u8string. This way, we have a unified way of dealing with the result
	// independently of what the result is, which will depend on whether we are
	// in c++17, or a later standard.
	auto get_string_view_from_path = [](const auto& fname) -> std::string_view {
		if constexpr (std::is_same_v<std::string, decltype(fname)>) {
			return {fname};
		} else {
			const auto* data = reinterpret_cast<const char*>(fname.data());
			return {data, fname.size()};
		}
	};

	if (!fs::exists(source)) {
		std::cerr << "Directory does not exist: " << source << std::endl;
		return std::make_error_code(std::errc::no_such_file_or_directory);
	}

	std::error_code result{};
	for (const auto& entry : fs::directory_iterator(source, result)) {
		if (!entry.is_regular_file() && !entry.is_symlink()) {
			continue;
		}
		const fs::path& path = entry.path();
		const auto      file = path.filename().native();
		if (std::regex_match(file, regex)) {
			files.emplace_back(
					get_string_view_from_path(path.generic_u8string()));
		}
	}
	if (result != std::error_code{}) {
		std::cerr << "Error while listing files: " << result.message()
				  << std::endl;
	}
	return result;
}

int U7ListFiles(
		const std::string& directory, const std::string& mask,
		FileList& files) {
	// In here, we assume that we have a UTF-8-encoded string, and we convert it
	// to the native string type of the filesystem.
	auto convert_mask = [](auto& value) -> string_type {
		if constexpr (std::is_same_v<string_type, std::string>) {
			return value;
		} else {
			// TODO: This will need fixing in c++20 as fs::u8path is deprecated.
			// Moreover, the addition of char8_t and u8string will complicate
			// some things and simplify others.
			return fs::u8path(value).native();
		}
	};
	fs::path        path(fs::u8path(get_system_path(directory)));
	regex_t         regex(convert_mask(mask));
	std::error_code result = U7ListFilesImp(path, regex, files);
#ifdef ANDROID
	// TODO: If SDL ever adds directory traversal to rwops use it instead so
	// that we pick up platform-specific paths and behaviors like this.
	if (result != std::error_code{}) {
		result = U7ListFilesImp(
				fs::u8path(SDL_AndroidGetInternalStoragePath()) / path, regex,
				files);
	}
#endif
	return result.value();
}
