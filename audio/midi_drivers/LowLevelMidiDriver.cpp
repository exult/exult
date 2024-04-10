/*
Copyright (C) 2003-2005  The Pentagram Team
Copyright (C) 2010-2022  The Exult team

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

#include "LowLevelMidiDriver.h"

#include "XMidiEvent.h"
#include "XMidiEventList.h"
#include "XMidiFile.h"
#include "XMidiSequence.h"

#include <chrono>
#include <cstring>
#include <mutex>
// If the time to wait is less than this then we yield instead of waiting on the
// condition variable This must be great than or equal to 2
#define LLMD_MINIMUM_YIELD_THRESHOLD 6

// #define DO_SMP_TEST

#ifdef DO_SMP_TEST
#	define giveinfo()                                    \
		perr << __FILE__ << ":" << __LINE__ << std::endl; \
		perr.flush();
#else
#	define giveinfo()
#endif

//
// MT32 SysEx
//
static const uint32 sysex_data_start = 7;    // Data starts at byte 7
// static const uint32 sysex_max_data_size = 256;

//
// Percussion
//

static const uint32 rhythm_base     = 0x030110;    // Note, these are 7 bit!
static const uint32 rhythm_mem_size = 4;

static const uint32 rhythm_first_note = 24;

// static const uint32 rhythm_num_notes = 64;

// Memory offset based on index in the table
static inline uint32 rhythm_mem_offset(uint32 index_num) {
	return index_num * 4;
}

// Memory offset based on note key num
static inline uint32 rhythm_mem_offset_note(uint32 rhythm_note_num) {
	return (rhythm_note_num - rhythm_first_note) * 4;
}

//
// Timbre Memory Consts
//
static const uint32 timbre_temp_base = 0x040000;
static const uint32 timbre_unk_base  = 0x060000;
static const uint32 timbre_base      = 0x080000;    // Note, these are 7 bit!
static const uint32 timbre_mem_size  = 246;

static inline uint32 timbre_mem_offset(uint32 timbre_num) {
	return timbre_num * 256;
}

//
// Patch Temp Consts
//
static const uint32 patch_temp_base = 0x030000;
static const uint32 patch_temp_size = 16;

static inline uint32 patch_temp_offset(uint32 patch_num) {
	return patch_num * patch_temp_size;
}

//
// Patch Memory Consts
//
static const uint32 patch_base     = 0x050000;    // Note, these are 7 bit!
static const uint32 patch_mem_size = 8;

static inline uint32 patch_mem_offset(uint32 patch_num) {
	return patch_num * 8;
}

const LowLevelMidiDriver::MT32Patch LowLevelMidiDriver::mt32_patch_template = {
		0,     // timbre_group
		0,     // timbre_num
		24,    // key_shift
		50,    // fine_tune
		24,    // bender_range
		0,     // assign_mode
		1,     // reverb_switch
		0      // dummy
};

bool LowLevelMidiDriver::precacheTimbresOnStartup = false;

bool LowLevelMidiDriver::precacheTimbresOnPlay = false;

//
// All Dev Reset
//
static const uint32 all_dev_reset_base = 0x7f0000;

// Display  Consts
//
static const uint32 display_base = 0x200000;    // Note, these are 7 bit!
static const uint32 display_mem_size
		= 0x14;    // Display is 20 ASCII characters (32-127)

// Convert 14 Bit base address to real
static inline int ConvBaseToActual(uint32 address_base) {
	return ((address_base >> 2) & (0x7f << 14))
		   | ((address_base >> 1) & (0x7f << 7))
		   | ((address_base >> 0) & (0x7f << 0));
}

using std::endl;
using std::string;

LowLevelMidiDriver::~LowLevelMidiDriver() {
	// Just kill it
	if (initialized) {
		perr << "Warning: Destructing LowLevelMidiDriver and "
				"destroyMidiDriver() wasn't called!"
			 << std::endl;
		// destroyMidiDriver();
		quit_thread = true;    // The thread should stop based upon this flag
		if (thread) {
			thread->detach();    // calling join might not be safe as the driver
								 // is kind of in a indeterminate state and
								 // joining might block this thread forever so
								 // use detach instead
		}
		thread.reset();
	}
}

//
// MidiDriver API
//

int LowLevelMidiDriver::initMidiDriver(uint32 samp_rate, bool is_stereo) {
	// Destroy first before re-initing
	if (initialized) {
		destroyMidiDriver();
	}

	// Reset the current stream states
	std::fill(std::begin(sequences), std::end(sequences), nullptr);
	std::fill(std::begin(chan_locks), std::end(chan_locks), -1);
	for (auto& chan : chan_map) {
		std::fill(std::begin(chan), std::end(chan), -1);
	}
	for (int i = 0; i < LLMD_NUM_SEQ; i++) {
		playing[i]       = false;
		callback_data[i] = -1;
	}

	if (thread) {
		destroyThreadedSynth();
		thread.reset();
	}
	mutex             = std::make_unique<std::mutex>();
	cbmutex           = std::make_unique<std::mutex>();
	cond              = std::make_unique<std::condition_variable>();
	sample_rate       = samp_rate;
	stereo            = is_stereo;
	uploading_timbres = false;
	next_sysex        = std::chrono::milliseconds(0);

	// Zero the memory
	std::fill(
			std::begin(mt32_patch_banks), std::end(mt32_patch_banks), nullptr);
	std::fill(
			std::begin(mt32_timbre_banks), std::end(mt32_timbre_banks),
			nullptr);
	for (auto& timbre : mt32_timbre_used) {
		std::fill(std::begin(timbre), std::end(timbre), -1);
	}
	for (auto& bank : mt32_bank_sel) {
		std::fill(std::begin(bank), std::end(bank), 0);
	}
	// Note: This is inconsistent with the one from
	// LowLevelMidiDriver::loadTimbreLibrary
	std::fill(
			std::begin(mt32_patch_bank_sel), std::end(mt32_patch_bank_sel), 0);
	std::fill(
			std::begin(mt32_rhythm_bank), std::end(mt32_rhythm_bank), nullptr);

	int code = 0;

	if (isSampleProducer()) {
		code = initSoftwareSynth();
	} else {
		code = initThreadedSynth();
	}

	if (code) {
		perr << "Failed to initialize midi player (code: " << code << ")"
			 << endl;
		if (thread) {
			destroyThreadedSynth();
			thread.reset();
		}
		mutex.reset();
		cbmutex.reset();
		cond.reset();
	} else {
		initialized = true;
	}

	return code;
}

void LowLevelMidiDriver::destroyMidiDriver() {
	if (!initialized) {
		return;
	}

	waitTillNoComMessages();

	if (isSampleProducer()) {
		destroySoftwareSynth();
	} else {
		destroyThreadedSynth();
	}

	initialized = false;

	mutex.reset();
	cbmutex.reset();
	cond.reset();

	thread.reset();

	giveinfo();
}

int LowLevelMidiDriver::maxSequences() {
	return LLMD_NUM_SEQ;
}

void LowLevelMidiDriver::startSequence(
		int seq_num, XMidiEventList* eventlist, bool repeat, int vol,
		int branch) {
	if (seq_num >= LLMD_NUM_SEQ || seq_num < 0) {
		return;
	}

	if (!initialized) {
		return;
	}

	// Special handling for if we were uploading a timbre
	// Wait till the timbres have finished being sent
	while (uploading_timbres) {
		waitTillNoComMessages();

		const bool isplaying = [this]() {
			const std::lock_guard<std::mutex> lock(*mutex);
			return playing[3];
		}();

		// If sequence is still playing timbres are still loading
		// so we need to wait and try it again
		if (isplaying) {
			yield();
		} else {
			uploading_timbres = false;
		}
	}

	eventlist->incrementCounter();

	if (precacheTimbresOnPlay) {
		ComMessage precache(LLMD_MSG_PRECACHE_TIMBRES, -1);
		precache.data.precache.list = eventlist;
		sendComMessage(precache);
	}

	ComMessage message(LLMD_MSG_PLAY, seq_num);
	message.data.play.list   = eventlist;
	message.data.play.repeat = repeat;
	message.data.play.volume = vol;
	message.data.play.branch = branch;

	sendComMessage(message);
}

void LowLevelMidiDriver::finishSequence(int seq_num) {
	if (seq_num >= LLMD_NUM_SEQ || seq_num < 0) {
		return;
	}
	if (!initialized) {
		return;
	}
	if (uploading_timbres) {
		return;
	}

	ComMessage message(LLMD_MSG_FINISH, seq_num);
	sendComMessage(message);
}

void LowLevelMidiDriver::setSequenceVolume(int seq_num, int vol) {
	if (seq_num >= LLMD_NUM_SEQ || seq_num < 0) {
		return;
	}
	if (vol < 0 || vol > 255) {
		return;
	}
	if (!initialized) {
		return;
	}
	if (uploading_timbres) {
		return;
	}

	ComMessage message(LLMD_MSG_SET_VOLUME, seq_num);
	message.data.volume.level = vol;

	sendComMessage(message);
}

void LowLevelMidiDriver::setGlobalVolume(int vol) {
	if (vol < 0 || vol > 255) {
		return;
	}
	if (!initialized) {
		return;
	}

	ComMessage message(LLMD_MSG_SET_VOLUME, -1);
	message.data.volume.level = vol;

	sendComMessage(message);
}

void LowLevelMidiDriver::setSequenceSpeed(int seq_num, int speed) {
	if (seq_num >= LLMD_NUM_SEQ || seq_num < 0) {
		return;
	}
	if (speed < 0) {
		return;
	}
	if (!initialized) {
		return;
	}
	if (uploading_timbres) {
		return;
	}

	ComMessage message(LLMD_MSG_SET_SPEED, seq_num);
	message.data.speed.percentage = speed;

	sendComMessage(message);
}

bool LowLevelMidiDriver::isSequencePlaying(int seq_num) {
	if (seq_num >= LLMD_NUM_SEQ || seq_num < 0) {
		return false;
	}
	if (uploading_timbres) {
		return false;
	}

	waitTillNoComMessages();

	const std::lock_guard<std::mutex> lock(*mutex);
	return playing[seq_num];
}

void LowLevelMidiDriver::setSequenceRepeat(int seq_num, bool newrepeat) {
	ComMessage message(LLMD_MSG_SET_REPEAT, seq_num);
	message.data.set_repeat.newrepeat = newrepeat;

	sendComMessage(message);
}

void LowLevelMidiDriver::pauseSequence(int seq_num) {
	if (seq_num >= LLMD_NUM_SEQ || seq_num < 0) {
		return;
	}
	if (!initialized) {
		return;
	}
	if (uploading_timbres) {
		return;
	}

	ComMessage message(LLMD_MSG_PAUSE, seq_num);
	message.data.pause.paused = true;

	sendComMessage(message);
}

void LowLevelMidiDriver::unpauseSequence(int seq_num) {
	if (seq_num >= LLMD_NUM_SEQ || seq_num < 0) {
		return;
	}
	if (!initialized) {
		return;
	}
	if (uploading_timbres) {
		return;
	}

	ComMessage message(LLMD_MSG_PAUSE, seq_num);
	message.data.pause.paused = false;

	sendComMessage(message);
}

uint32 LowLevelMidiDriver::getSequenceCallbackData(int seq_num) {
	if (seq_num >= LLMD_NUM_SEQ || seq_num < 0) {
		return 0;
	}

	const std::lock_guard<std::mutex> lock(*cbmutex);
	return callback_data[seq_num];
}

//
// Communications
//

sint32 LowLevelMidiDriver::peekComMessageType() {
	const std::lock_guard<std::mutex> lock(*mutex);
	if (!messages.empty()) {
		return messages.front().type;
	}
	return 0;
}

void LowLevelMidiDriver::sendComMessage(ComMessage& message) {
	const std::lock_guard<std::mutex> lock(*mutex);
	messages.push(message);
	cond->notify_one();
}

void LowLevelMidiDriver::waitTillNoComMessages() {
	while (peekComMessageType()) {
		yield();
	}
}

//
// Thread Stuff
//

int LowLevelMidiDriver::initThreadedSynth() {
	// Create the thread
	giveinfo();

	ComMessage message(LLMD_MSG_THREAD_INIT, -1);
	sendComMessage(message);

	if (thread) {
		destroyThreadedSynth();
	}
	quit_thread = false;
	thread      = std::make_unique<std::thread>(
            threadMain_Static,
            std::static_pointer_cast<LowLevelMidiDriver>(selfptr.lock()));

	while (peekComMessageType() == LLMD_MSG_THREAD_INIT) {
		yield();
	}

	int                               code = 0;
	const std::lock_guard<std::mutex> lock(*mutex);
	while (!messages.empty()) {
		if (messages.front().type == LLMD_MSG_THREAD_INIT_FAILED) {
			code = messages.front().data.init_failed.code;
		}
		messages.pop();
	}

	return code;
}

void LowLevelMidiDriver::destroyThreadedSynth() {
	initialized = false;

	ComMessage message(LLMD_MSG_THREAD_EXIT, -1);
	sendComMessage(message);

	int count = 0;

	while (count < 400) {
		giveinfo();
		// Wait 1 MS before trying again
		if (peekComMessageType() != 0) {
			yield();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		} else {
			break;
		}

		count++;
	}

	quit_thread = true;    // The thread should stop based upon this flag
	// We waited a while and it still didn't terminate
	if (count == 400 && peekComMessageType() != 0) {
		perr << "MidiPlayer Thread failed to stop in time. Killing it."
			 << std::endl;
		thread->join();
	} else {
		thread->detach();
	}
	const std::lock_guard<std::mutex> lock(*mutex);
	// Get rid of all the messages
	while (!messages.empty()) {
		messages.pop();
	}
}

int LowLevelMidiDriver::threadMain_Static(
		std::shared_ptr<LowLevelMidiDriver> ptr) {
	giveinfo();
	return ptr->threadMain();
}

int LowLevelMidiDriver::threadMain() {
	giveinfo();

	// Open the device
	const int code = open();
	{
		const std::lock_guard<std::mutex> lock(*mutex);
		// Pop all the messages
		while (!messages.empty()) {
			messages.pop();
		}

		// If we have an error code, push it to the message queue then return
		// else we are ok to go
		if (code) {
			ComMessage message(LLMD_MSG_THREAD_INIT_FAILED, -1);
			message.data.init_failed.code = code;
			messages.push(message);
		}
	}

	if (code) {
		return code;
	}

	// Increase the thread priority, IF possible
	increaseThreadPriority();

	auto clock_start = std::chrono::steady_clock::now();
	// Execute the play loop
	while (!quit_thread) {
		xmidi_clock = std::chrono::duration_cast<decltype(xmidi_clock)>(
				std::chrono::steady_clock::now() - clock_start);

		if (playSequences()) {
			break;
		}

		sint32 time_till_next = INT32_MAX;

		for (int i = 0; i < LLMD_NUM_SEQ; i++) {
			const int seq = i;

			if (sequences[seq]) {
				const sint32 ms = sequences[seq]->timeTillNext();
				if (ms < time_till_next) {
					time_till_next = ms;
				}
			}
		}

		if (time_till_next <= LLMD_MINIMUM_YIELD_THRESHOLD) {
			bool wait = false;
			{
				// No Blocking allowed here!
				const std::unique_lock<std::mutex> lock(
						*mutex, std::try_to_lock);
				if (lock.owns_lock() && messages.empty()) {
					wait = true;
				}
			}
			if (wait) {
				// printf("Yielding\n");
				yield();
				// printf("Finished Yielding\n");
			} else {
				// printf("Messages in queue, not Yielding\n");
			}
		} else {
			// We do not want blocking here
			std::unique_lock<std::mutex> lock(*mutex, std::try_to_lock);
			if (lock.owns_lock()) {
				if (messages.empty()) {
					// printf("Waiting %i ms\n", time_till_next - 2);
					cond->wait_for(
							lock,
							std::chrono::milliseconds(time_till_next - 2));
					// printf("Finished Waiting\n");
				} else {
					// printf("Messages in queue, not waiting\n");
				}
			}
		}
	}

	// Display messages          0123456789ABCDEF0123
	const char exit_display[] = "Poor Poor Avatar... ";
	sendMT32SystemMessage(display_base, 0, display_mem_size, exit_display);
	sendMT32SystemMessage(all_dev_reset_base, 0, 1, exit_display);
	std::this_thread::sleep_for(std::chrono::milliseconds(40));

	// Close the device
	close();

	const std::lock_guard<std::mutex> lock(*mutex);
	// Pop all messages
	while (!messages.empty()) {
		messages.pop();
	}
	initialized = false;
	return 0;
}

//
// Software Synth Stuff
//

int LowLevelMidiDriver::initSoftwareSynth() {
	// Open the device
	const int ret = open();

	// Uh oh, failed
	if (ret) {
		return 1;
	}

	// Now setup all our crap
	total_seconds       = 0;
	samples_this_second = 0;

	// This value doesn't 'really' matter all that much
	// Smaller values are more accurate (more iterations)

	if (sample_rate == 11025) {
		samples_per_iteration = 49;    // exactly 225 times a second
	}
	if (sample_rate == 22050) {
		samples_per_iteration = 98;    // exactly 225 times a second
	} else if (sample_rate == 44100) {
		samples_per_iteration = 147;    // exactly 300 times a second
	} else {
		samples_per_iteration
				= sample_rate / 480;    // Approx 480 times a second

		// Try to see if it can be 240 times/second
		if (!(samples_per_iteration & 1)) {
			samples_per_iteration >>= 1;
		}
		// Try to see if it can be 120 times/second
		if (!(samples_per_iteration & 1)) {
			samples_per_iteration >>= 1;
		}
	}

	return 0;
}

void LowLevelMidiDriver::destroySoftwareSynth() {
	// Will cause the synth to set it self uninitialized
	ComMessage message(LLMD_MSG_THREAD_EXIT, -1);
	sendComMessage(message);

	// Wait till all pending commands have been executed
	waitTillNoComMessages();

	initialized = false;

	close();
}

void LowLevelMidiDriver::produceSamples(sint16* samples, uint32 bytes) {
	// Hey, we're not supposed to be here
	if (!initialized) {
		return;
	}

	int stereo_mult = 1;
	if (stereo) {
		stereo_mult = 2;
	}

	uint32 num_samples = bytes / (2 * stereo_mult);

	// Now, do the note playing iterations
	while (num_samples > 0) {
		uint32 samples_to_produce = samples_per_iteration;
		if (samples_to_produce > num_samples) {
			samples_to_produce = num_samples;
		}

		// Increment the timing counter(s)
		samples_this_second += samples_to_produce;
		while (samples_this_second > sample_rate) {
			total_seconds++;
			samples_this_second -= sample_rate;
		}

		// Calc Xmidi Clock
		xmidi_clock = decltype(xmidi_clock)(
				(total_seconds * 6000)
				+ (samples_this_second * 6000) / sample_rate);

		// We care about the return code now
		if (playSequences()) {
			const std::lock_guard<std::mutex> lock(*mutex);
			// Pop all messages
			while (!messages.empty()) {
				messages.pop();
			}
			initialized = false;
			break;
		}

		// Produce the samples
		lowLevelProduceSamples(samples, samples_to_produce);

		// Increment the counters
		samples += samples_to_produce * stereo_mult;
		num_samples -= samples_to_produce;
	}
}

//
// Shared Stuff
//

bool LowLevelMidiDriver::playSequences() {
	int i;

	// Play all notes, from all sequences
	for (i = 0; i < LLMD_NUM_SEQ; i++) {
		const int seq = i;

		while (sequences[seq] && !peekComMessageType()) {
			const sint32 pending_events = sequences[seq]->playEvent();

			if (pending_events > 0) {
				break;
			} else if (pending_events == -1) {
				delete sequences[seq];
				sequences[seq] = nullptr;
				const std::lock_guard<std::mutex> lock(*mutex);
				playing[seq] = false;
			}
		}
	}

	// Did we get issued a music command?
	const std::lock_guard<std::mutex> lock(*mutex);
	while (!messages.empty()) {
		ComMessage message = messages.front();

		// Quick Exit if we get a 'queue' and get an exit command
		if (messages.back().type == LLMD_MSG_THREAD_EXIT) {
			message = messages.back();
		}

		switch (message.type) {
		case LLMD_MSG_FINISH: {
			delete sequences[message.sequence];
			sequences[message.sequence]     = nullptr;
			playing[message.sequence]       = false;
			callback_data[message.sequence] = -1;
			unlockAndUnprotectChannel(message.sequence);
		} break;

		case LLMD_MSG_THREAD_EXIT: {
			for (i = 0; i < LLMD_NUM_SEQ; i++) {
				delete sequences[i];
				sequences[i]     = nullptr;
				playing[i]       = false;
				callback_data[i] = -1;
				unlockAndUnprotectChannel(i);
			}
		}
			return true;

		case LLMD_MSG_SET_VOLUME: {
			if (message.sequence == -1) {
				global_volume = message.data.volume.level;
				for (i = 0; i < LLMD_NUM_SEQ; i++) {
					if (sequences[i]) {
						sequences[i]->setVolume(sequences[i]->getVolume());
					}
				}
			} else if (sequences[message.sequence]) {
				sequences[message.sequence]->setVolume(
						message.data.volume.level);
			}
		} break;

		case LLMD_MSG_SET_SPEED: {
			if (sequences[message.sequence]) {
				sequences[message.sequence]->setSpeed(
						message.data.speed.percentage);
			}
		} break;

		case LLMD_MSG_PAUSE: {
			if (sequences[message.sequence]) {
				if (!message.data.pause.paused) {
					sequences[message.sequence]->unpause();
				} else {
					sequences[message.sequence]->pause();
				}
			}
		} break;

		case LLMD_MSG_SET_REPEAT: {
			if (sequences[message.sequence]) {
				sequences[message.sequence]->setRepeat(
						message.data.set_repeat.newrepeat);
			}
		} break;

		case LLMD_MSG_PLAY: {
			// Kill the previous stream
			delete sequences[message.sequence];
			sequences[message.sequence]     = nullptr;
			playing[message.sequence]       = false;
			callback_data[message.sequence] = -1;
			unlockAndUnprotectChannel(message.sequence);

			giveinfo();

			if (message.data.play.list) {
				sequences[message.sequence] = new XMidiSequence(
						this, message.sequence, message.data.play.list,
						message.data.play.repeat, message.data.play.volume,
						message.data.play.branch);

				playing[message.sequence] = true;

				/*
				uint16 mask = sequences[message.sequence]->getChanMask();

				// Allocate some channels
				for (i = 0; i < 16; i++)
				if (mask & (1<<i)) allocateChannel(message.sequence, i);
				*/
			}

		} break;

			// Attempt to load first 64 timbres into memory
		case LLMD_MSG_PRECACHE_TIMBRES: {
			// pout << "Precaching Timbres..." << std::endl;

			// Display messages     0123456789ABCDEF0123
			const char display[] = " Precaching Timbres ";
			sendMT32SystemMessage(display_base, 0, display_mem_size, display);

			if (message.data.precache.list) {
				// Precache Timbres for this sequence
				if (message.data.precache.list->x_patch_bank) {
					int bank_sel[16] = {0};

					for (XMidiEvent* e   = message.data.play.list->x_patch_bank;
						 e != nullptr; e = e->next_patch_bank) {
						if ((e->status >> 4) == MIDI_STATUS_CONTROLLER) {
							if (e->data[0] == XMIDI_CONTROLLER_BANK_CHANGE) {
								bank_sel[e->status & 0xF] = e->data[1];
							}
						} else if (
								(e->status >> 4) == MIDI_STATUS_PROG_CHANGE) {
							if (mt32_patch_banks[0]) {
								const int bank  = bank_sel[e->status & 0xF];
								const int patch = e->data[0];
								// pout << "Program in seq: " <<
								// message.sequence << " Channel: " <<
								// (e->status & 0xF) << " Bank: " << bank <<
								// " Patch: " << patch << std::endl;
								if (bank != mt32_patch_bank_sel[patch]) {
									setPatchBank(bank, patch);
								}
							}
						} else if (
								e->status
								== ((MIDI_STATUS_NOTE_ON << 4) | 0x9)) {
							const int temp = e->data[0];
							if (mt32_rhythm_bank[temp]) {
								loadRhythmTemp(temp);
							}
						}
					}
				}
			} else {
				int count = 0;

				for (int bank = 0; bank < 128; bank++) {
					if (mt32_timbre_banks[bank]) {
						for (int timbre = 0; timbre < 128; timbre++) {
							if (mt32_timbre_banks[bank][timbre]) {
								uploadTimbre(bank, timbre);
								count++;

								if (count == 64) {
									break;
								}
							}
						}
					}
					if (count == 64) {
						break;
					}
				}

				if (mt32_patch_banks[0]) {
					for (int patch = 0; patch < 128; patch++) {
						if (mt32_patch_banks[0][patch]
							&& mt32_patch_banks[0][patch]->timbre_bank >= 2) {
							setPatchBank(0, patch);
						}
					}
				}
			}
		} break;

			// Uh we have no idea what it is
		default:
			break;
		}

		// Pop it
		messages.pop();
	}

	return false;
}

