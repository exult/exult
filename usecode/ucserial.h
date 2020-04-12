/*
 *  ucserial.h - Serialization of usecode objects
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

#ifndef UCSERIAL_H
#define UCSERIAL_H

#include "objserial.h"

class Usecode_value;

extern int Stack_frame_out(
    int fd,             // Socket.
    int functionid,
    int ip,
    int call_chain,
    int call_depth,
    int eventid,
    int caller_item,
    int num_args,
    int num_vars,
    Usecode_value *locals
);

extern bool Stack_frame_in(
    const unsigned char *data,        // Data that was read.
    int datalen,            // Length of data.
    int &functionid,
    int &ip,
    int &call_chain,
    int &call_depth,
    int &eventid,
    int &caller_item,
    int &num_args,
    int &num_vars,
    Usecode_value *&locals
);

#endif
