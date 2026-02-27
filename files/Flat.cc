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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "Flat.h"

#include "databuf.h"
#include "utils.h"

#include <fstream>

using std::ifstream;
using std::size_t;

/**
 *  Verifies if a datasource is a FLAT file.
 *  @param in   DataSource containing the data we wish to investigate.
 *  @return If the datasource is non-null and good, true; false otherwise.
 */
bool Flat::is_flat(IDataSource* in) {
	return in && in->good();
}

/**
 *  Verifies if a file is a FLAT file.
 *  @param fname    File name we wish to investigate.
 *  @return If the file exists, true; false otherwise.
 */
bool Flat::is_flat(const std::string& fname) {
	return U7exists(fname.c_str());
}
