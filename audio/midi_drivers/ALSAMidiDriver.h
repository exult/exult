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
#	include "LowLevelMidiDriver.h"

#	ifdef __GNUC__
#		pragma GCC diagnostic push
#		pragma GCC diagnostic ignored "-Wold-style-cast"
#		if defined(__llvm__) || defined(__clang__)
#			if __clang_major__ >= 16
#				pragma GCC diagnostic ignored "-Wzero-length-array"
#			endif
#		endif
#	endif    // __GNUC__
#	include <alsa/asoundlib.h>
#	ifdef __GNUC__
#		pragma GCC diagnostic pop
#	endif    // __GNUC__

#	include <string>

class ALSAMidiDriver : public LowLevelMidiDriver {
	static const MidiDriverDesc desc;

	static std::shared_ptr<MidiDriver> createInstance() {
		return std::make_shared<ALSAMidiDriver>();
	}

public:
	ALSAMidiDriver() : LowLevelMidiDriver(std::string(desc.name)) {}

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
	void send(uint32 b) override;
	void send_sysex(uint8 status, const uint8* msg, uint16 length) override;

private:
	bool isOpen = false;

	snd_seq_event_t        ev;
	snd_seq_t*             seq_handle = nullptr;
	snd_seq_client_info_t* clt_info   = nullptr;
	snd_seq_port_info_t*   prt_info   = nullptr;

	int          seq_client = 0;
	int          seq_port   = 0;
	int          my_client  = 0;
	int          my_port    = 0;
	int          clt_id;
	int          prt_id;
	unsigned int prt_capability;
	unsigned int prt_type;
	int          prt_midi_channels;
	int          prt_write_use;
	const char*  clt_name;
	const char*  prt_name;

protected:
	void send_event(bool do_flush);
	int  parse_addr(const std::string& arg, int* client, int* port) const;
	bool find_next_port(bool first);
	bool identify_port(int seq_client = -1, int seq_port = -1);
};

#endif

#endif
