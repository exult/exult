/*
 *  ucdebugging.cc - Debugging-related functions for usecode
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "ucdebugging.h"
#include "ucinternal.h"
#include "stackframe.h"
#include "ucfunction.h"
#include "utils.h"

#include "servemsg.h"
#include "debugmsg.h"

Breakpoint::Breakpoint(bool once) {
	this->once = once;
	id = Breakpoints::getNewID();
}

AnywhereBreakpoint::AnywhereBreakpoint()
	: Breakpoint(true)
{ }

LocationBreakpoint::LocationBreakpoint(int functionid, int ip, bool once)
	: Breakpoint(once) {
	this->functionid = functionid;
	this->ip = ip;
}

bool LocationBreakpoint::check(Stack_frame *frame) const {
	return (frame->function->id == functionid) &&
	       (static_cast<int>(frame->ip - frame->code) == ip);
}

void LocationBreakpoint::serialize(int fd) const {
	unsigned char d[13];
	d[0] = static_cast<unsigned char>(Exult_server::dbg_set_location_bp);
	unsigned char *dptr = &d[1];
	Write4(dptr, functionid);
	Write4(dptr, ip);
	Write4(dptr, id);
	Exult_server::Send_data(fd, Exult_server::usecode_debugging, d, 13);
}

StepoverBreakpoint::StepoverBreakpoint(Stack_frame *frame)
	: Breakpoint(true) {
	call_chain = frame->call_chain;
	call_depth = frame->call_depth;
}

bool StepoverBreakpoint::check(Stack_frame *frame) const {
	return (frame->call_chain == call_chain &&
	         frame->call_depth <= call_depth) ||
	        (frame->call_chain < call_chain);
}

FinishBreakpoint::FinishBreakpoint(Stack_frame *frame)
	: Breakpoint(true) {
	call_chain = frame->call_chain;
	call_depth = frame->call_depth;
}

bool FinishBreakpoint::check(Stack_frame *frame) const {
	return (frame->call_chain == call_chain &&
	         frame->call_depth < call_depth) ||
	        (frame->call_chain < call_chain);
}



int Breakpoints::lastID = 0;

Breakpoints::~Breakpoints() {
	std::list<Breakpoint *>::iterator iter;

	// clear queue
	for (iter = breaks.begin(); iter != breaks.end(); ++iter) {
		delete(*iter);
	}
}


// returns ID of a (any) breakpoint encountered, or -1 if no breakpoints
int Breakpoints::check(Stack_frame *frame) {
	// check all breakpoints (always check all of them, even if match found)
	// and delete the matching ones with 'once' set

	// question: do we really want to delete a breakpoint (with once set)
	// as soon as it is triggered? (or wait a while until it has been
	// processed properly?)
	// also, it might be necessary to return a list of all matching and/or
	// deleted breakpoints
	// OTOH, which 'once' breakpoint is hit is generally not interesting,
	// and also, only one of these will be set, usually.

	int breakID = -1;

	std::list<Breakpoint *>::iterator iter;

	for (iter = breaks.begin(); iter != breaks.end(); ++iter) {
		if ((*iter)->check(frame)) {
			breakID = (*iter)->id;
			if ((*iter)->once) {
				delete(*iter);
				(*iter) = nullptr;
			}
		}
	}

	breaks.remove(nullptr); // delete all nullptr from the list

	return breakID;
}

void Breakpoints::add(Breakpoint *breakpoint) {
	breaks.remove(breakpoint); // avoid double occurences

	breaks.push_back(breakpoint);
}

void Breakpoints::remove(Breakpoint *breakpoint) {
	breaks.remove(breakpoint);
}

bool Breakpoints::remove(int id) {
	bool found = false;

	std::list<Breakpoint *>::iterator iter;

	for (iter = breaks.begin(); iter != breaks.end(); ++iter) {
		if ((*iter)->id == id) {
			found = true;
			delete(*iter);
			(*iter) = nullptr;
		}
	}

	breaks.remove(nullptr); // delete all nullptr from the list

	return found;
}

void Breakpoints::transmit(int fd) {
	std::list<Breakpoint *>::iterator iter;

	for (iter = breaks.begin(); iter != breaks.end(); ++iter)
		(*iter)->serialize(fd);
}
