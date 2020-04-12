/*
 *  debugmsg.h: debugging messages
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

#ifndef DEBUGMSG_H
#define DEBUGMSG_H

namespace Exult_server {

enum Debug_msg_type {
    // c->s = client to server, s->c = server to client, * = both ways

    dbg_continue = 0,        // (c->s) continue running
    dbg_stepinto = 1,        // (c->s) step (into calls)
    dbg_stepover = 2,        // (c->s) step (over calls)
    dbg_finish = 3,          // (c->s) finish current function
    dbg_break = 4,           // (c->s) break imediately
    dbg_on_breakpoint = 5,   // (s->c) hit a breakpoint
    dbg_get_callstack = 6,   // (c->s) request callstack
    dbg_callstack = 7,       // (s->c) sending callstack
    dbg_stackframe = 8,      // (s->c) sending stack frame
    dbg_get_stack = 9,       // (c->s) request (data) stack
    dbg_stack = 10,          // (s->c) sending stack
    dbg_continuing = 11,     // (s->c) continuing from breakpoint
    dbg_get_status = 12,     // (c->s) request status (breakpoint/running)
    dbg_set_location_bp = 13,// ( *  ) set a breakpoint at a specific location
    dbg_clear_breakpoint = 14, // ( *  ) clear a breakpoint
    dbg_get_breakpoints = 15 // (c->s) request all (permanent) breakpoints
};

}
#endif
