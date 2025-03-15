
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
enum class MidiStatus {
	NoteOff      = 0x80,
	NoteOn       = 0x90,
	Aftertouch   = 0xa0,
	Controller   = 0xb0,
	Program      = 0xc0,
	ChannelTouch = 0xd0,
	PitchWheel   = 0xe0,
	Sysex        = 0xf0,
	ChannelMask  = 0xf,
	TypeMask     = 0xf0,
};

//
// Midi status is 2 4 bit values so it is really useful if we have operator & to
// do masking
//

template <typename Tint>
MidiStatus operator&(Tint left, MidiStatus right) {
	return MidiStatus(left & (int(right)));
}

template <typename Tint>
MidiStatus operator&(MidiStatus left, Tint right) {
	return MidiStatus(left & (int(right)));
}

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
enum class MidiController {
	FineOffset           = 32,
	Volume               = 7,
	VolumeFine           = Volume + FineOffset,
	Bank                 = 0,
	BankFine             = Bank + FineOffset,
	ModWheel             = 1,
	ModWheelFine         = ModWheel + FineOffset,
	FootPedal            = 4,
	FootPedalFine        = FootPedal + FineOffset,
	Pan                  = 9,
	PanFine              = Pan + FineOffset,
	Balance              = 10,
	BalanceFine          = Balance + FineOffset,
	Expression           = 11,
	ExpressionFine       = Expression + FineOffset,
	Sustain              = 64,
	Effect               = 91,
	Chorus               = 93,
	XChanLock            = 0x6e,    // XMIDI Channel Lock
	XChanLockProtect     = 0x6f,    // XMIDI Channel Lock Protect
	XVoiceProtect        = 0x70,    // XMIDI Voice Protect
	XTimbreProtect       = 0x71,    // XMIDI Timbre Protect
	XBankChange          = 0x72,    // XMIDI Bank Change
	XIndirectCtrlPrefix  = 0x73,    // XMIDI Indirect Controller Prefix
	XForLoop             = 0x74,    // XMIDI For Loop
	XNextBreak           = 0x75,    // XMIDI Next/Break
	XClearBBCount        = 0x76,    // XMIDI Clear Beat/Bar Count
	XCallbackTrigger     = 0x77,    // XMIDI Callback Trigger
	XSequenceBranchIndex = 0x78,    // XMIDI Sequence Branch Index
	ResetControllers     = 0x79,    // Immediately reset all controllers
	AllNotesOff = 0x7B,    // Immediately stop all notes excluding sustained
};

#include "XMidiRecyclable.h"
#include "common_types.h"
#include "span.h"

#include <ostream>
// Maximum number of for loops we'll allow (used by LowLevelMidiDriver)
// The specs say 4, so that is what we'll use
#define XMIDI_MAX_FOR_LOOP_COUNT 4

struct XMidiEvent : public XMidiRecyclable<XMidiEvent> {
	int           time   = 0;
	unsigned char status = 0;

	MidiStatus getStatusType() {
		return status & MidiStatus::TypeMask;
	}

	int getChannel() {
		return status & int(MidiStatus::ChannelMask);
	}

	// XMidiEvent()
	//{
	//  Zero out the ex union
	// std::memset(&ex, 0, sizeof(ex));
	//}

	unsigned char data[2] = {0, 0};

	union {
		struct {
			uint32 len;    // Length of SysEx Data

			// SysEx Data , uses fixed sized buffer if len fits in fixed or uses
			// dynamic if more needed
			union {
				uint8* dynamic;
				uint8  fixed[32];    // Fixed size array for SysEx Data
			} buffer_u;

			uint8* buffer() {
				if (len == 0) {
					return nullptr;
				} else if (len <= std::size(buffer_u.fixed)) {
					return buffer_u.fixed;
				} else {
					return buffer_u.dynamic;
				}
			}

			tcb::span<uint8> as_span() {
				return tcb::span(buffer(), len);
			}

			void free_buffer() {
				set_len(0);
			}

			uint8* set_len(uint32 newlen) {
				// if no change do nothing
				if (newlen == len) {
					return buffer();
				}

				bool olddynamic = len > std::size(buffer_u.fixed);
				bool newdynamic = newlen > std::size(buffer_u.fixed);
				// free existing dynamic buffer if it is too small or not needed
				if (olddynamic && (newlen > len || !newdynamic)) {
					delete[] buffer_u.dynamic;
				}

				// allocate a new dynamic buffer if needed
				if (newdynamic && newlen > len) {
					buffer_u.dynamic = new unsigned char[newlen];
					CERR("allocating dynamic sysex buffer of size " << newlen);
				}
				len = newlen;
				return buffer();
			}
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

	} ex = {};

	XMidiEvent* next_patch_bank = nullptr;    // next patch or bank change event

	void FreeThis() {
		// Free all our children first. Using a loop instead of recursive
		// because it could have nasty effects on the stack otherwise
		for (XMidiEvent* e = next; e; e = next) {
			next    = e->next;
			e->next = nullptr;
			e->FreeThis();
		}
		// clear our next pointer
		next = nullptr;

		// We only do this with sysex
		if (getStatusType() == MidiStatus::Sysex) {
			ex.sysex_data.free_buffer();
		}
		XMidiRecyclable::FreeThis();
	}

