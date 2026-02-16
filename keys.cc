/*
 *  Copyright (C) 2001-2025 The Exult Team
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

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

#include "Scroll_gump.h"
#include "U7file.h"
#include "actors.h"
#include "cheat.h"
#include "exult.h"
#include "game.h"
#include "gamewin.h"
#include "items.h"
#include "keyactions.h"
#include "keys.h"
#include "mouse.h"
#include "ucmachine.h"
#include "utils.h"

using std::atoi;
using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::isspace;
using std::strchr;
using std::string;
using std::strlen;

static class Chardata {    // ctype-like character lists
public:
	std::string whitespace;

	Chardata() {
		for (size_t i = 0; i < 256; i++) {
			if (isspace(i)) {
				whitespace += static_cast<char>(i);
			}
		}
	}
} chardata;

using ActionFunc = void (*)(const int*);

const struct Action {
	const char*  s;
	ActionFunc   func;
	ActionFunc   func_release;
	unsigned int desg_msg_id;

	const char* desc() const {
		return get_text_msg(desg_msg_id - msg_file_start);
	}

	enum {
		dont_show = 0,
		normal_keys,
		cheat_keys,
		mapedit_keys
	} key_type;

	Exult_Game game;
	bool       allow_during_dont_move;
	bool       allow_if_cant_act;
	bool       allow_if_cant_act_charmed;
	bool       is_movement;
} ExultActions[] = {
		// Disable clang-format which condenses four spaces into an horizontal tab after a brace or a comma.
		//  This causes misalignments.
		// clang-format off
		{        "VOLUME_MIXER",          ActionMixerGump,           nullptr, 0x800,  Action::normal_keys,         NONE,  true,  true,  true, false},
		{                "QUIT",               ActionQuit,           nullptr, 0x801,  Action::normal_keys,         NONE,  true,  true,  true, false},
		{        "SAVE_RESTORE",           ActionFileGump,           nullptr, 0x802,  Action::normal_keys,         NONE,  true,  true,  true, false},
		{           "QUICKSAVE",          ActionQuicksave,           nullptr, 0x803,  Action::normal_keys,         NONE, false,  true,  true, false},
		{        "QUICKRESTORE",       ActionQuickrestore,           nullptr, 0x804,  Action::normal_keys,         NONE,  true,  true,  true, false},
		{               "ABOUT",              ActionAbout,           nullptr, 0x805,  Action::normal_keys,         NONE, false,  true,  true, false},
		{                "HELP",               ActionHelp,           nullptr, 0x806,  Action::normal_keys,         NONE, false,  true,  true, false},
		{         "CLOSE_GUMPS",         ActionCloseGumps,           nullptr, 0x807,    Action::dont_show,         NONE, false,  true,  true, false},
		{       "CLOSE_OR_MENU",        ActionCloseOrMenu,           nullptr, 0x808,  Action::normal_keys,         NONE,  true,  true,  true, false},
		{          "SCREENSHOT",         ActionScreenshot,           nullptr, 0x809,  Action::normal_keys,         NONE,  true,  true,  true, false},
		{           "GAME_MENU",           ActionMenuGump,           nullptr, 0x80a,  Action::normal_keys,         NONE,  true,  true,  true, false},
		{       "OLD_FILE_GUMP",        ActionOldFileGump,           nullptr, 0x80b,  Action::normal_keys,         NONE,  true,  true,  true, false},
		{   "SCALEVAL_INCREASE",   ActionScalevalIncrease,           nullptr, 0x80c, Action::mapedit_keys,         NONE,  true,  true,  true, false},
		{   "SCALEVAL_DECREASE",   ActionScalevalDecrease,           nullptr, 0x80d, Action::mapedit_keys,         NONE,  true,  true,  true, false},
		{            "BRIGHTER",           ActionBrighter,           nullptr, 0x80e,  Action::normal_keys,         NONE,  true,  true,  true, false},
		{              "DARKER",             ActionDarker,           nullptr, 0x80f,  Action::normal_keys,         NONE,  true,  true,  true, false},
		{   "TOGGLE_FULLSCREEN",         ActionFullscreen,           nullptr, 0x810,  Action::normal_keys,         NONE,  true,  true,  true, false},

		{             "USEITEM",            ActionUseItem,           nullptr, 0x811,    Action::dont_show,         NONE, false, false, false, false},
		{             "USEFOOD",            ActionUseFood,           nullptr, 0x812,  Action::normal_keys,         NONE, false, false, false, false},
		{        "CALL_USECODE",        ActionCallUsecode,           nullptr, 0x813,    Action::dont_show,         NONE, false, false, false, false},
		{       "TOGGLE_COMBAT",             ActionCombat,           nullptr, 0x814,  Action::normal_keys,         NONE, false, false,  true, false},
		{        "PAUSE_COMBAT",        ActionCombatPause,           nullptr, 0x815,  Action::normal_keys,         NONE, false, false,  true, false},
		{         "TARGET_MODE",             ActionTarget,           nullptr, 0x816,  Action::normal_keys,         NONE, false, false,  true, false},
		{           "INVENTORY",          ActionInventory,           nullptr, 0x817,  Action::normal_keys,         NONE, false, false,  true, false},
		{            "TRY_KEYS",            ActionTryKeys,           nullptr, 0x818,  Action::normal_keys,         NONE, false, false, false, false},
		{               "STATS",              ActionStats,           nullptr, 0x819,  Action::normal_keys,         NONE, false,  true,  true, false},
		{        "COMBAT_STATS",        ActionCombatStats,           nullptr, 0x81A,  Action::normal_keys, SERPENT_ISLE, false, false,  true, false},
		{          "FACE_STATS",          ActionFaceStats,           nullptr, 0x81B,  Action::normal_keys,         NONE, false,  true,  true, false},
		{   "USE_HEALING_ITEMS",    ActionUseHealingItems,           nullptr, 0x81C,  Action::normal_keys,         NONE, false, false, false, false},
		{       "SHOW_SI_INTRO",            ActionSIIntro,           nullptr, 0x81D,   Action::cheat_keys, SERPENT_ISLE, false,  true,  true, false},
		{        "SHOW_ENDGAME",            ActionEndgame,           nullptr, 0x81E,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{         "SCROLL_LEFT",         ActionScrollLeft,           nullptr, 0x81F,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{        "SCROLL_RIGHT",        ActionScrollRight,           nullptr, 0x820,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{           "SCROLL_UP",           ActionScrollUp,           nullptr, 0x821,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{         "SCROLL_DOWN",         ActionScrollDown,           nullptr, 0x822,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{           "WALK_WEST",           ActionWalkWest, ActionStopWalking, 0x823,  Action::normal_keys,         NONE, false, false, false,  true},
		{           "WALK_EAST",           ActionWalkEast, ActionStopWalking, 0x824,  Action::normal_keys,         NONE, false, false, false,  true},
		{          "WALK_NORTH",          ActionWalkNorth, ActionStopWalking, 0x825,  Action::normal_keys,         NONE, false, false, false,  true},
		{          "WALK_SOUTH",          ActionWalkSouth, ActionStopWalking, 0x826,  Action::normal_keys,         NONE, false, false, false,  true},
		{     "WALK_NORTH_EAST",      ActionWalkNorthEast, ActionStopWalking, 0x827,  Action::normal_keys,         NONE, false, false, false,  true},
		{     "WALK_SOUTH_EAST",      ActionWalkSouthEast, ActionStopWalking, 0x828,  Action::normal_keys,         NONE, false, false, false,  true},
		{     "WALK_NORTH_WEST",      ActionWalkNorthWest, ActionStopWalking, 0x829,  Action::normal_keys,         NONE, false, false, false,  true},
		{     "WALK_SOUTH_WEST",      ActionWalkSouthWest, ActionStopWalking, 0x82A,  Action::normal_keys,         NONE, false, false, false,  true},
		{       "CENTER_SCREEN",             ActionCenter,           nullptr, 0x82B,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{       "SHAPE_BROWSER",       ActionShapeBrowser,           nullptr, 0x82c,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{  "SHAPE_BROWSER_HELP",   ActionShapeBrowserHelp,           nullptr, 0x82d,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{         "CREATE_ITEM",        ActionCreateShape,           nullptr, 0x82E,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{       "DELETE_OBJECT",       ActionDeleteObject,           nullptr, 0x82F,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{         "TOGGLE_EGGS",         ActionToggleEggs,           nullptr, 0x830,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{     "TOGGLE_GOD_MODE",            ActionGodMode,           nullptr, 0x831,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{       "CHANGE_GENDER",             ActionGender,           nullptr, 0x832,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{          "CHEAT_HELP",          ActionCheatHelp,           nullptr, 0x833,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{  "TOGGLE_INFRAVISION",        ActionInfravision,           nullptr, 0x834,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{   "TOGGLE_HACK_MOVER",          ActionHackMover,           nullptr, 0x835,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{        "MAP_TELEPORT",        ActionMapTeleport,           nullptr, 0x836,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{     "CURSOR_TELEPORT",           ActionTeleport,           nullptr, 0x837,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{"TARGET_MODE_TELEPORT", ActionTeleportTargetMode,           nullptr, 0x838,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{   "NEXT_MAP_TELEPORT",    ActionNextMapTeleport,           nullptr, 0x839,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{    "NEXT_TIME_PERIOD",               ActionTime,           nullptr, 0x83A,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{  "TOGGLE_WIZARD_MODE",             ActionWizard,           nullptr, 0x83B,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{          "PARTY_HEAL",               ActionHeal,           nullptr, 0x83C,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{"PARTY_INCREASE_LEVEL",            ActionLevelup,           nullptr, 0x83D,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{        "CHEAT_SCREEN",        ActionCheatScreen,           nullptr, 0x83E,   Action::cheat_keys,         NONE,  true,  true,  true, false},
		{         "PICK_POCKET",         ActionPickPocket,           nullptr, 0x83F,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{         "NPC_NUMBERS",         ActionNPCNumbers,           nullptr, 0x840,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{          "GRAB_ACTOR",          ActionGrabActor,           nullptr, 0x841,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{          "PLAY_MUSIC",          ActionPlayMusic,           nullptr, 0x842,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{        "TOGGLE_NAKED",              ActionNaked,           nullptr, 0x843,   Action::cheat_keys, SERPENT_ISLE, false,  true,  true, false},
		{        "TOGGLE_PETRA",              ActionPetra,           nullptr, 0x844,   Action::cheat_keys, SERPENT_ISLE, false,  true,  true, false},
		{         "CHANGE_SKIN",         ActionSkinColour,           nullptr, 0x845,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{            "NOTEBOOK",           ActionNotebook,           nullptr, 0x846,  Action::normal_keys,         NONE, false, false, false, false},
		{        "SOUND_TESTER",        ActionSoundTester,           nullptr, 0x847,   Action::cheat_keys,         NONE, false,  true,  true, false},
		{                "TEST",               ActionTest,           nullptr, 0x848,    Action::dont_show,         NONE, false,  true,  true, false},

		{        "MAPEDIT_HELP",        ActionMapeditHelp,           nullptr, 0x849, Action::mapedit_keys,         NONE, false,  true,  true, false},
		{   "TOGGLE_MAP_EDITOR",          ActionMapEditor,           nullptr, 0X84A, Action::mapedit_keys,         NONE,  true,  true,  true, false},
		{  "SKIPLIFT_DECREMENT",           ActionSkipLift,           nullptr, 0x84B, Action::mapedit_keys,         NONE, false,  true,  true, false},
		{                 "CUT",                ActionCut,           nullptr, 0x84C, Action::mapedit_keys,         NONE,  true,  true,  true, false},
		{                "COPY",               ActionCopy,           nullptr, 0x84D, Action::mapedit_keys,         NONE,  true,  true,  true, false},
		{               "PASTE",              ActionPaste,           nullptr, 0x84E, Action::mapedit_keys,         NONE,  true,  true,  true, false},
		{     "DELETE_SELECTED",     ActionDeleteSelected,           nullptr, 0x84F, Action::mapedit_keys,         NONE,  true,  true,  true, false},
		{       "MOVE_SELECTED",       ActionMoveSelected,           nullptr, 0x850, Action::mapedit_keys,         NONE,  true,  true,  true, false},
		{   "CYCLE_FRAMES_NEXT",        ActionCycleFrames,           nullptr, 0x851, Action::mapedit_keys,         NONE,  true,  true,  true, false},
		{   "CYCLE_FRAMES_PREV",        ActionCycleFrames,           nullptr, 0X852, Action::mapedit_keys,         NONE,  true,  true,  true, false},
		{       "ROTATE_FRAMES",       ActionRotateFrames,           nullptr, 0x853, Action::mapedit_keys,         NONE,  true,  true,  true, false},
		{       "WRITE_MINIMAP",       ActionWriteMiniMap,           nullptr, 0x854, Action::mapedit_keys,         NONE, false,  true,  true, false},
		{             "REPAINT",            ActionRepaint,           nullptr, 0x855,    Action::dont_show,         NONE,  true,  true,  true, false},
		{       "TOGGLE_BBOXES",       ActionToggleBBoxes,           nullptr, 0x856, Action::mapedit_keys,         NONE, false,  true,  true, false},
		{                    "",                  nullptr,           nullptr,     0,    Action::dont_show,         NONE, false, false, false, false}  //  terminator
		// clang-format on
};

const struct {
	const char* s;
	SDL_Keycode k;
} SDLKeyStringTable[] = {
		// clang-format off
		{"BACKSPACE",   SDLK_BACKSPACE},
		{      "TAB",         SDLK_TAB},
		{    "ENTER",      SDLK_RETURN},
		{    "PAUSE",       SDLK_PAUSE},
		{      "ESC",      SDLK_ESCAPE},
		{    "SPACE",       SDLK_SPACE},
		{      "DEL",      SDLK_DELETE},
		{      "KP0",        SDLK_KP_0},
		{      "KP1",        SDLK_KP_1},
		{      "KP2",        SDLK_KP_2},
		{      "KP3",        SDLK_KP_3},
		{      "KP4",        SDLK_KP_4},
		{      "KP5",        SDLK_KP_5},
		{      "KP6",        SDLK_KP_6},
		{      "KP7",        SDLK_KP_7},
		{      "KP8",        SDLK_KP_8},
		{      "KP9",        SDLK_KP_9},
		{      "KP0",        SDLK_KP_0},
		{      "KP.",   SDLK_KP_PERIOD},
		{      "KP/",   SDLK_KP_DIVIDE},
		{      "KP*", SDLK_KP_MULTIPLY},
		{      "KP-",    SDLK_KP_MINUS},
		{      "KP+",     SDLK_KP_PLUS},
		{ "KP_ENTER",    SDLK_KP_ENTER},
		{       "UP",          SDLK_UP},
		{     "DOWN",        SDLK_DOWN},
		{    "RIGHT",       SDLK_RIGHT},
		{     "LEFT",        SDLK_LEFT},
		{   "INSERT",      SDLK_INSERT},
		{     "HOME",        SDLK_HOME},
		{      "END",         SDLK_END},
		{   "PAGEUP",      SDLK_PAGEUP},
		{ "PAGEDOWN",    SDLK_PAGEDOWN},
		{       "F1",          SDLK_F1},
		{       "F2",          SDLK_F2},
		{       "F3",          SDLK_F3},
		{       "F4",          SDLK_F4},
		{       "F5",          SDLK_F5},
		{       "F6",          SDLK_F6},
		{       "F7",          SDLK_F7},
		{       "F8",          SDLK_F8},
		{       "F9",          SDLK_F9},
		{      "F10",         SDLK_F10},
		{      "F11",         SDLK_F11},
		{      "F12",         SDLK_F12},
		{      "F13",         SDLK_F13},
		{      "F14",         SDLK_F14},
		{      "F15",         SDLK_F15},
		{         "",     SDLK_UNKNOWN}  // terminator
		// clang-format on
};

using ParseKeyMap    = std::map<std::string, SDL_Keycode>;
using ParseActionMap = std::map<std::string, const Action*>;

static ParseKeyMap    keys;
static ParseActionMap actions;

KeyBinder::KeyBinder() {
	FillParseMaps();
}

void KeyBinder::AddKeyBinding(SDL_Keycode key, SDL_Keymod mod, const Action* action, int nparams, const int* params) {
	ExultKey   k;
	ActionType a;
	k.key    = key;
	k.mod    = mod;
	a.action = action;
	int i;    // For MSVC
	for (i = 0; i < c_maxparams && i < nparams; i++) {
		a.params[i] = params[i];
	}
	for (i = nparams; i < c_maxparams; i++) {
		a.params[i] = -1;
	}

	bindings[k] = a;
}

bool KeyBinder::DoAction(const ActionType& a, bool press) const {
	if (!cheat() && (a.action->key_type == Action::cheat_keys || a.action->key_type == Action::mapedit_keys)) {
		return true;
	}
	if (a.action->game != NONE && a.action->game != Game::get_game_type()) {
		return true;
	}

	// Restrict key actions in dont_move mode
	if (!a.action->allow_during_dont_move && Game_window::get_instance()->main_actor_dont_move()) {
		return true;
	}

	// Restrict keys if avatar is sleeping/paralyzed/unconscious/dead
	if (!a.action->allow_if_cant_act && !Game_window::get_instance()->main_actor_can_act()) {
		return true;
	}
	if (!a.action->allow_if_cant_act_charmed && !Game_window::get_instance()->main_actor_can_act_charmed()) {
		return true;
	}
	if (press) {
#ifndef USE_EXULTSTUDIO
		// Display unsupported message if no ES support.
		if (a.action->key_type == Action::mapedit_keys) {
			Scroll_gump* scroll;
			scroll = new Scroll_gump();

			scroll->add_text("Map editor functionality is disabled in this "
							 "version of Exult.\n");
			scroll->add_text("If you want or need this functionality, you "
							 "should install a snapshot, as they have support "
							 "for Exult Studio.\n");
			scroll->add_text("If no snapshots are available for your platform, "
							 "you will have to compile it on your own.\n");

			scroll->paint();
			do {
				int x, y;
				Get_click(x, y, Mouse::hand, nullptr, false, scroll);
			} while (scroll->show_next_page());
			Game_window::get_instance()->paint();
			delete scroll;
			return true;
		}
#endif
		a.action->func(a.params);
	} else {
#ifndef USE_EXULTSTUDIO
		// Do nothing when releasing mapedit keys if no ES support.
		if (a.action->key_type == Action::mapedit_keys) {
			return true;
		}
#endif
		if (a.action->func_release != nullptr) {
			a.action->func_release(a.params);
		}
	}

	return true;
}

KeyMap::const_iterator KeyBinder::TranslateEvent(const SDL_Event& ev) const {
	ExultKey key{ev.key.key, ev.key.mod};

	if (ev.type != SDL_EVENT_KEY_DOWN && ev.type != SDL_EVENT_KEY_UP) {
		return bindings.end();
	}

	key.mod = SDL_KMOD_NONE;
	if (ev.key.mod & SDL_KMOD_SHIFT) {
		key.mod = key.mod | SDL_KMOD_SHIFT;
	}
	if (ev.key.mod & SDL_KMOD_CTRL) {
		key.mod = key.mod | SDL_KMOD_CTRL;
	}
#ifdef MACOSX
	// map Meta to Alt on OS X
	if (ev.key.mod & SDL_KMOD_GUI) {
		key.mod = key.mod | SDL_KMOD_ALT;
	}
#else
	if (ev.key.mod & SDL_KMOD_ALT) {
		key.mod = key.mod | SDL_KMOD_ALT;
	}
#endif

	return bindings.find(key);
}

bool KeyBinder::HandleEvent(const SDL_Event& ev) const {
	auto sdlkey_index = TranslateEvent(ev);
	if (sdlkey_index != bindings.end()) {
		return DoAction(sdlkey_index->second, ev.type == SDL_EVENT_KEY_DOWN);
	}

	return false;
}

bool KeyBinder::IsMotionEvent(const SDL_Event& ev) const {
	auto sdlkey_index = TranslateEvent(ev);
	if (sdlkey_index == bindings.end()) {
		return false;
	}
	const ActionType& act = sdlkey_index->second;
	return act.action->is_movement;
}

void KeyBinder::ShowHelp() const {
	Scroll_gump* scroll;
	scroll = new Scroll_gump();

	for (const auto& iter : keyhelp) {
		scroll->set_from_help(true);
		scroll->add_text(iter.c_str());
	}

	scroll->paint();
	do {
		int x;
		int y;
		Get_click(x, y, Mouse::hand, nullptr, false, scroll);
	} while (scroll->show_next_page());
	Game_window::get_instance()->paint();
	delete scroll;
}

void KeyBinder::ShowCheatHelp() const {
	Scroll_gump* scroll;
	scroll = new Scroll_gump();

	for (const auto& iter : cheathelp) {
		scroll->set_from_help(true);
		scroll->add_text(iter.c_str());
	}

	scroll->paint();
	do {
		int x;
		int y;
		Get_click(x, y, Mouse::hand, nullptr, false, scroll);
	} while (scroll->show_next_page());
	Game_window::get_instance()->paint();
	delete scroll;
}

void KeyBinder::ShowMapeditHelp() const {
	Scroll_gump* scroll;
	scroll = new Scroll_gump();

	for (const auto& iter : mapedithelp) {
		scroll->set_from_help(true);
		scroll->add_text(iter.c_str());
	}

	scroll->paint();
	do {
		int x;
		int y;
		Get_click(x, y, Mouse::hand, nullptr, false, scroll);
	} while (scroll->show_next_page());
	Game_window::get_instance()->paint();
	delete scroll;
}

void KeyBinder::ShowBrowserKeys() const {
	auto* scroll = new Scroll_gump();
	scroll->set_from_help(true);
	int msgid = 0x860;
	for (; msgid < 0x900; ++msgid) {
		const char* text = get_text_msg(msgid - msg_file_start);
		// hit the terminator
		if (!text || !*text) {
			break;
		}
		scroll->add_text(text);
	}
	char returned_key[200];
	if (last_created_key.empty()) {
		strcpy(returned_key, get_text_msg(0x87D - msg_file_start));    // "Error: No key assigned"
	} else {
		strcpy(returned_key, "");    // prevent garbage text
		int extra_keys = 0;
		for (const auto& iter : last_created_key) {
			if (extra_keys >= 5) {
				continue;
			} else if (extra_keys > 0) {
				strcat(returned_key, get_text_msg(0x87E - msg_file_start));    // " or "
			}
			strcat(returned_key, iter.c_str());
			extra_keys += 1;
		}
	}

	strcat(returned_key, get_text_msg(0x87F - msg_file_start));    // " - when pressed in game will create the last shape
																   // viewed in shapes.vga."
	scroll->add_text(returned_key);
	scroll->paint();
	do {
		int x;
		int y;
		Get_click(x, y, Mouse::hand, nullptr, false, scroll);
	} while (scroll->show_next_page());
	Game_window::get_instance()->paint();
	delete scroll;
}

void KeyBinder::ParseText(char* text, int len) {
	char*      ptr;
	char*      end;
	const char LF                 = '\n';
	int        currentLineInBlock = 0;    // Added line counter

	ptr = text;

	// last (useful) line must end with LF
	while ((ptr - text) < len && (end = strchr(ptr, LF)) != nullptr) {
		*end = '\0';
		currentLineInBlock++;                  // Increment line counter
		ParseLine(ptr, currentLineInBlock);    // Pass line number
		ptr = end + 1;
	}
}

static void skipspace(string& s) {
	const size_t i = s.find_first_not_of(chardata.whitespace);
	if (i && i != string::npos) {
		s.erase(0, i);
	} else if (i == string::npos && !s.empty() && s.find_first_of(chardata.whitespace) == 0) {
		s.clear();
	}
}

void KeyBinder::ParseLine(char* line, int lineNumber) {
	size_t     i;
	ExultKey   k;
	ActionType a;
	k.key    = SDLK_UNKNOWN;
	k.mod    = SDL_KMOD_NONE;
	string s = line;
	string u;
	string desc;
	string keycode;
	bool   show;

	skipspace(s);

	// comments and empty lines
	if (s.length() == 0 || s[0] == '#') {
		return;
	}

	u = s;
	to_uppercase(u);

	// get key
	while (s.length() && !isspace(static_cast<unsigned char>(s[0]))) {
		// check modifiers
		//    if (u.compare("ALT-",0,4) == 0) {
		if (u.substr(0, 4) == "ALT-") {
			k.mod = k.mod | SDL_KMOD_ALT;
			s.erase(0, 4);
			u.erase(0, 4);
			//    } else if (u.compare("CTRL-",0,5) == 0) {
		} else if (u.substr(0, 5) == "CTRL-") {
			k.mod = k.mod | SDL_KMOD_CTRL;
			s.erase(0, 5);
			u.erase(0, 5);
			//    } else if (u.compare("SHIFT-",0,6) == 0) {
		} else if (u.substr(0, 6) == "SHIFT-") {
			k.mod = k.mod | SDL_KMOD_SHIFT;
			s.erase(0, 6);
			u.erase(0, 6);
		} else {
			i = s.find_first_of(chardata.whitespace);

			keycode = s.substr(0, i);
			s.erase(0, i);
			string t(keycode);
			to_uppercase(t);

			if (t.length() == 0) {
				cerr << "Keybinder: parse error (empty key token) in line " << lineNumber << ": " << line << endl;
				return;
			} else if (t.length() == 1) {
				// translate 1-letter keys straight to SDL_Keycode
				auto c = static_cast<unsigned char>(t[0]);
				if (std::isgraph(c) && c != '%' && c != '{' && c != '|' && c != '}' && c != '~') {
					c     = std::tolower(c);    // need lowercase
					k.key = static_cast<SDL_Keycode>(c);
				} else {
					cerr << "Keybinder: unsupported key '" << keycode << "' in line " << lineNumber << ": " << line << endl;
					return;
				}
			} else {
				// lookup in table
				auto key_index = keys.find(t);
				if (key_index != keys.end()) {
					k.key = key_index->second;
				} else {
					cerr << "Keybinder: unsupported key '" << keycode << "' in line " << lineNumber << ": " << line << endl;
					return;
				}
			}
		}
	}

	if (k.key == SDLK_UNKNOWN) {
		cerr << "Keybinder: parse error (unknown key symbol) in line " << lineNumber << ": " << line << endl;
		return;
	}

	// get function
	skipspace(s);

	i               = s.find_first_of(chardata.whitespace);
	string t_action = s.substr(0, i);
	s.erase(0, i);
	to_uppercase(t_action);

	if (t_action.empty() && i == string::npos && s.empty()) {    // Check if action string is genuinely missing
		cerr << "Keybinder: parse error (missing action) in line " << lineNumber << ": " << line << endl;
		return;
	}

	auto action_index = actions.find(t_action);
	if (action_index != actions.end()) {
		a.action = action_index->second;
	} else {
		cerr << "Keybinder: unsupported action '" << t_action << "' in line " << lineNumber << ": " << line << endl;
		return;
	}

	// get params
	skipspace(s);

	int np = 0;
	// Special case.
	if (!strcmp(a.action->s, "CALL_USECODE")) {
		// Want to allow function name.
		if (s.length() && s[0] != '#') {
			i                        = s.find_first_of(chardata.whitespace);
			const string param_token = s.substr(0, i);
			s.erase(0, i);
			skipspace(s);

			int p = atoi(param_token.c_str());
			if (!p && param_token != "0") {
				// No conversion? Try as function name.
				Usecode_machine* usecode = Game_window::get_instance()->get_usecode();
				p                        = usecode->find_function(param_token.c_str());
				if (p == -1 && !param_token.empty()) {
					cerr << "Keybinder: warning: CALL_USECODE function '" << param_token << "' not found (line " << lineNumber
						 << ": " << line << ")" << endl;
				}
			}
			a.params[np++] = p;
		}
	}
	while (s.length() && s[0] != '#' && np < c_maxparams) {
		i                        = s.find_first_of(chardata.whitespace);
		const string param_token = s.substr(0, i);
		s.erase(0, i);
		skipspace(s);
		if (param_token.empty()) {
			continue;    // Skip if empty param string (e.g. multiple spaces)
		}

		const int p    = atoi(param_token.c_str());
		a.params[np++] = p;
	}

	// read optional help comment
	if (s.length() >= 1 && s[0] == '#') {
		if (s.length() >= 2 && s[1] == '-') {    // don't show in help
			show = false;
		} else {
			s.erase(0, 1);
			skipspace(s);
			// Always show if there is a comment.
			show = true;
		}
	} else {
		// Action::dont_show doesn't have default display names, so do not
		// show them if they don't have a comment.
		s    = a.action->desc();
		show = a.action->key_type != Action::dont_show;
	}

	if (show) {
		desc = "";
		if (k.mod & SDL_KMOD_CTRL) {
			desc += "Ctrl-";
		}
#ifdef MACOSX
		if (k.mod & SDL_KMOD_ALT) {
			desc += "Cmd-";
		}
#else
		if (k.mod & SDL_KMOD_ALT) {
			desc += "Alt-";
		}
#endif
		if (k.mod & SDL_KMOD_SHIFT) {
			desc += "Shift-";
		}
		desc += keycode;
		if (!strcmp(a.action->s, "CREATE_ITEM") && a.params[0] == -1) {
			last_created_key.push_back(desc);
		}

		desc += " - " + s;

		// add to help list
		if (a.action->key_type == Action::cheat_keys) {
			cheathelp.push_back(desc);
		} else if (a.action->key_type == Action::mapedit_keys) {
			mapedithelp.push_back(std::move(desc));
		} else {    // Either Action::normal_keys or Action::dont_show with
					// comment.
			keyhelp.push_back(desc);
		}
	}

	// bind key
	AddKeyBinding(k.key, k.mod, a.action, np, a.params);
}

void KeyBinder::LoadFromFileInternal(const char* filename) {
	auto pKeyfile = U7open_in(filename, true);
	if (!pKeyfile) {
		return;
	}
	auto& keyfile = *pKeyfile;
	char  temp[1024];        // 1024 should be long enough
	int   lineNumber = 0;    // Initialize line number
	while (!keyfile.eof()) {
		lineNumber++;    // Increment for each line attempt
		keyfile.getline(temp, 1024);
		// Check if getline failed (and not just EOF)
		if (keyfile.fail() && !keyfile.eof()) {    // Genuine read error
			cerr << "Keybinder: file read error on line " << lineNumber << " of " << filename << endl;
			return;
		}
		ParseLine(temp, lineNumber);
	}
}

void KeyBinder::LoadFromFile(const char* filename) {
	Flush();

	cout << "Loading keybindings from file " << filename << endl;
	LoadFromFileInternal(filename);
}

void KeyBinder::LoadFromPatch() {
	if (U7exists(PATCH_KEYS)) {
		cout << "Loading patch keybindings" << endl;
		LoadFromFileInternal(PATCH_KEYS);
	}
}

void KeyBinder::LoadDefaults() {
	Flush();

	cout << "Loading default keybindings" << endl;

	const str_int_pair& resource = game->get_resource("config/defaultkeys");

	const U7object txtobj(resource.str, resource.num);
	size_t         len;
	auto           txt = txtobj.retrieve(len, true);
	if (txt && len > 0) {
		ParseText(reinterpret_cast<char*>(txt.get()), len);
	}
}

// codes used in keybindings-files. (use uppercase here)
void KeyBinder::FillParseMaps() {
	int i;    // For MSVC
	for (i = 0; strlen(SDLKeyStringTable[i].s) > 0; i++) {
		keys[SDLKeyStringTable[i].s] = SDLKeyStringTable[i].k;
	}

	for (i = 0; strlen(ExultActions[i].s) > 0; i++) {
		actions[ExultActions[i].s] = &(ExultActions[i]);
	}
}
