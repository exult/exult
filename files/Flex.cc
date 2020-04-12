/*
 *  Copyright (C) 2000-2013  The Exult Team
 *
 *  Original file by Dancer A.L Vesperman
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

#define DEBUGFLEX 0

#include "Flex.h"

#include <fstream>
#if DEBUGFLEX
#	include <iostream>
#endif
#include "exceptions.h"
#include "utils.h"
#include "databuf.h"

using std::size_t;
using std::string;

bool Flex_header::read(IDataSource* in) {
	in->read(title, FLEX_TITLE_LEN);
	magic1 = in->read4();
	count = in->read4();
	magic2 = in->read4();
	for (uint32& elem : padding) {
		elem = in->read4();
	}
	return magic1 == FLEX_MAGIC1;
}

/**
 *  Write out a FLEX header.  Note that this is a STATIC method.
 *  @param out  DataSource to which the data is written.
 *  @param title    Flex file title.
 *  @param count    Number of entries in the flex.
 *  @param vers Version of the flex file.
 */

void Flex_header::write(
    ODataSource *out,            // File to write to.
    const char *title,
    size_t count,           // # entries.
    Flex_header::Flex_vers vers
) {
	string titlebuf(title);        // Use savename for title.
	titlebuf.resize(FLEX_TITLE_LEN, '\0');
	out->write(titlebuf);
	out->write4(FLEX_MAGIC1);    // Magic number.
	out->write4(static_cast<uint32>(count));
	if (vers == orig) {       // 2nd magic #:  use for version.
		out->write4(FLEX_MAGIC2);
	} else {
		out->write4(EXULT_FLEX_MAGIC2 + static_cast<uint32>(vers));
	}
	for (size_t ii = 0; ii < FLEX_HEADER_PADDING + 2 * count; ii++) {
		out->write4(0);
	}
}

/**
 *  Verify if a file is a FLEX.  Note that this is a STATIC method.
 *  @param in   DataSource to verify.
 *  @return Whether or not the DataSource is a FLEX file.
 */
bool Flex_header::is_flex(IDataSource *in) {
	size_t pos = in->getPos();        // Fill to data (past table at 0x80).
	size_t len = in->getSize();   // Check length.
	uint32 magic = 0;
	if (len >= FLEX_HEADER_LEN) {      // Has to be at least this long.
		in->seek(FLEX_TITLE_LEN);
		magic = in->read4();
	}
	in->seek(pos);

	// Is a flex?
	return magic == Flex_header::FLEX_MAGIC1;
}

/**
 *  Reads the header from a flex and builds an object index.
 */
void Flex::index_file() {
	if (!data) {
		throw file_read_exception(identifier.name);
	}
	start_pos = data->getPos();
	if (!hdr.read(data.get())) {
		// Not a flex file.
		throw wrong_file_type_exception(identifier.name, "FLEX");
	}
#if DEBUGFLEX
	cout << "Title: " << hdr.title << endl;
	cout << "Count: " << hdr.count << endl;
#endif

	// We should already be there.
	data->seek(start_pos + Flex_header::FLEX_HEADER_LEN);
	for (uint32 c = 0; c < hdr.count; c++) {
		Reference f;
		f.offset = data->read4() + start_pos;
		f.size = data->read4();
#if DEBUGFLEX
		cout << "Item " << c << ": " << f.size << " bytes @ " << f.offset << endl;
#endif
		object_list.push_back(f);
	}
}

/**
 *  Obtains information about an object from the flex.
 *  @param objnum   Number of object.
 *  @param len  Receives the length of the object, or zero in any failure.
 *  @return Offset of object from the beginning of the file.
 */
size_t Flex::get_entry_info(uint32 objnum, size_t &len) {
	if (!data || objnum >= object_list.size()) {
		len = 0;
		return 0;
	}
	len = object_list[objnum].size;
	return object_list[objnum].offset;
}

/**
 *  Verify if a file is a FLEX.  Note that this is a STATIC method.
 *  @param fname    Name of file to verify.
 *  @return Whether or not the file is a FLEX file. Returns false if
 *  the file does not exist.
 */
bool Flex::is_flex(const std::string& fname) {
	IFileDataSource ds(fname.c_str());
	return ds.good() && is_flex(&ds);
}

/**
 *  Start writing out a new Flex file.
 */
Flex_writer::Flex_writer(
    OStreamDataSource& o,        ///< Where to write.
    const char *title,          ///< Flex title.
    size_t cnt,             ///< Number of entries we'll write.
    Flex_header::Flex_vers vers    ///< Version of flex file.
) : dout(o), count(cnt), start_pos(dout.getPos()) {
	// Write out header.
	Flex_header::write(&dout, title, count, vers);
	// Create table.
	table = std::make_unique<uint8[]>(2 * count * 4);
	tptr = table.get();
	cur_start = dout.getPos(); // Store start of 1st entry.
}

/**
 *  Clean up.
 */

Flex_writer::~Flex_writer(
) {
	flush();
}

void Flex_writer::flush(
) {
	if (table) {
		dout.seek(Flex_header::FLEX_HEADER_LEN);       // Write table.
		dout.write(table.get(), 2 * count * 4);
		dout.flush();
		table.reset();
	}
}

/**
 *  Call this when done writing out a section.
 */

void Flex_writer::finish_object(
) {
	// Location past end of section.
	size_t pos = dout.getPos();
	Write4(tptr, static_cast<uint32>(cur_start - start_pos));   // Store start of section.
	Write4(tptr, static_cast<uint32>(pos - cur_start)); // Store length.
	cur_start = pos;
}

