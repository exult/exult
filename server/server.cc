/**
 ** Server.cc - Server functions for Exult (NOT ExultStudio).
 **
 ** Written: 5/2/2001 - JSF
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
#	include <config.h>    // All the ifdefs aren't useful if we don't do this
#endif

// only if compiled with "exult studio support"
#ifdef USE_EXULTSTUDIO

#	include "server.h"

#	include "Gump_manager.h"
#	include "actors.h"
#	include "barge.h"
#	include "cheat.h"
#	include "chunkter.h"
#	include "effects.h"
#	include "egg.h"
#	include "endianio.h"
#	include "gamemap.h"
#	include "gamewin.h"
#	include "objserial.h"
#	include "servemsg.h"

#	include <fcntl.h>
#	include <unistd.h>

#	include <cstdio>
#	include <cstdlib>

#	if HAVE_SYS_TYPES_H
#		include <sys/types.h>
#	endif

#	if HAVE_SYS_TIME_H
#		include <sys/time.h>
#	endif

#	if HAVE_SYS_SOCKET_H
#		include <sys/socket.h>
#	endif

#	if HAVE_NETDB_H
#		include <netdb.h>
#	endif

#	ifdef USECODE_DEBUGGER
#		include "debugserver.h"
#	endif

#	ifdef _WIN32
#		include "servewin32.h"
#	else
#		include <sys/un.h>
#	endif

#	ifdef __GNUC__
#		pragma GCC diagnostic push
#		pragma GCC diagnostic ignored "-Wold-style-cast"
#		pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	endif    // __GNUC__
#	include <SDL3/SDL.h>
#	ifdef __GNUC__
#		pragma GCC diagnostic pop
#	endif    // __GNUC__

using std::cerr;
using std::cout;
using std::endl;
/*
 *  Sockets, etc.
 */
int listen_socket = -1;    // Listen here for map-editor.
int client_socket = -1;    // Socket to the map-editor.
int highest_fd    = -1;    // Largest fd + 1.

/*
 *  Set the 'highest_fd' value to 1 + <largest fd>.
 */
inline void Set_highest_fd() {
	highest_fd = -1;    // Figure highest to listen to.
	if (listen_socket > highest_fd) {
		highest_fd = listen_socket;
	}
	if (client_socket > highest_fd) {
		highest_fd = client_socket;
	}
	highest_fd++;    // Select wants 1+highest.
}

/*
 *  Initialize server for map-editing.  Call this AFTER xfd has been set.
 */

void Server_init() {
	// Get location of socket file.
	const std::string servename = get_system_path(EXULT_SERVER);
#	ifndef _WIN32
	// Make sure it isn't there.
	unlink(servename.c_str());
#		ifdef HAVE_GETADDRINFOX
	// Don't use the old deprecated network API
	int             r;
	struct addrinfo hints, *ai;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	r              = getaddrinfo(0, servename.c_str(), &hints, &ai);
	if (r != 0) {
		cerr << "getaddrinfo(): " << gai_strerror(r) << endl;
		return;
	}

	listen_socket = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
#		else
	// Deprecated
	listen_socket = socket(PF_UNIX, SOCK_STREAM, 0);
#		endif
	if (listen_socket < 0) {
		perror("Failed to open map-editor socket");
	} else {
#		ifdef HAVE_GETADDRINFOX
		if (bind(listen_socket, ai->ai_addr, ai->ai_addrlen) == -1
			|| listen(listen_socket, 1) == -1)
#		else
		sockaddr_un addr;
		addr.sun_family = AF_UNIX;
		strcpy(addr.sun_path, servename.c_str());
		if (bind(listen_socket, reinterpret_cast<sockaddr*>(&addr),
				 sizeof(addr.sun_family) + strlen(addr.sun_path) + 1)
					== -1
			|| listen(listen_socket, 1) == -1)
#		endif
		{
			perror("Bind or listen on socket failed");
			close(listen_socket);
			listen_socket = -1;
		} else {    // Set to be non-blocking.
			cout << "Listening on socket " << listen_socket << endl;
			fcntl(listen_socket, F_SETFL,
				  fcntl(listen_socket, F_GETFL) | O_NONBLOCK);
		}
#		ifdef HAVE_GETADDRINFOX
		freeaddrinfo(ai);
#		endif
	}
	Set_highest_fd();
#	else

	listen_socket = client_socket = -1;
	if (Exult_server::create_pipe(servename.c_str())) {
		listen_socket = 1;
	}

#	endif
}

