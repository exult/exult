/**
 ** Servemsg.cc - Server msgs.
 ** NOTE:   This is for inclusion by both client and server.
 **
 ** Written: 5/28/2001 - JSF
 **/

/*
Copyright (C) 2001-2022 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "servemsg.h"

#include "ignore_unused_variable_warning.h"

#include <unistd.h>

#include <climits>
#include <cstring>
#include <iostream> /* For debugging msgs. */

#ifdef _WIN32
#	include "servewin32.h"
#endif

using std::cout;
using std::endl;

namespace Exult_server {

	/*
	 *  Send data.
	 *
	 *  Output: -1 if error.
	 */

	int Send_data(
			int socket, Msg_type id, const unsigned char* data, int datalen) {
#ifdef USE_EXULTSTUDIO
		unsigned char buf[maxlength + hdrlength];
		buf[0] = magic & 0xff;    // Store magic (low-byte first).
		buf[1] = (magic >> 8) & 0xff;
		buf[2] = datalen & 0xff;    // Data length.
		buf[3] = (datalen >> 8) & 0xff;
		buf[4] = id;
		if (datalen > 0) {
			std::memcpy(&buf[5], data, datalen);    // The data itself.
		}
		const int len = datalen + hdrlength;

		return write(socket, buf, len) == len ? 0 : -1;
#else  /* USE_EXULTSTUDIO */
		ignore_unused_variable_warning(socket, id, data, datalen);
		return -1;
#endif /* USE_EXULTSTUDIO */
	}

	/*
	 *  Read message from client.
	 *
	 *  Output: Length of data, else -1.
	 */

	int Receive_data(
			int&           socket,    // Closed, set to -1 if disconnected.
			Msg_type&      id,        // ID returned.
			unsigned char* data, int datalen) {
#ifdef USE_EXULTSTUDIO
		unsigned char buf[hdrlength];
		const int     len = read(socket, buf, 2);    // Get magic.
		if (!len) {                                  // Closed?
			close(socket);
			socket = -1;
			return -1;
		}
		if (len == -1) {    // Nothing available?
			return -1;
		}
		const int magic = buf[0] + (buf[1] << 8);
		if (magic != Exult_server::magic) {
			cout << "Bad magic read" << endl;
			return -1;
		}
		if (read(socket, buf, 3) != 3) {
			cout << "Couldn't read length+type" << endl;
			return -1;
		}
		const int dlen = buf[0] | (buf[1] << 8);
		// Message type.
		id = static_cast<Exult_server::Msg_type>(buf[2]);
		if (dlen > Exult_server::maxlength || dlen > datalen) {
			cout << "Length " << datalen << " exceeds max" << endl;
			//+++++++++Eat the chars.
			return -1;
		}
#	ifdef _WIN32
		const int bytes_read = read(socket, data, dlen);    // Read data.
#	else
		const ssize_t bytes_read = read(socket, data, dlen);    // Read data.
#	endif
		if (bytes_read < 0 || bytes_read > INT_MAX) {
			cout << "Read error or overflow" << endl;
			return -1;
		}
		datalen = static_cast<int>(bytes_read);

		if (datalen < dlen) {
			cout << "Failed to read all " << dlen << " bytes" << endl;
			return -1;
		}
		return datalen;
#else  /* USE_EXULTSTUDIO */
		ignore_unused_variable_warning(socket, id, data, datalen);
		return -1;
#endif /* USE_EXULTSTUDIO */
	}

	bool wait_for_response(int socket, int ms) {
		ignore_unused_variable_warning(socket, ms);
#if defined(_WIN32) && defined(USE_EXULTSTUDIO)
		/*
		int ticks = GetTickCount();
		while(GetTickCount() < ticks+ms) {
			if (peek_pipe() > 0) return true;
			SleepEx(1, TRUE);
		}
		if (peek_pipe() > 0) return true;
		return false;
		*/
#endif
		return true;
	}

}    // namespace Exult_server