void LowLevelMidiDriver::sequenceSendEvent(uint16 sequence_id, uint32 message) {
	const int log_chan = message & 0xF;
	message &= 0xFFFFFFF0;    // Strip the channel number

	// Controller handling
	if ((message & 0x00F0) == (MIDI_STATUS_CONTROLLER << 4)) {
		// Screw around with volume
		if ((message & 0xFF00) == (7 << 8)) {
			int vol = (message >> 16) & 0xFF;
			message &= 0x00FFFF;
			// Global volume
			vol = (vol * global_volume) / 0xFF;
			message |= vol << 16;
		} else if ((message & 0xFF00) == (XMIDI_CONTROLLER_CHAN_LOCK << 8)) {
			lockChannel(sequence_id, log_chan, ((message >> 16) & 0xFF) >= 64);
			return;
		} else if (
				(message & 0xFF00) == (XMIDI_CONTROLLER_CHAN_LOCK_PROT << 8)) {
			protectChannel(
					sequence_id, log_chan, ((message >> 16) & 0xFF) >= 64);
			return;
		} else if ((message & 0xFF00) == (XMIDI_CONTROLLER_BANK_CHANGE << 8)) {
			// pout << "Bank change in seq: " << sequence_id << " Channel: "
			// << log_chan << " Bank: " << ((message>>16)&0xFF) <<
			// std::endl;
			mt32_bank_sel[sequence_id][log_chan] = (message >> 16) & 0xFF;
			// Note, we will pass this onto the midi driver, because they
			// (i.e. FMOpl) might want them)
		}
	} else if ((message & 0x00F0) == (MIDI_STATUS_PROG_CHANGE << 4)) {
		if (mt32_patch_banks[0]) {
			const int bank  = mt32_bank_sel[sequence_id][log_chan];
			const int patch = (message & 0xFF00) >> 8;
			// pout << "Program in seq: " << sequence_id << " Channel: " <<
			// log_chan << " Bank: " << bank << " Patch: " << patch <<
			// std::endl;
			if (bank != mt32_patch_bank_sel[patch]) {
				setPatchBank(bank, patch);
			}
		}
	} else if (
			(message & 0x00F0) == (MIDI_STATUS_NOTE_ON << 4)
			&& log_chan == 0x9) {
		const int temp = (message >> 8) & 0xFF;
		if (temp < 127 && mt32_rhythm_bank[temp]) {
			loadRhythmTemp(temp);
		}
	}

	// Ok, get the physical channel number from the logical.
	int phys_chan = chan_map[sequence_id][log_chan];

	if (phys_chan == -2) {
		return;
	} else if (phys_chan < 0) {
		phys_chan = log_chan;
	}

	send(message | phys_chan);
}

