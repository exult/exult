/*
Copyright (C) 2003-2005  The Pentagram Team
Copyright (C) 2007-2022  The Exult Team

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

#include "pent_include.h"

#include "XMidiSequence.h"

#include "XMidiFile.h"
#include "XMidiSequenceHandler.h"

// Define this to stop the Midisequencer from attempting to
// catch up time if it has missed over 1200 ticks or 1/5th of a second
// This is useful for when debugging, since the Sequencer will not attempt
// to play hundreds of events at the same time if execution is broken, and
// later resumed.
#define XMIDISEQUENCER_NO_CATCHUP_WAIT_OVER 1200

// Play non time critical events at most this many ticks after last event. 60
// seems like a good value to my ears
#define NON_CRIT_ADJUSTMENT 60

const uint16 XMidiSequence::ChannelShadow::centre_value = 0x2000;
const uint8  XMidiSequence::ChannelShadow::fine_value   = centre_value & 127;
const uint8  XMidiSequence::ChannelShadow::coarse_value = centre_value >> 7;
const uint16 XMidiSequence::ChannelShadow::combined_value
		= (coarse_value << 8) | fine_value;

XMidiSequence::XMidiSequence(
		XMidiSequenceHandler* Handler, uint16 seq_id, XMidiEventList* events,
		bool Repeat, int volume, int branch)
		: handler(Handler), sequence_id(seq_id), evntlist(events),
		  event(nullptr), repeat(Repeat), last_tick(0), loop_num(-1),
		  vol_multi(volume), paused(false), speed(100) {
	std::memset(loop_event, 0, XMIDI_MAX_FOR_LOOP_COUNT * sizeof(int));
	std::memset(loop_count, 0, XMIDI_MAX_FOR_LOOP_COUNT * sizeof(int));
	event = evntlist->events;

	for (int i = 0; i < 16; i++) {
		shadows[i].reset();
		handler->sequenceSendEvent(
				sequence_id, i | (MIDI_STATUS_CONTROLLER << 4)
									 | (XMIDI_CONTROLLER_BANK_CHANGE << 8));
	}

	// Jump to branch
	XMidiEvent* brnch = events->findBranchEvent(branch);
	if (brnch) {
		last_tick = brnch->time;
		event     = brnch;

		XMidiEvent* next_event = evntlist->events;
		while (next_event != brnch) {
			updateShadowForEvent(next_event);
			next_event = next_event->next;
		}
		for (int i = 0; i < 16; i++) {
			gainChannel(i);
		}
	}

	// initClock();
	start = 0xFFFFFFFF;
}

XMidiSequence::~XMidiSequence() {
	// Handle note off's here
	while (XMidiEvent* note = notes_on.Pop()) {
		handler->sequenceSendEvent(
				sequence_id, note->status + (note->data[0] << 8));
	}

	for (int i = 0; i < 16; i++) {
		shadows[i].reset();
		applyShadow(i);
	}

	// 'Release' it
	evntlist->decrementCounter();
}

void XMidiSequence::ChannelShadow::reset() {
	pitchWheel = combined_value;

	program = -1;

	// Bank Select
	bank[0] = 0;
	bank[1] = 0;

	// Modulation Wheel
	modWheel[0] = coarse_value;
	modWheel[1] = fine_value;

	// Foot pedal
	footpedal[0] = 0;
	footpedal[1] = 0;

	// Volume
	volumes[0] = coarse_value;
	volumes[1] = fine_value;

	// Pan
	pan[0] = coarse_value;
	pan[1] = fine_value;

	// Balance
	balance[0] = coarse_value;
	balance[1] = fine_value;

	// Expression
	expression[0] = 127;
	expression[1] = 0;

	// sustain
	sustain = 0;

	// Effects (Reverb)
	effects = 0;

	// Chorus
	chorus = 0;

	// Xmidi Bank
	xbank = 0;
}

int XMidiSequence::playEvent() {
	if (start == 0xFFFFFFFF) {
		initClock();
	}

	XMidiEvent* note;

	// Handle note off's here
	while ((note = notes_on.PopTime(getRealTime())) != nullptr) {
		handler->sequenceSendEvent(
				sequence_id, note->status + (note->data[0] << 8));
	}

	// No events left, but we still have notes on, so say we are still playing,
	// if not report we've finished
	if (!event) {
		if (notes_on.GetNotes()) {
			return 1;
		} else {
			return -1;
		}
	}

	// Effectively paused, so indicate it
	if (speed <= 0 || paused) {
		return 1;
	}

	// Play all waiting events;
	sint32 aim = ((event->time - last_tick) * 5000) / speed;
#ifdef NON_CRIT_ADJUSTMENT
	int nc_ticks = event->time - last_tick;

	if (!event->is_time_critical() && nc_ticks > NON_CRIT_ADJUSTMENT) {
		if (event->next) {
			nc_ticks = std::min(
					NON_CRIT_ADJUSTMENT,
					event->next->time - static_cast<int>(last_tick));
		} else {
			nc_ticks = NON_CRIT_ADJUSTMENT;
		}
		aim = nc_ticks * 5000 / speed;
	}
#endif    // NON_CRIT_ADJUSTMENT

	sint32 diff = aim - getTime();

	if (diff > 5) {
		return 1;
	}

	addOffset(aim);

#ifdef NON_CRIT_ADJUSTMENT
	if (event->is_time_critical()) {
		last_tick += nc_ticks;
	} else
#endif
		last_tick = event->time;

#ifdef XMIDISEQUENCER_NO_CATCHUP_WAIT_OVER
	if (diff < -XMIDISEQUENCER_NO_CATCHUP_WAIT_OVER) {
		addOffset(-diff);
	}
#endif

	// Handle note off's here too
	while ((note = notes_on.PopTime(getRealTime())) != nullptr) {
		handler->sequenceSendEvent(
				sequence_id, note->status + (note->data[0] << 8));
	}

	// XMidi For Loop
	if ((event->status >> 4) == MIDI_STATUS_CONTROLLER
		&& event->data[0] == XMIDI_CONTROLLER_FOR_LOOP) {
		if (loop_num < XMIDI_MAX_FOR_LOOP_COUNT - 1) {
			loop_num++;
		} else {
			CERR("XMIDI: Exceeding maximum loop count");
		}

		loop_count[loop_num] = event->data[1];
		loop_event[loop_num] = event;

	}    // XMidi Next/Break
	else if (
			(event->status >> 4) == MIDI_STATUS_CONTROLLER
			&& event->data[0] == XMIDI_CONTROLLER_NEXT_BREAK) {
		if (loop_num != -1) {
			if (event->data[1] < 64) {
				loop_num--;
			}
		} else {
			// See if we can find the branch index
			// If we can, jump to that
			XMidiEvent* branch = evntlist->findBranchEvent(126);

			if (branch) {
				loop_num             = 0;
				loop_count[loop_num] = 1;
				loop_event[loop_num] = branch;
			}
		}
		event = nullptr;
	}    // XMidi Callback Trigger
	else if (
			(event->status >> 4) == MIDI_STATUS_CONTROLLER
			&& event->data[0] == XMIDI_CONTROLLER_CALLBACK_TRIG) {
		handler->handleCallbackTrigger(sequence_id, event->data[1]);
	}    // Not SysEx
	else if (event->status < 0xF0) {
		sendEvent();
	}
	// SysEx gets sent immediately
	else if (event->status != 0xFF) {
		handler->sequenceSendSysEx(
				sequence_id, event->status, event->ex.sysex_data.buffer,
				event->ex.sysex_data.len);
	}

	// If we've got another note, play that next
	if (event) {
		event = event->next;
	}

	// Now, handle what to do when we are at the end
	if (!event) {
		// If we have for loop events, follow them
		if (loop_num != -1) {
			event     = loop_event[loop_num]->next;
			last_tick = loop_event[loop_num]->time;

			if (loop_count[loop_num]) {
				if (!--loop_count[loop_num]) {
					loop_num--;
				}
			}
		}
		// Or, if we are repeating, but there hasn't been any for loop events,
		// repeat from the start
		else if (repeat) {
			event = evntlist->events;
			if (last_tick == 0) {
				return 1;
			}
			last_tick = 0;
		}
		// If we are not repeating, then return saying we are end
		else {
			if (notes_on.GetNotes()) {
				return 1;
			}
			return -1;
		}
	}

	if (!event) {
		if (notes_on.GetNotes()) {
			return 1;
		} else {
			return -1;
		}
	}

	aim = ((event->time - last_tick) * 5000) / speed;
	if (!event->is_time_critical()) {
		aim = getTime();
	}
	diff = aim - getTime();

	if (diff < 0) {
		return 0;    // Next event is ready now!
	}
	return 1;
}

sint32 XMidiSequence::timeTillNext() {
	sint32 sixthoToNext = 0x7FFFFFFF;    // Int max

	// Time remaining on notes currently being played
	XMidiEvent* note = notes_on.GetNotes();
	if (note != nullptr) {
		const sint32 diff = note->ex.note_on.note_time - getRealTime();
		if (diff < sixthoToNext) {
			sixthoToNext = diff;
		}
	}

	// Time till the next event, if we are playing
	if (speed > 0 && event && !paused) {
		sint32 aim = ((event->time - last_tick) * 5000) / speed;
#ifdef NON_CRIT_ADJUSTMENT
		if (!event->is_time_critical()
			&& (event->time - last_tick) > NON_CRIT_ADJUSTMENT) {
			int ticks = NON_CRIT_ADJUSTMENT;
			if (event->next) {
				ticks = std::min(
						ticks, event->next->time - static_cast<int>(last_tick));
			} else {
				ticks = NON_CRIT_ADJUSTMENT;
			}
			aim = ticks * 5000 / speed;
		}
#endif    // NON_CRIT_ADJUSTMENT

		const sint32 diff = aim - getTime();

		if (diff < sixthoToNext) {
			sixthoToNext = diff;
		}
	}
	return sixthoToNext / 6;
}

void XMidiSequence::updateShadowForEvent(XMidiEvent* new_event) {
	const unsigned int chan = new_event->status & 0xF;
	const unsigned int type = new_event->status >> 4;
	const uint32       data = new_event->data[0] | (new_event->data[1] << 8);

	// Shouldn't be required. XMidi should automatically detect all anyway
	// evntlist->chan_mask |= 1 << chan;

	// Update the shadows here

	if (type == MIDI_STATUS_CONTROLLER) {
		// Channel volume
		if (new_event->data[0] == 7) {
			shadows[chan].volumes[0] = new_event->data[1];
		} else if (new_event->data[0] == 39) {
			shadows[chan].volumes[1] = new_event->data[1];
		}
		// Bank
		else if (new_event->data[0] == 0 || new_event->data[0] == 32) {
			shadows[chan].bank[new_event->data[0] / 32] = new_event->data[1];
		}
		// modWheel
		else if (new_event->data[0] == 1 || new_event->data[0] == 33) {
			shadows[chan].modWheel[new_event->data[0] / 32]
					= new_event->data[1];
		}
		// footpedal
		else if (new_event->data[0] == 4 || new_event->data[0] == 36) {
			shadows[chan].footpedal[new_event->data[0] / 32]
					= new_event->data[1];
		}
		// pan
		else if (new_event->data[0] == 9 || new_event->data[0] == 41) {
			shadows[chan].pan[new_event->data[0] / 32] = new_event->data[1];
		}
		// balance
		else if (new_event->data[0] == 10 || new_event->data[0] == 42) {
			shadows[chan].balance[new_event->data[0] / 32] = new_event->data[1];
		}
		// expression
		else if (new_event->data[0] == 11 || new_event->data[0] == 43) {
			shadows[chan].expression[new_event->data[0] / 32]
					= new_event->data[1];
		}
		// sustain
		else if (new_event->data[0] == 64) {
			shadows[chan].effects = new_event->data[1];
		}
		// effect
		else if (new_event->data[0] == 91) {
			shadows[chan].effects = new_event->data[1];
		}
		// chorus
		else if (new_event->data[0] == 93) {
			shadows[chan].chorus = new_event->data[1];
		}
		// XMidi bank
		else if (new_event->data[0] == XMIDI_CONTROLLER_BANK_CHANGE) {
			shadows[chan].xbank = new_event->data[1];
		}
	} else if (type == MIDI_STATUS_PROG_CHANGE) {
		shadows[chan].program = data;
	} else if (type == MIDI_STATUS_PITCH_WHEEL) {
		shadows[chan].pitchWheel = data;
	}
}

void XMidiSequence::sendEvent() {
	// unsigned int chan = event->status & 0xF;
	const unsigned int type = event->status >> 4;
	uint32             data = event->data[0] | (event->data[1] << 8);

	// Shouldn't be required. XMidi should automatically detect all anyway
	// evntlist->chan_mask |= 1 << chan;

	// Update the shadows here
	updateShadowForEvent(event);

	if (type == MIDI_STATUS_CONTROLLER) {
		// Channel volume
		if (event->data[0] == 7) {
			data = event->data[0]
				   | (((event->data[1] * vol_multi) / 0xFF) << 8);
		}
	} else if (type == MIDI_STATUS_AFTERTOUCH) {
		notes_on.SetAftertouch(event);
	}

	if ((type != MIDI_STATUS_NOTE_ON || event->data[1])
		&& type != MIDI_STATUS_NOTE_OFF) {
		if (type == MIDI_STATUS_NOTE_ON) {
			if (!event->data[1]) {
				return;
			}

			notes_on.Remove(event);

			handler->sequenceSendEvent(
					sequence_id, event->status | (data << 8));
			event->ex.note_on.actualvel = event->data[1];

			notes_on.Push(
					event, ((event->ex.note_on.duration - 1) * 5000 / speed)
								   + getStart());
		}
		// Only send IF the channel has been marked enabled
		else {
			handler->sequenceSendEvent(
					sequence_id, event->status | (data << 8));
		}
	}
}

void XMidiSequence::sendController(
		int ctrl, int i, int (&controller)[2]) const {
	handler->sequenceSendEvent(
			sequence_id, i | (MIDI_STATUS_CONTROLLER << 4) | ((ctrl) << 8)
								 | (controller[0] << 16));
	handler->sequenceSendEvent(
			sequence_id, i | (MIDI_STATUS_CONTROLLER << 4)
								 | (((ctrl) + 32) << 8)
								 | (controller[1] << 16));
}

void XMidiSequence::applyShadow(int i) {
	// Pitch Wheel
	handler->sequenceSendEvent(
			sequence_id,
			i | (MIDI_STATUS_PITCH_WHEEL << 4) | (shadows[i].pitchWheel << 8));

	// Modulation Wheel
	sendController(1, i, shadows[i].modWheel);

	// Footpedal
	sendController(4, i, shadows[i].footpedal);

	// Volume
	handler->sequenceSendEvent(
			sequence_id,
			i | (MIDI_STATUS_CONTROLLER << 4) | (7 << 8)
					| (((shadows[i].volumes[0] * vol_multi) / 0xFF) << 16));
	handler->sequenceSendEvent(
			sequence_id, i | (MIDI_STATUS_CONTROLLER << 4) | (39 << 8)
								 | (shadows[i].volumes[1] << 16));

	// Pan
	sendController(9, i, shadows[i].pan);

	// Balance
	sendController(10, i, shadows[i].balance);

	// expression
	sendController(11, i, shadows[i].expression);

	// Sustain
	handler->sequenceSendEvent(
			sequence_id, i | (MIDI_STATUS_CONTROLLER << 4) | (64 << 8)
								 | (shadows[i].sustain << 16));

	// Effects (Reverb)
	handler->sequenceSendEvent(
			sequence_id, i | (MIDI_STATUS_CONTROLLER << 4) | (91 << 8)
								 | (shadows[i].effects << 16));

	// Chorus
	handler->sequenceSendEvent(
			sequence_id, i | (MIDI_STATUS_CONTROLLER << 4) | (93 << 8)
								 | (shadows[i].chorus << 16));

	// XMidi Bank
	handler->sequenceSendEvent(
			sequence_id, i | (MIDI_STATUS_CONTROLLER << 4)
								 | (XMIDI_CONTROLLER_BANK_CHANGE << 8)
								 | (shadows[i].xbank << 16));

	// Bank Select
	if (shadows[i].program != -1) {
		handler->sequenceSendEvent(
				sequence_id,
				i | (MIDI_STATUS_PROG_CHANGE << 4) | (shadows[i].program << 8));
	}
	sendController(0, i, shadows[i].bank);
}

void XMidiSequence::setVolume(int new_volume) {
	vol_multi = new_volume;

	// Only update used channels
	for (int i = 0; i < 16; i++) {
		if (evntlist->chan_mask & (1 << i)) {
			uint32 message = i;
			message |= MIDI_STATUS_CONTROLLER << 4;
			message |= 7 << 8;
			message |= ((shadows[i].volumes[0] * vol_multi) / 0xFF) << 16;
			handler->sequenceSendEvent(sequence_id, message);
		}
	}
}

void XMidiSequence::loseChannel(int i) {
	// If the channel matches, send a note off for any note
	XMidiEvent* note = notes_on.GetNotes();
	while (note) {
		if ((note->status & 0xF) == i) {
			handler->sequenceSendEvent(
					sequence_id, note->status + (note->data[0] << 8));
		}
		note = note->ex.note_on.next_note;
	}
}

void XMidiSequence::gainChannel(int i) {
	applyShadow(i);

	// If the channel matches, send a note on for any note
	XMidiEvent* note = notes_on.GetNotes();
	while (note) {
		if ((note->status & 0xF) == i) {
			handler->sequenceSendEvent(
					sequence_id, note->status | (note->data[0] << 8)
										 | (note->ex.note_on.actualvel << 16));
		}
		note = note->ex.note_on.next_note;
	}
}

void XMidiSequence::pause() {
	paused = true;
	for (int i = 0; i < 16; i++) {
		if (evntlist->chan_mask & (1 << i)) {
			loseChannel(i);
		}
	}
}

void XMidiSequence::unpause() {
	paused = false;
	for (int i = 0; i < 16; i++) {
		if (evntlist->chan_mask & (1 << i)) {
			applyShadow(i);
		}
	}
}

int XMidiSequence::countNotesOn(int chan) {
	if (paused) {
		return 0;
	}

	int         ret  = 0;
	XMidiEvent* note = notes_on.GetNotes();
	while (note) {
		if ((note->status & 0xF) == chan) {
			ret++;
		}
		note = note->ex.note_on.next_note;
	}
	return ret;
}

// Protection
