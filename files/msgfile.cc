/**
 ** Msgfile.cc - Read in text message file.
 **
 ** Written: 6/25/03
 **/

/*
 *  Copyright (C) 2002-2013  The Exult Team
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

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>
#include <iomanip>
#include <cstdlib>
#include "utils.h"
#include "databuf.h"
#include "ios_state.hpp"

using std::stringstream;
using std::istream;
using std::ostream;
using std::cerr;
using std::endl;
using std::hex;
using std::vector;
using std::string;

/*
 *  Read in text, where each line is of the form "nnn:sssss", where nnn is
 *  to be the Flex entry #, and anything after the ':' is the string to
 *  store.
 *  NOTES:  Entry #'s may be skipped, and may be given in hex (0xnnn)
 *          or decimal.
 *      Max. text length is 1024.
 *      A line beginning with a '#' is a comment.
 *      A 'section' can be marked:
 *          %%section shapes
 *              ....
 *          %%endsection
 *  Output: # of first message (i.e., lowest-numbered msg), or -1 if
 *      error.
 */

int Read_text_msg_file(IDataSource *in, vector<string> &strings,
                       const char *section) {
	strings.resize(0);      // Initialize.
	strings.reserve(1000);
	int linenum = 0;
#define NONEFOUND 0xffffffff
	unsigned long first = NONEFOUND;// Index of first one found.
	long next_index = 0;// For auto-indexing of lines
	static const string sectionStart("%%section");
	static const string sectionEnd("%%endsection");
	while (!in->eof()) {
		++linenum;
		std::string line;
		in->readline(line);
		if (line.empty())
			continue;   // Empty line.

		if (section) {
			if (line.compare(0, sectionStart.length(), sectionStart))
				continue;
			const string sectionName(line.substr(line.find_first_not_of(" \t\b", sectionStart.length())));
			if (sectionName == section) {
				// Found the section.
				section = nullptr;
				continue;
			}
			cerr << "Line #" << linenum <<
			     " has the wrong section name: " << sectionName << " != " << section << endl;
			return -1;
		}
		if (!line.compare(0, sectionEnd.length(), sectionEnd))
			break;

		unsigned long index;
		string lineVal;
		if (line[0] == ':') {
			// Auto-index lines missing an index.
			index = next_index++;
			lineVal = line.substr(1);
		} else if (line[0] == '#') {
			continue;
		} else {
			char *endptr = &line[0];
			const char *ptr = endptr;
			// Get line# in decimal, hex, or oct.
			index = strtol(ptr, &endptr, 0);
			if (endptr == ptr) { // No #?
				cerr << "Line " << linenum <<
				     " doesn't start with a number" << endl;
				return -1;
			}
			if (*endptr != ':') {
				cerr << "Missing ':' in line " << linenum <<
				     ".  Ignoring line" << endl;
				continue;
			}
			lineVal = line.substr(endptr - ptr + 1);
		}
		if (index >= strings.size())
			strings.resize(index + 1);
		strings[index] = lineVal;
		if (index < first)
			first = index;
	}
	return first == NONEFOUND ? -1 : static_cast<int>(first);
}

/*
 *  Searches for the start of section in a text msg file.
 *  Returns true if section is found. The data source will
 *  be just before the section start.
 */

bool Search_text_msg_section(IDataSource *in, const char *section) {
	static const string sectionStart("%%section");
	while (!in->eof()) {
		std::string line;
		size_t pos = in->getPos();
		in->readline(line);
		if (line.empty())
			continue;   // Empty line.

		if (line.compare(0, sectionStart.length(), sectionStart))
			continue;
		const string sectionName(line.substr(line.find_first_not_of(" \t\b", sectionStart.length())));
		if (sectionName == section) {
			// Found the section.
			// Seek to just before it.
			in->seek(pos);
			return true;
		}
	}
	in->clear_error();
	in->seek(0);
	// Section was not found.
	return false;
}


int Read_text_msg_file(
    istream &in,
    vector<string> &strings,    // Strings returned here, each
    //   allocated on heap.
    const char *section         // Section name, or nullptr.  If given
    //   the section must be next infile.
) {
	IStreamDataSource ds(&in);
	return Read_text_msg_file(&ds, strings, section);
}

int Read_text_msg_file_sections(
    IDataSource *in,
    vector<vector<string> > &strings,   // Strings returned here
    const char *sections[],         // Section names
    int numsections
) {
	strings.resize(numsections);
	int version = 1;

	vector<string> versioninfo;
	// Read version.
	const char *versionstr = "version";
	if (Search_text_msg_section(in, versionstr) &&
	        Read_text_msg_file(in, versioninfo, versionstr) != -1) {
		version = static_cast<int>(strtol(versioninfo[0].c_str(), nullptr, 0));
	}

	for (int i = 0; i < numsections; i++) {
		in->clear_error();
		in->seek(0);
		if (!Search_text_msg_section(in, sections[i]))
			continue;
		Read_text_msg_file(in, strings[i], sections[i]);
	}

	return version;
}

int Read_text_msg_file_sections(
    istream &in,
    vector<vector<string> > &strings,   // Strings returned here
    const char *sections[],         // Section names
    int numsections
) {

	IStreamDataSource ds(&in);
	return Read_text_msg_file_sections(&ds, strings, sections, numsections);
}

/*
 *  Write one section.
 */

void Write_msg_file_section(
    ostream &out,
    const char *section,
    vector<string> &items
) {
	boost::io::ios_flags_saver flags(out);
	out << "%%section " << section << hex << endl;
	for (unsigned i = 0; i < items.size(); ++i)
		if (!items[i].empty())
			out << "0x" << i << ':' << items[i] << endl;
	out << "%%endsection " << section << endl;
}
