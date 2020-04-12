/*
 *  Xdrag.h - Drag-and-drop under X.
 *
 *  Copyright (C) 2000-2013  The Exult Team
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

#ifndef INCL_XDRAG
#define INCL_XDRAG

#include "u7drag.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#if defined(USE_EXULTSTUDIO) && !defined(_WIN32)

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
const unsigned long max_types = 15;

/*
 *  This supports the 'drop' side of Xdnd:
 */
class Xdnd {
	Display *display;
	Window xwmwin;          // Gets WM window.
	Window xgamewin;        // Game window within xwmwin.
	Atom shapeid_atom;      // For drag-and-drop of shapes.
	Atom chunkid_atom;      // For chunks.
	Atom npcid_atom;        // For NPC's.
	Atom comboid_atom;      // For combos.
	Atom xdnd_aware;        // For XdndAware.
	Atom xdnd_enter;
	Atom xdnd_leave;
	Atom xdnd_position;
	Atom xdnd_drop;
	Atom xdnd_status;
	Atom xdnd_copy;
	Atom xdnd_ask;
	Atom xdnd_typelist;
	Atom xdnd_selection;
	unsigned long xdnd_version;
	unsigned long num_types;
	Atom drag_types[max_types]; // Data type atoms source can supply.
	// Current drag info:
	int lastx, lasty;       // Last mouse pos. during drag, within
	//   our window.
	int winx, winy;         // Window coords. at start of drag.
	int file, shape, frame;     // Set when a shape is being dragged.
	int chunknum;           // Set when a chunk is dragged.
	int npcnum;         // Set when an NPC is dragged.
	int combo_cnt;          // Set when combo is dragged.
	int combo_xtiles, combo_ytiles, combo_tiles_right, combo_tiles_below;
	struct U7_combo_data *combo;    // Combo elements (allocated);
	bool data_valid;        // True when the above is retrieved.

	Move_shape_handler_fun move_shape_handler;  // For dragging shapes.
	Move_combo_handler_fun move_combo_handler;  // For dragging combos.
	Drop_shape_handler_fun shape_handler;   // For dropping shapes.
	Drop_chunk_handler_fun chunk_handler;   // For dropping chunks.
	Drop_npc_handler_fun npc_handler;   // For dropping npcs.
	Drop_combo_handler_fun combo_handler;   // For dropping combos.
public:
	Xdnd(Display *d, Window xw, Window xgw, Move_shape_handler_fun movefun,
	     Move_combo_handler_fun movecmbfun,
	     Drop_shape_handler_fun shapefun, Drop_chunk_handler_fun cfun,
	     Drop_npc_handler_fun npcfun, Drop_combo_handler_fun cmbfun);
	~Xdnd();
	void client_msg(XClientMessageEvent &cev);
	void select_msg(XSelectionEvent &sev);
};

#endif

#endif
