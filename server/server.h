/**
 ** Server.h - Server functions.
 **
 ** Written: 5/3/2001 - JSF
 **/

#ifndef INCL_SERVER
#define INCL_SERVER 1
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

/*
 *  For the time being, we'll only inflict this on X users.
 */
#ifdef USE_EXULTSTUDIO

using Message_handler = void (*)(int&);

extern int  client_socket;
extern void Server_init();
extern void Server_delay(Message_handler handle_message);
extern void Server_delay();
extern void Server_close();

#endif /* USE_EXULTSTUDIO */

#endif /* INCL_SERVER */
