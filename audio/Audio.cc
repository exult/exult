/*
 *  Copyright (C) 2000-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "pent_include.h"

#include "Audio.h"

#include "AudioMixer.h"
#include "AudioSample.h"
#include "Configuration.h"
#include "Flex.h"
#include "actors.h"
#include "conv.h"
#include "databuf.h"
#include "exult.h"
#include "fnames.h"
#include "game.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "utils.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>

// #include <crtdbg.h>

using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::ios;
using std::memcpy;
using std::string;

using namespace Pentagram;

#define MIXER_CHANNELS 32

struct Chunk {
	size_t length;
	uint8* data;

	Chunk(size_t l, uint8* d) : length(l), data(d) {}
};

Audio*     Audio::self        = nullptr;
const int* Audio::bg2si_sfxs  = nullptr;
const int* Audio::bg2si_songs = nullptr;

//----- Utilities ----------------------------------------------------

#ifdef DEBUG
inline char* formatTicks() {
	static char formattedTicks[32];
	uint64      ticks = SDL_GetTicks();
	snprintf(
			formattedTicks, 32, "[ %5u.%03u ] ",
			static_cast<uint32>(ticks / 1000),
			static_cast<uint32>(ticks % 1000));
	return formattedTicks;
}
#endif

//----- SFX ----------------------------------------------------------

// Tries to locate a sfx in the cache based on sfx num.
SFX_cache_manager::SFX_cached* SFX_cache_manager::find_sfx(int id) {
	auto found = cache.find(id);
	if (found == cache.end()) {
		return nullptr;
	}
	return &(found->second);
}

SFX_cache_manager::~SFX_cache_manager() {
	flush();
}

// For SFX played through 'play_wave_sfx'. Searched cache for
// the sfx first, then loads from the sfx file if needed.
AudioSample* SFX_cache_manager::request(Flex* sfx_file, int id) {
	SFX_cached* loaded = find_sfx(id);
	if (!loaded) {
		SFX_cached& new_sfx = cache[id];
		new_sfx.first       = 0;
		new_sfx.second      = nullptr;
		loaded              = &new_sfx;
	}

	if (!loaded->second) {
		garbage_collect();

		size_t wavlen;    // Read .wav file.
		auto   wavbuf = sfx_file->retrieve(id, wavlen);
		loaded->second
				= AudioSample::createAudioSample(std::move(wavbuf), wavlen);
	}

	if (!loaded->second) {
		return nullptr;
	}

	// Increment counter
	++loaded->first;

	return loaded->second;
}

// Empties the cache.
void SFX_cache_manager::flush(AudioMixer* mixer) {
	for (auto it = cache.begin(); it != cache.end(); it = cache.begin()) {
		if (it->second.second) {
			if (it->second.second->getRefCount() != 1 && mixer) {
				mixer->stopSample(it->second.second);
			}
			it->second.second->Release();
		}
		it->second.second = nullptr;
		cache.erase(it);
	}
}

// Remove unused sounds from the cache.
void SFX_cache_manager::garbage_collect() {
	// Maximum 'stable' number of sounds we will cache (actual
	// count may be higher if all of the cached sounds are
	// being played).
	const int max_fixed = 6;

	std::multiset<int> sorted;

	for (auto& it : cache) {
		if (it.second.second && it.second.second->getRefCount() == 1) {
			sorted.insert(it.second.first);
		}
	}

	if (sorted.empty()) {
		return;
	}

	int threshold = INT_MAX;
	int count     = 0;

	for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
		if (count < max_fixed) {
			threshold = *it;
			count++;
		} else if (*it == threshold) {
			count++;
		} else {
			break;
		}
	}

	if (count <= max_fixed) {
		return;
	}

	for (auto& it : cache) {
		if (it.second.second) {
			if (it.second.first < threshold) {
				it.second.second->Release();
				it.second.second = nullptr;
			} else if (it.second.first == threshold && count > max_fixed) {
				it.second.second->Release();
				it.second.second = nullptr;
				count--;
			}
		}
	}
}

//---- Audio ---------------------------------------------------------
void Audio::Init() {
	// Crate the Audio singleton object
	if (!self) {
		int  sample_rate = 44100;
		bool stereo      = true;

		config->value("config/audio/sample_rate", sample_rate, sample_rate);
		config->value("config/audio/stereo", stereo, stereo);

		self = new Audio();
		self->Init(sample_rate, stereo ? 2 : 1);
	}
}

void Audio::Destroy() {
	delete self;
	self = nullptr;
}

Audio* Audio::get_ptr() {
	// The following assert here might be too harsh, maybe we should leave
	// it to the caller to handle non-inited audio-system?
	assert(self != nullptr);

	return self;
}

Audio::Audio() {
	assert(self == nullptr);

	string s, newval;

	config->value("config/audio/enabled", s, "yes");
	audio_enabled = (s != "no");
	config->set("config/audio/enabled", audio_enabled ? "yes" : "no", false);

	config->value("config/audio/effects/sfx_volume", sfx_volume, sfx_volume);
	config->value(
			"config/audio/speech/speech_volume", speech_volume, speech_volume);
	config->set("config/audio/effects/sfx_volume", sfx_volume, false);
	config->set("config/audio/speech/speech_volume", speech_volume, false);

	config->value("config/audio/speech/enabled", s, "yes");
	speech_enabled = (s != "no");
	config->value("config/audio/speech/with_subs", s, "no");
	speech_with_subs = (s != "no");
	config->value("config/audio/midi/enabled", s, "---");
	music_enabled = (s != "no");
	config->value("config/audio/effects/enabled", s, "---");
	effects_enabled = (s != "no");
	config->value("config/audio/midi/looping", s, "auto");

	newval = s;
	if (s == "no" || s == "never") {
		newval        = "never";
		music_looping = LoopingType::Never;
	} else if (s == "endless") {
		music_looping = LoopingType::Endless;
	} else if (s == "limited") {
		music_looping = LoopingType::Limited;
	} else {
		newval        = "auto";
		music_looping = LoopingType::Auto;
	}
	// Update config file with new value if needed
	if (newval != s) {
		config->set("config/audio/midi/looping", newval, false);
	}
	config->write_back();

	mixer.reset();
	sfxs = std::make_unique<SFX_cache_manager>();
}

void Audio::Init(int _samplerate, int _channels) {
	if (!audio_enabled) {
		return;
	}

	mixer = std::make_unique<AudioMixer>(
			_samplerate, _channels == 2, MIXER_CHANNELS);

	COUT("Audio initialisation OK");

	mixer->openMidiOutput();
	initialized = true;
}

bool Audio::can_sfx(const std::string& file, std::string* out) {
	if (file.empty()) {
		return false;
	}
	string options[] = {"", "<BUNDLE>", "<DATA>"};
	for (auto& d : options) {
		string f;
		if (!d.empty()) {
			if (!is_system_path_defined(d)) {
				continue;
			}
			f = d;
			f += '/';
			f += file;
		} else {
			f = file;
		}
		if (U7exists(f.c_str())) {
			if (out) {
				*out = std::move(f);
			}
			return true;
		}
	}
	return false;
}

bool Audio::have_roland_sfx(Exult_Game game, std::string* out) {
	if (game == BLACK_GATE) {
		return can_sfx(SFX_ROLAND_BG, out);
	} else if (game == SERPENT_ISLE) {
		return can_sfx(SFX_ROLAND_SI, out);
	}
	return false;
}

bool Audio::have_sblaster_sfx(Exult_Game game, std::string* out) {
	if (game == BLACK_GATE) {
		return can_sfx(SFX_BLASTER_BG, out);
	} else if (game == SERPENT_ISLE) {
		return can_sfx(SFX_BLASTER_SI, out);
	}
	return false;
}

bool Audio::have_midi_sfx(std::string* out) {
#ifdef ENABLE_MIDISFX
	return can_sfx(SFX_MIDIFILE, out);
#else
	ignore_unused_variable_warning(out);
	return false;
#endif
}

bool Audio::have_config_sfx(const std::string& game, std::string* out) {
	string       s;
	const string d = "config/disk/game/" + game + "/waves";
	config->value(d.c_str(), s, "---");
	return (s != "---") && can_sfx(s, out);
}

void Audio::Init_sfx() {
	sfx_file.reset();

	if (sfxs) {
		sfxs->flush(mixer.get());
		sfxs->garbage_collect();
	}

	const Exult_Game game = Game::get_game_type();
	if (game == SERPENT_ISLE) {
		bg2si_sfxs  = bgconv;
		bg2si_songs = bgconvsong;
	} else {
		bg2si_sfxs  = nullptr;
		bg2si_songs = nullptr;
	}
	// Collection of .wav's?
	string flex;
#ifdef ENABLE_MIDISFX
	string v;
	config->value("config/audio/effects/midi", v, "no");
	if (have_midi_sfx(&flex) && v != "no") {
		cout << "Opening midi SFX's file: \"" << flex << "\"" << endl;
		sfx_file = std::make_unique<FlexFile>(std::move(flex));
		return;
	} else if (!have_midi_sfx(&flex)) {
		config->set("config/audio/effects/midi", "no", true);
	}
#endif
	if (!have_config_sfx(Game::get_gametitle(), &flex)) {
		if (have_roland_sfx(game, &flex) || have_sblaster_sfx(game, &flex)) {
			const string d
					= "config/disk/game/" + Game::get_gametitle() + "/waves";
			size_t      sep = flex.rfind('/');
			std::string pflex;
			if (sep != string::npos) {
				sep++;
				pflex = flex.substr(sep);
			} else {
				pflex = flex;
			}
			config->set(d.c_str(), pflex, true);
		} else {
			cerr << "Digital SFX's file specified: " << flex
				 << "... but file not found, and fallbacks are missing" << endl;
			return;
		}
	}
	cout << "Opening digital SFX's file: \"" << flex << "\"" << endl;
	sfx_file = std::make_unique<FlexFile>(std::move(flex));
}

Audio::~Audio() {
	if (!initialized) {
		// SDL_open = false;
		return;
	}

	CERR("~Audio:  about to stop_music()");
	stop_music();

	CERR("~Audio:  about to quit subsystem");
}

sint32 Audio::copy_and_play_speech(
		const uint8* sound_data, uint32 len, bool wait, int volume) {
	if (!speech_enabled) {
		return -1;
	}
	auto new_sound_data = std::make_unique<uint8[]>(len);
	std::memcpy(new_sound_data.get(), sound_data, len);
	volume = (volume * speech_volume) / 100;

	return speech_id = play(std::move(new_sound_data), len, wait, volume);
}

sint32 Audio::copy_and_play_sfx(
		const uint8* sound_data, uint32 len, bool wait, int volume) {
	auto new_sound_data = std::make_unique<uint8[]>(len);
	std::memcpy(new_sound_data.get(), sound_data, len);
	volume = (volume * sfx_volume) / 100;

	return play(std::move(new_sound_data), len, wait, volume);
}

sint32 Audio::play(
		std::unique_ptr<uint8[]> sound_data, uint32 len, bool wait,
		int volume) {
	ignore_unused_variable_warning(wait);
	if (!audio_enabled || !len) {
		return -1;
	}

	AudioSample* audio_sample
			= AudioSample::createAudioSample(std::move(sound_data), len);

	if (audio_sample) {
		sint32 id = mixer->playSample(
				audio_sample, 0, 128, false, 65536, volume, volume);
		audio_sample->Release();
		return id;
	}

	return -1;
}

void Audio::cancel_streams() {
	if (!audio_enabled) {
		return;
	}

	// Mix_HaltChannel(-1);
	mixer->reset();
}

void Audio::pause_audio() {
	if (!audio_enabled) {
		return;
	}

	mixer->setPausedAll(true);
}

void Audio::resume_audio() {
	if (!audio_enabled) {
		return;
	}

	mixer->setPausedAll(false);
}

sint32 Audio::playSpeechfile(
		const char* fname, const char* fpatch, bool wait, int volume) {
#ifdef DEBUG
	cout << formatTicks() << "Audio subsystem request: File play in " << fname
		 << " patch " << fpatch << " volume " << volume << endl;
#endif
	if (!audio_enabled) {
		return -1;
	}

	const U7multiobject sample(fname, fpatch, 0);

	size_t len;
	auto   buf = sample.retrieve(len);
	if (!buf || len == 0) {
		// Failed to find file in patch or static dirs.
		CERR("Audio::playfile: Error reading file '" << fname << "'");
		return -1;
	}
	volume           = (volume * speech_volume) / 100;
	return speech_id = play(std::move(buf), len, wait, volume);
}

bool Audio::playing() {
	return false;
}

bool Audio::start_music(
		int num, bool continuous, MyMidiPlayer::ForceType force,
		const std::string& flex) {
#ifdef DEBUG
	cout << formatTicks() << "Audio subsystem request: Music start " << num
		 << " in flex " << flex << endl;
#endif
	if (audio_enabled && music_enabled && mixer && mixer->getMidiPlayer()) {
		return mixer->getMidiPlayer()->start_music(
				num, music_looping == LoopingType::Never ? false : continuous,
				force, flex);
	}
	return false;
}

void Audio::change_repeat(bool newrepeat) {
	mixer->getMidiPlayer()->set_repeat(newrepeat);
}

bool Audio::start_music(
		const std::string& fname, int num, bool continuous,
		MyMidiPlayer::ForceType force) {
#ifdef DEBUG
	cout << formatTicks() << "Audio subsystem request: Music start " << num
		 << " in file " << fname << endl;
#endif
	if (audio_enabled && music_enabled && mixer && mixer->getMidiPlayer()) {
		return mixer->getMidiPlayer()->start_music(
				fname, num,
				music_looping == LoopingType::Never ? false : continuous,
				force);
	}
	return false;
}

void Audio::start_music_combat(Combat_song song, bool continuous) {
#ifdef DEBUG
	cout << formatTicks() << "Audio subsystem request: Combat Music start "
		 << int(song) << endl;
#endif
	if (!audio_enabled || !music_enabled || !mixer || !mixer->getMidiPlayer()) {
		return;
	}

	int num = -1;

	switch (song) {
	case CSBattle_Over:
		num = Audio::game_music(9);
		break;

	case CSAttacked1:
		num = Audio::game_music(11);
		break;

	case CSAttacked2:
		num = Audio::game_music(12);
		break;

	case CSVictory:
		num = Audio::game_music(15);
		break;

	case CSRun_Away:
		num = Audio::game_music(16);
		break;

	case CSDanger:
		num = Audio::game_music(10);
		break;

	case CSHidden_Danger:
		num = Audio::game_music(18);
		break;

	default:
		CERR("Error: Unable to Find combat track for song " << song << ".");
		break;
	}

	mixer->getMidiPlayer()->start_music(
			num, continuous && music_looping != LoopingType::Never);
}

void Audio::stop_music() {
#ifdef DEBUG
	cout << formatTicks() << "Audio subsystem request: Music stop" << endl;
#endif
	if (!audio_enabled) {
		return;
	}

	if (mixer && mixer->getMidiPlayer()) {
		mixer->getMidiPlayer()->stop_music(true);
	}
}

bool Audio::start_speech(int num, bool wait) {
	if (!audio_enabled || !speech_enabled) {
		return false;
	}

	const char* filename;
	const char* patchfile;

	if (Game::get_game_type() == SERPENT_ISLE) {
		filename  = SISPEECH;
		patchfile = PATCH_SISPEECH;
	} else {
		filename  = U7SPEECH;
		patchfile = PATCH_U7SPEECH;
	}

#ifdef DEBUG
	cout << formatTicks() << "Audio subsystem request: Speech start " << num
		 << " in " << filename << " and " << patchfile << endl;
#endif
	const U7multiobject sample(filename, patchfile, num);

	size_t len;
	auto   buf = sample.retrieve(len);
	if (!buf || len == 0) {
		return false;
	}

	speech_id = play(std::move(buf), len, wait, (speech_volume * 255) / 100);
	return true;
}

void Audio::stop_speech() {
#ifdef DEBUG
	cout << formatTicks() << "Audio subsystem request: Speech stop" << endl;
#endif
	if (!audio_enabled || !speech_enabled) {
		return;
	}

	// doing a mixer reset is not appropriate here
	// as it will stop all playing sfx and music
	// mixer->reset();
	if (speech_id != -1) {
		mixer->stopSample(speech_id);
	}
	speech_id = -1;
}

bool Audio::is_speech_playing() {
	return speech_id != -1 && mixer->isPlaying(speech_id);
}

int Audio::wait_for_speech(std::function<int(Uint32 ms)> waitfunc) {
	if (speech_id == -1) {
		return -1;
	}

	if (!waitfunc) {
		waitfunc = [](int ms) {
			SDL_Delay(ms);
			return 0;
		};
	}

	while (mixer->isPlaying(speech_id)) {
		// 50 ms or 20times a second
		int canceled = waitfunc(50);
		if (canceled > 0) {
			mixer->stopSample(speech_id);
			speech_id = -1;
			return canceled;
		}
	}
	speech_id = -1;
	return 0;
}

/*
 *	This returns a 'unique' ID, but only for .wav SFX's (for now).
 */
