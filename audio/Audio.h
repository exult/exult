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

#ifndef AUDIO_H
#define AUDIO_H

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "AudioMixer.h"
#include "Flex.h"
#include "Midi.h"
#include "exceptions.h"
#include "exult_constants.h"

#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace Pentagram {
	class AudioSample;
}
struct File_spec;
class Game_object;
class MyMidiPlayer;
class SFX_cached;
class Tile_coord;

#define MAX_SOUND_FALLOFF 24

/*
 *	Music:
 */
enum Combat_song {
	CSBattle_Over,
	CSAttacked1,
	CSAttacked2,
	CSVictory,
	CSRun_Away,
	CSDanger,
	CSHidden_Danger
};

/*
 *	This is a resource-management class for SFX. Maybe make it a
 *	template class and use for other resources also?
 *	Based on code by Sam Lantinga et al on:
 *	http://www.ibm.com/developerworks/library/l-pirates2/
 */
class SFX_cache_manager {
	using SFX_cached = std::pair<int, Pentagram::AudioSample*>;

	std::map<int, SFX_cached> cache;

	// Tries to locate a sfx in the cache based on sfx num.
	SFX_cached* find_sfx(int id);

public:
	SFX_cache_manager() = default;
	~SFX_cache_manager();
	SFX_cache_manager(const SFX_cache_manager&)            = default;
	SFX_cache_manager(SFX_cache_manager&&)                 = default;
	SFX_cache_manager& operator=(const SFX_cache_manager&) = default;
	SFX_cache_manager& operator=(SFX_cache_manager&&)      = default;
	// For SFX played through 'play_wave_sfx'. Searched cache for
	// the sfx first, then loads from the sfx file if needed.
	Pentagram::AudioSample* request(Flex* sfx_file, int id);
	// Empties the cache.
	void flush(Pentagram::AudioMixer* mixer = nullptr);
	// Remove unused sounds from the cache.
	void garbage_collect();
};

//---- Audio -----------------------------------------------------------

class Audio : nonreplicatable {
private:
	static Audio*     self;
	static const int* bg2si_songs;    // Converts BG songs to SI songs.
	static const int* bg2si_sfxs;     // Converts BG sfx's to SI sfx's.
	bool              truthful_ = false;
	bool speech_enabled = true, music_enabled = true, effects_enabled = true,
		 speech_with_subs                            = false;
	int                                speech_volume = 100;
	int                                sfx_volume    = 100;
	std::unique_ptr<SFX_cache_manager> sfxs;    // SFX and voice cache manager
	bool                               initialized = false;
	SDL_AudioSpec                      wanted;
	std::unique_ptr<Pentagram::AudioMixer> mixer;
	sint32                                 speech_id = -1;
	bool                                   audio_enabled;
	std::unique_ptr<Flex> sfx_file;    // Holds .wav sound effects.
	// You never allocate an Audio object directly, you rather access it using
	// get_ptr()
	Audio();
	~Audio();
	void Init(int _samplerate, int _channels);

public:
	friend class Tired_of_compiler_warnings;
	static void   Init();
	static void   Destroy();
	static Audio* get_ptr();

	// Given BG sfx, get SI if playing SI.
	static int game_sfx(int sfx) {
		return bg2si_sfxs ? bg2si_sfxs[sfx] : sfx;
	}

	// Given BG song, get SI if playing SI.
	static int game_music(int mus) {
		return bg2si_songs ? bg2si_songs[mus] : mus;
	}

	void Init_sfx();

	void honest_sample_rates() {
		truthful_ = true;
	}

	void cancel_streams();    // Dump any audio streams

	void pause_audio();
	void resume_audio();