	void DumpText(std::ostream& out) {
		out << time * 25 / 3 << " Ch " << getChannel() << " ";

		switch (getStatusType()) {
		case MidiStatus::NoteOff:
			out << "Note Off" << int(data[0]);
			break;
		case MidiStatus::NoteOn:
			out << "Note On " << int(data[0]) << " " << int(data[1]);
			if (ex.note_on.duration) {
				out << " Off at " << (time + ex.note_on.duration) * 25 / 3;
			}
			break;

		case MidiStatus::Aftertouch:
			out << "Aftertouch " << int(data[0]) << " " << int(data[1]);
			break;
		case MidiStatus::Program:
			out << "Program Change " << int(data[0]) << " " << int(data[1]);
			break;
		case MidiStatus::ChannelTouch:
			out << "Channel Touch " << int(data[0]) << " " << int(data[1]);
			break;
		case MidiStatus::PitchWheel:
			out << "Pitch Wheel " << int(data[0]) << " " << int(data[1]);
			break;
		case MidiStatus::Controller: {
			switch (MidiController(data[0])) {
			case MidiController::Volume:
				out << "Controller Volume " << int(data[1]);
				break;
			case MidiController::VolumeFine:
				out << "Controller Volume Fine " << int(data[1]);
				break;
			case MidiController::Bank:
				out << "Controller Bank " << int(data[1]);
				break;
			case MidiController::BankFine:
				out << "Controller Bank Fine " << int(data[1]);
				break;
			case MidiController::ModWheel:
				out << "Controller Bank " << int(data[1]);
				break;
			case MidiController::ModWheelFine:
				out << "Controller Bank Fine " << int(data[1]);
				break;
			case MidiController::FootPedal:
				out << "Controller FootPedal " << int(data[1]);
				break;
			case MidiController::FootPedalFine:
				out << "Controller Foot Pedal Fine " << int(data[1]);
				break;
			case MidiController::Pan:
				out << "Controller Pan " << int(data[1]);
				break;
			case MidiController::PanFine:
				out << "Controller Pan Fine " << int(data[1]);
				break;
			case MidiController::Balance:
				out << "Controller Balance " << int(data[1]);
				break;
			case MidiController::BalanceFine:
				out << "Controller Balance Fine " << int(data[1]);
				break;
			case MidiController::Expression:
				out << "Controller Expression " << int(data[1]);
				break;
			case MidiController::ExpressionFine:
				out << "Controller Expression Fine " << int(data[1]);
				break;
			case MidiController::Sustain:
				out << "Controller Sustain " << int(data[1]);
				break;
			case MidiController::Effect:
				out << "Controller Effect " << int(data[1]);
				break;
			case MidiController::Chorus:
				out << "Controller Chorus " << int(data[1]);
				break;
			case MidiController::XChanLockProtect:
				out << "Controller XChannel Lock Protect " << int(data[1]);
				break;
			case MidiController::XVoiceProtect:
				out << "Controller XVoice Protect " << int(data[1]);
				break;
			case MidiController::XTimbreProtect:
				out << "Controller XTimbre Protect " << int(data[1]);
				break;
			case MidiController::XBankChange:
				out << "Controller XBank Change " << int(data[1]);
				break;
			case MidiController::XIndirectCtrlPrefix:
				out << "Controller XIndirect Controller Prefix "
					<< int(data[1]);
				break;
			case MidiController::XForLoop:
				out << "Controller XFor Loop " << int(data[1]);
				break;
			case MidiController::XNextBreak:
				out << "Controller XNext/Break " << int(data[1]);
				break;
			case MidiController::XClearBBCount:
				out << "Controller XClear Beat/Bar Count " << int(data[1]);
				break;
			case MidiController::XCallbackTrigger:
				out << "Controller XCallback Trigger " << int(data[1]);
				break;
			case MidiController::XSequenceBranchIndex:
				out << "Controller XSequence Branch Index " << int(data[1]);
				break;
			case MidiController::ResetControllers:
				out << "Reset Controllers ";
				break;
			case MidiController::AllNotesOff:
				out << "All Notes Off ";
				break;

			default:
				out << "Controller " << int(data[0]) << " " << int(data[1]);
				break;
			}
		} break;

		case MidiStatus::Sysex: {
			auto systex_buffer = ex.sysex_data.as_span();
			out << "sysex " << systex_buffer.size_bytes() << " bytes "
				<< std::hex;

			// Hex dump
			for (auto c : systex_buffer) {
				out << " " << int(c) << " ";
			}
			// printable characters
			out << std::dec;
			for (auto c : systex_buffer) {
				out << char(std::isprint(c) ? c : ' ');
			}
		} break;

			// default should never happen
		default:
			out << "Status_" << int(getStatusType()) << " " << int(data[0])
				<< " " << int(data[1]);
			break;
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
		MidiStatus type = getStatusType();
		if (type == MidiStatus::Controller) {
			switch (MidiController(data[0])) {
			case MidiController::Bank:
			case MidiController::BankFine:
			case MidiController::XBankChange:
			case MidiController::XChanLockProtect:
			case MidiController::XVoiceProtect:
			case MidiController::XTimbreProtect:
				return false;

			default:
				return true;
			}
		}
		// these shouldn't be time critical. needed to fix BG21 and SI12
		if (type == MidiStatus::Program || type == MidiStatus::Sysex) {
			return false;
		}
		return true;
	}
};

// needed explicit instantiation declaration to supress warnings from clang
template <>
XMidiRecyclable<XMidiEvent>::FreeList
		XMidiRecyclable<XMidiEvent>::FreeList::instance;

#endif    // XMIDIEVENT_H_INCLUDED
