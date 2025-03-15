/*
Copyright (C) 2003  The Pentagram Team
Copyright (C) 2018-2022  The Exult Team

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

#ifndef WINDOWSMIDIDRIVER_H_INCLUDED
#define WINDOWSMIDIDRIVER_H_INCLUDED

#if defined(_WIN32)
// Guard the define in case the build process already has defined it
#	ifndef USE_WINDOWS_MIDI
#		define USE_WINDOWS_MIDI
#	endif

#	include "LowLevelMidiDriver.h"

// Slight hack here. Uncomment it to enable the ability to use
// both A and B devices on an SB Live to distribute the notes
// #define WIN32_USE_DUAL_MIDIDRIVERS

#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif

#	include <windows.h>

#	include <mmsystem.h>

class WindowsMidiDriver : public LowLevelMidiDriver {
	signed int dev_num   = -1;
	HMIDIOUT   midi_port = nullptr;
#	ifdef WIN32_USE_DUAL_MIDIDRIVERS
	HMIDIOUT midi_port2 = nullptr;
#	endif

	// SysEx stuff. Borrowed from ScummVM
	MIDIHDR _streamHeader;
	uint8*  _streamBuffer     = nullptr;
	int     _streamBufferSize = 0;
	HANDLE  _streamEvent      = nullptr;

	static const MidiDriverDesc desc;

	static std::shared_ptr<MidiDriver> createInstance() {
		return std::make_shared<WindowsMidiDriver>();
	}

	static bool doMCIError(MMRESULT mmsys_err);

public:
	WindowsMidiDriver() : LowLevelMidiDriver(std::string(desc.name)) {}

	static const MidiDriverDesc* getDesc() {
		return &desc;
	}

	std::vector<ConfigSetting_widget::Definition> GetSettings() override;

	bool isRealMT32Supported() const override {
		return true;
	}

protected:
	int  open() override;
	void close() override;
	void send(uint32 message) override;
	void send_sysex(uint8 status, const uint8* msg, uint16 length) override;
	void increaseThreadPriority() override;
};

#endif    //_WIN32

#endif    // WINDOWSMIDIDRIVER_H_INCLUDED
