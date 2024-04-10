/*
 * Copyright (C) 2001-2011 The ScummVM project
 * Copyright (C) 2005 The Pentagram Team
 * Copyright (C) 2006-2022 The Exult Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef USE_FLUIDSYNTH_MIDI
#	include "LowLevelMidiDriver.h"
#	include "common_types.h"

#	include <fluidsynth.h>

#	include <stack>

class FluidSynthMidiDriver : public LowLevelMidiDriver {
private:
	fluid_settings_t* _settings = nullptr;
	fluid_synth_t*    _synth    = nullptr;
	std::stack<int>   _soundFont;

	static const MidiDriverDesc desc;

	static std::shared_ptr<MidiDriver> createInstance() {
		return std::make_shared<FluidSynthMidiDriver>();
	}

public:
	static const MidiDriverDesc* getDesc() {
		return &desc;
	}

protected:
	// Because GCC complains about casting from const to non-const...
	int setInt(const char* name, int val);
	int setNum(const char* name, double val);
	int setStr(const char* name, const char* val);
	int getStr(const char* name, char** pval);

	// LowLevelMidiDriver implementation
	int  open() override;
	void close() override;
	void send(uint32 b) override;
	void lowLevelProduceSamples(sint16* samples, uint32 num_samples) override;

	// MidiDriver overloads
	bool isSampleProducer() override {
		return true;
	}

	bool noTimbreSupport() override {
		return true;
	}
};

#endif
