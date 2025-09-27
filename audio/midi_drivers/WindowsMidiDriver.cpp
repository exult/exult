/*
Copyright (C) 2003-2005  The Pentagram Team
Copyright (C) 2013-2022  The Exult Team

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

#include "WindowsMidiDriver.h"

#ifdef USE_WINDOWS_MIDI

const MidiDriver::MidiDriverDesc WindowsMidiDriver::desc
		= MidiDriver::MidiDriverDesc("Windows", createInstance);

using std::endl;

#	ifdef WIN32_USE_DUAL_MIDIDRIVERS
#		include "utils.h"
#	endif

#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif

#	include <winbase.h>
#	include <windows.h>

#	include <mmsystem.h>

#	include <cstdlib>

bool WindowsMidiDriver::doMCIError(MMRESULT mmsys_err) {
	if (mmsys_err != MMSYSERR_NOERROR) {
		char buf[512];
		midiOutGetErrorTextA(mmsys_err, buf, 512);
		perr << buf << endl;
		return true;
	}
	return false;
}

int WindowsMidiDriver::open() {
	int i;
	// Get Win32 Midi Device num
	const std::string device = getConfigSetting("win32_device", "-1");

	const char* begin = device.c_str();
	char*       end;

	dev_num = std::strtol(begin, &end, 10);

	// If not the terminator, we assume that the string was the device name, not
	// num
	if (end[0]) {
		dev_num = -1;
	}

#	ifdef WIN32_USE_DUAL_MIDIDRIVERS
	dev_num      = -1;
	int dev_num2 = -2;
#	endif

	// List all the midi devices.
	MIDIOUTCAPS caps;
	auto        dev_count = static_cast<signed long>(midiOutGetNumDevs());
	pout << dev_count << " Midi Devices Detected" << endl;
	pout << "Listing midi devices:" << endl;

	for (i = -1; i < dev_count; i++) {
		midiOutGetDevCaps(static_cast<UINT>(i), &caps, sizeof(caps));
		pout << i << ": " << caps.szPname << endl;
		if (!Pentagram::strcasecmp(caps.szPname, device.c_str())) {
			dev_num = i;
		}
#	ifdef WIN32_USE_DUAL_MIDIDRIVERS
		if (!Pentagram::strncasecmp(caps.szPname, "SB Live! Synth A", 16)) {
			dev_num = i;
		} else if (!Pentagram::strncasecmp(
						   caps.szPname, "SB Live! Synth B", 16)) {
			dev_num2 = i;
		}
#	endif
	}

	if (dev_num < -1 || dev_num >= dev_count) {
		perr << "Warning Midi device in config is out of range." << endl;
		dev_num = -1;
	}

	midiOutGetDevCaps(static_cast<UINT>(dev_num), &caps, sizeof(caps));
	pout << "Using device " << dev_num << ": " << caps.szPname << endl;

	_streamEvent         = CreateEvent(nullptr, true, true, nullptr);
	const UINT mmsys_err = midiOutOpen(
			&midi_port, dev_num, reinterpret_cast<uintptr>(_streamEvent), 0,
			CALLBACK_EVENT);

#	ifdef WIN32_USE_DUAL_MIDIDRIVERS
	if (dev_num2 != -2 && mmsys_err != MMSYSERR_NOERROR) {
		midiOutGetDevCaps(static_cast<UINT>(dev_num2), &caps, sizeof(caps));
		if (dev_num2 != -2) {
			pout << "Using device " << dev_num2 << ": " << caps.szPname << endl;
		}
		mmsys_err = midiOutOpen(&midi_port2, dev_num2, 0, 0, 0);
	}
#	endif

	if (doMCIError(mmsys_err)) {
		perr << "Error: Unable to open win32 midi device" << endl;
		CloseHandle(_streamEvent);
		_streamEvent = nullptr;
		return 1;
	}

	// Set Win32 Midi Device num
	// config->set("config/audio/midi/win32_device", dev_num, true);

	return 0;
}

void WindowsMidiDriver::close() {
#	ifdef WIN32_USE_DUAL_MIDIDRIVERS
	if (midi_port2 != nullptr) {
		midiOutClose(midi_port2);
	}
	midi_port2 = nullptr;
#	endif
	midiOutClose(midi_port);
	midi_port = nullptr;
	CloseHandle(_streamEvent);
	_streamEvent = nullptr;
	delete[] _streamBuffer;
	_streamBuffer     = nullptr;
	_streamBufferSize = 0;
}

void WindowsMidiDriver::send(uint32 message) {
#	ifdef WIN32_USE_DUAL_MIDIDRIVERS
	if (message & 0x1 && midi_port2 != nullptr) {
		midiOutShortMsg(midi_port2, message);
	} else {
		midiOutShortMsg(midi_port, message);
	}
#	else
	midiOutShortMsg(midi_port, message);
#	endif
}

void WindowsMidiDriver::send_sysex(
		uint8 status, const uint8* msg, uint16 length) {
#	ifdef WIN32_USE_DUAL_MIDIDRIVERS
	// Hack for multiple devices. Not exactly 'fast'
	if (midi_port2 != nullptr) {
		HMIDIOUT orig_midi_port  = midi_port;
		HMIDIOUT orig_midi_port2 = midi_port2;

		// Send to port 1
		midi_port2 = nullptr;
		send_sysex(status, msg, length);

		// Send to port 2
		midi_port = orig_midi_port2;
		send_sysex(status, msg, length);

		// Return the ports to normal
		midi_port  = orig_midi_port;
		midi_port2 = orig_midi_port2;
	}
#	endif

	if (WaitForSingleObject(_streamEvent, 2000) == WAIT_TIMEOUT) {
		perr << "Error: Could not send SysEx - MMSYSTEM is still trying to "
				"send data after 2 seconds."
			 << std::endl;
		return;
	}

	if (_streamBuffer) {
		const MMRESULT result = midiOutUnprepareHeader(
				midi_port, &_streamHeader, sizeof(_streamHeader));
		if (doMCIError(result)) {
			// check_error(result);
			perr << "Error: Could not send SysEx - midiOutUnprepareHeader "
					"failed."
				 << std::endl;
			return;
		}
	}

	if (_streamBufferSize < length) {
		delete[] _streamBuffer;
		_streamBufferSize = length * 2;
		_streamBuffer     = new uint8[_streamBufferSize];
	}

	_streamBuffer[0] = status;
	memcpy(_streamBuffer + 1, msg, length);

	_streamHeader.lpData          = reinterpret_cast<char*>(_streamBuffer);
	_streamHeader.dwBufferLength  = length + 1;
	_streamHeader.dwBytesRecorded = length + 1;
	_streamHeader.dwUser          = 0;
	_streamHeader.dwFlags         = 0;

	MMRESULT result = midiOutPrepareHeader(
			midi_port, &_streamHeader, sizeof(_streamHeader));
	if (doMCIError(result)) {
		// check_error(result);
		perr << "Error: Could not send SysEx - midiOutPrepareHeader failed."
			 << std::endl;
		return;
	}

	ResetEvent(_streamEvent);
	result = midiOutLongMsg(midi_port, &_streamHeader, sizeof(_streamHeader));
	if (doMCIError(result)) {
		// check_error(result);
		perr << "Error: Could not send SysEx - midiOutLongMsg failed."
			 << std::endl;
		SetEvent(_streamEvent);
		return;
	}
}

void WindowsMidiDriver::increaseThreadPriority() {
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
}

std::vector<ConfigSetting_widget::Definition> WindowsMidiDriver::GetSettings() {
	ConfigSetting_widget::Definition midi_device{
			Strings::Win32MIDIDevice(),                   // label
			"config/audio/midi/win32_device",             // config_setting
			0,                                            // additional
			false,                                        // required
			false,                                        // unique
			ConfigSetting_widget::Definition::dropdown    // setting_type
	};

	// List all the midi devices and fill midi_device.valid.string
	MIDIOUTCAPS caps;
	auto        dev_count = static_cast<signed long>(midiOutGetNumDevs());
	midi_device.choices.reserve(dev_count + 1);

	for (signed long i = -1; i < dev_count; i++) {
		midiOutGetDevCaps(static_cast<UINT>(i), &caps, sizeof(caps));
		ConfigSetting_widget::Definition::Choice choice{
				caps.szPname, caps.szPname, std::to_string(i)};
		if (i == -1) {
			choice.value.swap(choice.alternative);
		}
		midi_device.choices.push_back(choice);
	}
	midi_device.default_value = "-1";

	auto settings = MidiDriver::GetSettings();
	settings.push_back(std::move(midi_device));
	return settings;
}

#endif    // USE_WINDOWS_MIDI