void LowLevelMidiDriver::sequenceSendSysEx(
		uint16 sequence_id, uint8 status, const uint8* msg, uint16 length) {
	ignore_unused_variable_warning(sequence_id);
	// Ignore Metadata
	if (status == 0xFF) {
		return;
	}

	// Ignore what would appear to be invalid SysEx data
	if (!msg || !length) {
		return;
	}

	// When uploading timbres, we will not send certain data types
	if (uploading_timbres && length > 7) {
		// make sure it's for MT32

		if (msg[0] == 0x41 && msg[1] == 0x10 && msg[2] == 0x16
			&& msg[3] == 0x12) {
			const uint32 actual_address
					= (msg[4] << 14) | (msg[5] << 7) | msg[6];

			const uint32 timbre_add_start = ConvBaseToActual(timbre_base);
			const uint32 timbre_add_end
					= timbre_add_start + timbre_mem_offset(64);
			const uint32 patch_add_start = ConvBaseToActual(patch_base);
			const uint32 patch_add_end
					= patch_add_start + patch_mem_offset(128);
			const uint32 rhythm_add_start = ConvBaseToActual(rhythm_base);
			const uint32 rhythm_add_end
					= rhythm_add_start + rhythm_mem_offset(85);
			const uint32 timbre_temp_add_start
					= ConvBaseToActual(timbre_temp_base);
			const uint32 timbre_temp_add_end
					= timbre_temp_add_start + timbre_mem_offset(8);
			const uint32 patch_temp_add_start
					= ConvBaseToActual(patch_temp_base);
			const uint32 patch_temp_add_end
					= patch_temp_add_start + patch_temp_offset(8);
			const uint32 timbre_unk_add_start
					= ConvBaseToActual(timbre_unk_base);
			const uint32 timbre_unk_add_end
					= timbre_unk_add_start + timbre_mem_offset(128);

			// uint32 sysex_size = length-(4+3+1+1);

			if (actual_address >= timbre_add_start
				&& actual_address < timbre_add_end) {
				return;
			}
			if (actual_address >= timbre_temp_add_start
				&& actual_address < timbre_temp_add_end) {
				return;
			}
			if (actual_address >= timbre_unk_add_start
				&& actual_address < timbre_unk_add_end) {
				return;
			}
			if (actual_address >= patch_add_start
				&& actual_address < patch_add_end) {
				return;
			}
			if (actual_address >= patch_temp_add_start
				&& actual_address < patch_temp_add_end) {
				return;
			}
			if (actual_address >= rhythm_add_start
				&& actual_address < rhythm_add_end) {
				return;
			}
		}
	}

	// Just send it

	auto now = std::chrono::steady_clock::now().time_since_epoch();
	// Making assumption that software MT32 can instantly consume sysex
	if (!isSampleProducer() && next_sysex > now) {
		std::this_thread::sleep_for(
				next_sysex - now);    // Wait till we think the buffer is empty
	}
	send_sysex(status, msg, length);
	next_sysex = std::chrono::duration_cast<decltype(next_sysex)>(
			std::chrono::steady_clock::now().time_since_epoch()
			+ std::chrono::milliseconds(
					static_cast<std::chrono::milliseconds::rep>(
							(40 + (length + 2) * 1000.0) / 3125.0)));
}

