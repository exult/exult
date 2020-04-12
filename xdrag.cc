/*
 *  Xdrag.cc - Drag-and-drop under X.
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


#include "xdrag.h"

#if defined(USE_EXULTSTUDIO) && !defined(_WIN32)

#include <iostream>         /* Debugging messages */

using std::cout;
using std::endl;

/*
 *  Get a window's screen coords.
 */

static void Get_window_coords(
    Display *display,
    Window win,
    int &sx, int &sy        // Coords. returned.
) {
	Window root;
	Window parent;        // Get parent window.
	Window *children;
	unsigned int nchildren;
	XQueryTree(display, win, &root, &parent, &children, &nchildren);
	if (children)
		XFree(children);
	if (parent && parent != root)   // Recurse on parent.
		Get_window_coords(display, parent, sx, sy);
	else
		sx = sy = 0;
	XWindowAttributes atts;     // Get position within parent.
	XGetWindowAttributes(display, win, &atts);
	sx += atts.x;
	sy += atts.y;
}

/*
 *  Initialize.
 */

Xdnd::Xdnd(
    Display *d,
    Window xw,          // Window-manager window.
    Window xgw,         // Game's display window in xw.
    Move_shape_handler_fun movefun,
    Move_combo_handler_fun movecmbfun,
    Drop_shape_handler_fun shapefun,
    Drop_chunk_handler_fun cfun,
    Drop_npc_handler_fun npcfun,
    Drop_combo_handler_fun cmbfun
) : display(d), xwmwin(xw), xgamewin(xgw),
	num_types(0), lastx(-1), lasty(-1),
	file(-1), shape(-1), frame(-1), chunknum(-1), npcnum(-1),
	combo_cnt(-1), combo_xtiles(0), combo_ytiles(0), combo(nullptr),
	data_valid(false), move_shape_handler(movefun),
	move_combo_handler(movecmbfun),
	shape_handler(shapefun), chunk_handler(cfun),
	npc_handler(npcfun), combo_handler(cmbfun) {
	shapeid_atom = XInternAtom(display, U7_TARGET_SHAPEID_NAME, 0);
	chunkid_atom = XInternAtom(display, U7_TARGET_CHUNKID_NAME, 0);
	npcid_atom = XInternAtom(display, U7_TARGET_NPCID_NAME, 0);
	comboid_atom = XInternAtom(display, U7_TARGET_COMBOID_NAME, 0);
	// Atom for Xdnd protocol:
	xdnd_aware = XInternAtom(display, "XdndAware", 0);
	xdnd_enter = XInternAtom(display, "XdndEnter", 0);
	xdnd_leave = XInternAtom(display, "XdndLeave", 0);
	xdnd_position = XInternAtom(display, "XdndPosition", 0);
	xdnd_drop = XInternAtom(display, "XdndDrop", 0);
	xdnd_status = XInternAtom(display, "XdndStatus", 0);
	xdnd_copy = XInternAtom(display, "XdndActionCopy", 0);
	xdnd_ask = XInternAtom(display, "XdndActionAsk", 0);
	xdnd_typelist = XInternAtom(display, "XdndTypeList", 0);
	xdnd_selection = XInternAtom(display, "XdndSelection", 0);
	xdnd_version = 3;
	// Create XdndAware property.
	if (xwmwin)
		XChangeProperty(display, xwmwin, xdnd_aware, XA_ATOM, 32,
		                PropModeReplace, reinterpret_cast<unsigned char *>(
		                    &xdnd_version), 1);
}

/*
 *  Cleanup.
 */

Xdnd::~Xdnd(
) {
	delete combo;
}

/*
 *  Handle drag-and-drop.
 */

