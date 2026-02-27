/*
 *  Copyright (C) 2000-2022  The Exult Team
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

#ifndef TABLE_H
#define TABLE_H

#include "U7file.h"
#include "common_types.h"
#include "exceptions.h"

#include <string>
#include <vector>

class DataSource;

/**
 *  The Table class is an data reader which reads data in the table
 *  file format. The actual data need not be in a file, however.
 */
class Table : public U7file {
protected:
	/// List of objects in the table file.
	std::vector<Reference> object_list;

	void index_file() override;

	Reference get_object_reference(uint32 objnum) const override {
		return object_list[objnum];
	}

public:
	/// Basic constructor.
	/// @param spec File name and object index pair.
	explicit Table(const File_spec& spec) : U7file(spec) {}

	size_t number_of_objects() override {
		return object_list.size();
	}

	const char* get_archive_type() override {
		return "TABLE";
	}

	static bool is_table(IDataSource* in);
	static bool is_table(const std::string& fname);
};

using TableFile   = U7DataFile<Table>;
using TableBuffer = U7DataBuffer<Table>;

#endif