int Audio::play_sound_effect(
		int num, int volume, int balance, int repeat, int distance) {
#ifdef DEBUG
	cout << formatTicks() << "Audio subsystem request: Sound Effect play "
		 << num << " volume " << volume << " balance " << balance
		 << " distance " << distance << endl;
#endif
	if (!audio_enabled || !effects_enabled) {
		return -1;
	}

#ifdef ENABLE_MIDISFX
	string v;    // TODO: should make this check faster
	config->value("config/audio/effects/midi", v, "no");
	if (v != "no" && mixer && mixer->getMidiPlayer()) {
		mixer->getMidiPlayer()->start_sound_effect(num);
		return -1;
	}
#endif
	// Where sort of sfx are we using????
	if (sfx_file != nullptr) {    // Digital .wav's?
		return play_wave_sfx(num, volume, balance, repeat, distance);
	}
	return -1;
}

/*
 *	Play a .wav format sound effect,
 *  return the channel number playing on or -1 if not playing, (0 is a valid
 *channel in SDL_Mixer!)
 */
int Audio::play_wave_sfx(
		int num,
		int volume,     // 0-256.
		int balance,    // balance, -256 (left) - +256 (right)
		int repeat,     // Keep playing.
		int distance) {
#ifdef DEBUG
	cout << formatTicks() << "Audio subsystem request: Wave play " << num
		 << " volume " << volume << " balance " << balance << " distance "
		 << distance << endl;
#endif
	if (!effects_enabled || !sfx_file || !mixer) {
		return -1;    // no .wav sfx available
	}

	if (num < 0
		|| static_cast<unsigned>(num) >= sfx_file->number_of_objects()) {
		cerr << "SFX " << num << " is out of range" << endl;
		return -1;
	}
	AudioSample* wave = sfxs->request(sfx_file.get(), num);
	if (!wave) {
		cerr << "Couldn't play sfx '" << num << "'" << endl;
		return -1;
	}

	volume = (volume * sfx_volume) / 100;

	const int instance_id = mixer->playSample(
			wave, repeat, 0, true, AUDIO_DEF_PITCH, volume, volume);
	if (instance_id < 0) {
		CERR("No channel was available to play sfx '" << num << "'");
		return -1;
	}

	CERR("Playing SFX: " << num << " with real volume " << std::hex << volume
						 << std::dec);

	mixer->set2DPosition(instance_id, distance, balance);
	mixer->setPaused(instance_id, false);

	return instance_id;
}

