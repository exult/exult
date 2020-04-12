/*
Copyright (C) 2003  The Pentagram Team

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
#include "TimidityMidiDriver.h"

#ifdef USE_TIMIDITY_MIDI

#include "timidity/timidity.h"

const MidiDriver::MidiDriverDesc TimidityMidiDriver::desc = 
		MidiDriver::MidiDriverDesc ("Timidity", createInstance);

TimidityMidiDriver::TimidityMidiDriver()
{
}

int TimidityMidiDriver::open()
{
	sint32 encoding = PE_16BIT|PE_SIGNED;
	if (!stereo) encoding |= PE_MONO;
	if (NS_TIMIDITY::Timidity_Init_Simple(sample_rate,65536,encoding)) 
	{
		perr << NS_TIMIDITY::Timidity_Error() << std::endl;
		return 1;
	}

	memset (used_inst, true, sizeof(bool)*128);
	memset (used_drums, true, sizeof(bool)*128);

	NS_TIMIDITY::Timidity_FinalInit(used_inst,used_drums);
	return 0;
}

void TimidityMidiDriver::close()
{
	NS_TIMIDITY::Timidity_DeInit();
}

void TimidityMidiDriver::send(uint32 b)
{
	NS_TIMIDITY::Timidity_PlayEvent(b&0xFF, (b>>8)&0x7F, (b>>16)&0x7F);
}

void TimidityMidiDriver::lowLevelProduceSamples(sint16 *samples, uint32 num_samples)
{
	NS_TIMIDITY::Timidity_GenerateSamples(samples,num_samples);
}

#endif //USE_TIMIDITY_MIDI
