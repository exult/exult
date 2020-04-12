/*
 *  stackframe.h - a usecode interpreter stack frame
 *
 *  Copyright (C) 2002  The Exult Team
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

#ifndef STACKFRAME_H
#define STACKFRAME_H

#include "common_types.h"
#include "useval.h"
#include <iosfwd>
#include <memory>

class Game_object;
class Usecode_function;
using Game_object_shared = std::shared_ptr<Game_object>;

class Stack_frame {
public:
	Stack_frame(Usecode_function *fun, int event,
	            Game_object *caller, int chain, int depth);
	~Stack_frame();

	Usecode_function *function;
	const uint8 *ip; // current IP
	const uint8 *data; // pointer to start of data segment
	const uint8 *externs; // pointer to start of externs
	const uint8 *code; // pointer to (actual) code
	const uint8 *endp; // pointer directly past code segment
	int line_number; // if debugging info present

	// should probably add source filename?

	int call_chain; // unique ID for this call chain
	int call_depth; // zero for top level function

	int num_externs;
	int num_args;
	int num_vars;
	Usecode_value *locals;

	int eventid;
	Game_object_shared caller_item;

	Usecode_value *save_sp;

	Usecode_value &get_this() {
		return locals[num_args - 1];
	}
	static int LastCallChainID;
	static int getCallChainID() {
		return ++LastCallChainID;
	}
};

std::ostream &operator<<(std::ostream &out, Stack_frame &frame);

#endif