/*
 *	This returns a 'unique' ID, but only for .wav SFX's (for now).
 */
int Audio::play_sound_effect(
		const File_spec& sfxfile, int num, int volume, int balance, int repeat,
		int distance) {
#ifdef DEBUG
	cout << formatTicks() << "Audio subsystem request: Sound Effect play "
		 << num << " in " << sfxfile.name << " volume " << volume << " balance "
		 << balance << " distance " << distance << endl;
#endif
	if (!audio_enabled || !effects_enabled) {
		return -1;
	}
	// TODO: No support for MIDI SFX at this time here.
	return play_wave_sfx(sfxfile, num, volume, balance, repeat, distance);
}

/*
 *	Play a .wav format sound effect,
 *  return the channel number playing on or -1 if not playing, (0 is a valid
 *channel in SDL_Mixer!)
 */
int Audio::play_wave_sfx(
		const File_spec& sfxfile, int num,
		int volume,     // 0-256.
		int balance,    // balance, -256 (left) - +256 (right)
		int repeat,     // Keep playing.
		int distance) {
#ifdef DEBUG
	cout << formatTicks() << "Audio subsystem request: Sound Effect play "
		 << num << " volume " << volume << " balance " << balance
		 << " distance " << distance << endl;
#endif
	if (!effects_enabled || !mixer || !U7exists(sfxfile.name)) {
		return -1;    // no .wav sfx available
	}
	IExultDataSource ds(sfxfile, num);
	if (!ds.good()) {
		cerr << "SFX " << num << " from {" << sfxfile.name << ", "
			 << sfxfile.index << "} is out of range" << endl;
		return -1;
	}

	size_t wavlen;    // Read .wav file.
	auto   wavbuf = ds.steal_data(wavlen);
	auto*  wave   = AudioSample::createAudioSample(std::move(wavbuf), wavlen);

	volume                = (volume * sfx_volume) / 100;
	const int instance_id = mixer->playSample(
			wave, repeat, 0, true, AUDIO_DEF_PITCH, volume, volume);
	// Either AudioMixer::playSample called IncRef through
	// AudioChannel::playSample and the sample will be played, or the sample was
	// not queued for playback. In either case we need to Release the sample to
	// avoid a memory leak.
	wave->Release();
	if (instance_id < 0) {
		CERR("No channel was available to play sfx '"
			 << num << "' from {" << sfxfile.name << ", " << sfxfile.index
			 << "}");
		return -1;
	}

	CERR("Playing SFX: " << num << " from {" << sfxfile.name << ", "
						 << sfxfile.index << "}");

	mixer->set2DPosition(instance_id, distance, balance);
	mixer->setPaused(instance_id, false);

	return instance_id;
}

