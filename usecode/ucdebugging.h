/*
 *  ucdebugging.h - Debugging-related functions for usecode
 *
 *  Copyright (C) 2002-2022  The Exult Team
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

#ifndef UCDEBUGGING_H
#define UCDEBUGGING_H

#include "ignore_unused_variable_warning.h"

#include <list>

class Stack_frame;

enum Breakpoint_type {
	BP_anywhere,
	BP_location,
	BP_stepover,
	BP_finish
};

// BP_anywhere: break on any instruction (for "step into")
// BP_location: break on a specific code offset (function + ip)
// BP_stepover: break on next instruction in same function (or previous func)
// BP_finish  : finish current function and break in previous function

// other (future) possibilities: break on access to local/global vars,
//    break when usecode is run on a specific object,
//    break on errors,
//    break on a special 'break to debugger' opcode,
//    break on a specific or any intrinsic call

class Breakpoint {
public:
	int  id;      // used to identify breakpoint to remote debugger
	bool once;    // delete when triggered

	virtual Breakpoint_type get_type() const = 0;
	virtual ~Breakpoint()                    = default;

	virtual bool check(Stack_frame* frame) const = 0;

	virtual void serialize(int fd) const = 0;

protected:
	Breakpoint(bool once);
};

class AnywhereBreakpoint : public Breakpoint {
public:
	AnywhereBreakpoint();

	Breakpoint_type get_type() const override {
		return BP_anywhere;
	}

	bool check(Stack_frame* frame) const override {
		ignore_unused_variable_warning(frame);
		return true;
	}

	void serialize(int fd) const override {
		ignore_unused_variable_warning(fd);
	}    // +++++ implement this?
};

class LocationBreakpoint : public Breakpoint {
public:
	LocationBreakpoint(int functionid, int ip, bool once = false);

	Breakpoint_type get_type() const override {
		return BP_location;
	}

	bool check(Stack_frame* frame) const override;

	void serialize(int fd) const override;

private:
	int functionid;
	int ip;
};

class StepoverBreakpoint : public Breakpoint {
public:
	StepoverBreakpoint(Stack_frame* frame);

	Breakpoint_type get_type() const override {
		return BP_stepover;
	}

	bool check(Stack_frame* frame) const override;

	void serialize(int fd) const override {
		ignore_unused_variable_warning(fd);
	}    // +++++ implement this?

private:
	int call_chain;
	int call_depth;
};

class FinishBreakpoint : public Breakpoint {
public:
	FinishBreakpoint(Stack_frame* frame);

	Breakpoint_type get_type() const override {
		return BP_finish;
	}

	bool check(Stack_frame* frame) const override;

	void serialize(int fd) const override {
		ignore_unused_variable_warning(fd);
	}    // +++++ implement this?

private:
	int call_chain;
	int call_depth;
};

class Breakpoints {
public:
	~Breakpoints();

	void add(Breakpoint* breakpoint);
	void remove(Breakpoint* breakpoint);
	bool remove(int id);

	int check(Stack_frame* frame);

	void transmit(int fd);

	static int getNewID() {
		return lastID++;
	}

private:
	static int lastID;

	std::list<Breakpoint*> breaks;
};

#endif
