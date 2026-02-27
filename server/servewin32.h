/*
Copyright (C) 2002-2022 The Exult Team

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

// Win32 Only file

#ifdef _WIN32

#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif

#	include <windows.h>

namespace Exult_server {
	// Hack functions
	int write(int file, const void* v, unsigned int len);
	int read(int file, void* v, unsigned int len);
	int close(int file);

	// Server Functions
	bool create_pipe(const char* path);
	void setup_connect();
	bool try_connect_to_client(const char* path);
	void disconnect_from_client();
	void close_pipe();

	// Client Functions
	int  try_connect_to_server(const char* path);
	void disconnect_from_server();

	// General Functions
	int  peek_pipe();
	bool is_broken();
	bool notify_connection_lost();
	bool is_win9x();
}    // namespace Exult_server

#endif
