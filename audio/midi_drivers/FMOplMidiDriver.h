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

#ifndef FMOPLMIDIDRIVER_H_INCLUDED
#define FMOPLMIDIDRIVER_H_INCLUDED

#ifdef USE_FMOPL_MIDI

#include "LowLevelMidiDriver.h"
#include "fmopl.h"

class IDataSource;

class FMOplMidiDriver : public LowLevelMidiDriver
{
	const static MidiDriverDesc	desc;
	static MidiDriver *createInstance() {
		return new FMOplMidiDriver();
	}

public:
	const static MidiDriverDesc* getDesc() { return &desc; }

protected:
	// LowLevelMidiDriver implementation
	int			open() override;
	void		close() override;
	void		send(uint32 b) override;
	void		lowLevelProduceSamples(sint16 *samples, uint32 num_samples) override;

	// MidiDriver overloads
	bool		isSampleProducer() override { return true; }
	bool		isFMSynth() override { return true; }
	void		loadTimbreLibrary(IDataSource*, TimbreLibraryType type) override;

private:

	static const unsigned char midi_fm_instruments_table[128][11];
	static const int my_midi_fm_vol_table[128];
	static int lucas_fm_vol_table[128];
	static const unsigned char adlib_opadd[9];
	static const int fnums[12];
	static const double bend_fine[256];
	static const double bend_coarse[128];

	struct midi_channel {
		int inum;
		unsigned char ins[12];
		bool xmidi;
		int	xmidi_bank;
		int vol;
		int expression;
		int nshift;
		int on;
		int pitchbend;
		int pan;
	};
	struct xmidibank {
		unsigned char	insbank[128][12];
	};

	enum {
		ADLIB_MELODIC = 0,
		ADLIB_RYTHM = 1
	};

	void midi_write_adlib(unsigned int reg, unsigned char val);
	void midi_fm_instrument(int voice, unsigned char *inst);
	int  midi_calc_volume(int chan, int vel);
	void midi_update_volume(int chan);
	void midi_fm_volume(int voice, int volume);
	void midi_fm_playnote(int voice, int note, int volume, int pitchbend);
	void midi_fm_endnote(int voice);
	unsigned char adlib_data[256];


	int chp[9][4];
	unsigned char	myinsbank[128][12];
	xmidibank		*xmidibanks[128];
	void			loadXMIDITimbres(IDataSource *ds);
	void			loadU7VoiceTimbres(IDataSource *ds);

	FMOpl_Pentagram::FM_OPL *opl;
	midi_channel ch[16];
};

#endif //USE_FMOPL_MIDI

#endif //FMOPLDRV_H
