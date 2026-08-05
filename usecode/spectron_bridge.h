/*
 * Spectron sidecar bridge — fire-and-forget JSON events to the local Rust UI.
 * Disable with SPECTRON_BRIDGE=0. Default URL: http://127.0.0.1:8765/event
 * Set SPECTRON_BRIDGE_DEBUG=1 for stderr + /tmp/spectron_bridge.log.
 */

#ifndef SPECTRON_BRIDGE_H
#define SPECTRON_BRIDGE_H

#include <string>
#include <vector>

class Actor;
class Container_game_object;
class Game_object;
class Tile_coord;

namespace Spectron_bridge {

// Call once conversation with an NPC is identifiable (first face).
// face_shape is the conversation face id when Actor* cannot be resolved
// (common for Black Gate show_npc_face integer args).
void talk_start(Actor* npc, int face_shape = -1);

// Spoken line shown in the conversation UI.
void talk_line(const char* speaker, const char* text);

// Player clicked a conversation choice (e.g. "Bye").
void talk_choice(const char* choice);

// Conversation closed.
void talk_end();

// Book/scroll text the player just opened (may be called per chunk).
void book_append(const char* text);

// Book/scroll display finished.
void book_close();

// Sign / plaque / tombstone (display_runes). script is "runic", "latin", or
// "serpentine". text_raw is the usecode encoding; text_latin is always the
// readable transliteration (TH/EE/NG ligatures expanded).
void sign_read(
		const char* gump_kind, const char* script, const std::vector<std::string>& lines_raw,
		const std::vector<std::string>& lines_latin);

// Single-click examine name ("a guard", "gold", …).
void examine(Game_object* obj, const char* text);

// Player opened a world container gump (chest, bag, corpse, …) and can see
// its top-level contents. Party / Avatar inventory opens are ignored.
void container_opened(Container_game_object* cont);

// Combat mode toggled (Avatar party).
void combat_changed(bool in_combat);

// Avatar entered or left sleep schedule.
void sleep_changed(bool sleeping);

// Avatar entered or left dungeon lighting (underground).
void dungeon_changed(bool in_dungeon);

// Successful Main_actor tile step (for situation / travel quantisation).
void avatar_step(const Tile_coord& from, const Tile_coord& to);

// Successful barge (cart / ship / magic carpet) tile step while the Avatar is
// operating that barge. mode is "cart", "ship", or "carpet".
void barge_step(const Tile_coord& from, const Tile_coord& to, const char* mode);

// Sudden party relocation (moongate / teleport egg, usecode jail move,
// virtue stone, cheat teleport, …). Call after the Avatar has arrived.
// Discards any open travel stretch and emits a glance at the new place.
// Discards any open walk stretch and emits a situation glance at the new place.
void party_relocated(const Tile_coord& from, const Tile_coord& to);

// Flush any open situation stretch at the *current* Avatar tile (call just before
// a sudden relocate so the walk to the gate is not discarded or blended).
void flush_travel_before_relocate();

// Avatar died (Main_actor::die — before usecode resurrection path).
void avatar_died();

// Cloth-map sextant reading (only when the Avatar opens the world map outdoors
// with a sextant). Attached to the open situation stretch and flushed with it
// (trigger=sextant). Magnitudes are non-negative; hemi is "N"/"S" and "E"/"W".
void sextant_reading(int latitude, const char* ns, int longitude, const char* ew);

// Watch / sundial / desk-clock bark ("10 o'clock", "Noon", "14:05", …).
// source_obj is the item when known; may be the Avatar when they speak the time.
void time_reading(Game_object* source_obj, const char* text);

// NPC joined or left the Avatar's party.
void companion_changed(Actor* npc, bool joined);

// Avatar or party companion reached HP ≤ 0 without dying (lies down / "knocked out"
// until they recover if combat leaves them alone).
void party_knocked_out(Actor* npc);

// Party companion died for real (Actor::die — body left, party dead-list).
// Avatar true death remains `avatar_died` / event type `death`.
void companion_died(Actor* npc);

}    // namespace Spectron_bridge

#endif
