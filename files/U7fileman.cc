/*
 *  Copyright (C) 2008-2022  The Exult Team
 *
 *  Based on code by Dancer A.L Vesperman
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

#include "U7fileman.h"

#include "Flat.h"
#include "Flex.h"
#include "IFF.h"
#include "Table.h"
#include "U7file.h"
#include "U7obj.h"

using std::make_unique;
using std::string;

static U7FileManager filemanager;
U7FileManager*       U7FileManager::self = &filemanager;

/**
 *  Searches for the desired "file". If it is not already open,
 *  it is opened by the creation of an appropriate reader class.
 *  @param s    Unique identifier.
 *  @param allow_errors If false, this function will throw an
 *  exception if the file cannot be found. If true, it will return
 *  a null file pointer instead.
 *  @return Pointer to data reading class.
 */
U7file* U7FileManager::get_file_object(const File_spec& s, bool allow_errors) {
	if (file_list.count(s) != 0) {
		return file_list[s].get();
	}
	// Not in our cache. Attempt to figure it out.
	std::unique_ptr<U7file> uf;
	if (s.index >= 0) {
		auto data = make_unique<IExultDataSource>(s.name, s.index);
		if (data->good()) {
			if (IFF::is_iff(data.get())) {
				uf = std::make_unique<IFFBuffer>(s, std::move(data));
			} else if (Flex::is_flex(data.get())) {
				uf = std::make_unique<FlexBuffer>(s, std::move(data));
			} else if (Table::is_table(data.get())) {
				uf = std::make_unique<TableBuffer>(s, std::move(data));
			} else if (Flat::is_flat(data.get())) {
				uf = std::make_unique<FlatBuffer>(s, std::move(data));
			}
		}
	} else {
		if (IFF::is_iff(s.name)) {
			uf = std::make_unique<IFFFile>(s.name);
		} else if (Flex::is_flex(s.name)) {
			uf = std::make_unique<FlexFile>(s.name);
		} else if (Table::is_table(s.name)) {
			uf = std::make_unique<TableFile>(s.name);
		} else if (Flat::is_flat(s.name)) {
			uf = std::make_unique<FlatFile>(s.name);
		}
	}

	// Failed
	if (uf == nullptr) {
		if (!allow_errors) {
			throw file_open_exception(s.name);
		}
		return nullptr;
	}

	U7file* pt   = uf.get();
	file_list[s] = std::move(uf);
	return pt;
}

/**
 *  Cleans the whole file list.
 */
void U7FileManager::reset() {
	file_list.clear();
}