void Xdnd::client_msg(
    XClientMessageEvent &cev    // Message received.
) {
	XEvent xev;         // Return event.
	xev.xclient.type = ClientMessage;
	Window drag_win = cev.data.l[0];// Where drag comes from.
	xev.xclient.format = 32;
	xev.xclient.window = drag_win;
	xev.xclient.data.l[0] = xwmwin;
	if (cev.message_type == xdnd_enter) {
		data_valid = false;
		if (cev.data.l[1] & 1) { // More than 3 types?
			Atom type;
			int format;
			unsigned long nitems;
			unsigned long after;
			Atom *data;
			XGetWindowProperty(display, drag_win,
			                   xdnd_typelist, 0, 65536,
			                   false, XA_ATOM, &type, &format, &nitems,
			                   &after, reinterpret_cast<unsigned char **>(
			                       &data));
			if (format != 32 || type != XA_ATOM)
				return; // No good.
			if (nitems > max_types)
				nitems = max_types;
			for (num_types = 0; num_types < nitems; num_types++)
				drag_types[num_types] = data[num_types];
		} else {
			num_types = 0;
			for (int i = 0; i < 3; i++)
				if (cev.data.l[2 + i])
					drag_types[num_types++] =
					    cev.data.l[2 + i];
			cout << "num_types = " << num_types << endl;
		}
		// Save current window coords.
		Get_window_coords(display, xgamewin, winx, winy);
	} else if (cev.message_type == xdnd_position) {
		unsigned long i;            // For now, just do shapeid.
		for (i = 0; i < num_types; i++)
			if (drag_types[i] == shapeid_atom ||
			        drag_types[i] == chunkid_atom ||
			        drag_types[i] == npcid_atom ||
			        drag_types[i] == comboid_atom)
				break;
		xev.xclient.message_type = xdnd_status;
		// Flags??:  3=good, 0=can't accept.
		xev.xclient.data.l[1] = i < num_types ? 3 : 0;
		// I think next 2 should be a rect.??
		xev.xclient.data.l[2] = 0;
		xev.xclient.data.l[3] = 0;
		xev.xclient.data.l[4] = xdnd_copy;
		XSendEvent(display, drag_win, false, 0, &xev);
		// Save mouse position.
		int x = ((cev.data.l[2] >> 16) & 0xffff) - winx;
		int y = (cev.data.l[2] & 0xffff) - winy;
		// Get timestamp.
		unsigned long time = 0; //????++++++++++++++++
		if (i == num_types)
			return;
		if (!data_valid)    // Tell owner we want data.
			XConvertSelection(display, xdnd_selection,
			                  drag_types[i], xdnd_selection, xwmwin, time);
		else if (file == U7_SHAPE_SHAPES)
			(*move_shape_handler)(shape, frame, x, y,
			                      lastx, lasty, true);
		else if (combo_cnt > 0)
			(*move_combo_handler)(combo_xtiles, combo_ytiles,
			                      combo_tiles_right, combo_tiles_below,
			                      x, y, lastx, lasty, true);
		lastx = x;
		lasty = y;
	} else if (cev.message_type == xdnd_leave) {
		num_types = 0;      // Clear list.
		// Force repaint to clear grid.
		(*move_shape_handler)(-1, -1, 0, 0, lastx, lasty, true);
		data_valid = false;
	} else if (cev.message_type == xdnd_drop) {
		unsigned long i;
		for (i = 0; i < num_types; i++)
			if (drag_types[i] == shapeid_atom ||
			        drag_types[i] == chunkid_atom ||
			        drag_types[i] == npcid_atom ||
			        drag_types[i] == comboid_atom)
				break;
		bool okay = data_valid && i < num_types;
		num_types = 0;
		if (!okay)
			return;
		if (shape >= 0) {   // Dropping a shape?
			if (file == U7_SHAPE_SHAPES)
				// For now, just allow "shapes.vga".
				(*shape_handler)(shape, frame, lastx, lasty, nullptr);
		} else if (chunknum >= 0) // A whole chunk.
			(*chunk_handler)(chunknum, lastx, lasty, nullptr);
		else if (npcnum >= 0)   // An NPC.
			(*npc_handler)(npcnum, lastx, lasty, nullptr);
		else if (combo_cnt >= 0 && combo)
			(*combo_handler)(combo_cnt, combo, lastx, lasty, nullptr);

		data_valid = false;
	}
}

/*
 *  Get the selection data.
 */

void Xdnd::select_msg(
    XSelectionEvent &sev
) {
	cout << "SelectionEvent received with target type: " <<
	     XGetAtomName(display, sev.target) << endl;
	if (sev.selection != xdnd_selection ||
	        (sev.target != shapeid_atom && sev.target != chunkid_atom &&
	         sev.target != npcid_atom && sev.target != comboid_atom) ||
	        sev.property == None)
		return;         // Wrong type.
	cout << "HERE" << endl;
	file = shape = frame = -1;  // Invalidate old data.
	chunknum = npcnum = -1;
	combo_cnt = -1;
	combo_xtiles = combo_ytiles = 0;
	delete combo;
	combo = nullptr;
	Atom type = None;       // Get data.
	int format;
	unsigned long nitems;
	unsigned long after;
	unsigned char *data;
	if (XGetWindowProperty(display, sev.requestor, sev.property,
	                       0, 65000, False, AnyPropertyType,
	                       &type, &format, &nitems, &after, &data) != Success) {
		cout << "Error in getting selection" << endl;
		return;
	}
	if (sev.target == shapeid_atom) { // Dropping a shape?
		// Get shape info.
		Get_u7_shapeid(data, file, shape, frame);
		data_valid = true;
	} else if (sev.target == chunkid_atom) {
		// A whole chunk.
		Get_u7_chunkid(data, chunknum);
		data_valid = true;
	} else if (sev.target == npcid_atom) {
		Get_u7_npcid(data, npcnum);
		data_valid = true;
	} else if (sev.target == comboid_atom) {
		Get_u7_comboid(data, combo_xtiles, combo_ytiles,
		               combo_tiles_right, combo_tiles_below, combo_cnt, combo);
		cout << "Combo: xtiles=" << combo_xtiles << ", ytiles=" << combo_ytiles <<
		     ", tiles_right=" << combo_tiles_right << ", tiles_below=" <<
		     combo_tiles_below << endl;
		data_valid = true;
	}
	XFree(data);
}


#endif  /* USE_EXULTSTUDIO */