/*
 *  Close the server.
 */

void Server_close() {
#	ifdef _WIN32
	Exult_server::close_pipe();
	listen_socket = client_socket = -1;
#	else
	// unlink socket file+++++++
	std::string servename = get_system_path(EXULT_SERVER);
	unlink(servename.c_str());
#	endif
}

/*
 *  A message from a client is available, so handle it.
 */

static void Handle_client_message(
		int& fd    // Socket to client.  May be closed.
) {
	unsigned char          data[Exult_server::maxlength];
	Exult_server::Msg_type id;
	const int datalen = Exult_server::Receive_data(fd, id, data, sizeof(data));
	if (datalen < 0) {
		return;
	}
	const unsigned char* ptr               = &data[0];
	Game_window*         gwin              = Game_window::get_instance();
	auto                 unhandled_message = [](Exult_server::Msg_type id) {
        cout << "Unhandled message received from Exult Studio (id = " << id
             << ")" << endl;
	};
	switch (id) {
	case Exult_server::obj:
		Game_object::update_from_studio(&data[0], datalen);
		break;
	case Exult_server::barge:
		Barge_object::update_from_studio(&data[0], datalen);
		break;
	case Exult_server::egg:
		Egg_object::update_from_studio(&data[0], datalen);
		break;
	case Exult_server::container:
		Container_game_object::update_from_studio(&data[0], datalen);
		break;
	case Exult_server::npc:
		Actor::update_from_studio(&data[0], datalen);
		break;
	case Exult_server::info:
		Game_info_out(
				client_socket, Exult_server::version, cheat.get_edit_lift(),
				gwin->skip_lift, cheat.in_map_editor(), cheat.show_tile_grid(),
				gwin->was_map_modified(),
				static_cast<int>(cheat.get_edit_mode()));
		break;
	case Exult_server::write_map:
		gwin->write_map();    // Send feedback?+++++
		break;
	case Exult_server::read_map:
		gwin->read_map();
		break;
	case Exult_server::map_editing_mode: {
		const int onoff = little_endian::Read2(ptr);
		if ((onoff != 0) != cheat.in_map_editor()) {
			cheat.toggle_map_editor();
		}
		break;
	}
	case Exult_server::tile_grid: {
		const int onoff = little_endian::Read2(ptr);
		if ((onoff != 0) != cheat.show_tile_grid()) {
			cheat.toggle_tile_grid();
		}
		break;
	}
	case Exult_server::edit_lift: {
		const int lift = little_endian::Read2(ptr);
		cheat.set_edit_lift(lift);
		break;
	}
	case Exult_server::reload_usecode:
		gwin->reload_usecode();
		break;
	case Exult_server::locate_terrain: {
		const int      tnum = little_endian::Read2(ptr);
		int            cx   = little_endian::Read2s(ptr);
		int            cy   = little_endian::Read2s(ptr);
		const bool     up   = Read1(ptr) != 0;
		const bool     okay = gwin->get_map()->locate_terrain(tnum, cx, cy, up);
		unsigned char* wptr = &data[2];
		// Set back reply.
		little_endian::Write2(wptr, cx);
		little_endian::Write2(wptr, cy);
		wptr++;    // Skip 'up' flag.
		Write1(wptr, okay ? 1 : 0);
		Exult_server::Send_data(
				client_socket, Exult_server::locate_terrain, data, wptr - data);
		break;
	}
	case Exult_server::swap_terrain: {
		const int      tnum = little_endian::Read2(ptr);
		const bool     okay = Game_map::swap_terrains(tnum);
		unsigned char* wptr = &data[2];
		Write1(wptr, okay ? 1 : 0);
		Exult_server::Send_data(
				client_socket, Exult_server::swap_terrain, data, wptr - data);
		break;
	}
	case Exult_server::insert_terrain: {
		const int      tnum = little_endian::Read2s(ptr);
		const bool     dup  = Read1(ptr) != 0;
		const bool     okay = Game_map::insert_terrain(tnum, dup);
		unsigned char* wptr = &data[3];
		Write1(wptr, okay ? 1 : 0);
		Exult_server::Send_data(
				client_socket, Exult_server::insert_terrain, data, wptr - data);
		break;
	}
	case Exult_server::delete_terrain: {
		const int      tnum = little_endian::Read2s(ptr);
		const bool     okay = Game_map::delete_terrain(tnum);
		unsigned char* wptr = &data[2];
		Write1(wptr, okay ? 1 : 0);
		Exult_server::Send_data(
				client_socket, Exult_server::delete_terrain, data, wptr - data);
		break;
	}
	case Exult_server::send_terrain: {
		// Send back #, total, 512-bytes data.
		const int      tnum = little_endian::Read2s(ptr);
		unsigned char* wptr = &data[2];
		little_endian::Write2(wptr, gwin->get_map()->get_num_chunk_terrains());
		Chunk_terrain* ter = Game_map::get_terrain(tnum);
		// Serialize it.
		wptr += ter->write_flats(wptr, Game_map::is_v2_chunks());
		Exult_server::Send_data(
				client_socket, Exult_server::send_terrain, data, wptr - data);
		break;
	}
	case Exult_server::terrain_editing_mode: {
		// 1=on, 0=off, -1=undo.
		const int onoff = little_endian::Read2s(ptr);
		// skip_lift==0 <==> terrain-editing.
		gwin->skip_lift = onoff == 1 ? 0 : 256;
		static const char* const msgs[3]
				= {"Terrain-Editing Aborted", "Terrain-Editing Done",
				   "Terrain-Editing Enabled"};
		if (onoff == 0) {    // End/commit.
			Game_map::commit_terrain_edits();
		} else if (onoff == -1) {
			Game_map::abort_terrain_edits();
		}
		if (onoff >= -1 && onoff <= 1) {
			gwin->get_effects()->center_text(msgs[onoff + 1]);
		}
		gwin->set_all_dirty();
		break;
	}
	case Exult_server::set_edit_shape: {
		const int shnum = little_endian::Read2s(ptr);
		const int frnum = little_endian::Read2s(ptr);
		cheat.set_edit_shape(shnum, frnum);
		break;
	}
	case Exult_server::view_pos: {
		const int tx = little_endian::Read4(ptr);
		if (tx == -1) {    // This is a query?
			gwin->send_location();
			break;
		}
		const int ty = little_endian::Read4(ptr);
		// +++Later int txs = Read4(ptr);
		// int tys = Read4(ptr);
		// int scale = Read4(ptr);
		// Only set if chunk changed.
		if (tx / c_tiles_per_chunk != gwin->get_scrolltx() / c_tiles_per_chunk
			|| ty / c_tiles_per_chunk
					   != gwin->get_scrollty() / c_tiles_per_chunk) {
			gwin->set_scrolls(tx, ty);
			gwin->set_all_dirty();
		}
		break;
	}
	case Exult_server::set_edit_mode: {
		const int md = little_endian::Read2(ptr);
		if (md >= 0 && md <= 4) {
			cheat.set_edit_mode(static_cast<Cheat::Map_editor_mode>(md));
		}
		break;
	}
	case Exult_server::hide_lift: {
		const int lift  = little_endian::Read2(ptr);
		gwin->skip_lift = lift;
		gwin->set_all_dirty();
		break;
	}
	case Exult_server::reload_shapes:
		Shape_manager::get_instance()->reload_shapes(little_endian::Read2(ptr));
		break;
	case Exult_server::unused_shapes: {
		// Send back shapes not used in game.
		unsigned char data[Exult_server::maxlength];
		size_t        sz = 1024u / 8;    // Gets bits for unused shapes.
		sz               = sz > sizeof(data) ? sizeof(data) : sz;
		gwin->get_map()->find_unused_shapes(data, sz);
		Exult_server::Send_data(
				client_socket, Exult_server::unused_shapes, data, sz);
		break;
	}
	case Exult_server::locate_shape: {
		const int      shnum = little_endian::Read2(ptr);
		const int      frnum = little_endian::Read2s(ptr);
		const int      qual  = little_endian::Read2s(ptr);
		const bool     up    = Read1(ptr) != 0;
		const bool     okay  = gwin->locate_shape(shnum, up, frnum, qual);
		unsigned char* wptr  = &data[6];    // Send back reply.
		wptr++;                             // Skip 'up' flag.
		Write1(wptr, okay ? 1 : 0);
		Exult_server::Send_data(
				client_socket, Exult_server::locate_shape, data, wptr - data);
		break;
	}
	case Exult_server::cut:    // Cut/copy.
		cheat.cut(*ptr != 0);
		break;
	case Exult_server::paste:
		cheat.paste();
		break;
	case Exult_server::npc_unused: {
		unsigned char* wptr = &data[0];
		little_endian::Write2(wptr, gwin->get_num_npcs());
		little_endian::Write2(wptr, gwin->get_unused_npc());
		Exult_server::Send_data(
				client_socket, Exult_server::npc_unused, data, wptr - data);
		break;
	}
	case Exult_server::npc_info: {
		const int      npcnum = little_endian::Read2(ptr);
		Actor*         npc    = gwin->get_npc(npcnum);
		unsigned char* wptr   = &data[2];
		if (npc) {
			little_endian::Write2(wptr, npc->get_shapenum());
			Write1(wptr, npc->is_unused());
			const std::string nm = npc->get_npc_name();
			strcpy(reinterpret_cast<char*>(wptr), nm.c_str());
			// Point past ending nullptr.
			wptr += strlen(reinterpret_cast<char*>(wptr)) + 1;
		} else {
			little_endian::Write2(wptr, static_cast<unsigned short>(-1));
		}
		Exult_server::Send_data(
				client_socket, Exult_server::npc_info, data, wptr - data);
		break;
	}
	case Exult_server::locate_npc:
		gwin->locate_npc(little_endian::Read2(ptr));
		break;
	case Exult_server::edit_npc: {
		const int npcnum = little_endian::Read2(ptr);
		Actor*    npc    = gwin->get_npc(npcnum);
		if (npc) {
			npc->edit();
		}
		break;
	}
	case Exult_server::edit_selected: {
		const unsigned char              basic = *ptr;
		const Game_object_shared_vector& sel   = cheat.get_selected();
		if (!sel.empty()) {
			if (basic) {    // Basic obj. props?
				sel.back()->Game_object::edit();
			} else {
				sel.back()->edit();
			}
		}
		break;
	}
	case Exult_server::set_edit_chunknum:
		cheat.set_edit_chunknum(little_endian::Read2s(ptr));
		break;
	case Exult_server::game_pos: {
		const Tile_coord pos  = gwin->get_main_actor()->get_tile();
		unsigned char*   wptr = &data[0];
		little_endian::Write2(wptr, pos.tx);
		little_endian::Write2(wptr, pos.ty);
		little_endian::Write2(wptr, pos.tz);
		Exult_server::Send_data(
				client_socket, Exult_server::game_pos, data, wptr - data);
		break;
	}
	case Exult_server::goto_map: {
		char             msg[80];
		const Tile_coord pos = gwin->get_main_actor()->get_tile();
		const int        num = little_endian::Read2(ptr);
		gwin->teleport_party(pos, true, num);
		snprintf(msg, sizeof(msg), "Map #%02x", num);
		gwin->get_effects()->center_text(msg);
		break;
	}
	case Exult_server::cont_show_gump: {
		Serial_in io(ptr);
		uintptr   addr;
		io << addr;
		auto* p   = reinterpret_cast<Game_object*>(addr);
		auto* obj = p->as_container();
		if (!obj) {
			cout << "Error decoding container data" << endl;
			break;
		}
		cout << "Displaying container's gump" << endl;
		Actor* act = obj->as_actor();
		if (act) {
			act->show_inventory();
		} else {
			// Avoid all frame-usecode and force-usecode subtleties.
			const int gump
					= ShapeID::get_info(obj->get_shapenum()).get_gump_shape();
			if (gump >= 0) {
				Gump_manager* gump_man
						= Game_window::get_instance()->get_gump_man();
				gump_man->add_gump(obj, gump);
			} else {
				cerr << "The selected container has no gump!" << endl;
			}
		}
		break;
	}
	case Exult_server::reload_shapes_info:
		Shape_manager::get_instance()->reload_shape_info();
		break;
	case Exult_server::usecode_debugging:
#	ifdef USECODE_DEBUGGER
		Handle_debug_message(&data[0], datalen);
#	else
		unhandled_message(id);
#	endif
		break;
	default:
		unhandled_message(id);
	}
}

