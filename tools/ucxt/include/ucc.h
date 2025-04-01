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

#ifndef UCC_H
#define UCC_H

#include <string>
#include <vector>

class UCc;

class UCc {
public:
	UCc(const unsigned int offset = 0, const unsigned int id = 0,
		const std::vector<unsigned char>& params = std::vector<unsigned char>())
			: _id(id), _offset(offset), _params(params) {}

	UCc(const unsigned int id, const std::string& miscstr)
			: _id(id), _miscstr(miscstr) {}

	UCc(const std::vector<unsigned int>& params_parsed)
			: _params_parsed(params_parsed) {}

	unsigned int               _id = 0;
	std::string                _miscstr;
	unsigned int               _offset = 0;
	std::vector<unsigned char> _params;
	std::vector<unsigned int>  _params_parsed;
	bool                       _tagged = false;
	std::vector<unsigned int>  _jump_offsets;
	unsigned int               _missing_args = 0;

	/* A temporary array to hold the items this opcode theoretically popped
	   from the stack. This should probably go in it's own wrapper class with
	   the current UCc. */
	std::vector<UCc*> _popped;
};

class UCOptions {
public:
	bool game_bg() const {
		return _game == GAME_BG;
	}

	bool game_si() const {
		return _game == GAME_SI;
	}

	bool game_fov() const {
		return _game == GAME_FOV;
	}

	bool game_ss() const {
		return _game == GAME_SS;
	}

	bool game_sib() const {
		return _game == GAME_SIB;
	}

	bool game_u7() const {
		return game_bg() || game_si() || game_fov() || game_ss() || game_sib();
	}

	bool game_u8() const {
		return _game == GAME_U8;
	}

	bool output_extern_header = false;

	bool noconf         = false;
	bool rawops         = false;
	bool autocomment    = false;
	bool uselesscomment = false;
	bool verbose        = false;
	bool very_verbose   = false;
	bool ucdebug        = false;
	bool basic          = false;

	bool output_list        = false;
	bool output_asm         = false;
	bool output_ucs         = false;
	bool output_flag        = false;
	bool output_trans_table = false;

	bool mode_all = false;
	bool mode_dis = false;

	bool force_ext32
			= false;    // force ext32 function format output for all functions

	// private:
	enum {
		GAME_BG  = 1,
		GAME_FOV = 2,
		GAME_SI  = 3,
		GAME_SS  = 4,
		GAME_SIB = 5,
		GAME_U8  = 6
	};

	unsigned int _game = GAME_BG;
};

#endif
