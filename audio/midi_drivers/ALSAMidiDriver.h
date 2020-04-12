/*
Copyright (C) 2004  The Pentagram Team

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

#ifndef ALSAMIDIDRIVER_H_INCLUDED
#define ALSAMIDIDRIVER_H_INCLUDED

#ifdef USE_ALSA_MIDI

#include <alsa/asoundlib.h>
#include <string>
#include "LowLevelMidiDriver.h"

class ALSAMidiDriver : public LowLevelMidiDriver
{
	const static MidiDriverDesc desc;
	static MidiDriver *createInstance() {
		return new ALSAMidiDriver();
	}

public:	
	static const MidiDriverDesc* getDesc() { return &desc; }
	ALSAMidiDriver();

protected:
	int			open() override;
	void		close() override;
	void		send(uint32 b) override;
//	void		yield() override;
	void		send_sysex(uint8 status, const uint8 *msg,
								   uint16 length) override;

	std::string devname;
	bool isOpen;

	snd_seq_event_t ev;
	snd_seq_t *seq_handle;
	int seq_client, seq_port;
	int my_client, my_port;

	void send_event(int do_flush);
	int parse_addr(const std::string& arg, int *client, int *port);
};

#endif

#endif
