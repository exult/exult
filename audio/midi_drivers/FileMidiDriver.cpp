/*
Copyright (C) 2000  Dancer A.L Vesperman

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
#include "FileMidiDriver.h"
#include "ignore_unused_variable_warning.h"

#include "fnames.h"
#include "exceptions.h"

#include "XMidiEventList.h"

using std::endl;

FileMidiDriver::~FileMidiDriver()
{
	destroyMidiDriver();
}

const char *FileMidiDriver::get_temp_name()
{
	return MIDITMPFILE;
}

int FileMidiDriver::initMidiDriver(uint32 sample_rate, bool stereo)
{
	ignore_unused_variable_warning(sample_rate, stereo);
	if (!initialized) destroyMidiDriver();

	global_volume = 255;
	seq_volume = 255;

	int ret = open();
	if (ret==0) initialized = true;
	return ret;
}

void FileMidiDriver::destroyMidiDriver()
{
	if (!initialized) return;

	// Stop any current player
	finishSequence(0);
	close();
	initialized = false;
}

int FileMidiDriver::maxSequences()
{
	return 1;
}

void FileMidiDriver::setGlobalVolume(int vol)
{
	global_volume = vol;
	set_volume((seq_volume*global_volume)/255);
}

void FileMidiDriver::finishSequence(int seq_num)
{
	if (seq_num != 0) return;

	stop_track();
}

bool FileMidiDriver::isSequencePlaying(int seq_num)
{
	if (seq_num != 0) return false;
	return is_playing();
}

void FileMidiDriver::startSequence(int seq_num, XMidiEventList *list, bool repeat, int vol, int branch)
{
	ignore_unused_variable_warning(branch);
	if (seq_num != 0) return;

	stop_track();

	std::string filename = get_temp_name();

	try {
		OFileDataSource file(filename);
		list->write(&file);
	} catch (exult_exception&) {
		return;
	}

#ifdef DEBUG
	perr << "Starting midi sequence with FileMidiDriver" << endl;
#endif
	seq_volume = vol;
	start_track(filename.c_str(), repeat, (seq_volume*global_volume)/255);
}

void FileMidiDriver::pauseSequence(int seq_num)
{
	if (seq_num != 0) return;
}

void FileMidiDriver::unpauseSequence(int seq_num)
{
	if (seq_num != 0) return;
}

void FileMidiDriver::setSequenceVolume(int seq_num, int vol)
{
	if (seq_num != 0) return;

	seq_volume = vol;
	set_volume((seq_volume*global_volume)/255);
}

void FileMidiDriver::setSequenceSpeed(int seq_num, int speed)
{
	ignore_unused_variable_warning(seq_num, speed);
	if (seq_num != 0) return;
}
