/*
Copyright (C) 2003  The Pentagram Team
Copyright (C) 2010-2025  The Exult Team

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

// Tab Size = 4

#ifndef XMIDIEVENTLIST_H_INCLUDED
#define XMIDIEVENTLIST_H_INCLUDED

class ODataSource;

#include "XMidiEvent.h"
#include "XMidiRecyclable.h"

class XMidiEventList : public XMidiRecyclable<XMidiEventList> {
	int    counter = 0;
	uint32 length  = 0;

	// Helper funcs for Write
	int    putVLQ(ODataSource* dest, uint32 value);
	uint32 convertListToMTrk(ODataSource* dest);

public:
	uint16      chan_mask = 0;
	XMidiEvent* events    = nullptr;

	//! XMidiFile Seq Branch Index controllers. For Ult126 = loop start
	XMidiEvent* branches = nullptr;

	//! Patch and Bank change events
	XMidiEvent* x_patch_bank = nullptr;

	XMidiEventList() {}

	//! Write the list to a DataSource
	int write(ODataSource* dest);

	//! Increments the counter
	void incrementCounter() {
		counter++;
	}

	//! Decrement the counter and delete the event list, if possible
	void decrementCounter();

	uint32 getLength();

	void setLength(uint32 length) {
		this->length = length;
	}

	//! Find the Sequence Branch Event for the index
	//! \param index The index to search for
	//! \return The event found, or 0
	XMidiEvent* findBranchEvent(int index) {
		XMidiEvent* b = branches;

		while (b) {
			if (b->data[1] == (index & 0xFF)) {
				break;
			}
			b = b->ex.branch_index.next_branch;
		}

		return b;
	}
};

// needed explicit instantiation declaration to supress warnings from clang
template <>
XMidiRecyclable<XMidiEventList>::FreeList
		XMidiRecyclable<XMidiEventList>::FreeList::instance;

#endif    // XMIDIEVENTLIST_H_INCLUDED
