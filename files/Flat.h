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

#ifndef FLAT_H
#define FLAT_H

#include "U7file.h"
#include "common_types.h"
#include "exceptions.h"

#include <string>

class DataSource;

/**
 *  This is a "flat" file, which contains but a single object.
 *  Strictly speaking, the Flat class, as its base, has an
 *  undefined data origin, and may well be a buffer in memory.
 */
class Flat : public U7file {
protected:
	Reference get_object_reference(uint32) const override {
		return Reference{0, data->getSize()};
	}

public:
	/// Basic constructor.
	/// @param spec File name and object index pair.
	explicit Flat(const File_spec& spec) : U7file(spec) {}

	size_t number_of_objects() override {
		return 1;
	}

	const char* get_archive_type() override {
		return "FLAT";
	}

	static bool is_flat(IDataSource* in);
	static bool is_flat(const std::string& fname);
};

using FlatFile   = U7DataFile<Flat>;
using FlatBuffer = U7DataBuffer<Flat>;

#endif
