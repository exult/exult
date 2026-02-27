/*
 *  debugserver.cc: handle usecode debugging messages from client
 *
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

#if (defined(USE_EXULTSTUDIO) && defined(USECODE_DEBUGGER))

#	include "debugmsg.h"
#	include "debugserver.h"
#	include "endianio.h"
#	include "gamewin.h"
#	include "servemsg.h"
#	include "server.h"
#	include "stackframe.h"
#	include "ucdebugging.h"
#	include "ucfunction.h"
#	include "ucinternal.h"
#	include "ucserial.h"
#	include "useval.h"

#	include <algorithm>
#	include <iomanip>
#	include <iostream>
#	include <sstream>

using std::ios;
using std::stringstream;

// message handler that only handles debugging messages.
void Handle_client_debug_message(int& fd) {
	unsigned char          data[Exult_server::maxlength];
	Exult_server::Msg_type id;
	const int              datalen = Exult_server::Receive_data(fd, id, data, sizeof(data));
	if (datalen < 0) {
		return;
	}
	switch (id) {
	case Exult_server::usecode_debugging:
		Handle_debug_message(&data[0], datalen);
		break;
	default:
		// maybe reply something like "unable to handle message" ?
		break;
	}
}

void Handle_debug_message(unsigned char* data, int datalen) {
	ignore_unused_variable_warning(datalen);
	Usecode_machine* ucm = Game_window::get_instance()->get_usecode();
	auto*            uci = dynamic_cast<Usecode_internal*>(ucm);

	if (uci == nullptr) {
		return;    // huh?
	}

	auto id = static_cast<Exult_server::Debug_msg_type>(data[0]);

	const unsigned char* ptr = data;

	if (uci->is_on_breakpoint()) {
		// accept extra messages now

		switch (id) {
		case Exult_server::dbg_continue:
			uci->set_breakpoint_action(0);
			break;
		case Exult_server::dbg_stepinto:
			uci->set_breakpoint_action(0);
			uci->set_breakpoint();
			break;
		case Exult_server::dbg_stepover:
			uci->set_breakpoint_action(0);
			uci->dbg_stepover();
			break;
		case Exult_server::dbg_finish:
			uci->set_breakpoint_action(0);
			uci->dbg_finish();
			break;
		case Exult_server::dbg_get_callstack: {
			unsigned char d[3];
			d[0]                         = static_cast<unsigned char>(Exult_server::dbg_callstack);
			unsigned char* ptr           = &d[1];
			const int      callstacksize = uci->get_callstack_size();
			little_endian::Write2(ptr, callstacksize);
			Exult_server::Send_data(client_socket, Exult_server::usecode_debugging, d, 3);
			for (int i = 0; i < callstacksize; i++) {
				Stack_frame* frame = uci->get_stackframe(i);
				Stack_frame_out(
						client_socket, frame->function->id, static_cast<int>(frame->ip - frame->code), frame->call_chain,
						frame->call_depth, frame->eventid, reinterpret_cast<uintptr>(frame->caller_item.get()), frame->num_args,
						frame->num_vars, frame->locals);
			}
			break;
		}
		case Exult_server::dbg_get_stack: {
			const Usecode_value zeroval(0);
			stringstream        dataio(ios::in | ios::out | ios::binary);
			OStreamDataSource   ds(&dataio);
			ds.write1(static_cast<unsigned char>(Exult_server::dbg_stack));
			const int stacksize = uci->get_stack_size();
			ds.write2(stacksize);
			for (int i = 0; i < stacksize; i++) {
				Usecode_value* val = uci->peek_stack(i);
				if (val && !val->save(&ds)) {
					std::cerr << "Error: Could not save usecode value to message!" << std::endl;
				}
				if (ds.getSize() >= Exult_server::maxlength) {
					break;
				}
			}
			if (ds.getSize() > Exult_server::maxlength) {
				std::cerr << "Error: stack larger than max. message!" << std::endl;
			}
			std::string data(dataio.str());
			const auto* dptr    = reinterpret_cast<const unsigned char*>(data.data());
			const int   datalen = std::min(static_cast<int>(data.size()), Exult_server::maxlength);
			Exult_server::Send_data(client_socket, Exult_server::usecode_debugging, dptr, datalen);
			break;
		}
		default:
			break;
		}
	}

	// these messages can always be handled
	switch (id) {
	case Exult_server::dbg_break:
		uci->set_breakpoint();
		break;
	case Exult_server::dbg_get_status: {
		unsigned char c;
		if (uci->is_on_breakpoint()) {
			c = static_cast<unsigned char>(Exult_server::dbg_on_breakpoint);
		} else {
			c = static_cast<unsigned char>(Exult_server::dbg_continuing);
		}
		Exult_server::Send_data(client_socket, Exult_server::usecode_debugging, &c, 1);
		break;
	}
	case Exult_server::dbg_set_location_bp: {
		ptr++;
		const int funcid = little_endian::Read4(ptr);
		const int ip     = little_endian::Read4(ptr);

		std::cout << "Setting breakpoint at " << std::hex << std::setfill('0') << std::setw(4) << funcid << ", " << std::setw(4)
				  << ip << std::setfill(' ') << std::dec << std::endl;

		// +++++ check for duplicates?

		const int     breakpoint_id = uci->set_location_breakpoint(funcid, ip);
		unsigned char d[13];
		d[0]                = static_cast<unsigned char>(Exult_server::dbg_set_location_bp);
		unsigned char* dptr = &d[1];
		little_endian::Write4(dptr, funcid);
		little_endian::Write4(dptr, ip);
		little_endian::Write4(dptr, breakpoint_id);
		Exult_server::Send_data(client_socket, Exult_server::usecode_debugging, d, 13);
		break;
	}
	case Exult_server::dbg_clear_breakpoint: {
		ptr++;
		const int  breakpoint_id = little_endian::Read4(ptr);
		const bool ok            = uci->clear_breakpoint(breakpoint_id);
		if (ok) {
			// reply
			Exult_server::Send_data(client_socket, Exult_server::usecode_debugging, data, 5);
		}
		break;
	}
	case Exult_server::dbg_get_breakpoints: {
		uci->transmit_breakpoints(client_socket);
		break;
	}
	default:
		break;
	}
}

#endif
