/*
Copyright (C) 2004-2005  The Pentagram Team
Copyright (C) 2006-2022  The Exult Team

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
  ALSA MIDI driver
  Adapted from ScummVM's backends/midi/alsa.cpp
  (which is in turn
   "Mostly cut'n'pasted from Virtual Tiny Keyboard (vkeybd) by Takashi Iwai" )
*/

#include "pent_include.h"

#ifdef USE_ALSA_MIDI
#	include "ALSAMidiDriver.h"

#	include <algorithm>
#	include <array>
#	include <string_view>

const MidiDriver::MidiDriverDesc ALSAMidiDriver::desc
		= MidiDriver::MidiDriverDesc("Alsa", createInstance);

#	if SND_LIB_MAJOR >= 1 || SND_LIB_MINOR >= 6
#		define snd_seq_flush_output(x)           snd_seq_drain_output(x)
#		define snd_seq_set_client_group(x, name) /*nop */
#		define my_snd_seq_open(seqp) \
			snd_seq_open(seqp, "hw", SND_SEQ_OPEN_OUTPUT, 0)
#	else
/* SND_SEQ_OPEN_OUT causes oops on early version of ALSA */
#		define my_snd_seq_open(seqp) snd_seq_open(seqp, SND_SEQ_OPEN)
#	endif

constexpr const std::string_view ALSAPort  = "65:0";
constexpr const std::string_view AddrDelim = ".:";

namespace {
	bool is_generic_midi_port(const snd_seq_port_info_t* info) {
		constexpr const unsigned mask = SND_SEQ_PORT_TYPE_MIDI_GENERIC;
		return (snd_seq_port_info_get_type(info) & mask) == mask;
	}

	bool is_midi_port_writable(const snd_seq_port_info_t* info) {
		constexpr const unsigned mask
				= SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
		return (snd_seq_port_info_get_capability(info) & mask) == mask;
	}

	bool is_writable_midi_port(const snd_seq_port_info_t* info) {
		return is_generic_midi_port(info) && is_midi_port_writable(info);
	}
}    // namespace

ALSAMidiDriver::ALSAMidiDriver() = default;

bool ALSAMidiDriver::find_next_port(bool first) {
	if (first) {
		snd_seq_client_info_set_client(clt_info, -1);
	} else {
		while (snd_seq_query_next_port(seq_handle, prt_info) >= 0) {
			if (is_writable_midi_port(prt_info)) {
				return true;
			}
		}
	}
	while (snd_seq_query_next_client(seq_handle, clt_info) >= 0) {
		int client_id = snd_seq_client_info_get_client(clt_info);
		snd_seq_port_info_set_client(prt_info, client_id);
		snd_seq_port_info_set_port(prt_info, -1);
		while (snd_seq_query_next_port(seq_handle, prt_info) >= 0) {
			if (is_writable_midi_port(prt_info)) {
				return true;
			}
		}
	}
	return false;
}

bool ALSAMidiDriver::identify_port(int arg_seq_client, int arg_seq_port) {
	if ((arg_seq_client != -1) && (arg_seq_port != -1)) {
		int ret = snd_seq_get_any_client_info(
				seq_handle, arg_seq_client, clt_info);
		if (ret < 0) {
			return false;
		}
		ret = snd_seq_get_any_port_info(
				seq_handle, arg_seq_client, arg_seq_port, prt_info);
		if (ret < 0) {
			return false;
		}
	}
	if (is_writable_midi_port(prt_info)) {
		clt_id            = snd_seq_client_info_get_client(clt_info);
		clt_name          = snd_seq_client_info_get_name(clt_info);
		prt_id            = snd_seq_port_info_get_port(prt_info);
		prt_capability    = snd_seq_port_info_get_capability(prt_info);
		prt_type          = snd_seq_port_info_get_type(prt_info);
		prt_midi_channels = snd_seq_port_info_get_midi_channels(prt_info);
		prt_write_use     = snd_seq_port_info_get_write_use(prt_info);
		prt_name          = snd_seq_port_info_get_name(prt_info);
		return true;
	}
	return false;
}

