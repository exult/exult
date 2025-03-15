/*
Copyright (C) 2004-2005  The Pentagram Team
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

#ifndef UNIXSEQMIDIDRIVER_H_INCLUDED
#define UNIXSEQMIDIDRIVER_H_INCLUDED

#if !defined(USE_UNIX_SEQ_MIDI) && !defined(ANDROID) \
		&& (defined(UNIX) || defined(__unix__) || defined(__unix))
#	define USE_UNIX_SEQ_MIDI
#endif

#ifdef USE_UNIX_SEQ_MIDI
#	include "LowLevelMidiDriver.h"

#	include <string>

class UnixSeqMidiDriver : public LowLevelMidiDriver {
	static const MidiDriverDesc desc;

	static std::shared_ptr<MidiDriver> createInstance() {
		return std::make_shared<UnixSeqMidiDriver>();
	}

public:
	static const MidiDriverDesc* getDesc() {
		return &desc;
	}

	UnixSeqMidiDriver();

	bool isRealMT32Supported() const override {
		return true;
	}

protected:
	int  open() override;
	void close() override;
	void send(uint32 b) override;
	void send_sysex(uint8 status, const uint8* msg, uint16 length) override;

private:
	std::string devname;
	bool        isOpen    = false;
	int         device    = 0;
	int         deviceNum = 0;
};

#endif

#endif
