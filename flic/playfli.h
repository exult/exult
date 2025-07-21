/*
 *  playfli.cc - Play Autodesk Animator FLIs
 *
 *  Copyright (C) 2000-2022  The Exult Team
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

#ifndef PLAYFLI_H
#define PLAYFLI_H

#include "databuf.h"
#include "imagewin.h"

class Palette;

class playfli {
public:
	struct fliinfo {
		int frames;
		int width;
		int height;
		int depth;
		int speed;
	};

	void set_speed(int speed) {
		fli_speed = speed;
	}

private:
	enum FlicChunks {
		FLI_COLOR256 = 4,
		FLI_SS2      = 7,
		FLI_COLOR    = 11,
		FLI_LC       = 12,
		FLI_BLACK    = 13,
		FLI_BRUN     = 15,
		FLI_COPY     = 16,
		FLI_PSTAMP   = 18,    // Unsupported
	};

	IExultDataSource              fli_data;
	std::unique_ptr<Image_buffer> fli_buf;
	std::unique_ptr<Palette>      palette;
	int                           fli_size;
	int                           fli_magic;
	int                           fli_frames;
	int                           fli_width;
	int                           fli_height;
	int                           fli_depth;
	int                           fli_flags;
	int                           fli_speed;
	size_t                        streamstart;
	size_t                        streampos;
	int                           frame;
	char                          fli_name[9]{};

public:
	template <typename... T>
	explicit playfli(T&&... args)
			: fli_data(std::forward<T>(args)...),
			  palette(std::make_unique<Palette>()) {
		initfli();
	}

	playfli(const playfli&)                = delete;
	playfli(playfli&&) noexcept            = default;
	playfli& operator=(const playfli&)     = delete;
	playfli& operator=(playfli&&) noexcept = default;
	~playfli() noexcept                    = default;
	void info(fliinfo* fi = nullptr) const;
	int  play(
			 Image_window* win, int first_frame = 0, int last_frame = -1,
			 unsigned long ticks = 0, int brightness = 100);
	void put_buffer(Image_window* win) const;

	inline Palette* get_palette() {
		return palette.get();
	}

private:
	void initfli();
	int  nextpal;
	int  thispal;
	bool changepal;
};

#endif
