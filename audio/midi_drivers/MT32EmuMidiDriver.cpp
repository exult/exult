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
#include "MT32EmuMidiDriver.h"

#ifdef USE_MT32EMU_MIDI

#include "ignore_unused_variable_warning.h"

#include <cstring>

#include "XMidiFile.h"
#include "XMidiEvent.h"
#include "XMidiEventList.h"

#include "databuf.h"
#include "utils.h"

using namespace MT32Emu;

const MidiDriver::MidiDriverDesc MT32EmuMidiDriver::desc =
		MidiDriver::MidiDriverDesc ("MT32Emu", createInstance);

/*
 *	This file open proc redirects writes to a writable directory
 *	and looks in this and other directories for the MT32 data.
 */

static bool openROMFile(
	FileStream& file,
	const std::string& filename,
	bool writable
) {
	std::string basedir;
	if (!writable) {
		// May be in bundle.
		if (is_system_path_defined("<BUNDLE>")) {
			basedir = "<BUNDLE>/" + filename;
			if (file.open(get_system_path(basedir).c_str()))
				return true;
		}
		// Now try data dir.
		basedir = "<DATA>/" + filename;
		if (file.open(get_system_path(basedir).c_str()))
			return true;
		// We now fall back to a writable data dir, as the emulator
		// may have written something there.
	}
	basedir = "<SAVEHOME>/data/" + filename;
	return file.open(get_system_path(basedir).c_str());
}

static const ROMImage *getROM(
	FileStream& file,
	const std::string& filename
) {
	if (openROMFile(file, filename, false)) {
		return ROMImage::makeROMImage(&file);
	}
	return nullptr;
}

int MT32EmuMidiDriver::open() {
	// Must be stereo
	if (!stereo)
		return 1;

	// Make sure dir exists; this is the dir where data will be saved.
	U7mkdir("<SAVEHOME>/data", 0755);

	FileStream controlROMFile;
	const ROMImage *controlROMImage;
	controlROMImage = getROM(controlROMFile, "CM32L_CONTROL.ROM");
	if (!controlROMImage)
		controlROMImage = getROM(controlROMFile, "MT32_CONTROL.ROM");
	if (!controlROMImage) {
		FileStream part1;
		FileStream part2;
		if (openROMFile(part1, "MT32A.BIN", false) && openROMFile(part2, "MT32B.BIN", false)) {
			std::ofstream out;
			if (U7open(out, "<SAVEHOME>/data/MT32_CONTROL.ROM", false)) {
				const Bit8u *data1 = part1.getData();
				const Bit8u *data2 = part2.getData();
				for (size_t ii = 0; ii < std::min(part1.getSize(), part2.getSize()); ii++) {
					out.put(static_cast<char>(data1[ii]));
					out.put(static_cast<char>(data2[ii]));
				}
				out.close();
				controlROMImage = getROM(controlROMFile, "MT32_CONTROL.ROM");
			}
		}
	}
	if (!controlROMImage)
		return 1;

	FileStream pcmROMFile;
	const ROMImage *pcmROMImage;
	pcmROMImage = getROM(pcmROMFile, "CM32L_PCM.ROM");
	if (!pcmROMImage)
		pcmROMImage = getROM(pcmROMFile, "MT32_PCM.ROM");
	if (!pcmROMImage) {
		ROMImage::freeROMImage(controlROMImage);
		return 1;
	}

	mt32 = new Synth(nullptr);

	if (!mt32->open(*controlROMImage, *pcmROMImage)) {
		ROMImage::freeROMImage(controlROMImage);
		ROMImage::freeROMImage(pcmROMImage);
		delete mt32;
		mt32 = nullptr;
		return 1;
	}

	ROMImage::freeROMImage(controlROMImage);
	ROMImage::freeROMImage(pcmROMImage);
	return 0;
}

void MT32EmuMidiDriver::close() {
	if (mt32) {
		mt32->close();
		delete mt32;
		mt32 = nullptr;
	}
}

void MT32EmuMidiDriver::send(uint32 b) {
	mt32->playMsg(b);
}

void MT32EmuMidiDriver::send_sysex(
	uint8 status,
	const uint8 *msg,
	uint16 length
) {
	if (!msg || !length)
		return;

	if (status != 0xF0 && msg[length-1] != 0xF7)
		return;

	mt32->playSysexWithoutFraming(msg, length-1);
}

void MT32EmuMidiDriver::lowLevelProduceSamples(
	sint16 *samples,
	uint32 num_samples
) {
	mt32->render(samples,num_samples);
}

#endif //USE_MT32EMU_MIDI

