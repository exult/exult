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

#include "sdlrwopsstreambuf.h"

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include <cstring>
#include <memory>

// Partly derived from libc++'s basic_filebuf
// https://github.com/llvm-mirror/libcxx/blob/master/include/fstream
//   Part of the LLVM Project, under the Apache License v2.0 with LLVM
//   Exceptions. See https://llvm.org/LICENSE.txt for license information.
//   SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

constexpr static auto NO_OPENMODE = static_cast<std::ios_base::openmode>(0);

SdlRwopsStreambuf::SdlRwopsStreambuf()
		: m_context(nullptr), m_openMode(NO_OPENMODE),
		  m_currentMode(NO_OPENMODE) {
	setg(nullptr, nullptr, nullptr);
	setp(nullptr, nullptr);
}

SdlRwopsStreambuf::~SdlRwopsStreambuf() {
	try {
		close();
	} catch (...) {
	}
}

bool SdlRwopsStreambuf::is_open() const {
	return m_context;
}

SdlRwopsStreambuf* SdlRwopsStreambuf::open(
		const char* s, std::ios_base::openmode mode) {
	if (m_context) {
		return nullptr;
	}

	const char* mdstr   = nullptr;
	const int   modeval = mode & ~std::ios_base::ate;
	switch (modeval) {
	case std::ios_base::out:
	case std::ios_base::out | std::ios_base::trunc:
		mdstr = "w";
		break;
	case std::ios_base::out | std::ios_base::app:
	case std::ios_base::app:
		mdstr = "a";
		break;
	case std::ios_base::in:
		mdstr = "r";
		break;
	case std::ios_base::in | std::ios_base::out:
		mdstr = "r+";
		break;
	case std::ios_base::in | std::ios_base::out | std::ios_base::trunc:
		mdstr = "w+";
		break;
	case std::ios_base::in | std::ios_base::out | std::ios_base::app:
	case std::ios_base::in | std::ios_base::app:
		mdstr = "a+";
		break;
	case std::ios_base::out | std::ios_base::binary:
	case std::ios_base::out | std::ios_base::trunc | std::ios_base::binary:
		mdstr = "wb";
		break;
	case std::ios_base::out | std::ios_base::app | std::ios_base::binary:
	case std::ios_base::app | std::ios_base::binary:
		mdstr = "ab";
		break;
	case std::ios_base::in | std::ios_base::binary:
		mdstr = "rb";
		break;
	case std::ios_base::in | std::ios_base::out | std::ios_base::binary:
		mdstr = "r+b";
		break;
	case std::ios_base::in | std::ios_base::out | std::ios_base::trunc
			| std::ios_base::binary:
		mdstr = "w+b";
		break;
	case std::ios_base::in | std::ios_base::out | std::ios_base::app
			| std::ios_base::binary:
	case std::ios_base::in | std::ios_base::app | std::ios_base::binary:
		mdstr = "a+b";
		break;
	default:
		return nullptr;
	}

	m_context = SDL_IOFromFile(s, mdstr);
	if (!m_context) {
		return nullptr;
	}
	m_openMode = mode;
	if (mode & std::ios_base::ate) {
		if (SDL_SeekIO(m_context, 0, SDL_IO_SEEK_END) == -1) {
			SDL_CloseIO(m_context);
			m_context = nullptr;
			return nullptr;
		}
	}
	return this;
}

SdlRwopsStreambuf* SdlRwopsStreambuf::close() {
	if (!m_context) {
		return nullptr;
	}

	std::unique_ptr<SDL_IOStream, bool (*)(SDL_IOStream*)> h(
			m_context, SDL_CloseIO);
	auto rt = this;
	if (sync()) {
		rt = nullptr;
	}
	if (SDL_CloseIO(h.release())) {
		rt = nullptr;
	}
	m_context = nullptr;
	setg(nullptr, nullptr, nullptr);
	setp(nullptr, nullptr);

	return rt;
}

typename SdlRwopsStreambuf::int_type SdlRwopsStreambuf::underflow() {
	if (!m_context) {
		return traits_type::eof();
	}

	bool initial = false;
	if (!(m_currentMode & std::ios_base::in)) {
		setp(nullptr, nullptr);
		setg(m_buffer, m_buffer + sizeof(m_buffer),
			 m_buffer + sizeof(m_buffer));
		m_currentMode = std::ios_base::in;
		initial       = true;
	}

	char_type buf1;
	if (gptr() == nullptr) {
		setg(&buf1, &buf1 + 1, &buf1 + 1);
	}
	const std::size_t unget_sz
			= initial ? 0 : std::min<std::size_t>((egptr() - eback()) / 2, 4);
	int_type c = traits_type::eof();
	if (gptr() == egptr()) {
		std::memmove(eback(), egptr() - unget_sz, unget_sz * sizeof(char_type));
		std::size_t nmemb = egptr() - eback() - unget_sz;
		nmemb             = SDL_ReadIO(m_context, eback() + unget_sz, nmemb);
		if (nmemb != 0) {
			setg(eback(), eback() + unget_sz, eback() + unget_sz + nmemb);
			c = traits_type::to_int_type(*gptr());
		}
	} else {
		c = traits_type::to_int_type(*gptr());
	}
	if (eback() == &buf1) {
		setg(nullptr, nullptr, nullptr);
	}
	return c;
}