/*
static int slow_sqrt(int i)
{
	for (int r = i/2; r != 0; r--)
	{
		if (r*r <= i) return r;
	}

	return 0;
}
*/

void Audio::get_2d_position_for_tile(
		const Tile_coord& tile, int& distance, int& balance) {
	distance = 0;
	balance  = 0;

	Game_window*     gwin = Game_window::get_instance();
	const TileRect   size = gwin->get_win_tile_rect();
	const Tile_coord apos(
			size.x + size.w / 2, size.y + size.h / 2,
			gwin->get_camera_actor()->get_lift());

	const int sqr_dist = apos.square_distance_screen_space(tile);
	if (sqr_dist > MAX_SOUND_FALLOFF * MAX_SOUND_FALLOFF) {
		distance = 257;
		return;
	}

	// distance = sqrt((double) sqr_dist) * 256 / MAX_SOUND_FALLOFF;
	// distance = slow_sqrt(sqr_dist) * 256 / MAX_SOUND_FALLOFF;
	distance = sqr_dist * 256 / (MAX_SOUND_FALLOFF * MAX_SOUND_FALLOFF);

	balance = (Tile_coord::delta(apos.tx, tile.tx) * 2 - tile.tz - apos.tz) * 32
			  / 5;
}

int Audio::play_sound_effect(
		int num, const Game_object* obj, int volume, int repeat) {
	const Game_object* outer = obj->get_outermost();
	const Tile_coord   tile  = outer->get_center_tile();
	return play_sound_effect(num, tile, volume, repeat);
}

