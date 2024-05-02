
/*
Copyright (C) 2003  The Pentagram Team
Copyright (C) 2010-2022  The Exult Team

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

#ifndef XMIDIEVENT_H_INCLUDED
#define XMIDIEVENT_H_INCLUDED

// Midi Status Bytes
#define MIDI_STATUS_NOTE_OFF    0x8
#define MIDI_STATUS_NOTE_ON     0x9
#define MIDI_STATUS_AFTERTOUCH  0xA
#define MIDI_STATUS_CONTROLLER  0xB
#define MIDI_STATUS_PROG_CHANGE 0xC
#define MIDI_STATUS_PRESSURE    0xD
#define MIDI_STATUS_PITCH_WHEEL 0xE
#define MIDI_STATUS_SYSEX       0xF

//
// XMidiFile Controllers
//

//
// Channel Lock	(110)
//  < 64 : Release Lock
// >= 64 : Lock an unlocked unprotected physical channel to be used exclusively
//         by this logical channel. Traditionally the physical channel would be
//         bettween 1 and 9, and the logical channel between 11 and 16
//
// When a channel is locked, any notes already playing on it are turned off.
// When the lock is released, the previous state of the channel is restored.
// Locks are automatically released when the sequences finishes playing
//
#define XMIDI_CONTROLLER_CHAN_LOCK 0x6e

#define MIDI_CONTROLLER_VOLUME            7
#define MIDI_CONTROLLER_BANK              0
#define MIDI_CONTROLLER_MODWHEEL          1
#define MIDI_CONTROLLER_FOOTPEDAL         4
#define MIDI_CONTROLLER_PAN               9
#define MIDI_CONTROLLER_BALANCE           10
#define MIDI_CONTROLLER_EXPRESSION        11
#define MIDI_CONTROLLER_SUSTAIN           64
#define MIDI_CONTROLLER_EFFECT            91
#define MIDI_CONTROLLER_CHORUS            93
#define XMIDI_CONTROLLER_CHAN_LOCK_PROT   0x6f    // Channel Lock Protect
#define XMIDI_CONTROLLER_VOICE_PROT       0x70    // Voice Protect
#define XMIDI_CONTROLLER_TIMBRE_PROT      0x71    // Timbre Protect
#define XMIDI_CONTROLLER_BANK_CHANGE      0x72    // Bank Change
#define XMIDI_CONTROLLER_IND_CTRL_PREFIX  0x73    // Indirect Controller Prefix
#define XMIDI_CONTROLLER_FOR_LOOP         0x74    // For Loop
#define XMIDI_CONTROLLER_NEXT_BREAK       0x75    // Next/Break
#define XMIDI_CONTROLLER_CLEAR_BB_COUNT   0x76    // Clear Beat/Bar Count
#define XMIDI_CONTROLLER_CALLBACK_TRIG    0x77    // Callback Trigger
#define XMIDI_CONTROLLER_SEQ_BRANCH_INDEX 0x78    // Sequence Branch Index

#include "common_types.h"

#include <ostream>
// Maximum number of for loops we'll allow (used by LowLevelMidiDriver)
// The specs say 4, so that is what we;ll use
#define XMIDI_MAX_FOR_LOOP_COUNT 4

struct XMidiEvent {
	int           time;
	unsigned char status;

	unsigned char data[2];

	union {
		struct {
			uint32         len;       // Length of SysEx Data
			unsigned char* buffer;    // SysEx Data
		} sysex_data;

		struct {
			int         duration;     // Duration of note (120 Hz)
			XMidiEvent* next_note;    // The next note on the stack
			uint32 note_time;    // Time note stops playing (6000th of second)
			uint8  actualvel;    // Actual velocity of playing note
		} note_on;

		struct {
			XMidiEvent* next_branch;    // Next branch index contoller
		} branch_index;

	} ex;

	XMidiEvent* next;

	XMidiEvent* next_patch_bank;    // next patch or bank change event

	void FreeThis() {
		// Free all our children first. Using a loop instead of recursive
		// because it could have nasty effects on the stack otherwise
		for (XMidiEvent* e = next; e; e = next) {
			next    = e->next;
			e->next = nullptr;
			e->FreeThis();
		}

		// We only do this with sysex
		if ((status >> 4) == 0xF && ex.sysex_data.buffer) {
			delete[] ex.sysex_data.buffer;
		}
		delete this;
	}

	void DumpText(std::ostream& out) {
		out << time << " Ch " << (static_cast<int>(status) & 0xf) << " ";

		switch (status >> 4) {
		case MIDI_STATUS_NOTE_OFF:
			out << "Note Off" << static_cast<int>(data[0]);
			break;
		case MIDI_STATUS_NOTE_ON:
			out << "Note On " << static_cast<int>(data[0]) << " "
				<< static_cast<int>(data[1]);
			if (ex.note_on.duration) {
				out << " d " << ex.note_on.duration;
			}
			break;

		case MIDI_STATUS_AFTERTOUCH:
			out << "Aftertouch " << static_cast<int>(data[0]) << " "
				<< static_cast<int>(data[1]);
			break;
		case MIDI_STATUS_PROG_CHANGE:
			out << "Program Change " << static_cast<int>(data[0]) << " "
				<< static_cast<int>(data[1]);
			break;
		case MIDI_STATUS_PRESSURE:
			out << "Pressure " << static_cast<int>(data[0]) << " "
				<< static_cast<int>(data[1]);
			break;
		case MIDI_STATUS_PITCH_WHEEL:
			out << "Pitch Wheel " << static_cast<int>(data[0]) << " "
				<< static_cast<int>(data[1]);
			break;
		case MIDI_STATUS_CONTROLLER: {
			switch (data[0]) {
			case MIDI_CONTROLLER_VOLUME:
				out << "Controller Volume " << static_cast<int>(data[1]);
				break;
			case MIDI_CONTROLLER_VOLUME + 32:
				out << "Controller Volume Fine" << static_cast<int>(data[1]);
				break;
			case MIDI_CONTROLLER_BANK:
				out << "Controller Bank " << static_cast<int>(data[1]);
				break;
			case MIDI_CONTROLLER_BANK + 32:
				out << "Controller Bank Fine" << static_cast<int>(data[1]);
				break;
			case MIDI_CONTROLLER_MODWHEEL:
				out << "Controller Bank " << static_cast<int>(data[1]);
				break;
			case MIDI_CONTROLLER_MODWHEEL + 32:
				out << "Controller Bank Fine" << static_cast<int>(data[1]);
				break;
			case MIDI_CONTROLLER_FOOTPEDAL:
				out << "Controller FootPedal " << static_cast<int>(data[1]);
				break;
			case MIDI_CONTROLLER_FOOTPEDAL + 32:
				out << "Controller Foot Pedal Fine"
					<< static_cast<int>(data[1]);
				break;
			case MIDI_CONTROLLER_PAN:
				out << "Controller Pan " << static_cast<int>(data[1]);
				break;
			case MIDI_CONTROLLER_PAN + 32:
				out << "Controller Pan Fine" << static_cast<int>(data[1]);
				break;
			case MIDI_CONTROLLER_BALANCE:
				out << "Controller Balance " << static_cast<int>(data[1]);
				break;
			case MIDI_CONTROLLER_BALANCE + 32:
				out << "Controller Balance Fine" << static_cast<int>(data[1]);
				break;
			case MIDI_CONTROLLER_EXPRESSION:
				out << "Controller Expression " << static_cast<int>(data[1]);
				break;
			case MIDI_CONTROLLER_EXPRESSION + 32:
				out << "Controller Expression Fine"
					<< static_cast<int>(data[1]);
				break;
			case MIDI_CONTROLLER_SUSTAIN:
				out << "Controller Sustain " << static_cast<int>(data[1]);
				break;
			case MIDI_CONTROLLER_EFFECT:
				out << "Controller Effect " << static_cast<int>(data[1]);
				break;
			case MIDI_CONTROLLER_CHORUS:
				out << "Controller Chorus " << static_cast<int>(data[1]);
				break;
			case XMIDI_CONTROLLER_CHAN_LOCK_PROT:
				out << "Controller XChannel Lock Protect "
					<< static_cast<int>(data[1]);
				break;
			case XMIDI_CONTROLLER_VOICE_PROT:
				out << "Controller XVoice Protect "
					<< static_cast<int>(data[1]);
				break;
			case XMIDI_CONTROLLER_TIMBRE_PROT:
				out << "Controller XTimbre Protect "
					<< static_cast<int>(data[1]);
				break;
			case XMIDI_CONTROLLER_BANK_CHANGE:
				out << "Controller XBank Change " << static_cast<int>(data[1]);
				break;
			case XMIDI_CONTROLLER_IND_CTRL_PREFIX:
				out << "Controller XIndirect Controller Prefix "
					<< static_cast<int>(data[1]);
				break;
			case XMIDI_CONTROLLER_FOR_LOOP:
				out << "Controller XFor Loop " << static_cast<int>(data[1]);
				break;
			case XMIDI_CONTROLLER_NEXT_BREAK:
				out << "Controller XNext/Break " << static_cast<int>(data[1]);
				break;
			case XMIDI_CONTROLLER_CLEAR_BB_COUNT:
				out << "Controller XClear Beat/Bar Count "
					<< static_cast<int>(data[1]);
				break;
			case XMIDI_CONTROLLER_CALLBACK_TRIG:
				out << "Controller XCallback Trigger "
					<< static_cast<int>(data[1]);
				break;
			case XMIDI_CONTROLLER_SEQ_BRANCH_INDEX:
				out << "Controller XSequence Branch Index "
					<< static_cast<int>(data[1]);
				break;

			default:
				out << "Controller " << static_cast<int>(data[0]) << " "
					<< static_cast<int>(data[1]);
			}
		} break;

		case MIDI_STATUS_SYSEX:
			out << "sysex " << ex.sysex_data.len << " bytes";
			break;

			// default should never happen
		default:
			out << "Status_" << (static_cast<int>(status) >> 4) << " "
				<< static_cast<int>(data[0]) << " "
				<< static_cast<int>(data[1]);
		}

		out << std::endl;
		if (next) {
			next->DumpText(out);
		}
	}

	// time critical events cannot have their time shifted
	// non time critcal events can be moved as long as they are played in order
	// pretty much time critical are notes and events that affect playing notes
	bool is_time_critical() {
		int type = status >> 4;
		if (type == MIDI_STATUS_CONTROLLER) {
			switch (data[0]) {
			case MIDI_CONTROLLER_BANK:
			case MIDI_CONTROLLER_BANK + 32:
			case XMIDI_CONTROLLER_BANK_CHANGE:
			case XMIDI_CONTROLLER_CHAN_LOCK_PROT:
			case XMIDI_CONTROLLER_VOICE_PROT:
			case XMIDI_CONTROLLER_TIMBRE_PROT:
				return false;

			default:
				return true;
			}
		}
		// these shouldn't be time critical. needed to fix BG21 and SI12
		if (type == MIDI_STATUS_PROG_CHANGE || type == MIDI_STATUS_SYSEX) {
			return false;
		}
		return true;
	}
};

#endif    // XMIDIEVENT_H_INCLUDED