typename SdlRwopsStreambuf::int_type SdlRwopsStreambuf::pbackfail(int_type c) {
	if (m_context && eback() < gptr()) {
		if (traits_type::eq_int_type(c, traits_type::eof())) {
			gbump(-1);
			return traits_type::not_eof(c);
		}
		if ((m_openMode & std::ios_base::out)
			|| traits_type::eq(traits_type::to_char_type(c), gptr()[-1])) {
			gbump(-1);
			*gptr() = traits_type::to_char_type(c);
			return c;
		}
	}
	return traits_type::eof();
}

typename SdlRwopsStreambuf::int_type SdlRwopsStreambuf::overflow(int_type c) {
	if (!m_context) {
		return traits_type::eof();
	}

	if (!(m_currentMode & std::ios_base::out)) {
		setg(nullptr, nullptr, nullptr);
		setp(nullptr, nullptr);
		m_currentMode = std::ios_base::out;
	}

	char_type  buf1;
	char_type* pb_save  = pbase();
	char_type* epb_save = epptr();
	if (!traits_type::eq_int_type(c, traits_type::eof())) {
		if (pptr() == nullptr) {
			setp(&buf1, &buf1 + 1);
		}
		*pptr() = traits_type::to_char_type(c);
		pbump(1);
	}
	if (pptr() != pbase()) {
		const std::size_t nmemb = static_cast<std::size_t>(pptr() - pbase());
		if (SDL_WriteIO(m_context, pbase(), nmemb) != nmemb) {
			return traits_type::eof();
		}
		setp(pb_save, epb_save);
	}
	return traits_type::not_eof(c);
}

typename SdlRwopsStreambuf::pos_type SdlRwopsStreambuf::seekoff(
		off_type off, std::ios_base::seekdir dir, std::ios_base::openmode) {
	const int width = 1;
	if (!m_context || sync()) {
		return pos_type(off_type(-1));
	}
	SDL_IOWhence whence;
	switch (dir) {
	case std::ios_base::beg:
		whence = SDL_IO_SEEK_SET;
		break;
	case std::ios_base::cur:
		whence = SDL_IO_SEEK_CUR;
		break;
	case std::ios_base::end:
		whence = SDL_IO_SEEK_END;
		break;
	default:
		return pos_type(off_type(-1));
	}
	if (SDL_SeekIO(m_context, width * off, whence) == -1) {
		return pos_type(off_type(-1));
	}
	const pos_type r = SDL_TellIO(m_context);
	return r;
}

typename SdlRwopsStreambuf::pos_type SdlRwopsStreambuf::seekpos(
		pos_type sp, std::ios_base::openmode) {
	if (!m_context || sync()) {
		return pos_type(off_type(-1));
	}
	if (SDL_SeekIO(m_context, sp, SDL_IO_SEEK_SET) == -1) {
		return pos_type(off_type(-1));
	}
	return sp;
}

int SdlRwopsStreambuf::sync() {
	if (!m_context) {
		return 0;
	}

	if (m_currentMode & std::ios_base::out) {
		if (pptr() != pbase()) {
			if (overflow() == traits_type::eof()) {
				return -1;
			}
		}
		// Would be calling SDL_RWflush() here if it existed.
		// Consider closing and re-opening the file to simulate a flush.
		const SDL_PropertiesID props = SDL_GetIOProperties(m_context);
		if (props) {
			FILE* fp = static_cast<FILE*>(SDL_GetPointerProperty(
					props, SDL_PROP_IOSTREAM_STDIO_FILE_POINTER, nullptr));
			if (fp) {
				fflush(fp);
			}
		}
	} else if (m_currentMode & std::ios_base::in) {
		off_type c;
		c = egptr() - gptr();
		if (SDL_SeekIO(m_context, -c, SDL_IO_SEEK_CUR) == -1) {
			return -1;
		}
		setg(nullptr, nullptr, nullptr);
		m_currentMode = NO_OPENMODE;
	}
	return 0;
}
