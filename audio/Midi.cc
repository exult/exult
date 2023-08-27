/*
Copyright (C) 2000-2022  The Exult Team

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

// Includes Pentagram headers so we must include pent_include.h
#include "pent_include.h"

#include <csignal>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <climits>

#include <vorbis/codec.h>

#include "fnames.h"
#include "exult.h"
#include "game.h"
#include "Audio.h"

#include "../files/U7file.h"
#include "../files/utils.h"
#include "Midi.h"
#include "MidiDriver.h"
#include "LowLevelMidiDriver.h"
#include "conv.h"
#include "databuf.h"
#include "convmusic.h"
#include "utils.h"
#include "array_size.h"

#include "data/exult_flx.h"

#include "../conf/Configuration.h"

#include "AudioMixer.h"
#include "OggAudioSample.h"

using std::cout;
using std::endl;
using std::string;

#ifdef DEBUG
inline char * formatTicks() {
	static char formattedTicks[ 32 ];
	uint64 ticks = SDL_GetTicks();
	snprintf(formattedTicks, 32, "[ %5u.%03u ] ",
	         static_cast<uint32>(ticks / 1000),
	         static_cast<uint32>(ticks % 1000));
	return formattedTicks;
}
#endif

//
// Midi devices types and conversions
//
// The midi player supports the following types of devices:
// Hardware GM/GS assumed (i.e. Windows Midi, Core Audio Midi)
// Hardware MT32 forced (MT32 connected to external Hardware midi port)
// Software GM/GS (Timidty)
// Software MT32 (MT32Emu)
// Software FMSynth (FMOpl)
//
// Hardware Midi Device can be forced into MT32 mode by setting the Converstion
// type to None.
//
// Midi Conversion setting is ignored devices that return true to
// isMT32Device() or isFMSynth(). They are assumed to properly support
// loadTimbreLibrary();
//
// Additionally as an Augmentation, we support Playing of Tracks as
// OGG Vorbis files
//
// The player supports the following states and does the following:
//
// General Midi Timbre Library:
// GM/GS Device: Do nothing
// MT32/FMSynth: Load GM Timbre Set into the MT32.
//
// Intro/Game/Endgame Timbre Library:
// GM/GS Device: Convert notes using XMIDI Convert setting
// MT32/FMSynth: Load correct Timbre Library from Game Data
//
//

#define SEQ_NUM_MUSIC	0
#define SEQ_NUM_SFX		1

static std::unique_ptr<IDataSource> open_music_flex(const std::string& flex, int num) {
	// Try in patch dir first.
	string pflex("<PATCH>/");
	size_t prefix_len = 0;
	if (flex[0] == '<')
	{
		prefix_len = flex.find(">/");
		if (prefix_len != string::npos)
			prefix_len += 2;
		else
			prefix_len = 0;
	}

	pflex += flex.c_str() + prefix_len;
	if (is_system_path_defined("<BUNDLE>")) {
		string bflex("<BUNDLE>/");
		bflex += flex.c_str() + prefix_len;
		return std::make_unique<IExultDataSource>(flex, bflex, pflex, num);
	} else {
		return std::make_unique<IExultDataSource>(flex, pflex, num);
	}
}

void	MyMidiPlayer::start_music(int num,bool repeat,std::string flex)
{
	// No output device
	if(!ogg_enabled && !midi_driver && !init_device(true))
		return;

	// -1 and 255 are stop tracks
	if (num == -1 || num == 255) {
		stop_music();
		return;
	}

	// Already playing it??
	if(current_track==num) {
		// OGG is playing?
		if (ogg_enabled && ogg_is_playing())
			return;
		// Midi driver is playing?
		if (midi_driver && midi_driver->isSequencePlaying(SEQ_NUM_MUSIC))
			return;
	}

	// Work around Usecode bug where track 0 is played at Intro Earthquake
	if(num == 0 && flex == MAINMUS && Game::get_game_type() == BLACK_GATE)
		return;

#ifdef DEBUG
	cout << formatTicks() << "Audio subsystem request: MIDI Music track # " << num << " in flex " << flex << endl;
#endif

	stop_music();

	current_track = num;
	repeating = repeat;

	// OGG Handling
	if (ogg_enabled) {
		// Play ogg for this track
		if (ogg_play_track(flex,num,repeat))
			return;

		// If we failed to play the track, call stop to clean up and put us back into
		// midi synth mode
		ogg_stop_track();

		// No midi driver or bg track and we can't play it properly so don't fall through
		if (!midi_driver || (!is_mt32() && Game_window::get_instance()->is_bg_track(num) && flex == MAINMUS)) return;
	}

	if (!midi_driver)
		return;

	// Handle FM Synth
	if (midi_driver->isFMSynth())  {
		// use the fmsynth music, which is bank 3
		if (flex == MAINMUS) flex = MAINMUS_AD;
		// Bank 1 is BG menu/intro. We need Bank 4
		else if (flex == INTROMUS) flex = INTROMUS_AD;
		// Bank 2 is SI menu. We need to offset -1
		else if (flex == MAINSHP_FLX) num--;
	}

	std::unique_ptr<IDataSource> mid_data = open_music_flex(flex, num);
	// Extra safety.
	if (!mid_data->getSize())
	{
		return;
	}

	XMidiFile midfile(mid_data.get(), setup_timbre_for_track(flex));

	// Now give the xmidi object to the midi device

	XMidiEventList *eventlist = midfile.GetEventList(0);
	if (eventlist)  midi_driver->startSequence(SEQ_NUM_MUSIC, eventlist, repeat, 255);
}

void	MyMidiPlayer::start_music(std::string fname,int num,bool repeat)
{
	// No output device
	if(!ogg_enabled && !midi_driver && !init_device(true))
		return;

	stop_music();

	// -1 and 255 are stop tracks
	if (num == -1 || num == 255) return;

	current_track = -1;
	repeating = repeat;

#ifdef DEBUG
	cout << formatTicks() << "Audio subsystem request: MIDI Music track # " << num << " in file " << fname << endl;
#endif

	// OGG Handling
	if (ogg_enabled) {
		// Play ogg for this track
		if (ogg_play_track(fname,num,repeat))
			return;

		// If we failed to play the track, call stop to clean up and put us back into
		// midi synth mode
		ogg_stop_track();
	}

	if (!midi_driver)
		return;

	// Handle FMSynth Stuff here
	if (midi_driver->isFMSynth()) {
		if (fname == ENDSCORE_XMI)
			num += 2;
		else if (fname == R_SINTRO)
			fname = A_SINTRO;
		else if (fname == R_SEND)
			fname = A_SEND;
	}

	// Read the data into the XMIDI class
	IFileDataSource mid_data(fname.c_str());
	if (!mid_data.good()) {
		return;
	}

	XMidiFile midfile(&mid_data, setup_timbre_for_track(fname));

	// Now give the xmidi object to the midi device
	XMidiEventList *eventlist = midfile.GetEventList(num);
	if (eventlist) {
		midi_driver->startSequence(SEQ_NUM_MUSIC, eventlist, repeat, 255);
	}
}

void MyMidiPlayer::set_timbre_lib(TimbreLibrary lib)
{
	// Fixme?? - This can be VERY SLOW
	if (lib != timbre_lib)
	{
		timbre_lib = lib;
		load_timbres();
	}
}

int MyMidiPlayer::setup_timbre_for_track(std::string &str)
{
	// Default to GM
	TimbreLibrary lib = TIMBRE_LIB_GM;

	// Both Games
	if (str == MAINMUS) lib = TIMBRE_LIB_GAME;
	else if (str == MAINMUS_AD) lib = TIMBRE_LIB_GAME;

	// BG
	else if (str == INTROMUS) lib = TIMBRE_LIB_MAINMENU;	// is both intro and menu
	else if (str == INTROMUS_AD) lib = TIMBRE_LIB_MAINMENU;	// is both intro and menu
	else if (str == ENDSCORE_XMI) lib = TIMBRE_LIB_ENDGAME;

	// SI
	else if (str == MAINSHP_FLX) lib = TIMBRE_LIB_MAINMENU;
	else if (str == EXULT_SI_FLX) lib = TIMBRE_LIB_INTRO;
	else if (str == R_SINTRO) lib = TIMBRE_LIB_INTRO;
	else if (str == A_SINTRO) lib = TIMBRE_LIB_INTRO;
	else if (str == R_SEND) lib = TIMBRE_LIB_ENDGAME;
	else if (str == A_SEND) lib = TIMBRE_LIB_ENDGAME;

	// Exult
	else if (str == EXULT_FLX) lib = TIMBRE_LIB_GM;

	set_timbre_lib(lib);

	// Nothing if the device is real
	if (midi_driver->isFMSynth() || midi_driver->isMT32())
		return XMIDIFILE_CONVERT_NOCONVERSION;

	// A 'Fake' MT32 Device ie Device with MT32 patchmaps but does not support SYSEX
	if (music_conversion == XMIDIFILE_CONVERT_GM_TO_MT32 ||
			(music_conversion == XMIDIFILE_CONVERT_NOCONVERSION && midi_driver->noTimbreSupport()))
	{
		if (timbre_lib == TIMBRE_LIB_GM)
			return XMIDIFILE_CONVERT_GM_TO_MT32;
		else
			return XMIDIFILE_CONVERT_NOCONVERSION;
	}
	// General Midi device
	else if (timbre_lib == TIMBRE_LIB_GM)
	{
		return XMIDIFILE_CONVERT_NOCONVERSION;
	}

	return music_conversion;
}

void MyMidiPlayer::load_timbres()
{
	if (!midi_driver) return;

	if (ogg_enabled) ogg_stop_track();

	// Stop all playing sequences
	for (int i = 0; i < midi_driver->maxSequences(); i++)
		midi_driver->finishSequence(i);

	// No timbre Support!
	if (midi_driver->noTimbreSupport()) return;

	// Not in a mode that uses Timbres
	if (!midi_driver->isFMSynth() && !midi_driver->isMT32() &&
		music_conversion != XMIDIFILE_CONVERT_NOCONVERSION)
		return;

	const char *u7voice = nullptr;

	// Black Gate Settings
	if (GAME_BG && timbre_lib == TIMBRE_LIB_INTRO)
		u7voice = INTRO_TIM;
	else if (GAME_BG && timbre_lib != TIMBRE_LIB_ENDGAME)
		u7voice = U7VOICE_FLX;
	// Serpent Isle
	else if (Game::get_game_type() == SERPENT_ISLE && timbre_lib == TIMBRE_LIB_MAINMENU)
		u7voice = MAINMENU_TIM;

	MidiDriver::TimbreLibraryType type;
	const char* filename;
	int index = -1;

	// General Midi Mode - AdLib
	if (timbre_lib == TIMBRE_LIB_GM && midi_driver->isFMSynth()) {
		type = MidiDriver::TIMBRE_LIBRARY_FMOPL_SETGM;
		filename = "FMOPL_SETGM";
		index = -2;
	}
	// General Midi Mode - MT32
	else if (timbre_lib == TIMBRE_LIB_GM) {
		type = MidiDriver::TIMBRE_LIBRARY_XMIDI_FILE;
		filename = BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX);
		index = EXULT_FLX_MTGM_MID;
	}
	// U7VOICE
	else if (u7voice) {
		if (midi_driver->isFMSynth()) {
			type = MidiDriver::TIMBRE_LIBRARY_U7VOICE_AD;
			index = 1;
		}
		else {
			type = MidiDriver::TIMBRE_LIBRARY_U7VOICE_MT;
			index = 0;
		}
		filename = u7voice;
	}
	// XMIDI_MT and XMIDI_AD
	else {
		if (midi_driver->isFMSynth()) {
			type = MidiDriver::TIMBRE_LIBRARY_XMIDI_AD;
			filename = XMIDI_AD;
		}
		else {
			type = MidiDriver::TIMBRE_LIBRARY_XMIDI_MT;
			filename = XMIDI_MT;
		}
	}

	if (timbre_lib_filename == filename && timbre_lib_index == index &&
		timbre_lib_game == Game::get_game_type()) {
		return;
	}

	timbre_lib_filename = filename;
	timbre_lib_index = index;
	timbre_lib_game = Game::get_game_type();

	std::unique_ptr<IDataSource> ds;

	if (index == -1) {
		ds = std::make_unique<IFileDataSource>(filename);
		if (!ds->good()) {
			return;
		}
	} else if (index >= 0) {
		ds = std::make_unique<IExultDataSource>(filename, index);
	}

	// Note: ds can be null here if inde == -2. In this case, the pointer
	// will never be used inside loadTimbreLibrary.
	midi_driver->loadTimbreLibrary(ds.get(), type);
}

void MyMidiPlayer::stop_music(bool quitting)
{
	if(!ogg_enabled && !midi_driver && !quitting && !init_device(false))
		return;

	current_track = -1;
	repeating = false;

	if (ogg_enabled) ogg_stop_track();
	if (midi_driver) midi_driver->finishSequence(SEQ_NUM_MUSIC);
}

bool MyMidiPlayer::is_track_playing(int num)
{
	if (current_track == -1 || current_track != num) return false;

	if (ogg_enabled && ogg_is_playing()) return true;
	if (midi_driver && midi_driver->isSequencePlaying(0)) return true;

	return false;
}

int MyMidiPlayer::get_current_track() const
{
	if (current_track == -1) return -1;

	if (ogg_enabled && ogg_is_playing()) return current_track;
	if (midi_driver && midi_driver->isSequencePlaying(0)) return current_track;

	return -1;
}

void MyMidiPlayer::set_music_conversion(int conv)
{
	// Same, do nothing
	if (music_conversion == conv) return;
	if (!ogg_enabled || !ogg_is_playing()) // if ogg is playing we don't care about drivers
		stop_music();
	music_conversion = conv;

	switch(music_conversion) {
	case XMIDIFILE_CONVERT_MT32_TO_GS:
		config->set("config/audio/midi/convert","gs",true);
		break;
	case XMIDIFILE_CONVERT_NOCONVERSION:
		config->set("config/audio/midi/convert","mt32",true);
		if ((!ogg_enabled || !ogg_is_playing()) && midi_driver && !midi_driver->isFMSynth() && !midi_driver->isMT32()) load_timbres();
		break;
	case XMIDIFILE_CONVERT_MT32_TO_GS127:
		config->set("config/audio/midi/convert","gs127",true);
		break;
	case XMIDIFILE_CONVERT_GM_TO_MT32:
		config->set("config/audio/midi/convert","fakemt32",true);
		break;
	default:
		config->set("config/audio/midi/convert","gm",true);
		break;
	}
}

#ifdef ENABLE_MIDISFX
void MyMidiPlayer::set_effects_conversion(int conv)
{
	// Same, do nothing
	if (effects_conversion == conv) return;

	effects_conversion = conv;

	switch(effects_conversion) {
	case XMIDIFILE_CONVERT_NOCONVERSION:
		config->set("config/audio/effects/convert","mt32",true);
		break;
	default:
		config->set("config/audio/effects/convert","gs",true);
		break;
	}
}
#endif

void MyMidiPlayer::set_midi_driver(const std::string& desired_driver, bool use_oggs)
{
	// Don't kill the device if we don't need to
	if ((midi_driver_name != desired_driver && (!ogg_enabled || !ogg_is_playing())) || // if ogg is playing we don't care about drivers
	    ogg_enabled != use_oggs) {
		stop_music();
		if (midi_driver) midi_driver->destroyMidiDriver();
		delete midi_driver;
		midi_driver = nullptr;
		initialized = false;
	}

	ogg_enabled = use_oggs;
	midi_driver_name = desired_driver;

	config->set("config/audio/midi/driver",midi_driver_name,true);
	config->set("config/audio/midi/use_oggs",ogg_enabled?"yes":"no",true);

	init_device(true);
}


bool MyMidiPlayer::init_device(bool timbre_load)
{
	// already initialized? Do this first
	if (initialized) return (midi_driver != nullptr) || ogg_enabled;

	string	s;
	string	driver_default = "default";

	const bool music = Audio::get_ptr()->is_music_enabled();

	if (!music) s = "no";
	else s = "yes";

	// Global Midi Enable/Disable
	config->set("config/audio/midi/enabled",s,true);

	// Music conversion
	config->value("config/audio/midi/convert",s,"gm");

	if (s == "gs")
		music_conversion = XMIDIFILE_CONVERT_MT32_TO_GS;
	else if (s == "mt32")
		music_conversion = XMIDIFILE_CONVERT_NOCONVERSION;
	else if (s == "none")
	{
		music_conversion = XMIDIFILE_CONVERT_NOCONVERSION;
		config->set("config/audio/midi/convert","mt32",true);
	}
	else if (s == "gs127")
		music_conversion = XMIDIFILE_CONVERT_MT32_TO_GS127;
	else if (s == "fakemt32")
		music_conversion = XMIDIFILE_CONVERT_GM_TO_MT32;
	else if (s == "gs127drum")
	{
		music_conversion = XMIDIFILE_CONVERT_MT32_TO_GS;
		config->set("config/audio/midi/convert","gs",true);
	}
	else
	{
		music_conversion = XMIDIFILE_CONVERT_MT32_TO_GM;
		config->set("config/audio/midi/convert","gm",true);
		driver_default = s;
	}

	// Timber Precaching
	config->value("config/audio/midi/precacheTimbers/onStartup",s,"no");
	LowLevelMidiDriver::precacheTimbresOnStartup = (s == "yes");
	config->value("config/audio/midi/precacheTimbers/onPlay",s,"yes");
	LowLevelMidiDriver::precacheTimbresOnPlay = (s != "no");

	//config->set("config/audio/midi/precacheTimbers/onStartup",LowLevelMidiDriver::precacheTimbresOnStartup?"yes":"no",true);
	//config->set("config/audio/midi/precacheTimbers/onPlay",LowLevelMidiDriver::precacheTimbresOnPlay?"yes":"no",true);

	std::cout << "Timbers Precached: ";

	if (LowLevelMidiDriver::precacheTimbresOnStartup && LowLevelMidiDriver::precacheTimbresOnPlay)
		std::cout << "On startup and play" << std::endl;
	else if (LowLevelMidiDriver::precacheTimbresOnStartup)
		std::cout << "On startup only" << std::endl;
	else if (LowLevelMidiDriver::precacheTimbresOnPlay)
		std::cout << "On play only" << std::endl;
	else
		std::cout << "Never" << std::endl;

#ifdef ENABLE_MIDISFX
	const bool sfx = Audio::get_ptr()->are_effects_enabled();

	// Effects conversion
	config->value("config/audio/effects/convert",s,"gs");

	if (s == "mt32")
		effects_conversion = XMIDIFILE_CONVERT_NOCONVERSION;
	else if (s == "none")
	{
		effects_conversion = XMIDIFILE_CONVERT_NOCONVERSION;
		config->set("config/audio/effects/convert","mt32",true);
	}
	else if (s == "gs127")
		effects_conversion = XMIDIFILE_CONVERT_NOCONVERSION;
	else
	{
		effects_conversion = XMIDIFILE_CONVERT_GS127_TO_GS;
		config->set("config/audio/effects/convert","gs",true);
	}
#else
	const bool sfx = false;
#endif

	// no need for a MIDI device (for now)
	if (!sfx && !music)
	{
		midi_driver = nullptr;
		return false;
	}

	// OGG Vorbis support
	config->value("config/audio/midi/use_oggs",s,"no");
	ogg_enabled = (s == "yes");
	config->set("config/audio/midi/use_oggs",ogg_enabled?"yes":"no",true);

	// Midi driver type.
	config->value("config/audio/midi/driver",s,driver_default.c_str());

	if (s == "normal")
	{
		config->set("config/audio/midi/driver","default",true);
		midi_driver_name = "default";
	}
	else if (s == "digital")
	{
		ogg_enabled = true;
		config->set("config/audio/midi/driver","default",true);
		config->set("config/audio/midi/use_oggs","yes",true);
		midi_driver_name = "default";
	}
	else
	{
		config->set("config/audio/midi/driver",s,true);
		midi_driver_name = s;
	}

	std::cout << "OGG Vorbis Digital Music: " << (ogg_enabled?"Enabled":"Disabled") << std::endl;

	Pentagram::AudioMixer *mixer = Pentagram::AudioMixer::get_instance();
	midi_driver = MidiDriver::createInstance(s,mixer->getSampleRate(),mixer->getStereo());

	initialized = true;

	if(!midi_driver) return ogg_enabled;

	timbre_lib_filename = "";
	if (timbre_load) load_timbres();

	return true;
}

// Check for true mt32, mt32emu or fakemt32
// primarily for the mt32 background music tracks

bool MyMidiPlayer::is_mt32() const
{
	return midi_driver && (midi_driver->isMT32() ||
		get_music_conversion() == XMIDIFILE_CONVERT_NOCONVERSION ||
		get_music_conversion() == XMIDIFILE_CONVERT_GM_TO_MT32) &&
		!midi_driver->isFMSynth();
}

bool MyMidiPlayer::is_adlib() const
{
	return midi_driver && (midi_driver->isFMSynth());
}

MyMidiPlayer::MyMidiPlayer()
{
	init_device(false);
}

MyMidiPlayer::~MyMidiPlayer()
{
	ogg_stop_track();
	if (midi_driver)
	{
		midi_driver->destroyMidiDriver();
		delete midi_driver;
		midi_driver = nullptr;
	}
}

void MyMidiPlayer::destroyMidiDriver()
{
	if (midi_driver) midi_driver->destroyMidiDriver();
}

void MyMidiPlayer::produceSamples(sint16 *stream, uint32 bytes)
{
	if (midi_driver && midi_driver->isInitialized() && midi_driver->isSampleProducer())
		midi_driver->produceSamples(stream, bytes);
}

#ifdef ENABLE_MIDISFX
void    MyMidiPlayer::start_sound_effect(int num)
{
#ifdef DEBUG
	cout << formatTicks() << "Audio subsystem request: MIDI SFX sound effect # " << num << endl;
#endif

	int real_num = num;

	if (Game::get_game_type() == BLACK_GATE) real_num = bgconv[num];

	cout << "Real num " << real_num << endl;

	// No driver
	if (!midi_driver && !init_device(true)) return;

	// midi_driver can be null here if ogg is enabled but midi failed
	if (!midi_driver) return;

	// Only support SFX on devices with 2 or more sequences
	if (midi_driver->maxSequences() < 2) return;

	std::unique_ptr<IExultDataSource> mid_data;
	if (is_system_path_defined("<BUNDLE>")) {
		mid_data = std::make_unique<IExultDataSource>("<DATA>/midisfx.flx",
		                                "<BUNDLE>/midisfx.flx", real_num);
	} else {
		mid_data = std::make_unique<IExultDataSource>("<DATA>/midisfx.flx", real_num);
	}

	if (!mid_data->good()) {
		return;
	}

	// Read the data into the XMIDI class
	// It's already GM, so dont convert
	XMidiFile midfile(mid_data.get(), effects_conversion);

	// Now give the xmidi object to the midi device
	XMidiEventList *eventlist = midfile.GetEventList(0);
	if (eventlist) midi_driver->startSequence(1,eventlist,false,255);
}

void    MyMidiPlayer::stop_sound_effects()
{
	if (midi_driver)
	{
		// Only support SFX on devices with 2 or more sequences
		if (midi_driver->maxSequences() >= 2) midi_driver->finishSequence(1);
	}
}
#endif

bool MyMidiPlayer::ogg_play_track(const std::string& filename, int num, bool repeat)
{
	string ogg_name;
	string basepath = "<MUSIC>/";

	if (filename == EXULT_FLX && num == EXULT_FLX_MEDITOWN_MID)
		ogg_name = "exult.ogg";
	else if (Game::get_game_type() == BLACK_GATE)
		{
		if(filename == INTROMUS || filename == INTROMUS_AD)
			{
			if (num == 0)
				ogg_name = "00bg.ogg";
			else if (num == 1)
				ogg_name = "01bg.ogg";
			else if (num == 2)
				ogg_name = "02bg.ogg";
			else if (num == 3)
				ogg_name = "03bg.ogg";
			else if (num == 4)
				ogg_name = "endcr01.ogg";
			else if (num == 5)
				ogg_name = "endcr02.ogg";
			}
		else if (filename == ENDSCORE_XMI)
			{
			if (num == 1 || num == 3)
				ogg_name = "end01bg.ogg";
			else if (num == 2 || num == 4)
				ogg_name = "end02bg.ogg";
			}
		else if (filename == MAINMUS || filename == MAINMUS_AD)
			{
			char outputstr[255];
			snprintf(outputstr, 255, "%02dbg.ogg", num);
			ogg_name = outputstr;
			}
		else if (filename == EXULT_BG_FLX)
			{
			ogg_name = filename;
			}
		}
	else if (Game::get_game_type() == SERPENT_ISLE)
		{
		if(filename == MAINSHP_FLX)
			{
			if (num == 28 || num == 27)
				ogg_name = "03bg.ogg";
			else if(num == 30 || num == 29)
				ogg_name = "endcr01.ogg";
			else if(num == 32 || num == 31)
				ogg_name = "endcr02.ogg";
			}
		else if(filename == R_SINTRO || filename == A_SINTRO)
			ogg_name = "si01.ogg";
		else if(filename == R_SEND || filename == A_SEND)
			ogg_name = "si13.ogg";
		else if (filename == MAINMUS || filename == MAINMUS_AD)
			{
			if (static_cast<unsigned>(num) < array_size(bgconvmusic))
				ogg_name = bgconvmusic[num];
			else
				{
				char outputstr[255];
				snprintf(outputstr, 255, "%02dsi.ogg", num);
				ogg_name = outputstr;
				}
			}
		else if (filename == EXULT_SI_FLX)
			{
			ogg_name = filename;
			}
		}
	else
		{
		char outputstr[255];
		snprintf(outputstr, 255, "%02dmus.ogg", num);
		ogg_name = outputstr;
		basepath = "<STATIC>/music/";
		}

	if (ogg_name.empty()) {
		return false;
	}

	const bool flex_source = ogg_name == filename;
	auto ds = [&ogg_name, &basepath, num, flex_source]() -> std::unique_ptr<IDataSource> {
		if (flex_source) {
			return open_music_flex(ogg_name, num);
		}
		if (U7exists("<PATCH>/music/" + ogg_name)) {
			ogg_name = get_system_path("<PATCH>/music/" + ogg_name);
		} else if (is_system_path_defined("<BUNDLE>") &&
				U7exists("<BUNDLE>/music/" + ogg_name)) {
			ogg_name = get_system_path("<BUNDLE>/music/" + ogg_name);
		} else {
			ogg_name = get_system_path(basepath + ogg_name);
		}
		return std::make_unique<IFileDataSource>(ogg_name);
	}();

#ifdef DEBUG
	cout << "OGG audio: Music track " << ogg_name << endl;
#endif

	if (!ds->good()) {
		return false;
	}

	if (!Pentagram::OggAudioSample::isThis(ds.get())) {
		std::cerr << "Failed to play OGG Music Track " << ogg_name << ". Reason: " << "Unknown" << std::endl;
		return false;
	}

	Pentagram::AudioMixer *mixer = Pentagram::AudioMixer::get_instance();

	if (ogg_instance_id != -1) {
		mixer->stopSample(ogg_instance_id);
		ogg_instance_id = -1;
	}

	ds->seek(0);
	Pentagram::AudioSample *ogg_sample = new Pentagram::OggAudioSample(std::move(ds));

	ogg_instance_id = mixer->playSample(ogg_sample, repeat?-1:0, INT_MAX);

	ogg_sample->Release();

	return  true;
}

void MyMidiPlayer::ogg_stop_track()
{
	if (ogg_instance_id != -1) {
		Pentagram::AudioMixer *mixer = Pentagram::AudioMixer::get_instance();
		mixer->stopSample(ogg_instance_id);
		ogg_instance_id = -1;
	}
}

bool MyMidiPlayer::ogg_is_playing() const
{
	if (ogg_instance_id != -1) {
		Pentagram::AudioMixer *mixer = Pentagram::AudioMixer::get_instance();
		return mixer->isPlaying(ogg_instance_id);
	}
	return false;
}

