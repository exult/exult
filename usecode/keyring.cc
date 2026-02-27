/*
 *  Copyright (C) 2001-2022  The Exult Team
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

#include "keyring.h"

#include "endianio.h"
#include "exceptions.h"
#include "fnames.h"
#include "utils.h"

#include <fstream>

using std::ifstream;
using std::ofstream;

void Keyring::read() {
	std::unique_ptr<std::istream> pIn;

	// clear keyring first
	keys.clear();

	try {
		pIn = U7open_in(KEYRINGDAT);
	} catch (exult_exception& /*e*/) {
		// maybe an old savegame, just leave the keyring empty
		return;
	}
	if (!pIn) {
		return;
	}
	auto& in = *pIn;

	do {
		const int val = little_endian::Read2(in);
		if (in.good()) {
			addkey(val);
		}
	} while (in.good());
}

void Keyring::write() {
	auto pOut = U7open_out(KEYRINGDAT);
	if (!pOut) {
		throw file_open_exception(KEYRINGDAT);
	}
	auto& out = *pOut;

	for (const int key : keys) {
		little_endian::Write2(out, key);
	}
}

void Keyring::clear() {
	keys.clear();
}

void Keyring::addkey(int qual) {
	keys.insert(qual);
}

bool Keyring::checkkey(int qual) {
	return keys.find(qual) != keys.end();
}

bool Keyring::removekey(int qual) {
	auto ent = keys.find(qual);
	if (ent == keys.end()) {
		return false;
	} else {
		keys.erase(ent);
		return true;
	}
}
