/*
Copyright (C) 2003-2005  The Pentagram Team
Copyright (C) 2013-2022  The ExultTeam

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

#ifndef MIDIDRIVER_H_INCLUDED
#define MIDIDRIVER_H_INCLUDED

#include "ConfigSetting_widget.h"
#include "common_types.h"
#include "ignore_unused_variable_warning.h"
#include "items.h"

#include <atomic>
#include <string>
class XMidiEventList;
class IDataSource;

//! The Basic High Level Pentagram Midi Driver interface.
class MidiDriver : public std::enable_shared_from_this<MidiDriver> {
protected:
	// Translatable Strings from exultmsg.txt used by Midi Drivers
	class Strings {
	public:
		static auto ReverbEffect() {
			return get_text_msg(0x6A0 - 0x400);
		}

		static auto ChorusEffect() {
			return get_text_msg(0x6A1 - 0x400);
		}

		static auto devicetype() {
			return get_text_msg(0x6A2 - 0x400);
		}

		static auto GeneralMidi() {
			return get_text_msg(0x6A3 - 0x400);
		}

		static auto GS127() {
			return get_text_msg(0x6A4 - 0x400);
		}

		static auto FakeMT32() {
			return get_text_msg(0x6A5 - 0x400);
		}

		static auto RealMT32() {
			return get_text_msg(0x6A6 - 0x400);
		}

		static auto Soundfont() {
			return get_text_msg(0x6A7 - 0x400);
		}

		static auto Win32MIDIDevice() {
			return get_text_msg(0x6A8 - 0x400);
		}

		static auto ALSAPORT() {
			return get_text_msg(0x6A9 - 0x400);
		}

		static auto CoreMIDIDevice() {
			return get_text_msg(0x6AA - 0x400);
		}
	};

	std::atomic_bool initialized = false;

public:
	//! Midi driver desription
	struct MidiDriverDesc {
		MidiDriverDesc(const char* const n, std::shared_ptr<MidiDriver> (*c)())
				: name(n), createInstance(c) {}

		const char* const name;    //!< Name of the driver (for config, dialogs)
		std::shared_ptr<MidiDriver> (
				*createInstance)();    //!< Pointer to a function
									   //!< to create
									   //!< an instance
	};

	enum TimbreLibraryType {
		TIMBRE_LIBRARY_U7VOICE_AD = 0,    // U7Voice for Adlib
		TIMBRE_LIBRARY_U7VOICE_MT = 1,    // U7Voice for MT32
		TIMBRE_LIBRARY_XMIDI_AD   = 2,    // XMIDI.AD
		TIMBRE_LIBRARY_XMIDI_MT   = 3,    // XMIDI.MT
		TIMBRE_LIBRARY_SYX_FILE   = 4,    // .SYX
		TIMBRE_LIBRARY_XMIDI_FILE
				= 5,    // Timbre is Sysex Data in MID/RMI/XMI file
		TIMBRE_LIBRARY_FMOPL_SETGM = 6    // Special to set FMOPL into GM mode
	};

	//! Initialize the driver
	//! \param sample_rate The sample rate for software synths
	//! \param stereo Specifies if a software synth must produce stero sound
	//! \return Non zero on failure
	virtual int initMidiDriver(uint32 sample_rate, bool stereo) = 0;

	//! Destroy the driver
	virtual void destroyMidiDriver() = 0;

	bool isInitialized() {
		return initialized;
	}

	//! Get the maximum number of playing sequences supported by this this
	//! driver \return The maximum number of playing sequences
	virtual int maxSequences() = 0;

	//! Start playing a sequence
	//! \param seq_num The Sequence number to use.
	//! \param list The XMidiEventList to play
	//! \param repeat If true, endlessly repeat the track
	//! \param activate If true, set the sequence as active
	//! \param vol The volume level to start playing the sequence at (0-255)
	virtual void startSequence(
			int seq_num, XMidiEventList* list, bool repeat, int vol,
			int branch = -1)
			= 0;

	//! Finish playing a sequence, and free the data
	//! \param seq_num The Sequence number to stop
	virtual void finishSequence(int seq_num) = 0;

	//! Pause the playback of a sequence
	//! \param seq_num The Sequence number to pause
	virtual void pauseSequence(int seq_num) = 0;

	//! Unpause the playback of a sequence
	//! \param seq_num The Sequence number to unpause
	virtual void unpauseSequence(int seq_num) = 0;

	//! Set the volume of a sequence
	//! \param seq_num The Sequence number to set the volume for
	//! \param vol The new volume level for the sequence, default is 255 (0-255)
	virtual void setSequenceVolume(int seq_num, int vol) = 0;

	//! Set global volume of this driver
	//! \param vol The new volume level for the sequence (0-100)
	virtual void setGlobalVolume(int vol) = 0;
	//! Get global volume of this driver
	//! \returns Current Volume [0-100]
	virtual int getGlobalVolume() = 0;

	//! Set the speed of a sequence
	//! \param seq_num The Sequence number to change it's speed
	//! \param speed The new speed for the sequence (percentage)
	virtual void setSequenceSpeed(int seq_num, int speed) = 0;

	//! Check to see if a sequence is playing (doesn't check for pause state)
	//! \param seq_num The Sequence number to check
	//! \return true is sequence is playing, false if not playing
	virtual bool isSequencePlaying(int seq_num) = 0;

	//! Set or Unset Sequence repeat flat
	//! \param seq_num The Sequence number to change
	//! \param repeat The new value for the repeaat flag
	virtual void setSequenceRepeat(int seq_num, bool repeat) = 0;

	// Get Sequence length in ms, returns UINT32_MAX if unsupported or error
	virtual uint32 getPlaybackLength(int seq_num) {
		ignore_unused_variable_warning(seq_num);
		return UINT32_MAX;
	}

	// Get Sequence position in ms, returns UINT32_MAX if unsupported or error
	virtual uint32 getPlaybackPosition(int seq_num) {
		ignore_unused_variable_warning(seq_num);
		return UINT32_MAX;
	}

	//! Get the callback data for a specified sequence
	//! \param seq_num The Sequence to get callback data from
	virtual uint32 getSequenceCallbackData(int seq_num) {
		ignore_unused_variable_warning(seq_num);
		return 0;
	}

	//! Is this a Software Synth/Sample producer
	virtual bool isSampleProducer() {
		return false;
	}

	/// can this device use the Real Mt32 Convert Option
	/// Should only be true for Hardwaer Midi Ports
	virtual bool isRealMT32Supported() const {
		return false;
	}

	//! Produce Samples when doing Software Synthesizing
	//! \param samples The buffer to fill with samples
	//! \param bytes The number of bytes of music to produce
	virtual void produceSamples(sint16* samples, uint32 bytes) {
		ignore_unused_variable_warning(samples, bytes);
	}

	//! Is this a FM Synth and should use the Adlib Tracks?
	virtual bool isFMSynth() {
		return false;
	}

	//! Is this a MT32 and supports MT32 SysEx?
	virtual bool isMT32() {
		return false;
	}

	//! Is this a devices that does not Timbres?
	virtual bool noTimbreSupport() {
		return false;
	}

	//! Load the Timbre Library
	virtual void loadTimbreLibrary(IDataSource*, TimbreLibraryType type) {
		ignore_unused_variable_warning(type);
	}

	//! Destructor
	virtual ~MidiDriver() = default;

	//
	// Statics to Initialize Midi Drivers and to get info
	//

	//! Get the number of devices
	static int getDriverCount();

	//! Get the name of a driver
	//! \param index Driver number
	static std::string getDriverName(uint32 index);

	const std::string& getName() const {
		return Name;
	}

	//! Create an Instance of a MidiDriver
	//! \param driverName Name of the prefered driver to create
	//! \return The created MidiDriver instance
	static std::shared_ptr<MidiDriver> createInstance(
			const std::string& desired_driver, uint32 sample_rate, bool stereo);

	static std::vector<ConfigSetting_widget::Definition> get_midi_driver_settings(
			const std::string& name);

	virtual std::vector<ConfigSetting_widget::Definition> GetSettings() {
		return {};
	}

protected:
	std::string Name;
	//! Get a configuration setting for the midi driver
	std::string getConfigSetting(
			const std::string& name, const std::string& defaultval);

	MidiDriver(std::string&& name) : Name(std::move(name)) {}
};

#endif    // MIDIDRIVER_H_INCLUDED
