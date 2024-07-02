/*
Copyright (C) 2004-2005  The Pentagram Team
Copyright (C) 2012-2022  The Exult Team

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

/*
  Unix MIDI sequencer
  Adapted from ScummVM's backends/midi/seq.cpp
*/

#include "pent_include.h"

#include "UnixSeqMidiDriver.h"

#ifdef USE_UNIX_SEQ_MIDI
#	include <fcntl.h>
#	include <unistd.h>

#	include <array>
#	include <cerrno>

const MidiDriver::MidiDriverDesc UnixSeqMidiDriver::desc
		= MidiDriver::MidiDriverDesc("UnixSeq", createInstance);

constexpr const int              SeqMidiPutC = 5;
constexpr const std::string_view SeqDevice   = "/dev/sequencer";

UnixSeqMidiDriver::UnixSeqMidiDriver() : LowLevelMidiDriver(std::string(desc.name)) {
	// see if the config file specifies an alternate midi device
	devname = getConfigSetting("unixseqdevice", SeqDevice.data());
}

int UnixSeqMidiDriver::open() {
	if (isOpen) {
		return 1;
	}
	isOpen = true;
	device = 0;

	pout << "UnixSeqDevice: opening device: " << devname << std::endl;

	device = ::open(devname.c_str(), O_RDWR, 0);
	if (device < 0) {
		perr << "UnixSeqDevice: failed: " << strerror(errno);
		perr << std::endl;
		return device;
	}

	return 0;
}

void UnixSeqMidiDriver::close() {
	::close(device);
	isOpen = false;
}

void UnixSeqMidiDriver::send(uint32 b) {
	std::array<uint8, 256> buf;
	uint8*                 out = buf.data();

	switch (b & 0xF0) {
	case 0x80:
	case 0x90:
	case 0xA0:
	case 0xB0:
	case 0xE0:
		Write1(out, SeqMidiPutC);
		Write1(out, static_cast<uint8>(b));
		Write1(out, static_cast<uint8>(deviceNum));
		Write1(out, 0);
		Write1(out, SeqMidiPutC);
		Write1(out, static_cast<uint8>((b >> 8) & 0x7F));
		Write1(out, static_cast<uint8>(deviceNum));
		Write1(out, 0);
		Write1(out, SeqMidiPutC);
		Write1(out, static_cast<uint8>((b >> 16) & 0x7F));
		Write1(out, static_cast<uint8>(deviceNum));
		Write1(out, 0);
		break;
	case 0xC0:
	case 0xD0:
		Write1(out, SeqMidiPutC);
		Write1(out, static_cast<uint8>(b));
		Write1(out, static_cast<uint8>(deviceNum));
		Write1(out, 0);
		Write1(out, SeqMidiPutC);
		Write1(out, static_cast<uint8>((b >> 8) & 0x7F));
		Write1(out, static_cast<uint8>(deviceNum));
		Write1(out, 0);
		break;
	default:
		perr << "UnixSeqMidiDriver: Unknown Command: " << std::hex
			 << static_cast<int>(b) << std::dec << std::endl;
		break;
	}

	size_t size  = out - buf.data();
	size_t count = ::write(device, buf.data(), static_cast<int>(size));
	assert(count == size);
}

void UnixSeqMidiDriver::send_sysex(
		uint8 status, const uint8* msg, uint16 length) {
	if (length > 511) {
		perr << "UnixSeqMidiDriver: "
			 << "Cannot send SysEx block - data too large" << std::endl;
		return;
	}

	std::array<uint8, 2048> buf;
	uint8*                  out = buf.data();
	const uint8*            chr = msg;

	Write1(out, SeqMidiPutC);
	Write1(out, status);
	Write1(out, static_cast<uint8>(deviceNum));
	Write1(out, 0);

	for (; length != 0u; --length) {
		Write1(out, SeqMidiPutC);
		Write1(out, Read1(chr));
		Write1(out, static_cast<uint8>(deviceNum));
		Write1(out, 0);
	}

	size_t size  = out - buf.data();
	size_t count = ::write(device, buf.data(), static_cast<int>(size));
	assert(count == size);
}

#endif