	sint32 copy_and_play_speech(
			const uint8* sound_data, uint32 len, bool, int volume = 255);
	sint32 copy_and_play_sfx(
			const uint8* sound_data, uint32 len, bool, int volume = 255);
	sint32 play(
			std::unique_ptr<uint8[]> sound_data, uint32 len, bool wait,
			int volume = 255);
	sint32 playSpeechfile(
			const char* fname, const char* fpatch, bool wait, int volume = 255);
	bool playing();
	bool start_music(
			int num, bool continuous = false, MyMidiPlayer::ForceType force= MyMidiPlayer::Force_None,
			const std::string& flex = MAINMUS);
	void change_repeat(bool newrepeat);
	bool start_music(
			const std::string& fname, int num, bool continuous = false,
			MyMidiPlayer::ForceType force = MyMidiPlayer::Force_None);
	void start_music_combat(Combat_song song, bool continuous);
	void stop_music();
	int  play_sound_effect(
			 int num, int volume = AUDIO_MAX_VOLUME, int balance = 0,
			 int repeat = 0, int distance = 0);
	int play_wave_sfx(
			int num, int volume = AUDIO_MAX_VOLUME, int balance = 0,
			int repeat = 0, int distance = 0);
	int play_sound_effect(
			int num, const Game_object* obj, int volume = AUDIO_MAX_VOLUME,
			int repeat = 0);
	int play_sound_effect(
			int num, const Tile_coord& tile, int volume = AUDIO_MAX_VOLUME,
			int repeat = 0);
	// These two do not cache the SFX, and play it directly from the file.
	int play_sound_effect(
			const File_spec& sfxfile, int num, int volume = AUDIO_MAX_VOLUME,
			int balance = 0, int repeat = 0, int distance = 0);
	int play_wave_sfx(
			const File_spec& sfxfile, int num, int volume = AUDIO_MAX_VOLUME,
			int balance = 0, int repeat = 0, int distance = 0);

	static void get_2d_position_for_tile(
			const Tile_coord& tile, int& distance, int& balance);

	int update_sound_effect(int chan, const Game_object* obj);
	int update_sound_effect(int chan, const Tile_coord& tile);

	void stop_sound_effect(int chan);

	void stop_sound_effects();
	bool start_speech(int num, bool wait = false);
	void stop_speech();
	bool is_speech_playing();
	int  wait_for_speech(
			 std::function<int(Uint32 ms)> waitfunc
			 = std::function<int(Uint32 ms)>());

	bool is_speech_enabled() const {
		return speech_enabled;
	}

	void set_speech_enabled(bool ena) {
		speech_enabled = ena;
	}

	bool is_speech_with_subs() const {
		return speech_with_subs;
	}

	void set_speech_with_subs(bool ena) {
		speech_with_subs = ena;
	}

	bool is_music_enabled() const {
		return music_enabled;
	}

	void set_music_enabled(bool ena) {
		music_enabled = ena;
	}

	bool are_effects_enabled() const {
		return effects_enabled;
	}

	void set_effects_enabled(bool ena) {
		effects_enabled = ena;
	}

	bool is_audio_enabled() const {
		return audio_enabled;
	}

	void set_audio_enabled(bool ena);

	enum class LoopingType {
		Never   = 0,
		Limited = 1,
		Auto    = 2,
		Endless = 3,
	};

private:
	LoopingType music_looping;

public:
	LoopingType get_music_looping() const {
		return music_looping;
	}

	void set_music_looping(LoopingType type) {
		music_looping = type;
	}

	static bool can_sfx(const std::string& file, std::string* out = nullptr);
	static bool have_roland_sfx(Exult_Game game, std::string* out = nullptr);
	static bool have_sblaster_sfx(Exult_Game game, std::string* out = nullptr);
	static bool have_midi_sfx(std::string* out = nullptr);
	static bool have_config_sfx(
			const std::string& game, std::string* out = nullptr);
	static void channel_complete_callback(int chan);

	bool is_track_playing(int num) const;
	bool is_voice_playing() const;

	Flex* get_sfx_file() {
		return sfx_file.get();
	}

	SFX_cache_manager* get_sfx_cache() const {
		return sfxs.get();
	}

	MyMidiPlayer* get_midi() const;

	int get_speech_volume() {
		return speech_volume;
	}

	int get_sfx_volume() {
		return sfx_volume;
	}

	void set_speech_volume(int newvol, bool savetoconfig);
	void set_sfx_volume(int newvol, bool savetoconfig);
};

#endif