/*
 *  Delay for a fraction of a second, or until there's data available.
 *  If a server request comes, it's handled here.
 */

#	ifndef _WIN32
#		define DELAY_TOTAL_MS  10
#		define DELAY_SINGLE_MS 1
#	endif

void Server_delay(Message_handler handle_message) {
#	ifndef _WIN32
	Uint32 expiration = DELAY_TOTAL_MS + SDL_GetTicks();
	for (;;) {
		SDL_PumpEvents();
		if ((SDL_PeepEvents(
					 nullptr, 0, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST)
			 != 0)
			|| (static_cast<Sint32>(SDL_GetTicks())
				>= static_cast<Sint32>(expiration))) {
			return;
		}

		fd_set  rfds;
		timeval timer;
		timer.tv_sec  = 0;
		timer.tv_usec = DELAY_SINGLE_MS * 1000;    // Try 1/1000 second.
		FD_ZERO(&rfds);
		if (listen_socket >= 0) {
			FD_SET(listen_socket, &rfds);
		}
		if (client_socket >= 0) {
			FD_SET(client_socket, &rfds);
		}
		// Wait for timeout or event.
		if (select(highest_fd, &rfds, nullptr, nullptr, &timer) > 0) {
			// Something's come in.
			if (listen_socket >= 0 && FD_ISSET(listen_socket, &rfds)) {
				// New client connection.
				// For now, just one at a time.
				if (client_socket >= 0) {
					close(client_socket);
				}
				client_socket = accept(listen_socket, nullptr, nullptr);
				cout << "Accept returned client_socket = " << client_socket
					 << endl;
				// Non-blocking.
				fcntl(client_socket, F_SETFL,
					  fcntl(client_socket, F_GETFL) | O_NONBLOCK);
				Set_highest_fd();
			}
			if (client_socket >= 0 && FD_ISSET(client_socket, &rfds)) {
				handle_message(client_socket);
				// Client gone?
				if (client_socket == -1) {
					Set_highest_fd();
				}
			}
		}
	}
#	else
	if (listen_socket == -1) {
		return;
	}

	if (client_socket == -1) {
		// Only do this in map edit mode
		if (!cheat.in_map_editor()) {
			return;
		}

		const std::string servename = get_system_path("<GAMEDAT>");
		if (!Exult_server::try_connect_to_client(servename.c_str())) {
			return;
		} else {
			client_socket = 1;
		}
		std::cout << "Connected to client" << endl;
	}

	while (Exult_server::peek_pipe() > 0) {
		handle_message(client_socket);
	}

	if (Exult_server::is_broken()) {
		std::cout << "Client disconnected." << endl;
		client_socket = -1;
	}
#	endif
}

void Server_delay() {
	Server_delay(Handle_client_message);
}

#endif
