/*
 *  Copyright (C) 2002-2022  The Exult Team
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

#include "common_types.h"
#include "endianio.h"

#include <cassert>
#include <iostream>
#include <sstream>

using std::cout;
using std::endl;
using std::istringstream;

const char* ss_data = "A BC DE FGHI JKLM NO\0";

int main() {
	istringstream iss(ss_data);

	const uint8 outread1 = Read1(iss);
	cout << static_cast<char>(outread1) << endl;
	assert(static_cast<char>(outread1) == 'A');

	assert(static_cast<char>(Read1(iss)) == ' ');

	const uint16 outread2 = little_endian::Read2(iss);
	cout << static_cast<char>(outread2 & 0xff) << static_cast<char>((outread2 >> 8) & 0xff) << endl;
	assert(static_cast<char>(outread2 & 0xff) == 'B');
	assert(static_cast<char>((outread2 >> 8) & 0xff) == 'C');

	assert(static_cast<char>(Read1(iss)) == ' ');

	const uint16 outread2high = big_endian::Read2(iss);
	cout << static_cast<char>((outread2high >> 8) & 0xff) << static_cast<char>(outread2high & 0xff) << endl;
	assert(static_cast<char>(outread2high & 0xff) == 'E');
	assert(static_cast<char>((outread2high >> 8) & 0xff) == 'D');

	assert(static_cast<char>(Read1(iss)) == ' ');

	const uint32 outread4 = little_endian::Read4(iss);
	cout << static_cast<char>(outread4 & 0xff) << static_cast<char>((outread4 >> 8) & 0xff)
		 << static_cast<char>((outread4 >> 16) & 0xff) << static_cast<char>((outread4 >> 24) & 0xff) << endl;
	assert(static_cast<char>(outread4 & 0xff) == 'F');
	assert(static_cast<char>((outread4 >> 8) & 0xff) == 'G');
	assert(static_cast<char>((outread4 >> 16) & 0xff) == 'H');
	assert(static_cast<char>((outread4 >> 24) & 0xff) == 'I');

	assert(static_cast<char>(Read1(iss)) == ' ');

	const uint32 outread4high = big_endian::Read4(iss);
	cout << static_cast<char>((outread4high >> 24) & 0xff) << static_cast<char>((outread4high >> 16) & 0xff)
		 << static_cast<char>((outread4high >> 8) & 0xff) << static_cast<char>(outread4high & 0xff) << endl;
	assert(static_cast<char>(outread4high & 0xff) == 'M');
	assert(static_cast<char>((outread4high >> 8) & 0xff) == 'L');
	assert(static_cast<char>((outread4high >> 16) & 0xff) == 'K');
	assert(static_cast<char>((outread4high >> 24) & 0xff) == 'J');

	// speedtest();

	return 0;
}
