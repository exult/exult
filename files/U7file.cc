/*
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

#include "U7file.h"

#include "U7fileman.h"
#include "U7obj.h"

#include <vector>

File_data::File_data(const File_spec& spec) {
	file  = U7FileManager::get_ptr()->get_file_object(spec, true);
	patch = !spec.name.compare(1, sizeof("<PATCH>/") - 1, "<PATCH>/");
	if (file) {
		count = file->number_of_objects();
	} else {
		count = 0;
	}
}

/// Initializes from a file spec. Needed by some constructors.
/// @param spec A file specification.
U7multifile::U7multifile(const File_spec& spec) {
	files.emplace_back(spec);
}

/// Initializes from file specs. Needed by some constructors.
/// @param spec0    First file specification (usually <STATIC>).
/// @param spec1    Second file specification (usually <PATCH>).
U7multifile::U7multifile(const File_spec& spec0, const File_spec& spec1) {
	files.emplace_back(spec0);
	files.emplace_back(spec1);
}

/// Initializes from file specs. Needed by some constructors.
/// @param spec0    First file specification (usually <STATIC>).
/// @param spec1    Second file specification.
/// @param spec2    Third file specification (usually <PATCH>).
U7multifile::U7multifile(const File_spec& spec0, const File_spec& spec1, const File_spec& spec2) {
	files.emplace_back(spec0);
	files.emplace_back(spec1);
	files.emplace_back(spec2);
}

/// Initializes from file specs. Needed by some constructors.
/// @param specs    List of file specs.
U7multifile::U7multifile(const std::vector<File_spec>& specs) {
	for (const auto& spec : specs) {
		files.emplace_back(spec);
	}
}

size_t U7multifile::number_of_objects() const {
	size_t num = 0;
	for (const auto& file : files) {
		if (file.number_of_objects() > num) {
			num = file.number_of_objects();
		}
	}
	return num;
}

template <typename T>
struct reverse_wrapper {
	T& iterable;
};

template <typename T>
auto begin(reverse_wrapper<T> container) {
	return std::rbegin(container.iterable);
}

template <typename T>
auto end(reverse_wrapper<T> container) {
	return std::rend(container.iterable);
}

template <typename T>
reverse_wrapper<T> reverse(T&& iterable) {
	return reverse_wrapper<T>{std::forward<T>(iterable)};
}

std::unique_ptr<unsigned char[]> U7multifile::retrieve(uint32 objnum, std::size_t& len, bool& patch) const {
	for (const auto& file : reverse(files)) {
		auto buf = file.retrieve(objnum, len, patch);
		if (buf && len > 0) {
			return buf;
		}
	}
	// Failed to retrieve the object.
	patch = false;
	len   = 0;
	return nullptr;
}
