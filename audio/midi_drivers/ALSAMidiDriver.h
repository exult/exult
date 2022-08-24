/*
Copyright (C) 2004  The Pentagram Team
Copyright (C) 2005-2022  The Exult Team

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
	int  open() override;
	void close() override;
	void send(uint32 b) override;
//	void yield() override;
	void send_sysex(uint8 status, const uint8 *msg,
	                uint16 length) override;

	bool isOpen;

	snd_seq_event_t        ev;
	snd_seq_t             *seq_handle;
	snd_seq_client_info_t *clt_info;
	snd_seq_port_info_t   *prt_info;

	int seq_client, seq_port;
	int my_client,  my_port;
	int clt_id,     prt_id;
	unsigned int prt_capability, prt_type;
	int prt_midi_channels, prt_write_use;
	char const* clt_name;
	char const* prt_name;

	void send_event(int do_flush);
	int  parse_addr(const std::string& arg, int *client, int *port);
	bool find_next_port(bool first);
	bool identify_port(int seq_client = -1, int seq_port = -1);
};

#endif

#endif
