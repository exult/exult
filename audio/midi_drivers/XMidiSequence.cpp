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

// XMidiRecyclable<XMidiSequence> FreeList
template <>
XMidiRecyclable<XMidiSequence>::FreeList
		XMidiRecyclable<XMidiSequence>::FreeList::instance{};

// Define this to stop the Midisequencer from attempting to
// catch up time if it has missed over 1200 ticks or 1/5th of a second
// This is useful for when debugging, since the Sequencer will not attempt
// to play hundreds of events at the same time if execution is broken, and
// later resumed.
#define XMIDISEQUENCER_NO_CATCHUP_WAIT_OVER 1200

// Play non time critical events at most this many ticks after last event. 60
// seems like a good value to my ears
#define NON_CRIT_ADJUSTMENT 60

XMidiSequence::XMidiSequence(
		XMidiSequenceHandler* Handler, uint16 seq_id, XMidiEventList* events,
		bool Repeat, int volume, int branch)
		: handler(Handler), sequence_id(seq_id), evntlist(events),
		  event(nullptr), repeat(Repeat), last_tick(0), start(0xFFFFFFFF),
		  loop_num(-1), vol_multi(volume), vol_changed(true), paused(false),
		  speed(100) {
	std::memset(loop_event, 0, sizeof(loop_event));
	std::memset(loop_count, 0, sizeof(loop_count));
	event = evntlist->events;

	for (int i = 0; i < 16; i++) {
		shadows[i].reset();
		handler->sequenceSendEvent(
				sequence_id, i | int(MidiStatus::Controller)
									 | (int(MidiController::XBankChange) << 8));
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
}

XMidiSequence::~XMidiSequence() {
	// 'Release' it
	evntlist->decrementCounter();
}

void XMidiSequence::finish() {
	for (int i = 0; i < 16; i++) {
		// Manually turn off sustain
		handler->sequenceSendEvent(
				sequence_id, i | int(MidiStatus::Controller)
									 | (int(MidiController::Sustain) << 8));
		// send all notes off
		handler->sequenceSendEvent(
				sequence_id, i | int(MidiStatus::Controller)
									 | (int(MidiController::AllNotesOff) << 8));
		// rese3t controllers
		handler->sequenceSendEvent(
				sequence_id,
				i | int(MidiStatus::Controller)
						| (int(MidiController::ResetControllers) << 8));
	}

	for (int i = 0; i < 16; i++) {
		shadows[i].reset();
	}
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
	UpdateVolume();

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
	//
	// convert the event time 120th/sec to xmidi clock 6000th of sec
	// by multiplying by 5000 and dividing by speed percentage
	sint32 aim = ((event->time - last_tick) * 5000) / speed;
#ifdef NON_CRIT_ADJUSTMENT
	int nc_ticks = event->time - last_tick;

	if (!event->is_time_critical() && nc_ticks > NON_CRIT_ADJUSTMENT) {
		if (event->next) {
			// time this event at NON_CRIT_ADJUSTMENT or halfway between last
			// and next which is smaller we do not want any simultaneous events
			// if we can avoid them
			nc_ticks = std::min(
					NON_CRIT_ADJUSTMENT,
					(event->next->time - static_cast<int>(last_tick)) / 2);
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
	if (event->getStatusType() == MidiStatus::Controller
		&& MidiController(event->data[0]) == MidiController::XForLoop) {
		if (loop_num < XMIDI_MAX_FOR_LOOP_COUNT - 1) {
			loop_num++;
		} else {
			CERR("XMIDI: Exceeding maximum loop count");
		}

		loop_count[loop_num] = event->data[1];
		loop_event[loop_num] = event;

	}    // XMidi Next/Break
	else if (
			event->getStatusType() == MidiStatus::Controller
			&& MidiController(event->data[0]) == MidiController::XNextBreak) {
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
			event->getStatusType() == MidiStatus::Controller
			&& MidiController(event->data[0])
					   == MidiController::XCallbackTrigger) {
		handler->handleCallbackTrigger(sequence_id, event->data[1]);
	}    // Not SysEx
	else if (event->status < int(MidiStatus::Sysex)) {
		sendEvent();
	}
	// Ignore sysex events on anything but track 0
	else if (event->getChannel() == 0) {
		handler->sequenceSendSysEx(
				sequence_id, event->status, event->ex.sysex_data.buffer(),
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
				// time this event at NON_CRIT_ADJUSTMENT or halfway between
				// last and next which is smaller
				ticks = std::min(
						ticks,
						(event->next->time - static_cast<int>(last_tick)) / 2);
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
	shadows[new_event->getChannel()].updateForEvent(new_event);
}

void XMidiSequence::ChannelShadow::updateForEvent(XMidiEvent* new_event) {
	const MidiStatus type = new_event->getStatusType();
	const uint32     data = new_event->data[0] | (new_event->data[1] << 8);

	// Shouldn't be required. XMidi should automatically detect all anyway
	// evntlist->chan_mask |= 1 << chan;

	// Update the shadows here

	if (type == MidiStatus::Controller) {
		MidiController ctrl = MidiController(new_event->data[0]);

		// Bank
		switch (ctrl) {
		case MidiController::Volume:
		case MidiController::VolumeFine:
			volume.update(ctrl, new_event->data[1]);
			break;

		case MidiController::Bank:
			bank.update(ctrl, new_event->data[1]);
			break;

			// modWheel
		case MidiController::ModWheel:
		case MidiController::ModWheelFine:
			modWheel.update(ctrl, new_event->data[1]);
			break;

		case MidiController::FootPedal:
		case MidiController::FootPedalFine:
			footpedal.update(ctrl, new_event->data[1]);
			break;

		case MidiController::Pan:
		case MidiController::PanFine:
			pan.update(ctrl, new_event->data[1]);
			break;

		case MidiController::Balance:
		case MidiController::BalanceFine:
			balance.update(ctrl, new_event->data[1]);
			break;

		case MidiController::Expression:
		case MidiController::ExpressionFine:
			expression.update(ctrl, new_event->data[1]);
			break;
		case MidiController::Sustain:
			sustain = new_event->data[1];
			break;
		case MidiController::Effect:
			effects = new_event->data[1];
			break;
		case MidiController::Chorus:
			chorus = new_event->data[1];
			break;
		case MidiController::XBankChange:
			xbank = new_event->data[1];
			break;
			// don't care about others so do nothing
		default:
			break;
		}
	} else if (type == MidiStatus::Program) {
		program = data;
	} else if (type == MidiStatus::PitchWheel) {
		pitchWheel = data;
	}
}

void XMidiSequence::sendEvent() {
	unsigned int chan = event->getChannel();
	const MidiStatus type = event->getStatusType();
	uint32           data = event->data[0] | (event->data[1] << 8);

	// Make sure chanmask is updated just in case
	evntlist->chan_mask |= 1 << chan;

	// Update the shadows here
	updateShadowForEvent(event);

	if (type == MidiStatus::Controller) {
		// Channel volume
		if (event->data[0] == 7) {
			int actualvolume
					= (event->data[1] * vol_multi * handler->getGlobalVolume())
					  / 25500;
			data = event->data[0] | (actualvolume << 8);
		}
	} else if (type == MidiStatus::Aftertouch) {
		notes_on.SetAftertouch(event);
	}

	if ((type != MidiStatus::NoteOn || event->data[1])
		&& type != MidiStatus::NoteOff) {
		if (type == MidiStatus::NoteOn) {
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
		MidiController ctrl, int i, int (&controller)[2]) const {
	handler->sequenceSendEvent(
			sequence_id, i | int(MidiStatus::Controller) | (int(ctrl) << 8)
								 | (controller[0] << 16));
	handler->sequenceSendEvent(
			sequence_id,
			i | int(MidiStatus::Controller)
					| ((int(ctrl) + int(MidiController::FineOffset)) << 8)
					| (controller[1] << 16));
}

void XMidiSequence::applyShadow(int i) {
	// Only send most values if they are different to default
	auto def = ChannelShadow();

	// clear with ff to force sending of all as no value can be > 127 in legal
	// midi, This is not needed and is only here if needed for testing
	// std::memset(static_cast<void*>( & def), 0xff, sizeof(def));

	// Send Reset all controllers first to reset everything to default.
	handler->sequenceSendEvent(
			sequence_id,
			i | int(MidiStatus::Controller)
					| (int(MidiController::ResetControllers) << 8));

	
	// Pitch Wheel
	if (shadows[i].pitchWheel != def.pitchWheel) {
		handler->sequenceSendEvent(
				sequence_id,
				i | int(MidiStatus::PitchWheel) | (shadows[i].pitchWheel << 8));
	}

	// Modulation Wheel
	if (shadows[i].modWheel != def.modWheel) {
		sendController(MidiController::ModWheel, i, shadows[i].modWheel);
	}

	// Footpedal
	if (shadows[i].footpedal != def.footpedal) {
		sendController(MidiController::FootPedal, i, shadows[i].footpedal);
	}

	// Volume - we always send this
	int actualvolume = (shadows[i].volume.coarse * vol_multi
						* handler->getGlobalVolume())
					   / 25500;
	handler->sequenceSendEvent(
			sequence_id, i | int(MidiStatus::Controller)
								 | (int(MidiController::Volume) << 8)
								 | (actualvolume << 16));
	handler->sequenceSendEvent(
			sequence_id, i | int(MidiStatus::Controller)
								 | (int(MidiController::VolumeFine) << 8)
								 | (shadows[i].volume.fine << 16));

	// Pan
	if (shadows[i].pan != def.pan) {
		sendController(MidiController::Pan, i, shadows[i].pan);
	}

	// Balance
	if (shadows[i].balance != def.balance) {
		sendController(MidiController::Balance, i, shadows[i].balance);
	}

	// expression
	if (shadows[i].expression != def.expression) {
		sendController(MidiController::Expression, i, shadows[i].expression);
	}

	// Sustain - Always send
	handler->sequenceSendEvent(
			sequence_id, i | int(MidiStatus::Controller)
								 | (int(MidiController::Sustain) << 8)
								 | (shadows[i].sustain << 16));

	// Effects (Reverb)
	if (shadows[i].effects != def.effects) {
		handler->sequenceSendEvent(
				sequence_id, i | int(MidiStatus::Controller)
									 | (int(MidiController::Effect) << 8)
									 | (shadows[i].effects << 16));
	}

	// Chorus
	if (shadows[i].chorus != def.chorus) {
		handler->sequenceSendEvent(
				sequence_id, i | int(MidiStatus::Controller)
									 | (int(MidiController::Chorus) << 8)
									 | (shadows[i].chorus << 16));
	}

	// XMidi Bank
	if (shadows[i].xbank != def.xbank) {
		handler->sequenceSendEvent(
				sequence_id, i | int(MidiStatus::Controller)
									 | (int(MidiController::XBankChange) << 8)
									 | (shadows[i].xbank << 16));
	}

	// Bank Select
	if (shadows[i].bank != def.bank) {
		sendController(MidiController::Bank, i, shadows[i].bank);
	}

	if (shadows[i].program != -1) {
		handler->sequenceSendEvent(
				sequence_id,
				i | int(MidiStatus::Program) | (shadows[i].program << 8));
	}
}

void XMidiSequence::UpdateVolume() {
	if (!vol_changed) {
		return;
	}
	for (int i = 0; i < 16; i++) {
		if (evntlist->chan_mask & (1 << i)) {
			uint32 message = i;
			message |= int(MidiStatus::Controller);
			message |= 7 << 8;
			int actualvolume = (shadows[i].volume.coarse * vol_multi
								* handler->getGlobalVolume())
							   / 25500;
			message |= actualvolume << 16;
			handler->sequenceSendEvent(sequence_id, message);
		}
	}
	vol_changed = false;
}

void XMidiSequence::loseChannel(int i) {
	// If the channel matches, send a note off for any note
	XMidiEvent* note = notes_on.GetNotes();
	while (note) {
		if (note->getChannel() == i) {
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
		if (note->getChannel() == i) {
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
		if ((note->getChannel()) == chan) {
			ret++;
		}
		note = note->ex.note_on.next_note;
	}
	return ret;
}

// Protection