int ALSAMidiDriver::open() {
	std::string  arg;
	unsigned int caps;

	if (isOpen) {
		return -1;
	}

	if (my_snd_seq_open(&seq_handle)) {
		perr << "ALSAMidiDriver: Can't open sequencer" << std::endl;
		return -1;
	}

	isOpen = true;
	snd_seq_client_info_malloc(&clt_info);
	snd_seq_port_info_malloc(&prt_info);

	my_client = snd_seq_client_id(seq_handle);
	snd_seq_set_client_name(seq_handle, "PENTAGRAM");
	snd_seq_set_client_group(seq_handle, "input");

	caps = SND_SEQ_PORT_CAP_READ;
	if (seq_client == SND_SEQ_ADDRESS_SUBSCRIBERS) {
		caps = ~SND_SEQ_PORT_CAP_SUBS_READ;
	}
	my_port = snd_seq_create_simple_port(
			seq_handle, "PENTAGRAM", caps,
			SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
	if (my_port < 0) {
		close();
		perr << "ALSAMidiDriver: Can't create port" << std::endl;
		return -1;
	}

	pout << "Listing midi devices:" << std::endl;
	bool first = true;
	while (find_next_port(first)) {
		first = false;
		if (identify_port()) {
			pout << clt_id << ":" << prt_id << " : [ " << clt_name << " : "
				 << prt_name
				 << (prt_write_use > 0 ? " ], in use, " : " ], available, ")
				 << prt_midi_channels << " channels" << ", RW capability"
				 << ((prt_capability & SND_SEQ_PORT_CAP_READ) != 0u ? " R" : "")
				 << ((prt_capability & SND_SEQ_PORT_CAP_WRITE) != 0u ? " W"
																	 : "")
				 << ((prt_capability & SND_SEQ_PORT_CAP_DUPLEX) != 0u ? " D"
																	  : "")
				 << ((prt_capability & SND_SEQ_PORT_CAP_SUBS_READ) != 0u ? " SR"
																		 : "")
				 << ((prt_capability & SND_SEQ_PORT_CAP_SUBS_WRITE) != 0u
							 ? " SW"
							 : "")
				 << " expected W SW" << std::endl;
		}
	}

	arg = getConfigSetting("alsa_port", ALSAPort.data());

	if (parse_addr(arg, &seq_client, &seq_port) < 0) {
		close();
		perr << "ALSAMidiDriver: Invalid port: " << arg << std::endl;
		return -1;
	}

	if (seq_client != SND_SEQ_ADDRESS_SUBSCRIBERS) {
		/* subscribe to MIDI port */
		if (seq_client != my_client
			&& snd_seq_connect_to(seq_handle, my_port, seq_client, seq_port)
					   >= 0) {
			pout << "ALSAMidiDriver: "
				 << "ALSA client initialised on MIDI port [" << seq_client
				 << ":" << seq_port << "]";
			if (identify_port(seq_client, seq_port)) {
				pout << " [ " << clt_name << " : " << prt_name << " ]";
			}
			pout << std::endl;
			return 0;
		}
		perr << "ALSAMidiDriver: " << "Can't subscribe to default MIDI port ["
			 << seq_client << ":" << seq_port << "]";
		if (identify_port(seq_client, seq_port)) {
			perr << " [ " << clt_name << " : " << prt_name << " ]";
		}
		perr << ", looking for other MIDI ports" << std::endl;

		bool first = true;
		// ALSA MIDI port selection checks :
		//   type has MIDI_GENERIC, done in find_next_port,
		//   capability has WRITE and SUBSCRIBE_WRITE, done in find_next_port,
		//   capability has not READ or SUBSCRIBE_READ or DUPLEX, done here,
		//   port is available and has channels, done here.
		// If this code does not allow the desired port, such as 14:0,
		//   it can be set in .exult.cfg as
		//   <alsa_port>14:0</alsa_port> in the <midi> section.
		while (find_next_port(first)) {
			first = false;
			if ((identify_port())
				&& ((prt_capability
					 & (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_WRITE
						| SND_SEQ_PORT_CAP_DUPLEX | SND_SEQ_PORT_CAP_SUBS_READ
						| SND_SEQ_PORT_CAP_SUBS_WRITE))
					== (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE))
				&& (prt_midi_channels > 0) && (prt_write_use == 0)) {
				seq_client = clt_id;
				seq_port   = prt_id;
				if (snd_seq_connect_to(
							seq_handle, my_port, seq_client, seq_port)
					>= 0) {
					pout << "ALSAMidiDriver: "
						 << "ALSA client initialised on MIDI port ["
						 << seq_client << ":" << seq_port << "]" << " [ "
						 << clt_name << " : " << prt_name << " ]" << std::endl;
					return 0;
				}
			}
		}
		close();
		return -1;
	}
	return 0;
}

void ALSAMidiDriver::close() {
	isOpen = false;
	if (clt_info != nullptr) {
		snd_seq_client_info_free(clt_info);
		clt_info = nullptr;
	}
	if (prt_info != nullptr) {
		snd_seq_port_info_free(prt_info);
		prt_info = nullptr;
	}
	if (seq_handle != nullptr) {
		snd_seq_close(seq_handle);
		seq_handle = nullptr;
	}
	seq_client = 0;
	my_client  = 0;
	seq_port   = 0;
	my_port    = 0;
}

void ALSAMidiDriver::send(uint32 b) {
	ev.type = SND_SEQ_EVENT_OSS;
	std::array midiCmd{
			(b & 0x000000FF),
			(b & 0x0000FF00) >> 8,
			(b & 0x00FF0000) >> 16,
			(b & 0xFF000000) >> 24,
	};
	const auto& [command, param1, param2, param3] = midiCmd;

	const size_t num_elements = std::distance(
			std::begin(ev.data.raw32.d), std::end(ev.data.raw32.d));
	std::copy_n(
			std::cbegin(midiCmd), num_elements, std::begin(ev.data.raw32.d));

	unsigned char chanID = command & 0x0F;
	switch (command & 0xF0) {
	case 0x80:
		snd_seq_ev_set_noteoff(&ev, chanID, param1, param2);
		send_event(true);
		break;
	case 0x90:
		snd_seq_ev_set_noteon(&ev, chanID, param1, param2);
		send_event(true);
		break;
	case 0xB0:
		/* is it this simple ? Wow... */
		snd_seq_ev_set_controller(&ev, chanID, param1, param2);
		send_event(true);
		break;
	case 0xC0:
		snd_seq_ev_set_pgmchange(&ev, chanID, param1);
		send_event(false);
		break;
	case 0xD0:
		snd_seq_ev_set_chanpress(&ev, chanID, param1);
		send_event(false);
		break;
	case 0xE0: {
		long theBend
				= (static_cast<long>(param1) + static_cast<long>(param2 << 7))
				  - 0x2000;
		snd_seq_ev_set_pitchbend(&ev, chanID, theBend);
		send_event(true);
		break;
	}
	default:
		perr << "ALSAMidiDriver: Unknown Command: " << std::hex
			 << static_cast<int>(b) << std::dec << std::endl;
		/* I don't know if this works but, well... */
		send_event(true);
		break;
	}
}

void ALSAMidiDriver::send_sysex(uint8 status, const uint8* msg, uint16 length) {
	if (length > 511) {
		perr << "ALSAMidiDriver: Cannot send SysEx block - data too large"
			 << std::endl;
		return;
	}
	std::array<unsigned char, 1024> buf;
	unsigned char*                  out = buf.data();
	Write1(out, status);
	std::copy_n(msg, length, out);
	snd_seq_ev_set_sysex(&ev, length + 1, buf.data());
	send_event(true);
}

// static
int ALSAMidiDriver::parse_addr(
		const std::string& _arg, int* client, int* port) const {
	const char* arg = _arg.c_str();
	if (isdigit(static_cast<unsigned char>(_arg.front())) != 0) {
		const char* p = strpbrk(arg, AddrDelim.data());
		if (p == nullptr) {
			return -1;
		}
		*client = atoi(arg);
		p       = std::next(p, 1);
		*port   = atoi(p);
	} else {
		if (*arg == 's' || *arg == 'S') {
			*client = SND_SEQ_ADDRESS_SUBSCRIBERS;
			*port   = 0;
		} else {
			return -1;
		}
	}
	return 0;
}

void ALSAMidiDriver::send_event(bool do_flush) {
	snd_seq_ev_set_direct(&ev);
	snd_seq_ev_set_source(&ev, my_port);
	snd_seq_ev_set_dest(&ev, seq_client, seq_port);

	snd_seq_event_output(seq_handle, &ev);
	if (do_flush) {
		snd_seq_flush_output(seq_handle);
	}
}

#endif
