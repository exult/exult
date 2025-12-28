/**
 ** Servemsg.h - Server msgs.
 ** NOTE:   This is for inclusion by both client and server.
 **
 ** Written: 5/3/2001 - JSF
 **/

/*
Copyright (C) 2000-2025 The Exult Team

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

#ifndef INCL_SERVEMSG
#define INCL_SERVEMSG 1

/*
 *  An entry sent between client and server will have the following format:
 *
Bytes   Description
0-1 Magic.
2-3 Length of the data, low-byte first.
4   Message type.  Defined below.
5-  Data.
 */

namespace Exult_server {
	const unsigned short magic = 0xf381;
	const int            maxlength
			= 16 * 16 * 3 + 50;    // Big enough to hold a 'terrain'.
	const int hdrlength = 5;
	const int version   = 0;    // Sent with 'info' message.

	enum Msg_type {
		say            = 0,    // For testing.  Data is text.
		obj            = 1,    // Generic object.
		egg            = 2,    // Egg_object.
		npc            = 3,    // Editing an NPC.
		user_responded = 4,    // User performed requested action.
		cancel         = 5,    // Cancel operation.
		info           = 6,    // Game info. request/response.  See
		//   Game_info_xxx in objserial.h.
		write_map            = 7,     // Save map.
		read_map             = 8,     // Read map.
		map_editing_mode     = 9,     // 1 to turn it on, 0 for off.
		tile_grid            = 10,    // 1 to show grid, 0 for off.
		edit_lift            = 11,    // Lift passed: 0-13.
		reload_usecode       = 12,    // Reload patched usecode.
		locate_terrain       = 13,    // Locate desired chunk terrain.
		swap_terrain         = 14,    // Swap two terrains.
		insert_terrain       = 15,    // Insert new chunk terrain.
		delete_terrain       = 16,    // Delete (unused) terrain.
		send_terrain         = 17,    // Send 512-byte terrain to client.
		terrain_editing_mode = 18,    // 1 to turn on, 0 for off, -1 to undo.
		set_edit_shape       = 19,    // Set shape/frame to 'paint' with.
		view_pos             = 20,    // Tile loc., size, scale of what's
		//   shown in gamewin.  Sent both ways.
		set_edit_mode      = 21,    // Move(0), paint(1), select(2).
		combo_pick         = 22,    // Sending shape to add to combo.
		combo_toggle       = 23,
		hide_lift          = 24,    // Lift passed: 1-16.
		reload_shapes      = 25,    // Reload a .vga file.
		unused_shapes      = 26,    // Find unused shapes in game.
		locate_shape       = 27,    // Find shape in world.
		cut                = 28,    // Cut/copy.
		paste              = 29,    // ... from clipboard.
		select_status      = 30,    // Flags:  selection, clipboard avail.
		npc_unused         = 31,    // Get #NPC's, first unused.
		edit_selected      = 32,    // Edit selected object.
		set_edit_chunknum  = 33,    // Set chunk# to paint with.
		game_pos           = 34,    // Returns tx, ty, tz of main char.
		goto_map           = 35,    // Teleport to map #n.
		barge              = 36,    // Edit barge object.
		npc_info           = 37,    // Send back NPC info.
		locate_npc         = 38,    // Show NPC on map.
		edit_npc           = 39,    // Bring up NPC-editor.
		container          = 40,    // Edit container object.
		cont_show_gump     = 41,    // Displays a container's gump.
		reload_shapes_info = 42,    // Reload shapes info.
		drag_shape         = 43,    // Begin drag of a Shape.
		drag_combo         = 44,    // Begin drag of a Combo.
		drag_npc           = 45,    // Begin drag of a NPC.
		drag_chunk         = 46,    // Begin drag of a Chunk.
		edit_pick          = 47,    // Selecting Shapes for editing.
		write_minimap      = 48,    // Save minimap.
		show_bboxes        = 49,    // Set bounding boxes color.
		play_audio         = 50,    // Play audio (music/sfx/voice).
		stop_audio         = 51,    // Stop all audio playback.
		clear_selection    = 52,    // Clear current selection.
		get_user_click     = 53,    // Request map click, returns tile coords.
		locate_egg         = 54,    // Locate egg by quality/path number.
		locate_intermap    = 55,    // Locate intermap destination (x,y,z,map).
		usecode_debugging  = 128
	};

	// I/O routines:
	int Send_data(
			int socket, Msg_type id,
			const unsigned char* data    = nullptr,    // For just sending id.
			int                  datalen = 0);
	int Receive_data(
			int&           socket,    // Closed, set to -1 if disconnected.
			Msg_type&      id,        // ID returned.
			unsigned char* data, int datalen);

	// Wait for given ms for a response. return false if no response
	bool wait_for_response(int socket, int ms);

}    // namespace Exult_server

#endif /* INCL_SERVEMSG */
