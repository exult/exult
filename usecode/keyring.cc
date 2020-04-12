/*
 *  Copyright (C) 2001  The Exult Team
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

#include <fstream>

#include "keyring.h"
#include "utils.h"
#include "fnames.h"
#include "exceptions.h"

using std::ifstream;
using std::ofstream;

void Keyring::read() {
	ifstream in;

	// clear keyring first
	keys.clear();

	try {
		U7open(in, KEYRINGDAT);
	} catch (exult_exception &/*e*/) {
		// maybe an old savegame, just leave the keyring empty
		return;
	}

	do {
		int val = Read2(in);
		if (in.good())
			addkey(val);
	} while (in.good());

	in.close();
}

void Keyring::write() {
	ofstream out;

	U7open(out, KEYRINGDAT);

	std::set<int>::iterator iter;

	for (iter = keys.begin(); iter != keys.end(); ++iter)
		Write2(out, *iter);

	out.close();
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
	std::set<int>::iterator ent = keys.find(qual);
	if (ent == keys.end())
		return false;
	else {
		keys.erase(ent);
		return true;
	}
}
