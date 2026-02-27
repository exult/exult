/*
 *  ucmachine.cc - Interpreter for usecode.
 *
 *  Copyright (C) 1999  Jeffrey S. Freedman
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "ucmachine.h"

#include "conversation.h"
#include "keyring.h"

#include <cstring>

Usecode_machine::Usecode_machine() {
	// Clear global flags.
	gflags.resize(gflags_page_size, 0);
	conv    = new Conversation;
	keyring = new Keyring;
}

/*
 *  Delete.
 */

Usecode_machine::~Usecode_machine() {
	delete conv;
	delete keyring;
}

void Usecode_machine::init_conversation() {
	conv->init_faces();
}

int Usecode_machine::get_num_faces_on_screen() const {
	return conv->get_num_faces_on_screen();
}