uint32 LowLevelMidiDriver::getTickCount(uint16 sequence_id) {
	ignore_unused_variable_warning(sequence_id);
	return xmidi_clock.count();
}

void LowLevelMidiDriver::handleCallbackTrigger(uint16 sequence_id, uint8 data) {
	const std::lock_guard<std::mutex> lock(*cbmutex);
	callback_data[sequence_id] = data;
}

int LowLevelMidiDriver::protectChannel(
		uint16 sequence_id, int chan, bool protect) {
	// Unprotect the channel
	if (!protect) {
		chan_locks[chan]            = -1;
		chan_map[sequence_id][chan] = -1;
	}
	// Protect the channel (if required)
	else if (chan_locks[chan] != -2) {
		// First check to see if the channel has been locked by something
		int relock_sid = -1;
		int relock_log = -1;
		if (chan_locks[chan] != -1) {
			relock_sid = chan_locks[chan];

			// It has, so what we want to do is unlock the channel, then
			for (int c = 0; c < 16; c++) {
				if (chan_map[relock_sid][c] == chan) {
					relock_log = c;
					break;
				}
			}

			// Release the previous lock
			lockChannel(relock_sid, relock_log, false);
		}

		// Now protect the channel
		chan_locks[chan]            = -2;
		chan_map[sequence_id][chan] = -3;

		// And attempt to get the other a new channel to lock
		if (relock_sid != -1) {
			lockChannel(relock_sid, relock_log, true);
		}
	}

	return 0;
}

