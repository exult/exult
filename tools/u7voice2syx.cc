/*
Timbres and patches in u7voice.flx:0

Format:
=======

Byte 0: Num timbres&patches
Byte 1-end: timbres&patches

Format of Timbre
================
247 Bytes total

0-246: Timbre (Common + 4 partials)
247: Patch num (range 1-128)

Send timbres to: 80000+(tnum << 9)

Sent patches to: 50000+(pnum << 3)

Send patches using: 2,tnum,24,50,24,0,1,0 (enables reverb)

*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <cstring>
#include <iostream>
#include "U7file.h"
#include "Flex.h"
#include "utils.h"
#include "databuf.h"
#include "exceptions.h"
#include "ignore_unused_variable_warning.h"

//
// MT32 SysEx
//

char sysex_buffer[8 + 256 + 2];

const uint32 sysex_data_start = 8;      // Data starts at byte 8
//const uint32 sysex_max_data_size = 256;


//
// Percussion
//

const uint32 rhythm_base = 0x030110;    // Note, these are 7 bit!
const uint32 rhythm_mem_size = 4;

const uint32 rhythm_first_note = 24;
//const uint32 rhythm_num_notes = 64;

// Memory offset based on index in the table
inline uint32 rhythm_mem_offset(uint32 index_num) {
	return index_num * rhythm_mem_size;
}

// Memory offset based on note key num
inline uint32 rhythm_mem_offset_note(uint32 rhythm_note_num) {
	return (rhythm_note_num - rhythm_first_note) * rhythm_mem_size;
}

struct RhythmSetupData {
	uint8       timbre;                 // 0-94 (M1-M64,R1-30,OFF)
	uint8       output_level;           // 0-100
	uint8       panpot;                 // 0-14 (L-R)
	uint8       reverb_switch;          // 0-1 (off,on)
};


//
// Timbre Memory Consts
//
const uint32 timbre_base = 0x080000;    // Note, these are 7 bit!
const uint32 timbre_mem_size = 246;
inline uint32 timbre_mem_offset(uint32 timbre_num) {
	return timbre_num * 256;
}


//
// Patch Memory Consts
//
const uint32 patch_base = 0x050000;     // Note, these are 7 bit!
const uint32 patch_mem_size = 8;
inline uint32 patch_mem_offset(uint32 patch_num) {
	return patch_num * 8;
}

struct PatchMemData {
	uint8       timbre_group;           // 0-3  (group A, group B, Memory, Rhythm)
	uint8       timbre_num;             // 0-63
	uint8       key_shift;              // 0-48
	uint8       fine_tune;              // 0-100 (-50 - +50)
	uint8       bender_range;           // 0-24
	uint8       assign_mode;            // 0-3 (POLY1, POLY2, POLY3, POLY4)
	uint8       reverb_switch;          // 0-1 (off,on)
	uint8       dummy;
};

const PatchMemData patch_template = {
	2,      // timbre_group
	0,      // timbre_num
	24,     // key_shift
	50,     // fine_tune
	24,     // bender_range
	0,      // assign_mode
	1,      // reverb_switch
	0       // dummy
};


//
// System Area Consts
//
const uint32 system_base = 0x100000;    // Note, these are 7 bit!
//const uint32 system_mem_size = 0x17;    // Display is 20 ASCII characters (32-127)
#include <cstddef>
#ifndef offsetof	// Broken <cstddef>? Just in case...
#   define offsetof(type, field) reinterpret_cast<uintptr>(&(static_cast<type *>(0)->field))
#endif
#define system_mem_offset(setting) offsetof(systemArea, setting)

struct systemArea {
	char masterTune;                    // MASTER TUNE 0-127 432.1-457.6Hz
	char reverbMode;                    // REVERB MODE 0-3 (room, hall, plate, tap delay)
	char reverbTime;                    // REVERB TIME 0-7 (1-8)
	char reverbLevel;                   // REVERB LEVEL 0-7 (1-8)
	char reserveSettings[9];            // PARTIAL RESERVE (PART 1) 0-32
	char chanAssign[9];                 // MIDI CHANNEL (PART1) 0-16 (1-16,OFF)
	char masterVol;                     // MASTER VOLUME 0-100
};

//
// Display  Consts
//
const uint32 display_base = 0x200000;   // Note, these are 7 bit!
const uint32 display_mem_size = 0x14;   // Display is 20 ASCII characters (32-127)

// Display messages                  0123456789ABCDEF0123
const char display_black_gate[]   = " U7: The Black Gate ";
const char display_serpent_isle[] = "U7: The Serpent Isle";

const char display_beginning_bg[] = "     3xU17 r0015    ";
const char display_beginning_si[] = "    3xU17 !5 1337   ";


//
// All Dev Reset
//
const uint32 all_dev_reset_base = 0x7f0000;


//
// U7 Percussion Table
//
// Why this crap wasn't in the flexes i will never know
//

// The key num that the data below belongs to (subtract 24 to get memory num)
uint8 U7PercussionNotes[] = {
	28, 33, 74, 76, 77, 78, 79, 80,
	81, 82, 83, 84, 85, 86, 87, 0
};

// The RhythmSetup data
RhythmSetupData U7PercussionData[] = {
	{   0,  0x5A,   0x07,   0   },  // 28
	{   6,  0x64,   0x07,   1   },  // 33
	{   1,  0x5A,   0x05,   0   },  // 74
	{   1,  0x5A,   0x06,   0   },  // 76
	{   1,  0x5A,   0x07,   0   },  // 77
	{   2,  0x64,   0x07,   1   },  // 78
	{   1,  0x5A,   0x08,   0   },  // 79
	{   5,  0x5A,   0x07,   1   },  // 80
	{   1,  0x5A,   0x09,   0   },  // 81
	{   3,  0x5F,   0x07,   1   },  // 82
	{   4,  0x64,   0x04,   1   },  // 83
	{   4,  0x64,   0x05,   1   },  // 84
	{   4,  0x64,   0x06,   1   },  // 85
	{   4,  0x64,   0x07,   1   },  // 86
	{   4,  0x64,   0x08,   1   }   // 87
};


// If data is nullptr, then it is assumed that sysex_buffer already contains the data
// address_base is 7-bit, while address_offset is 8 bit!
std::size_t fill_sysex_buffer(uint32 address_base, uint16 address_offset, uint32 len, const void *data = nullptr) {
	// SysEx status
	sysex_buffer[0] = static_cast<char>(0xF0);

	// MT32 Sysex Header
	sysex_buffer[1] = 0x41;     // Roland SysEx ID
	sysex_buffer[2] = 0x10;     // Device ID (assume 0x10, Device 17)
	sysex_buffer[3] = 0x16;     // MT-32 Model ID
	sysex_buffer[4] = 0x12;     // DTI Command ID (set data)

	// 7-bit address
	uint32 actual_address = address_offset;
	actual_address += (address_base >> 2) & (0x7f << 14);
	actual_address += (address_base >> 1) & (0x7f << 7);
	actual_address += (address_base >> 0) & (0x7f << 0);
	sysex_buffer[5] = (actual_address >> 14) & 0x7F;
	sysex_buffer[6] = (actual_address >> 7) & 0x7F;
	sysex_buffer[7] = actual_address & 0x7F;

	// Only copy if required
	if (data) std::memcpy(sysex_buffer + sysex_data_start, data, len);

	// Calc checksum
	char checksum = 0;
	for (uint32 j = 5; j < sysex_data_start + len; j++)
		checksum += sysex_buffer[j];

	// Set checksum
	sysex_buffer[sysex_data_start + len] = checksum;

	// Terminator
	sysex_buffer[sysex_data_start + len + 1] = static_cast<char>(0xF7);

	return sysex_data_start + len + 2;
}


int main(int argc, char *argv[]) {
	ignore_unused_variable_warning(argc, argv);
	uint32 i;
	uint32 j;
	uint32 patch_num;
	std::size_t num_to_write;

	char name[11];
	const char *filenames[] = { "u7voice.flx",
	                            "u7intro.tim",
	                            "mainmenu.tim"
	                          };
	const char *outnames[] = { "u7voice.syx",
	                           "u7intro.syx",
	                           "mainmenu.syx"
	                         };
	bool bgsi[] = { true, true, false };

	std::cout << "U7 Timbre Flex To MT-32 syx converter" << std::endl << std::endl;

	for (int num = 0; num < 3; num++) {
		const char *filename = filenames[num];
		const char *outname = outnames[num];

		try {
			if (!U7exists(filename)) {
				std::cerr << "Unable to find file \"" << filename << "\". You can find it in the \"";
				if (bgsi[num])
					std::cerr << "Black Gate";
				else
					std::cerr << "Serpent Isle";

				std::cerr << "\" static directory" << std::endl;

				continue;
			}

			std::cout << "Opening " << filename  << "..." << std::endl;

			FlexFile f(filename);

			std::cout << "Reading data..." << std::endl;

			// Create us a data source
			IBufferDataSource ds = f.retrieve(0);
			if (!ds.good())
				throw exult_exception("No data in index 0");

			uint32 num_timbres = ds.read1();
			std::cout << num_timbres << " custom timbres..." << std::endl;

			if (ds.getSize() != 247 * num_timbres + 1)
				throw exult_exception("File size didn't match timbre count. Wont convert.");

			std::cout << "Opening " << outname << " for writing..." << std::endl;
			std::ofstream sysex_file;
			U7open(sysex_file, outname, false);

			//
			// All Dev Reset
			//

			num_to_write = fill_sysex_buffer(all_dev_reset_base, 0, 1);

			// Write Reset
			sysex_file.write(sysex_buffer, num_to_write);

			//
			// Display
			//

			// Change the display

			if (bgsi[num])
				num_to_write = fill_sysex_buffer(
				                   display_base,
				                   0,
				                   display_mem_size,
				                   display_beginning_bg);
			else
				num_to_write = fill_sysex_buffer(
				                   display_base,
				                   0,
				                   display_mem_size,
				                   display_beginning_si);

			// Write Display
			sysex_file.write(sysex_buffer, num_to_write);


			// Now do each timbre and patch
			for (i = 0; i < num_timbres; i++) {
				ds.read(sysex_buffer + 8, timbre_mem_size);
				patch_num = ds.read1() - 1; // Patch is 1-128 when we want 0-127

				std::memcpy(name, sysex_buffer + 8, 10);
				name[10] = 0;

				// Some info
				std::cout << "Timbre " << i << " (patch " << patch_num << "): " << name << std::endl;

				//
				// Timbre
				//

				num_to_write = fill_sysex_buffer(
				                   timbre_base,
				                   timbre_mem_offset(i),
				                   timbre_mem_size);

				// Write it
				sysex_file.write(sysex_buffer, num_to_write);

				//
				// Patch
				//

				// Default patch
				PatchMemData patch_data = patch_template;

				// Set the timbre num
				patch_data.timbre_num = i;

				num_to_write = fill_sysex_buffer(
				                   patch_base,
				                   patch_mem_offset(patch_num),
				                   patch_mem_size,
				                   &patch_data);

				// Write it
				sysex_file.write(sysex_buffer, num_to_write);
			}

			//
			// Rhythm Setup
			//

			i = 0;
			while (U7PercussionNotes[i]) {
				// Work out how many we can send at a time
				for (j = i + 1; U7PercussionNotes[j]; j++) {
					// If the next isn't actually the next, then we can't upload it
					if (U7PercussionNotes[j - 1] + 1 != U7PercussionNotes[j]) break;
				}

				int count = j - i;

				num_to_write = fill_sysex_buffer(
				                   rhythm_base,
				                   rhythm_mem_offset_note(U7PercussionNotes[i]),
				                   rhythm_mem_size * count,
				                   &U7PercussionData[i]);

				// Write Reset
				sysex_file.write(sysex_buffer, num_to_write);

				i += count;
			}

			//
			// Partial Reserves
			//

			systemArea  sa;

			std::memset(&sa.reserveSettings[0], 4, 9);
			sa.reserveSettings[7] = 0;

			num_to_write = fill_sysex_buffer(
			                   system_base,
			                   system_mem_offset(reserveSettings[0]),
			                   sizeof(sa.reserveSettings),
			                   &sa.reserveSettings[0]);

			// Write Reset
			sysex_file.write(sysex_buffer, num_to_write);

			//
			// Display
			//

			// Change the display to something more appropriate

			if (bgsi[num])
				num_to_write = fill_sysex_buffer(
				                   display_base,
				                   0,
				                   display_mem_size,
				                   display_black_gate);
			else
				num_to_write = fill_sysex_buffer(
				                   display_base,
				                   0,
				                   display_mem_size,
				                   display_serpent_isle);

			// Write the 'real' Display
			sysex_file.write(sysex_buffer, num_to_write);

			// Close the file
			sysex_file.close();

		} catch (exult_exception &e) {
			std::cerr << "Something went wrong: " << e.what() << std::endl;
		}
	}

	std::cout << "Finished!" << std::endl;

	return 0;
}
