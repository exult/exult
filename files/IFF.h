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

#ifndef IFF_H
#define IFF_H

#include "U7file.h"
#include "common_types.h"
#include "exceptions.h"

#include <cstring>
#include <string>
#include <vector>

class DataSource;

/**
 *  The IFF class is an data reader which reads data in the IFF
 *  file format. The actual data need not be in a file, however.
 */
class IFF : public U7file {
public:
	struct IFFhdr {
		char   form_magic[4];
		uint32 size;
		char   data_type[4];
	};

	struct IFFobject {
		char   type[4];
		uint32 size;
		char   even;
	};

	struct u7IFFobj {
		char name[8];
		// char    data[]; // Variable
	};

protected:
	//  The IFF's header. ++++ Unused????
	// IFFhdr    header;
	/// List of objects in the IFF file.
	std::vector<Reference> object_list;

	void index_file() override;

	Reference get_object_reference(uint32 objnum) const override {
		return object_list[objnum];
	}

public:
	/// Basic constructor.
	/// @param spec File name and object index pair.
	explicit IFF(const File_spec& spec) : U7file(spec) {}

	size_t number_of_objects() override {
		return object_list.size();
	}

	const char* get_archive_type() override {
		return "IFF";
	}

	static bool is_iff(IDataSource* in);
	static bool is_iff(const std::string& fname);
};

using IFFFile   = U7DataFile<IFF>;
using IFFBuffer = U7DataBuffer<IFF>;

#endif
