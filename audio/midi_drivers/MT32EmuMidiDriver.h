/*
Copyright (C) 2003  The Pentagram Team
Copyright (C) 2016-2022  The Exult Team

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

#ifndef MT32EMUMIDIDRIVER_H_INCLUDED
#define MT32EMUMIDIDRIVER_H_INCLUDED

#ifdef USE_MT32EMU_MIDI

#	include "LowLevelMidiDriver.h"

#	ifdef __GNUC__
#		pragma GCC diagnostic push
#		pragma GCC diagnostic ignored "-Wundef"
#		pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#		if defined(__llvm__) || defined(__clang__)
#			pragma GCC diagnostic ignored "-Wmacro-redefined"
#		endif
#	endif    // __GNUC__
#	ifdef SDL_PLATFORM_IOS
#		include <mt32emu.h>
#	else
#		include <mt32emu/mt32emu.h>
#	endif
#	ifdef __GNUC__
#		pragma GCC diagnostic pop
#	endif    // __GNUC__

namespace MT32Emu {
	class Synth;
	class SampleRateConverter;
}    // namespace MT32Emu

class MT32EmuMidiDriver : public LowLevelMidiDriver {
	static const MidiDriverDesc desc;

	static std::shared_ptr<MidiDriver> createInstance() {
		return std::make_shared<MT32EmuMidiDriver>();
	}

	MT32Emu::Synth*               mt32;
	MT32Emu::SampleRateConverter* mt32src;

public:
	MT32EmuMidiDriver()
			: LowLevelMidiDriver(std::string(desc.name)), mt32(nullptr),
			  mt32src(nullptr) {}

	static const MidiDriverDesc* getDesc() {
		return &desc;
	}

protected:
	// LowLevelMidiDriver implementation
	int  open() override;
	void close() override;
	void send(uint32 b) override;
	void send_sysex(uint8 status, const uint8* msg, uint16 length) override;
	void lowLevelProduceSamples(sint16* samples, uint32 num_samples) override;

	// MidiDriver overloads
	bool isSampleProducer() override {
		return true;
	}

	bool isMT32() override {
		return true;
	}
};

#endif    // USE_MT32EMU_MIDI

#endif    // MT32EMUMIDIDRIVER_H_INCLUDED
