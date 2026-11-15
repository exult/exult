/*
 *  Copyright (C) 2001-2022 The Exult Team
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

#ifndef KEYS_H
#define KEYS_H

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "game.h"

#include <map>
#include <string>
#include <vector>

typedef struct ExultKey {
	SDL_Keycode key;
	SDL_Keymod  mod;
} EXULT_Keysym;

const int c_maxparams = 4;

using ActionFunc = void (*)(const int*);

struct Action;

ActionFunc GetExultAction(const std::string& name);

struct ActionType {
	const Action* action;
	int           params[c_maxparams];
};

struct ltExultKey {
	bool operator()(ExultKey k1, ExultKey k2) const {
		if (k1.key == k2.key) {
			return k1.mod < k2.mod;
		} else {
			return k1.key < k2.key;
		}
	}
};

using KeyMap = std::map<ExultKey, ActionType, ltExultKey>;

class KeyBinder {
public:
	// Where a binding line was read from.
	enum class Source {
		defaults,    // The built-in set (defaultkeys in exult.flx).
		file,        // The keys file named by config/disk/game/<game>/keys.
		patch        // <PATCH>/patchkeys.txt (a mod).
	};

	// One parsed line of a keys file, in load order.
	struct Binding {
		ExultKey    key{SDLK_UNKNOWN, SDL_KMOD_NONE};
		std::string action;    // Lower case, as written in a keys file.
		int         params[c_maxparams]{-1, -1, -1, -1};
		int         nparams = 0;    // Parameters that were written.
		Source      source  = Source::defaults;
		std::string comment;              // Text after '#', if any.
		bool        hidden    = false;    // "#-": not in the help screen.
		bool        effective = true;     // False if a later line replaced it.
	};

	// A bindable action from the action table.
	struct ActionInfo {
		enum class Type {
			normal,
			cheat,
			mapedit,
			hidden
		};

		std::string name;     // Lower case.
		std::string label;    // Description (empty for hidden actions).
		Type        type         = Type::normal;
		Exult_Game  game         = NONE;
		bool        needs_params = false;    // Cannot work without parameters.
	};

private:
	KeyMap bindings;
	std::vector<Binding> log;
	Source               cur_source = Source::defaults;

	std::vector<std::string> keyhelp;
	std::vector<std::string> cheathelp;
	std::vector<std::string> mapedithelp;
	std::vector<std::string> last_created_key;
	void                     LoadFromFileInternal(const char* filename);
	KeyMap::const_iterator   TranslateEvent(const SDL_Event& ev) const;

public:
	KeyBinder();
	/* Add keybinding */
	void AddKeyBinding(SDL_Keycode key, SDL_Keymod mod, const Action* action, int nparams, const int* params);

	/* Other methods */
	void Flush() {
		bindings.clear();
		log.clear();
		keyhelp.clear();
		cheathelp.clear();
		mapedithelp.clear();
		last_created_key.clear();
	}

	// By value: the caller's copy must outlive a reload of the bindings.
	bool DoAction(ActionType action, bool press) const;
	bool HandleEvent(const SDL_Event& ev) const;
	bool IsMotionEvent(const SDL_Event& ev) const;

	void LoadFromFile(const char* filename);
	void LoadFromPatch();
	void LoadDefaults(bool quiet = false);
	// Load the set a game uses: the keys file named in the configuration (or
	// the defaults), then <PATCH>/patchkeys.txt on top.
	void LoadConfigured();
	// Parse keys file text (additive; call Flush first for a clean set).
	void LoadFromText(const std::string& text, Source source);
	// Every parsed line, in load order.
	std::vector<Binding>           ListBindings() const;
	static std::vector<ActionInfo> ListActions();
	// The key an event stands for (Shift, Ctrl, Alt; Cmd counts as Alt on
	// macOS). False for events that are not key presses or releases.
	static bool NormaliseEvent(const SDL_Event& ev, ExultKey& out);
	// Keys file spelling, e.g. "Ctrl-Shift-D" or "KP7". Empty if the key
	// cannot be written in a keys file.
	static std::string FormatKeyToken(ExultKey key);
	// Parse such a spelling.
	static bool ParseKeyToken(const std::string& token, ExultKey& out);
	// The spelling shown to players (Cmd instead of Alt on macOS).
	static std::string DescribeKey(ExultKey key);
	// Lower-case action name of a bound action and its parameters.
	static std::string ActionName(const ActionType& a);
	void ShowHelp() const;
	void ShowCheatHelp() const;
	void ShowMapeditHelp() const;
	void ShowBrowserKeys() const;

private:
	void ParseText(char* text, int len);
	void ParseLine(char* line, int lineNumber);
	void FillParseMaps();
};

#endif /* KEYS_H */
