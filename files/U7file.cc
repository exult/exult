/*
Copyright (C) 2000  Dancer A.L Vesperman

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

// Not yet.
#if 0
#include "U7file.h"
#include "U7fileman.h"
#include "U7obj.h"
#include <vector>

File_data::File_data(const File_spec &spec) {
	file = U7FileManager::get_ptr()->get_file_object(spec, true);
	patch = !spec.name.compare(1, sizeof("<PATCH>/") - 1, "<PATCH>/");
	if (file)
		cnt = file->number_of_objects();
	else
		cnt = -1;
}

/// Initializes from a file spec. Needed by some constructors.
/// @param spec A file specification.
U7multifile::U7multifile(const File_spec &spec) {
	files.push_back(File_data(spec));
}

/// Initializes from file specs. Needed by some constructors.
/// @param spec0    First file specification (usually <STATIC>).
/// @param spec1    Second file specification (usually <PATCH>).
U7multifile::U7multifile(const File_spec &spec0, const File_spec &spec1) {
	files.push_back(File_data(spec0));
	files.push_back(File_data(spec1));
}

/// Initializes from file specs. Needed by some constructors.
/// @param spec0    First file specification (usually <STATIC>).
/// @param spec1    Second file specification.
/// @param spec2    Third file specification (usually <PATCH>).
U7multifile::U7multifile(const File_spec &spec0, const File_spec &spec1,
                         const File_spec &spec2) {
	files.push_back(File_data(spec0));
	files.push_back(File_data(spec1));
	files.push_back(File_data(spec2));
}

/// Initializes from file specs. Needed by some constructors.
/// @param specs    List of file specs.
U7multifile::U7multifile(const std::vector<File_spec> &specs) {
	for (std::vector<File_spec>::const_iterator it = specs.begin();
	        it != specs.end(); ++it)
		files.push_back(File_data(*it));
}

uint32 U7multifile::number_of_objects() const {
	int num = 0;
	for (std::vector<File_data>::const_iterator it = files.begin();
	        it != specs.end(); ++it)
		if (it->number_of_objects() > num)
			num = it->number_of_objects();
	return num;
}

char *U7multifile::retrieve(uint32 objnum, std::size_t &len, bool &patch) const {
	char *buf;
	for (std::vector<File_data>::const_reverse_iterator it = files.rbegin();
	        it != specs.rend(); ++it) {
		buf = it->retrieve(objnum, len, patch);
		if (buf && len > 0)
			return buf;
		else if (buf)
			delete [] buf;
	}
	// Failed to retrieve the object.
	patch = false;
	len = 0;
	return 0;
}
#endif
