/*
 *  Copyright (C) 2021-2022  The Exult Team
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

#ifndef SDLRWOPSISTREAM_H_
#define SDLRWOPSISTREAM_H_

#include "sdlrwopsstreambuf.h"

#include <istream>

class SdlRwopsIstream : public std::istream {
public:
	typedef typename traits_type::int_type int_type;
	typedef typename traits_type::pos_type pos_type;
	typedef typename traits_type::off_type off_type;

	SdlRwopsIstream();
	explicit SdlRwopsIstream(const char* s, std::ios_base::openmode mode = std::ios_base::in);

	SdlRwopsStreambuf* rdbuf() const;
	bool               is_open() const;
	void               open(const char* s, std::ios_base::openmode mode = std::ios_base::in);
	void               close();

private:
	SdlRwopsStreambuf m_streambuf;
};

#endif    // SDLRWOPSISTREAM_H_