int Audio::play_sound_effect(
		int num, const Tile_coord& tile, int volume, int repeat) {
	int distance;
	int balance;
	get_2d_position_for_tile(tile, distance, balance);
	if (distance > 256) {
		distance = 256;
	}
	return play_sound_effect(num, volume, balance, repeat, distance);
}

int Audio::update_sound_effect(int chan, const Game_object* obj) {
	const Game_object* outer = obj->get_outermost();
	const Tile_coord   tile  = outer->get_center_tile();
	return update_sound_effect(chan, tile);
}

int Audio::update_sound_effect(int chan, const Tile_coord& tile) {
	if (!mixer) {
		return -1;
	}

	int distance;
	int balance;
	get_2d_position_for_tile(tile, distance, balance);
	if (distance > 256) {
		mixer->stopSample(chan);
		return -1;
	} else if (mixer->set2DPosition(chan, distance, balance)) {
		return chan;
	} else {
		return -1;
	}
}

void Audio::stop_sound_effect(int chan) {
#ifdef DEBUG
	cout << formatTicks() << "Audio subsystem request: Sound Effect stop "
		 << chan << endl;
#endif
	if (!mixer) {
		return;
	}
	mixer->stopSample(chan);
}

/*
 *	Halt sound effects.
 */

void Audio::stop_sound_effects() {
#ifdef DEBUG
	cout << formatTicks() << "Audio subsystem request: Sound Effect stop all"
		 << endl;
#endif
	if (sfxs) {
		sfxs->flush(mixer.get());
	}

#ifdef ENABLE_MIDISFX
	if (mixer && mixer->getMidiPlayer()) {
		mixer->getMidiPlayer()->stop_sound_effects();
	}
#endif
}

