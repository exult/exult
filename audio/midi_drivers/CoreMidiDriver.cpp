/*
Code originally written by Max Horn for ScummVM,
minor tweaks by various other people of the ScummVM, and Exult teams.

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

#include "CoreMidiDriver.h"

#ifdef USE_CORE_MIDI
#	include "ignore_unused_variable_warning.h"

#	include <pthread.h>
#	include <sched.h>

#	include <algorithm>
#	include <iomanip>
#	include <type_traits>

using namespace std;

const MidiDriver::MidiDriverDesc CoreMidiDriver::desc
		= MidiDriver::MidiDriverDesc("CoreMidi", createInstance);

CoreMidiDriver::CoreMidiDriver()
		: LowLevelMidiDriver(std::string(desc.name)), mClient(0), mOutPort(0),
		  mDest(0) {
	OSStatus err = noErr;
	err          = MIDIClientCreate(
            CFSTR("CoreMidi Driver for macOS"), nullptr, nullptr, &mClient);
	if (err != noErr) {
		perr << "CoreMidi Driver initialization failed: " << err << std::endl;
	}
}

CoreMidiDriver::~CoreMidiDriver() {
	if (mClient != 0u) {
		MIDIClientDispose(mClient);
	}
	mClient = 0;
}

int CoreMidiDriver::open() {
	if (mDest != 0u) {
		return 1;
	}

	OSStatus err = noErr;

	mOutPort = 0;

	ItemCount dests = MIDIGetNumberOfDestinations();

	// List device ID and names of CoreMidi destinations
	// kMIDIPropertyDisplayName is not compatible with OS X SDK < 10.4
	std::cout << "CoreMidi driver found " << dests
			  << " destinations:" << std::endl;
	for (ItemCount i = 0; i < dests; i++) {
		MIDIEndpointRef dest     = MIDIGetDestination(i);
		std::string     destname = "Unknown / Invalid";
		if (dest != 0u) {
			CFStringRef midiname = nullptr;
			if (MIDIObjectGetStringProperty(
						dest, kMIDIPropertyDisplayName, &midiname)
				== noErr) {
				const char* s = CFStringGetCStringPtr(
						midiname, kCFStringEncodingMacRoman);
				if (s != nullptr) {
					destname = std::string(s);
				}
			}
		}
		std::cout << i << ": " << destname.c_str() << endl;
	}

	std::string deviceIdStr;
	deviceIdStr  = getConfigSetting("coremidi_device", "");
	int deviceId = 0;
	deviceId     = atoi(deviceIdStr.c_str());

	// Default to the first CoreMidi device (ID 0)
	// when the device ID in the cfg isn't possible anymore
	if (deviceId < 0 || static_cast<ItemCount>(deviceId) >= dests) {
		if (dests == 0) {
			// Bail out if we simply don't have any midi devices.
			return 3;
		}
		std::cout << "CoreMidi destination " << deviceId
				  << " not available, trying destination 0 instead."
				  << std::endl;
		deviceId = 0;
	}

	if (deviceId < 0 || dests < static_cast<ItemCount>(deviceId)
		|| mClient == 0u) {
		return 3;
	}
	mDest = MIDIGetDestination(deviceId);
	err = MIDIOutputPortCreate(mClient, CFSTR("exult_output_port"), &mOutPort);

	if (err != noErr) {
		return 1;
	}

	return 0;
}

void CoreMidiDriver::close() {
	if (mOutPort != 0u && mDest != 0u) {
		MIDIPortDispose(mOutPort);
		mOutPort = 0;
		mDest    = 0;
	}
}

void CoreMidiDriver::send(uint32 message) {
	assert(mOutPort != 0);
	assert(mDest != 0);

	// Extract the MIDI data
	uint8 status_byte = (message & 0x000000FF);
	uint8 first_byte  = (message & 0x0000FF00) >> 8;
	uint8 second_byte = (message & 0x00FF0000) >> 16;

	// Generate a single MIDI packet with that data
	MIDIPacketList packetList;
	MIDIPacket*    packet = &packetList.packet[0];

	packetList.numPackets = 1;

	packet->timeStamp = 0;
	auto* data        = packet->data;
	Write1(data, status_byte);
	Write1(data, first_byte);
	Write1(data, second_byte);

	// Compute the correct length of the MIDI command. This is important,
	// else things may screw up badly...
	switch (status_byte & 0xF0) {
	case 0x80:    // Note Off
	case 0x90:    // Note On
	case 0xA0:    // Polyphonic Aftertouch
	case 0xB0:    // Controller Change
	case 0xE0:    // Pitch Bending
		packet->length = 3;
		break;
	case 0xC0:    // Programm Change
	case 0xD0:    // Monophonic Aftertouch
		packet->length = 2;
		break;
	default:
		perr << "CoreMIDI driver encountered unsupported status byte: 0x" << hex
			 << setw(2) << status_byte << endl;
		packet->length = 3;
		break;
	}

	// Finally send it out to the synthesizer.
	MIDISend(mOutPort, mDest, &packetList);
}

void CoreMidiDriver::send_sysex(uint8 status, const uint8* msg, uint16 length) {
	ignore_unused_variable_warning(status);
	assert(mOutPort != 0);
	assert(mDest != 0);

	std::aligned_storage_t<
			sizeof(MIDIPacketList) + 128, alignof(MIDIPacketList)>
				buf;
	auto*       packetList = new (&buf) MIDIPacketList;
	MIDIPacket* packet     = packetList->packet;

	assert(sizeof(packet->data) + 128 >= length + 2);

	packetList->numPackets = 1;

	packet->timeStamp = 0;

	// Add SysEx frame
	packet->length = length + 2;
	auto* data     = packet->data;
	Write1(data, 0xF0);
	data = std::copy_n(msg, length, data);
	Write1(data, 0xF7);

	// Send it
	MIDISend(mOutPort, mDest, packetList);
	packetList->~MIDIPacketList();
}

void CoreMidiDriver::increaseThreadPriority() {
	pthread_t          self;
	int                policy;
	struct sched_param param;

	self = pthread_self();
	pthread_getschedparam(self, &policy, &param);
	param.sched_priority = sched_get_priority_max(policy);
	pthread_setschedparam(self, policy, &param);
}

std::vector<ConfigSetting_widget::Definition> CoreMidiDriver::GetSettings() {
	ConfigSetting_widget::Definition midi_device{
			Strings::CoreMIDIDevice(),                            // label
			"config/audio/midi/coremidi_device",          // config_setting
			0,                                            // additional
			false,                                        // required
			false,                                        // unique
			ConfigSetting_widget::Definition::dropdown    // setting_type
	};

	// List all the midi devices and fill midi_device.valid.string
	ItemCount dests = MIDIGetNumberOfDestinations();
	midi_device.choices.reserve(int(dests) + 1);

	// List device ID and names of CoreMidi destinations
	// kMIDIPropertyDisplayName is not compatible with OS X SDK < 10.4

	for (ItemCount i = 0; i < dests; i++) {
		MIDIEndpointRef dest     = MIDIGetDestination(i);
		std::string     destname = "Unknown / Invalid";
		if (dest != 0u) {
			CFStringRef midiname = nullptr;
			if (MIDIObjectGetStringProperty(
						dest, kMIDIPropertyDisplayName, &midiname)
				== noErr) {
				const char* s = CFStringGetCStringPtr(
						midiname, kCFStringEncodingMacRoman);
				if (s != nullptr) {
					std::string id = std::to_string(int(i));
					midi_device.choices.push_back({s, id, id});
				}
			}
		}
		std::cout << i << ": " << destname.c_str() << endl;
	}
	midi_device.default_value = "0";

	auto settings = MidiDriver::GetSettings();
	settings.push_back(std::move(midi_device));
	return settings;
}

#endif    // USE_CORE_MIDI