int LowLevelMidiDriver::lockChannel(uint16 sequence_id, int chan, bool lock) {
	// When locking, we want to get the highest chan number with the lowest
	// number of notes playing, that aren't already locked and don't have
	// protection
	if (lock) {
		int notes_on[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		int s;
		int c;
		int phys;

		for (s = 0; s < LLMD_NUM_SEQ; s++) {
			if (sequences[s]) {
				for (c = 0; c < 16; c++) {
					phys = chan_map[s][c];
					if (phys == -1) {
						phys = c;
					}
					if (phys != -2) {
						notes_on[phys] += sequences[s]->countNotesOn(c);
					}
				}
			}
		}

		phys         = -1;
		int prev_max = 128;
		for (c = 0; c < 16; c++) {
			// Protected or locked
			if (chan_locks[c] != -1) {
				continue;
			}

			if (notes_on[c] <= prev_max) {
				prev_max = notes_on[c];
				phys     = c;
			}
		}

		// Oh no, no channel to lock!
		if (phys == -1) {
			return -1;
		}

		// Now tell everyone that they lost their channel
		for (s = 0; s < LLMD_NUM_SEQ; s++) {
			// Make sure they are mapping defualt
			if (sequences[s] && chan_map[s][phys] == -1) {
				sequences[s]->loseChannel(phys);
				chan_map[s][phys] = -2;    // Can't use it
			}
		}

		// We are losing our old logical channel too
		if (phys != chan) {
			sequences[sequence_id]->loseChannel(chan);
		}

		// Ok, got our channel
		chan_map[sequence_id][chan] = phys;
		chan_locks[phys]            = sequence_id;
		sequences[sequence_id]->gainChannel(chan);
	}
	// Unlock the channel
	else {
		const int phys = chan_map[sequence_id][chan];

		// Not locked
		if (phys < 0) {
			return -1;
		}

		// First, we'll lose our logical channel
		if (sequences[sequence_id]) {
			sequences[sequence_id]->loseChannel(chan);
		}

		// Now unlock it
		chan_map[sequence_id][chan] = -1;
		chan_locks[phys]            = -1;

		// Gain our logical channel back
		if (phys != chan && sequences[sequence_id]) {
			sequences[sequence_id]->gainChannel(chan);
		}

		// Now let everyone gain their channel that we stole
		for (int s = 0; s < LLMD_NUM_SEQ; s++) {
			// Make sure they are mapping defualt
			if (sequences[s] && chan_map[s][phys] == -2) {
				chan_map[s][phys] = -1;
				sequences[s]->gainChannel(phys);
			}
		}
	}

	return 0;
}

int LowLevelMidiDriver::unlockAndUnprotectChannel(uint16 sequence_id) {
	// For each channel
	for (int c = 0; c < 16; c++) {
		const int phys = chan_map[sequence_id][c];

		// Doesn't need anything done to it
		if (phys >= 0 && chan_locks[phys] == sequence_id) {
			continue;
		}

		// We are protecting
		if (phys == -3) {
			protectChannel(sequence_id, c, false);
		}
		// We are locking
		else {
			lockChannel(sequence_id, c, false);
		}
	}
	return 0;
}

// Load Timbre Library
void LowLevelMidiDriver::loadTimbreLibrary(
		IDataSource* ds, TimbreLibraryType type) {
	// Ensure all sequences are stopped
	uint32 i;
	uint32 j;
	for (i = 0; i < LLMD_NUM_SEQ; i++) {
		ComMessage message(LLMD_MSG_FINISH, i);
		sendComMessage(message);
	}

	// Wait till all pending commands have been executed
	waitTillNoComMessages();
	std::unique_ptr<XMidiFile> xmidi;

	{
		// Lock!
		const std::lock_guard<std::mutex> lock(*mutex);

		// Kill all existing timbres and stuff

		// Free memory
		for (i = 0; i < 128; i++) {
			if (mt32_patch_banks[i]) {
				for (j = 0; j < 128; j++) {
					delete mt32_patch_banks[i][j];
				}
			}
			if (mt32_timbre_banks[i]) {
				for (j = 0; j < 128; j++) {
					delete mt32_timbre_banks[i][j];
				}
			}

			// Delete the bank
			delete[] mt32_patch_banks[i];
			delete[] mt32_timbre_banks[i];

			delete mt32_rhythm_bank[i];
		}

		// Zero the memory
		std::fill(
				std::begin(mt32_patch_banks), std::end(mt32_patch_banks),
				nullptr);

		// Zero the memory
		std::fill(
				std::begin(mt32_timbre_banks), std::end(mt32_timbre_banks),
				nullptr);

		// Mask it out
		for (auto& timbre : mt32_timbre_used) {
			std::fill(std::begin(timbre), std::end(timbre), -1);
		}

		// Zero the memory
		for (auto& bank : mt32_bank_sel) {
			std::fill(std::begin(bank), std::end(bank), 0);
		}

		// Zero the memory; note: this is inconsistent with
		// LowLevelMidiDriver::initMidiDriver
		std::fill(
				std::begin(mt32_patch_bank_sel), std::end(mt32_patch_bank_sel),
				-1);

		// Zero
		std::fill(
				std::begin(mt32_rhythm_bank), std::end(mt32_rhythm_bank),
				nullptr);

		// Setup Default Patch library
		mt32_patch_banks[0] = new MT32Patch*[128];
		for (i = 0; i < 128; i++) {
			mt32_patch_banks[0][i] = new MT32Patch;

			// Setup Patch Defaults
			*(mt32_patch_banks[0][i])           = mt32_patch_template;
			mt32_patch_banks[0][i]->timbre_bank = 0;
			mt32_patch_banks[0][i]->timbre_num  = i;
		}

		// TODO: This should not be hard-coded.
		// Setup default rhythm library (thanks to the munt folks for making
		// it show data on the terminal). This data is the same for all of
		// the originals.
		static const MT32RhythmSpec default_rhythms[] = {
				{28, {0x00, 0x5a, 0x07, 0x00}},
                {33, {0x06, 0x64, 0x07, 0x01}},
				{74, {0x01, 0x5a, 0x05, 0x00}},
                {76, {0x01, 0x5a, 0x06, 0x00}},
				{77, {0x01, 0x5a, 0x07, 0x00}},
                {78, {0x02, 0x64, 0x07, 0x01}},
				{79, {0x01, 0x5a, 0x08, 0x00}},
                {80, {0x05, 0x5a, 0x07, 0x01}},
				{81, {0x01, 0x5a, 0x09, 0x00}},
                {82, {0x03, 0x5f, 0x07, 0x01}},
				{83, {0x04, 0x64, 0x04, 0x01}},
                {84, {0x04, 0x64, 0x05, 0x01}},
				{85, {0x04, 0x64, 0x06, 0x01}},
                {86, {0x04, 0x64, 0x07, 0x01}},
				{87, {0x04, 0x64, 0x08, 0x01}}
        };

		for (const auto& default_rhythm : default_rhythms) {
			loadRhythm(default_rhythm.rhythm, default_rhythm.note);
		}

		if (type == TIMBRE_LIBRARY_U7VOICE_MT) {
			xmidi = std::make_unique<XMidiFile>(
					ds, XMIDIFILE_HINT_U7VOICE_MT_FILE);
		} else if (type == TIMBRE_LIBRARY_SYX_FILE) {
			xmidi = std::make_unique<XMidiFile>(ds, XMIDIFILE_HINT_SYX_FILE);
		} else if (type == TIMBRE_LIBRARY_XMIDI_FILE) {
			xmidi = std::make_unique<XMidiFile>(
					ds, XMIDIFILE_HINT_SYSEX_IN_MID);
		} else if (type == TIMBRE_LIBRARY_XMIDI_MT) {
			loadXMidiTimbreLibrary(ds);
			ds->seek(0);
			xmidi = std::make_unique<XMidiFile>(
					ds,
					XMIDIFILE_HINT_XMIDI_MT_FILE);    // a bit of a hack, it
													  // just sets up a few
													  // sysex messages
		}
	}

	if (!xmidi) {
		return;
	}

	// Subtle bug warning: the LLMD_MSG_PLAY adds the eventlist to the
	// play queue, resulting in ite eventual deletion!
	// XMidiEventList *eventlist = xmidi->GetEventList(0);
	XMidiEventList* eventlist = xmidi->StealEventList();
	if (!eventlist) {
		return;
	}

	extractTimbreLibrary(eventlist);

	uploading_timbres = true;

	// Play the SysEx data - no wait, don't it makes startup slow.
	// pout << "Loading Timbres" << std::endl;

	ComMessage message(LLMD_MSG_PLAY, 3);
	message.data.play.list   = eventlist;
	message.data.play.repeat = false;
	message.data.play.volume = 255;
	message.data.play.branch = 0;
	sendComMessage(message);

	if (type == TIMBRE_LIBRARY_XMIDI_FILE) {
		message.type                  = LLMD_MSG_SET_SPEED;
		message.sequence              = 3;
		message.data.speed.percentage = 100;
		sendComMessage(message);
	}

	// If we want to precache
	if (precacheTimbresOnStartup) {
		waitTillNoComMessages();

		bool is_playing = true;

		do {
			{
				const std::lock_guard<std::mutex> lock(*mutex);
				is_playing = playing[3];
			}

			if (is_playing) {
				yield();
			}
		} while (is_playing);
		uploading_timbres = false;

		ComMessage precache(LLMD_MSG_PRECACHE_TIMBRES, -1);
		precache.data.precache.list = nullptr;
		sendComMessage(precache);
	}
}

void LowLevelMidiDriver::extractTimbreLibrary(XMidiEventList* eventlist) {
	const uint32 timbre_add_start = ConvBaseToActual(timbre_base);
	const uint32 timbre_add_end   = timbre_add_start + timbre_mem_offset(64);
	const uint32 patch_add_start  = ConvBaseToActual(patch_base);
	const uint32 patch_add_end    = patch_add_start + patch_mem_offset(128);
	const uint32 rhythm_add_start = ConvBaseToActual(rhythm_base);
	const uint32 rhythm_add_end   = rhythm_add_start + rhythm_mem_offset(85);
	const uint32 timbre_temp_add_start = ConvBaseToActual(timbre_temp_base);
	const uint32 timbre_temp_add_end
			= timbre_temp_add_start + timbre_mem_offset(8);
	const uint32 patch_temp_add_start = ConvBaseToActual(patch_temp_base);
	const uint32 patch_temp_add_end
			= patch_temp_add_start + patch_temp_offset(8);
	const uint32 timbre_unk_add_start = ConvBaseToActual(timbre_unk_base);
	const uint32 timbre_unk_add_end
			= timbre_unk_add_start + timbre_mem_offset(128);

	int          i    = 0;
	XMidiEvent** next = nullptr;
	for (XMidiEvent** event = &eventlist->events; *event; event = next) {
		next = &((*event)->next);

		if ((*event)->status != 0xF0) {
			(*event)->time = i;
			if ((*event)->status != 0xFF) {
				i += 12;
			}
			continue;
		}

		const uint16 length = (*event)->ex.sysex_data.len;
		uint8*       msg    = (*event)->ex.sysex_data.buffer;

		if (msg && length > 7 && msg[0] == 0x41 && msg[1] == 0x10
			&& msg[2] == 0x16 && msg[3] == 0x12) {
			bool   remove         = false;
			uint32 actual_address = (msg[4] << 14) | (msg[5] << 7) | msg[6];

			uint32 sysex_size = length - (4 + 3 + 1 + 1);
			msg += 7;

			if ((actual_address + sysex_size) >= timbre_add_start
				&& actual_address < timbre_add_end) {
				uint32 start = actual_address;
				// uint32 size = sysex_size;
				if (actual_address < timbre_add_start) {
					sysex_size -= timbre_add_start - actual_address;
					actual_address = timbre_add_start;
				}
				// if ((start+size) > timbre_add_end) size =
				// timbre_add_end-start;
				start -= timbre_add_start;
				start /= 256;

				// Allocate memory
				if (!mt32_timbre_banks[2]) {
					mt32_timbre_banks[2] = new MT32Timbre* [128] {};
				}
				if (!mt32_timbre_banks[2][start]) {
					mt32_timbre_banks[2][start] = new MT32Timbre;
				}

				// Setup Timbre defaults
				mt32_timbre_banks[2][start]->time_uploaded = 0;
				mt32_timbre_banks[2][start]->index         = -1;
				mt32_timbre_banks[2][start]->protect       = false;

				// Read the timbre into the buffer
				std::memcpy(mt32_timbre_banks[2][start]->timbre, msg, 246);

				remove = true;
			}
			if ((actual_address + sysex_size) >= patch_add_start
				&& actual_address < patch_add_end) {
				uint32 start = actual_address;
				uint32 size  = sysex_size;
				if (actual_address < patch_add_start) {
					sysex_size -= patch_add_start - actual_address;
					actual_address = patch_add_start;
				}
				if ((start + size) > patch_add_end) {
					size = patch_add_end - start;
				}
				start -= patch_add_start;
				start /= 8;
				size /= 8;

				// Set the current patch bank to -1
				for (uint32 patch = start; patch < start + size;
					 patch++, msg += 8) {
					mt32_patch_bank_sel[patch] = -1;

					std::memcpy(mt32_patch_banks[0][patch], msg, 8);
					if (mt32_patch_banks[0][patch]->timbre_bank == 1) {
						mt32_patch_banks[0][patch]->timbre_bank = 0;
						mt32_patch_banks[0][patch]->timbre_num += 0x40;
					} else if (mt32_patch_banks[0][patch]->timbre_bank == 3) {
						mt32_patch_banks[0][patch]->timbre_bank = -1;
					}
				}

				remove = true;
			}
			if ((actual_address + sysex_size) >= rhythm_add_start
				&& actual_address < rhythm_add_end) {
				uint32 start = actual_address;
				uint32 size  = sysex_size;
				if (actual_address < rhythm_add_start) {
					// sysex_size -= rhythm_add_start-actual_address;
					actual_address = rhythm_add_start;
				}
				if ((start + size) > rhythm_add_end) {
					size = rhythm_add_end - start;
				}
				start -= rhythm_add_start;
				start /= 4;
				size /= 4;

				// Set the current patch bank to -1
				for (uint32 temp = start; temp < start + size;
					 temp++, msg += 4) {
					if (!mt32_rhythm_bank[temp]) {
						mt32_rhythm_bank[temp] = new MT32Rhythm;
					}
					std::memcpy(mt32_rhythm_bank[temp], msg, 4);
				}

				remove = true;
			}
			if (actual_address >= timbre_temp_add_start
				&& actual_address < timbre_temp_add_end) {
				remove = true;
			}
			if (actual_address >= timbre_unk_add_start
				&& actual_address < timbre_unk_add_end) {
				remove = true;
			}
			if (actual_address >= patch_temp_add_start
				&& actual_address < patch_temp_add_end) {
				remove = true;
			}

			if (remove) {
				XMidiEvent* e = *event;
				*event        = *next;
				next          = event;
				e->next       = nullptr;
				e->FreeThis();
			} else {
				(*event)->time = i;
				i += 12;
			}
		}
	}
}

// If data is nullptr, then it is assumed that sysex_buffer already contains the
// data address_base is 7-bit, while address_offset is 8 bit!
void LowLevelMidiDriver::sendMT32SystemMessage(
		uint32 address_base, uint16 address_offset, uint32 len,
		const void* data) {
	unsigned char sysex_buffer[512];

	// MT32 Sysex Header
	sysex_buffer[0] = 0x41;    // Roland SysEx ID
	sysex_buffer[1] = 0x10;    // Device ID (assume 0x10, Device 17)
	sysex_buffer[2] = 0x16;    // MT-32 Model ID
	sysex_buffer[3] = 0x12;    // DTI Command ID (set data)

	// 7-bit address
	const uint32 actual_address
			= address_offset + ConvBaseToActual(address_base);
	sysex_buffer[4] = (actual_address >> 14) & 0x7F;
	sysex_buffer[5] = (actual_address >> 7) & 0x7F;
	sysex_buffer[6] = actual_address & 0x7F;

	// Only copy if required
	if (data) {
		std::memcpy(sysex_buffer + sysex_data_start, data, len);
	}

	// Calc checksum
	char checksum = 0;
	for (uint32 j = 4; j < sysex_data_start + len; j++) {
		checksum += sysex_buffer[j];
	}

	checksum = checksum & 0x7f;
	if (checksum) {
		checksum = 0x80 - checksum;
	}

	// Set checksum
	sysex_buffer[sysex_data_start + len] = checksum;

	// Terminator
	sysex_buffer[sysex_data_start + len + 1] = 0xF7;

	// Just send it

	auto now = std::chrono::steady_clock::now().time_since_epoch();
	// Making assumption that software MT32 can instantly consume sysex
	if (!isSampleProducer() && next_sysex > now) {
		std::this_thread::sleep_for(
				next_sysex - now);    // Wait till we think the buffer is empty
	}
	send_sysex(0xF0, sysex_buffer, sysex_data_start + len + 2);
	next_sysex = std::chrono::duration_cast<decltype(next_sysex)>(
			std::chrono::steady_clock::now().time_since_epoch()
			+ std::chrono::milliseconds(
					static_cast<std::chrono::milliseconds::rep>(
							(40 + (sysex_data_start + len + 2 + 2) * 1000.0)
							/ 3125.0)));
}

void LowLevelMidiDriver::setPatchBank(int bank, int patch) {
	// Invalid bank
	if (bank < -1 || bank > 127) {
		perr << "LLMD: Invalid bank in setPatchBank" << std::endl;
		return;
	}
	// Invalid Patch
	else if (patch < 0 || patch > 127) {
		perr << "LLMD: Invalid patch in setPatchBank" << std::endl;
		return;
	}
	// Bank doesn't exist
	else if (bank != -1 && !mt32_patch_banks[bank]) {
		perr << "LLMD: Patch bank doesn't exist in setPatchBank" << std::endl;
		return;
	}
	// Patch hasn't been loaded
	if (bank != -1 && !mt32_patch_banks[bank][patch]) {
		perr << "LLMD: Patch not loaded in setPatchBank" << std::endl;
		return;
	}

	MT32Patch p;

	if (bank == -1) {
		p             = mt32_patch_template;
		p.timbre_bank = 0;
		p.timbre_num  = patch;
	} else {
		p = *(mt32_patch_banks[bank][patch]);
	}

	// Custom bank?
	if (p.timbre_bank > 0) {
		// If no timbre loaded, we do nothing!
		if (!mt32_timbre_banks[p.timbre_bank]
			|| !mt32_timbre_banks[p.timbre_bank][p.timbre_num]) {
			perr << "LLMD: Timbre not loaded in setPatchBank" << std::endl;
			return;
		}

		// Upload timbre if required
		if (mt32_timbre_banks[p.timbre_bank][p.timbre_num]->index == -1) {
			// pout << "LLMD: Need to upload timbre" << std::endl;
			uploadTimbre(p.timbre_bank, p.timbre_num);
		}

		p.timbre_num  = mt32_timbre_banks[p.timbre_bank][p.timbre_num]->index;
		p.timbre_bank = 2;

		// Upload failed so do nothing
		if (p.timbre_num == -1) {
			perr << "LLMD: Timbre failed to upload" << std::endl;
			return;
		}
	} else if (p.timbre_bank == -1) {    // Rhythm
		p.timbre_bank = 3;
	} else if (patch <= 0x3f) {    // Bank 0
		p.timbre_bank = 0;
	} else {    // Bank 1
		p.timbre_bank = 1;
		p.timbre_num  = p.timbre_num & 0x3f;
	}

	// Set the correct bank
	mt32_patch_bank_sel[patch] = bank;

	// Upload the patch
#ifdef DEBUG
	pout << "LLMD: Uploading Patch for " << bank << ":" << patch
		 << " using timbre " << static_cast<int>(p.timbre_bank) << ":"
		 << static_cast<int>(p.timbre_num) << std::endl;
#endif
	sendMT32SystemMessage(
			patch_base, patch_mem_offset(patch), patch_mem_size, &p);
}

void LowLevelMidiDriver::loadRhythm(const MT32Rhythm& rhythm, int note) {
#ifdef DEBUG
	pout << "LLMD: Uploading Rhythm for note " << note << std::endl;
#endif
	sendMT32SystemMessage(
			rhythm_base, rhythm_mem_offset_note(note), rhythm_mem_size,
			&rhythm);
}

void LowLevelMidiDriver::loadRhythmTemp(int temp) {
	if (!mt32_rhythm_bank[temp]) {
		return;
	}

	const int timbre = mt32_rhythm_bank[temp]->timbre;

	if (timbre < 0x40) {
		// If no timbre loaded, we do nothing!
		if (!mt32_timbre_banks[2] || !mt32_timbre_banks[2][timbre]) {
			perr << "LLMD: Timbre not loaded in loadRhythmTemp" << std::endl;
			return;
		}

		// Upload timbre if required
		if (mt32_timbre_banks[2][timbre]->index == -1) {
			// pout << "LLMD: Need to upload timbre" << std::endl;
			uploadTimbre(2, timbre);
		}

		mt32_timbre_banks[2][timbre]->protect = true;
		mt32_rhythm_bank[temp]->timbre = mt32_timbre_banks[2][timbre]->index;
	}

#ifdef DEBUG
	pout << "LLMD: Uploading Rhythm Temp " << temp << " using timbre "
		 << static_cast<int>(mt32_rhythm_bank[temp]->timbre) << std::endl;
#endif
	sendMT32SystemMessage(
			rhythm_base, rhythm_mem_offset(temp), rhythm_mem_size,
			mt32_rhythm_bank[temp]);

	delete mt32_rhythm_bank[temp];
	mt32_rhythm_bank[temp] = nullptr;
}

void LowLevelMidiDriver::uploadTimbre(int bank, int patch) {
	// If no timbre loaded, we do nothing!
	if (!mt32_timbre_banks[bank] || !mt32_timbre_banks[bank][patch]) {
		perr << "LLMD: No bank loaded in uploadTimbre" << std::endl;
		return;
	}

	// Already uploaded
	if (mt32_timbre_banks[bank][patch]->index != -1) {
		perr << "LLMD: Timbre already loaded in uploadTimbre" << std::endl;
		return;
	}

	int    lru_index = -1;
	uint32 lru_time  = 0xFFFFFFFF;

	for (int i = 0; i < 64; i++) {
		const int tbank  = mt32_timbre_used[i][0];
		const int tpatch = mt32_timbre_used[i][1];

		// Timbre is unused, so we will use it
		if (tbank == -1) {
			lru_index = i;
			break;
		}

		// Just make sure it exists
		if (!mt32_timbre_banks[tbank] || !mt32_timbre_banks[tbank][tpatch]) {
			continue;
		}

		// Protected so ignore
		if (mt32_timbre_banks[tbank][tpatch]->protect) {
			continue;
		}

		// Inuse so ignore
		if (mt32_patch_bank_sel[tpatch] == bank) {
			continue;
		}

		// Is it LRU??
		if (mt32_timbre_banks[tbank][tpatch]->time_uploaded < lru_time) {
			lru_index = i;
			lru_time  = mt32_timbre_banks[tbank][tpatch]->time_uploaded;
		}
	}

	if (lru_index == -1) {
		return;
	}

	const int lru_bank  = mt32_timbre_used[lru_index][0];
	const int lru_patch = mt32_timbre_used[lru_index][1];

	// Unsetup the old timbre
	if (lru_bank != -1) {
		mt32_timbre_banks[lru_bank][lru_patch]->index = -1;
	}

	// Setup the New one
	mt32_timbre_used[lru_index][0] = bank;
	mt32_timbre_used[lru_index][1] = patch;

	mt32_timbre_banks[bank][patch]->index         = lru_index;
	mt32_timbre_banks[bank][patch]->protect       = false;
	mt32_timbre_banks[bank][patch]->time_uploaded = xmidi_clock.count();

	// Now send the SysEx message
	char name[11] = {0};
	std::memcpy(name, mt32_timbre_banks[bank][patch]->timbre, 10);

#ifdef DEBUG
	pout << "LLMD: Uploading Custom Timbre `" << name << "` from " << bank
		 << ":" << patch << " into 2:" << lru_index << std::endl;
#endif
	sendMT32SystemMessage(
			timbre_base, timbre_mem_offset(lru_index), timbre_mem_size,
			mt32_timbre_banks[bank][patch]->timbre);
}

void LowLevelMidiDriver::loadXMidiTimbreLibrary(IDataSource* ds) {
	uint32 i;

	// Read all the timbres
	for (i = 0; ds->getAvail() > 0; i++) {
		// Seek to the entry
		ds->seek(i * 6);

		const uint32 patch = static_cast<uint8>(ds->read1());
		const uint32 bank  = static_cast<uint8>(ds->read1());

		// If we read both == 255 then we've read all of them
		if (patch == 255 || bank == 255) {
			// POUT ("Finished " << patch << ": ");
			break;
		}

		// Get offset and seek to it
		const uint32 offset = ds->read4();
		ds->seek(offset);

		const uint16 size = ds->read2();

		char name[11] = {0};
		ds->read(name, 10);
		// pout << name << ": " << i << " = " << bank << ":" << patch << " @ "
		// << offset << std::endl;

		if (size != 0xF8) {
			pout << "LLMD: Patch " << i << " = " << bank << ":" << patch
				 << " @ " << offset << ". Bad size:" << size << std::endl;
		}

		ds->seek(offset + 2);

		// Allocate memory
		if (!mt32_timbre_banks[bank]) {
			mt32_timbre_banks[bank] = new MT32Timbre* [128] {};
		}
		if (!mt32_timbre_banks[bank][patch]) {
			mt32_timbre_banks[bank][patch] = new MT32Timbre;
		}

		if (!mt32_patch_banks[bank]) {
			mt32_patch_banks[bank] = new MT32Patch* [128] {};
		}
		if (!mt32_patch_banks[bank][patch]) {
			mt32_patch_banks[bank][patch] = new MT32Patch;
		}

		// Setup Patch Defaults
		*(mt32_patch_banks[bank][patch])           = mt32_patch_template;
		mt32_patch_banks[bank][patch]->timbre_bank = bank;
		mt32_patch_banks[bank][patch]->timbre_num  = patch;

		// Setup Timbre defaults
		mt32_timbre_banks[bank][patch]->time_uploaded = 0;
		mt32_timbre_banks[bank][patch]->index         = -1;
		mt32_timbre_banks[bank][patch]->protect       = false;

		// Read the timbre into the buffer
		ds->read(mt32_timbre_banks[bank][patch]->timbre, 246);
	}
}

// Protection
