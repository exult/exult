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

#include "sdlrwopsostream.h"

SdlRwopsOstream::SdlRwopsOstream() : std::ostream(&m_streambuf) {}

SdlRwopsOstream::SdlRwopsOstream(const char* s, std::ios_base::openmode mode) : std::ostream(&m_streambuf) {
	if (m_streambuf.open(s, mode | std::ios_base::out) == nullptr) {
		setstate(std::ios_base::failbit);
	}
}

SdlRwopsStreambuf* SdlRwopsOstream::rdbuf() const {
	return const_cast<SdlRwopsStreambuf*>(&m_streambuf);
}

bool SdlRwopsOstream::is_open() const {
	return m_streambuf.is_open();
}

void SdlRwopsOstream::open(const char* s, std::ios_base::openmode mode) {
	if (m_streambuf.open(s, mode | std::ios_base::out)) {
		clear();
	} else {
		setstate(std::ios_base::failbit);
	}
}

void SdlRwopsOstream::close() {
	if (m_streambuf.close() == nullptr) {
		setstate(std::ios_base::failbit);
	}
}