void Audio::set_audio_enabled(bool ena) {
	if (ena && audio_enabled && initialized) {
	} else if (!ena && audio_enabled && initialized) {
		stop_sound_effects();
		stop_music();
		audio_enabled = false;
	} else if (ena && !audio_enabled && initialized) {
		audio_enabled = true;
	} else if (!ena && !audio_enabled && initialized) {
	} else if (ena && !audio_enabled && !initialized) {
		audio_enabled = true;

		int  sample_rate = 44100;
		bool stereo      = true;

		config->value("config/audio/sample_rate", sample_rate, sample_rate);
		config->value("config/audio/stereo", stereo, stereo);

		Init(sample_rate, stereo ? 2 : 1);
	} else if (!ena && !audio_enabled && !initialized) {
	}
}

bool Audio::is_track_playing(int num) const {
	MyMidiPlayer* midi = get_midi();
	return midi && midi->is_track_playing(num);
}

bool Audio::is_voice_playing() const {
	return mixer && mixer->isPlayingVoice();
}

MyMidiPlayer* Audio::get_midi() const {
	return mixer ? mixer->getMidiPlayer() : nullptr;
}

void Audio::set_speech_volume(int newvol, bool saveconfig) {
	speech_volume = newvol;
	if (speech_id != -1 && mixer->isPlaying(speech_id)) {
		newvol = (newvol * 255) / 100;
		mixer->setVolume(speech_id, newvol, newvol);
	}
	if (saveconfig) {
		config->set("config/audio/speech/speech_volume", speech_volume, false);
	}
}

void Audio::set_sfx_volume(int newvol, bool saveconfig) {
	sfx_volume = newvol;
	if (saveconfig) {
		config->set("config/audio/effects/sfx_volume", sfx_volume, false);
	}
}
