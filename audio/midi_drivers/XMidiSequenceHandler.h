/*
Copyright (C) 2003-2005  The Pentagram Team
Copyright (C) 2019-2022  The Exult Team

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

#ifndef SEQUENCEHANDLER_H_INCLUDED
#define SEQUENCEHANDLER_H_INCLUDED

#include "common_types.h"

struct XMidiEvent;

//! Abstract class for handling the playing of XMidiSequence objects
class XMidiSequenceHandler {
public:
	virtual ~XMidiSequenceHandler() = default;

	//! An event sent from a sequence to play
	//! \param sequence_id The id of the sequence that is attempting to send the event
	//! \param message The Event being sent
	virtual void sequenceSendEvent(uint16 sequence_id, uint32 message) = 0;

	//! Send a SysEx event from a Sequence
	//! \param sequence_id The id of the sequence that is attempting to send the event
	//! \param status The Status Byte of the SysEx event
	//! \param msg The SysEx data (including terminator byte)
	//! \param length The number of bytes of SysEx data to send
	virtual void sequenceSendSysEx(
			uint16 sequence_id, uint8 status, const uint8* msg, uint16 length)
			= 0;

	//! An event sent from a sequence to play
	//! \param sequence_id The id of the sequence requesting the tick count
	//! \return Number of ticks, in 6000ths of a second
	virtual uint32 getTickCount(uint16 sequence_id) = 0;

	//! Handle an XMIDI Callback Trigger Event
	//! \param sequence_id The id of the sequence doing the callback
	//! \param data data[1] of the MidiController::XCallbackTrigger event
	virtual void handleCallbackTrigger(uint16 sequence_id, uint8 data) = 0;

	//! Get global volume
	//! \returns Current Volume [0-100]
	virtual int getGlobalVolume() = 0;
};

#endif    // SEQUENCEHANDLER_H_INCLUDED
