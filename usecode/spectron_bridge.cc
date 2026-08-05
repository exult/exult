/*
 * Spectron sidecar bridge — async HTTP POST of conversation/book events.
 *
 * Identity model:
 *   - npc_id is the durable key (schedule slot).
 *   - appears_as is the shape/role label ("a guard"), not the schedule name.
 *   - Personal names belong only in talk_line text after the Avatar learns them.
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "spectron_bridge.h"

#include "actors.h"
#include "chunks.h"
#include "contain.h"
#include "dir.h"
#include "effects.h"
#include "gameclk.h"
#include "gamemap.h"
#include "gamewin.h"
#include "mouse.h"
#include "objiter.h"
#include "objs.h"
#include "tiles.h"

#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <set>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace Spectron_bridge {
namespace {

std::atomic<bool> talk_active{false};
int               current_npc_id = -1;
std::string       current_appears_as;
std::string       book_buf;
std::mutex        log_mu;

// Situation snapshots (Avatar foot / barge stretches + optional sextant).
// Heading and distance use signed net tile displacement so opposite steps
// cancel (30 east + 20 west → 10 east). Force-emit every TRAVEL_FORCE_STEPS
// during a continuous hike; otherwise flush on life events (talk, death,
// relocate, …) so a walk-then-talk stretch is one mental footnote. Mode is
// "foot", "cart", "ship", or "carpet" — switching mode flushes the prior stretch.
// One JSON shape (`type:situation`) carries seen / moved / optional sextant.
constexpr int TRAVEL_FORCE_STEPS      = 50;
constexpr int TRAVEL_FLUSH_MIN_STEPS  = 3;
// Glance radius and soft cap for organic "what's around me" on each flush.
constexpr int IN_VIEW_ACTOR_RADIUS    = 12;
constexpr int IN_VIEW_OBJECT_RADIUS   = 10;
constexpr int IN_VIEW_MAX_NAMES       = 8;
int           travel_step_count       = 0;
int           travel_short            = 0;
int           travel_medium           = 0;
int           travel_long             = 0;
int           travel_net_dx           = 0;
int           travel_net_dy           = 0;
std::string   travel_mode             = "foot";
// Latest cloth-map reading on this open stretch (replaced if read again).
bool          pending_sextant         = false;
int           pending_sextant_lat     = 0;
int           pending_sextant_lon     = 0;
std::string   pending_sextant_ns;
std::string   pending_sextant_ew;

void post_json(std::string body);
void flush_pending_situation(const char* trigger);
std::string appears_as_of(Actor* npc);
std::string json_escape(const char* s);

void ensure_travel_mode(const char* mode) {
	const char* m = (mode && *mode) ? mode : "foot";
	if (travel_mode != m) {
		flush_pending_situation("mode_change");
		travel_mode = m;
	}
}

const char* heading_label(int dir16) {
	static const char* labels[] = {
			"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
	if (dir16 < 0 || dir16 > 15) {
		return "unknown";
	}
	return labels[dir16];
}

const char* time_of_day_label(int hour) {
	if (hour >= 5 && hour <= 7) {
		return "dawn";
	}
	if (hour >= 19 && hour <= 21) {
		return "dusk";
	}
	if (hour < 6 || hour > 20) {
		return "night";
	}
	return "day";
}

const char* weather_label(int weather) {
	switch (weather) {
	case 0:
		return "clear";
	case 1:
		return "snow";
	case 2:
		return "rain";
	default:
		return "other";
	}
}

const char* setting_label(bool in_dungeon) {
	return in_dungeon ? "dungeon" : "outdoors";
}

std::string ambient_json() {
	Game_window* gwin = Game_window::get_instance();
	if (!gwin) {
		return "\"ambient\":{\"time_of_day\":\"day\",\"weather\":\"clear\",\"setting\":\"outdoors\"}";
	}
	int hour = 6;
	if (Game_clock* clk = gwin->get_clock()) {
		hour = clk->get_hour();
	}
	const int weather = gwin->get_effects() ? gwin->get_effects()->get_weather() : 0;
	const bool dungeon = gwin->is_in_dungeon() != 0;
	std::ostringstream out;
	out << "\"ambient\":{\"time_of_day\":\"" << time_of_day_label(hour) << "\",\"weather\":\""
		<< weather_label(weather) << "\",\"setting\":\"" << setting_label(dungeon) << "\"}";
	return out.str();
}

// Map mouse arrow speed onto short=1 / medium=2 / long=3.
// Mouse stores delay ticks: short≈base/100, medium≈base/200 (or /150 in
// combat), long≈base/400. Smaller avatar_speed = longer stride.
int classify_step_weight() {
	Game_window* gwin = Game_window::get_instance();
	if (!gwin) {
		return 2;
	}
	const int base = 200 * gwin->get_std_delay();
	if (base <= 0) {
		return 2;
	}
	Mouse* m = Mouse::mouse();
	if (!m) {
		return 2;
	}
	const int speed = m->avatar_speed;
	// Midpoints between Exult's Avatar_Speed_Factors.
	if (speed >= base / 125) {
		return 1;    // short arrow
	}
	if (speed <= base / 300) {
		return 3;    // long arrow
	}
	return 2;    // medium (and medium-combat)
}

// Minimum 8-way king-moves from origin to net (dx, dy). Matches U7 diagonals
// (15 east + 15 north ≈ 15 NE tiles, not 30).
int net_tile_distance(int dx, int dy) {
	const int ax = dx < 0 ? -dx : dx;
	const int ay = dy < 0 ? -dy : dy;
	return ax > ay ? ax : ay;
}

void reset_situation_accum() {
	travel_step_count = 0;
	travel_short = travel_medium = travel_long = 0;
	travel_net_dx = travel_net_dy = 0;
	pending_sextant = false;
	pending_sextant_lat = pending_sextant_lon = 0;
	pending_sextant_ns.clear();
	pending_sextant_ew.clear();
}

// Strip leading "N " quantity prefixes so "3 gold" and "gold" dedupe as one glance.
std::string strip_quantity_prefix(std::string name) {
	size_t i = 0;
	while (i < name.size() && std::isdigit(static_cast<unsigned char>(name[i]))) {
		++i;
	}
	if (i > 0 && i < name.size() && name[i] == ' ') {
		return name.substr(i + 1);
	}
	return name;
}

std::string lowercase_copy(const std::string& s) {
	std::string out;
	out.reserve(s.size());
	for (unsigned char c : s) {
		out.push_back(static_cast<char>(std::tolower(c)));
	}
	return out;
}

// Salience: people/creatures first, then containers, buildings, scenery, loose goods.
int in_view_priority(const Shape_info& info) {
	using SC = Shape_info::Shape_class;
	switch (info.get_shape_class()) {
	case SC::human:
	case SC::monster:
		return 0;
	case SC::container:
		return 1;
	case SC::building:
		return 2;
	case SC::unusable:
		return 3;
	case SC::has_hp:
	case SC::quality:
	case SC::quality_flags:
	case SC::quantity:
		return 4;
	default:
		return 9;    // skip eggs, barges, spellbooks, etc.
	}
}

bool in_view_class_wanted(const Shape_info& info) {
	return in_view_priority(info) < 9;
}

// Exult's inside/outside test: a tile is "inside" when a roof sits above it
// (is_roof < 31). Same heuristic as Map_chunk::Find_spot_where / Check_spot.
// Used so outdoor walks do not ingest furniture through house walls.
bool tile_is_inside(const Tile_coord& t) {
	Game_window* gwin = Game_window::get_instance();
	Game_map*    map  = gwin ? gwin->get_map() : nullptr;
	if (!map) {
		return false;
	}
	Map_chunk* chunk = map->get_chunk_safely(
			t.tx / c_tiles_per_chunk, t.ty / c_tiles_per_chunk);
	if (!chunk) {
		return false;
	}
	return chunk->is_roof(
				   t.tx % c_tiles_per_chunk, t.ty % c_tiles_per_chunk, t.tz)
		   < 31;
}

// Sample player-facing names near the Avatar for an organic glance (not a tile dump).
// Emits the JSON array value for the `seen` field (no key).
std::string seen_array_json() {
	Game_window* gwin = Game_window::get_instance();
	Actor*       avatar = gwin ? gwin->get_main_actor() : nullptr;
	if (!avatar) {
		return "[]";
	}
	const Tile_coord here          = avatar->get_tile();
	const bool       avatar_inside = tile_is_inside(here);

	struct Candidate {
		int         priority;
		int         dist;
		std::string name;
	};
	std::vector<Candidate> candidates;
	std::set<std::string>  seen_keys;

	auto consider = [&](Game_object* obj, int priority_override) {
		if (!obj || obj == avatar) {
			return;
		}
		if (Actor* act = obj->as_actor()) {
			if (act->is_in_party() || act->is_dead()) {
				return;
			}
		}
		const Tile_coord ot = obj->get_tile();
		// Same enclosure as the Avatar (Exult roof test). U7's isometric view
		// stacks storeys in one glance, so do not filter by lift/floor — only
		// drop the other side of a wall (outdoor furniture while inside, and
		// indoor clutter while outdoors).
		if (tile_is_inside(ot) != avatar_inside) {
			return;
		}
		const Shape_info& info = obj->get_info();
		int               pri  = priority_override;
		if (pri < 0) {
			if (!in_view_class_wanted(info)) {
				return;
			}
			pri = in_view_priority(info);
		}
		std::string name;
		if (Actor* act = obj->as_actor()) {
			name = appears_as_of(act);
		} else {
			name = strip_quantity_prefix(obj->get_name());
		}
		if (name.empty() || name == "someone") {
			return;
		}
		const std::string key = lowercase_copy(name);
		if (!seen_keys.insert(key).second) {
			return;
		}
		candidates.push_back(Candidate{pri, ot.distance(here), std::move(name)});
	};

	// Living NPCs / creatures within earshot-ish radius.
	Actor_vector actors;
	avatar->find_nearby_actors(actors, c_any_shapenum, IN_VIEW_ACTOR_RADIUS, 0x08);
	for (Actor* npc : actors) {
		consider(npc, 0);
	}

	// World objects (Normal mask skips eggs/barges/transparent clutter).
	Game_object_vector objs;
	avatar->find_nearby(objs, c_any_shapenum, IN_VIEW_OBJECT_RADIUS, 0);
	for (Game_object* obj : objs) {
		// Actors already considered with appears_as (not schedule names).
		if (obj->as_actor()) {
			continue;
		}
		consider(obj, -1);
	}

	std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
		if (a.priority != b.priority) {
			return a.priority < b.priority;
		}
		if (a.dist != b.dist) {
			return a.dist < b.dist;
		}
		return a.name < b.name;
	});

	std::ostringstream out;
	out << '[';
	const int n = std::min(static_cast<int>(candidates.size()), IN_VIEW_MAX_NAMES);
	for (int i = 0; i < n; ++i) {
		if (i > 0) {
			out << ',';
		}
		out << '"' << json_escape(candidates[i].name.c_str()) << '"';
	}
	out << ']';
	return out.str();
}

void emit_situation(const char* trigger) {
	const char* trig = (trigger && *trigger) ? trigger : "cadence";
	const bool  has_move = travel_step_count > 0;
	const bool  has_fix = pending_sextant;
	if (!has_move && !has_fix && std::strcmp(trig, "relocated") != 0) {
		return;
	}

	const int distance = net_tile_distance(travel_net_dx, travel_net_dy);
	std::string heading;
	std::string mode = travel_mode;
	if (std::strcmp(trig, "relocated") == 0) {
		mode = "teleport";
		heading = "sudden";
	} else if (!has_move) {
		heading = "none";
	} else if (travel_net_dx == 0 && travel_net_dy == 0) {
		heading = "circling";
	} else {
		// Tile Y grows south (screen-down). Get_direction16 wants cartesian
		// Y-up, so negate dy (same transform as effects.cc).
		heading = heading_label(Get_direction16(-travel_net_dy, travel_net_dx));
	}

	std::ostringstream body;
	body << "{\"type\":\"situation\",\"trigger\":\"" << trig << "\",\"seen\":" << seen_array_json()
		 << ",\"mode\":\"" << mode << "\",\"steps\":" << travel_step_count
		 << ",\"distance\":" << distance << ",\"heading\":\"" << heading << '"';
	if (has_fix) {
		body << ",\"sextant\":{\"latitude\":" << pending_sextant_lat << ",\"latitude_hemi\":\""
			 << pending_sextant_ns << "\",\"longitude\":" << pending_sextant_lon
			 << ",\"longitude_hemi\":\"" << pending_sextant_ew << "\"}";
	}
	body << ',' << ambient_json() << '}';
	post_json(body.str());
	reset_situation_accum();
}

// End the current open stretch so a walk-then-talk (etc.) is one footnote.
// Tiny trembles are discarded unless a sextant reading is pending.
void flush_pending_situation(const char* trigger) {
	if (travel_step_count >= TRAVEL_FLUSH_MIN_STEPS || pending_sextant) {
		emit_situation(trigger);
	} else if (travel_step_count > 0) {
		reset_situation_accum();
	}
}

void record_avatar_step(const Tile_coord& from, const Tile_coord& to) {
	ensure_travel_mode("foot");
	const int dx = Tile_coord::delta(from.tx, to.tx);
	const int dy = Tile_coord::delta(from.ty, to.ty);
	if (dx != 0 || dy != 0) {
		travel_net_dx += dx;
		travel_net_dy += dy;
	}
	switch (classify_step_weight()) {
	case 1:
		++travel_short;
		break;
	case 3:
		++travel_long;
		break;
	default:
		++travel_medium;
		break;
	}
	++travel_step_count;
	if (travel_step_count >= TRAVEL_FORCE_STEPS) {
		emit_situation("cadence");
	}
}

void record_barge_step(const Tile_coord& from, const Tile_coord& to, const char* mode) {
	ensure_travel_mode(mode);
	const int dx = Tile_coord::delta(from.tx, to.tx);
	const int dy = Tile_coord::delta(from.ty, to.ty);
	if (dx != 0 || dy != 0) {
		travel_net_dx += dx;
		travel_net_dy += dy;
	}
	// No mouse-driven stride on vehicles — count every barge tile as medium.
	++travel_medium;
	++travel_step_count;
	if (travel_step_count >= TRAVEL_FORCE_STEPS) {
		emit_situation("cadence");
	}
}

bool looks_like_time_reading(const char* text) {
	if (!text || !*text) {
		return false;
	}
	std::string lower;
	lower.reserve(std::strlen(text));
	for (const char* p = text; *p; ++p) {
		lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
	}
	if (lower.find("o'clock") != std::string::npos || lower.find("oclock") != std::string::npos) {
		return true;
	}
	if (lower == "noon" || lower.find("noon") != std::string::npos) {
		// Avoid matching random dialogue that merely mentions noon: require short bark.
		return lower.size() <= 12;
	}
	if (lower == "midnight" || (lower.find("midnight") != std::string::npos && lower.size() <= 16)) {
		return true;
	}
	// SI 24h desk clocks: "9:05" / "14:05" (optionally wrapped in @…@).
	for (size_t i = 0; i + 3 < lower.size(); ++i) {
		if (!std::isdigit(static_cast<unsigned char>(lower[i]))) {
			continue;
		}
		size_t j = i;
		while (j < lower.size() && std::isdigit(static_cast<unsigned char>(lower[j]))) {
			++j;
		}
		if (j < lower.size() && lower[j] == ':' && j + 1 < lower.size()
			&& std::isdigit(static_cast<unsigned char>(lower[j + 1]))) {
			return true;
		}
	}
	return false;
}

std::string strip_time_bark(const char* text) {
	std::string s = text ? text : "";
	// Usecode Avatar barks often wrap as @…@.
	while (!s.empty() && (s.front() == '@' || s.front() == ' ' || s.front() == '^')) {
		s.erase(s.begin());
	}
	while (!s.empty() && (s.back() == '@' || s.back() == ' ')) {
		s.pop_back();
	}
	return s;
}

std::string timepiece_source_label(Game_object* obj) {
	if (!obj) {
		return "timepiece";
	}
	if (Actor* act = obj->as_actor()) {
		if (act == Game_window::get_instance()->get_main_actor()) {
			return "timepiece";
		}
	}
	const int sh = obj->get_shapenum();
	// Black Gate / SI item shapes (docs/bgitems.txt, docs/siitems.txt).
	if (sh == 0x9f || sh == 0x547) {
		return "pocketwatch";
	}
	if (sh == 0x11c) {
		return "sundial";
	}
	if (sh == 0x2a3) {
		return "desk clock";
	}
	if (sh == 0x347 || sh == 0x5bf) {
		return "hourglass";
	}
	return "timepiece";
}

bool enabled() {
	const char* env = std::getenv("SPECTRON_BRIDGE");
	if (env && (std::strcmp(env, "0") == 0 || std::strcmp(env, "false") == 0)) {
		return false;
	}
	return true;
}

bool debug_on() {
	const char* env = std::getenv("SPECTRON_BRIDGE_DEBUG");
	return env && *env && std::strcmp(env, "0") != 0;
}

void debug_log(const std::string& msg) {
	if (!debug_on()) {
		return;
	}
	std::lock_guard<std::mutex> lock(log_mu);
	std::fprintf(stderr, "[spectron_bridge] %s\n", msg.c_str());
	std::ofstream out("/tmp/spectron_bridge.log", std::ios::app);
	if (out) {
		out << msg << '\n';
	}
}

int endpoint_port() {
	const char* env = std::getenv("SPECTRON_SIDECAR_PORT");
	if (!env || !*env) {
		return 8765;
	}
	const int p = std::atoi(env);
	return p > 0 ? p : 8765;
}

std::string json_escape(const char* s) {
	std::string out;
	if (!s) {
		return out;
	}
	out.reserve(std::strlen(s) + 8);
	for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p) {
		switch (*p) {
		case '\\':
			out += "\\\\";
			break;
		case '"':
			out += "\\\"";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		default:
			if (*p < 0x20) {
				char buf[8];
				std::snprintf(buf, sizeof(buf), "\\u%04x", *p);
				out += buf;
			} else {
				out += static_cast<char>(*p);
			}
			break;
		}
	}
	return out;
}

void post_json(std::string body) {
	if (!enabled()) {
		return;
	}
	debug_log(std::string("POST body=") + body);
	std::thread([body = std::move(body)]() {
		const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0) {
			debug_log(std::string("socket failed: ") + std::strerror(errno));
			return;
		}
		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port   = htons(static_cast<uint16_t>(endpoint_port()));
		::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

		struct timeval tv;
		tv.tv_sec  = 0;
		tv.tv_usec = 400000;    // 400ms
		::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

		if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
			debug_log(std::string("connect failed: ") + std::strerror(errno));
			::close(fd);
			return;
		}

		std::ostringstream req;
		req << "POST /event HTTP/1.1\r\n"
			<< "Host: 127.0.0.1:" << endpoint_port() << "\r\n"
			<< "Content-Type: application/json\r\n"
			<< "Content-Length: " << body.size() << "\r\n"
			<< "Connection: close\r\n"
			<< "\r\n"
			<< body;
		const auto payload = req.str();
		size_t         sent = 0;
		bool           fail = false;
		while (sent < payload.size()) {
			const auto n = ::send(fd, payload.data() + sent, payload.size() - sent, 0);
			if (n < 0) {
				debug_log(std::string("send failed: ") + std::strerror(errno));
				fail = true;
				break;
			}
			sent += static_cast<size_t>(n);
		}
		if (!fail) {
			// Wait for the HTTP status. Closing right after send (with or without
			// shutdown(SHUT_WR)) loses the request on macOS before axum reads it.
			// A single recv keeps the connection open long enough for 202/400.
			char        resp[256];
			const auto  rn = ::recv(fd, resp, sizeof(resp) - 1, 0);
			std::string status = "no-response";
			if (rn > 0) {
				resp[rn] = '\0';
				// "HTTP/1.1 202 Accepted"
				const char* sp = std::strchr(resp, ' ');
				if (sp) {
					status.assign(sp + 1, std::strcspn(sp + 1, "\r\n"));
				}
			} else if (rn < 0) {
				status = std::string("recv-error:") + std::strerror(errno);
			}
			debug_log(
				std::string("send ok bytes=") + std::to_string(sent) + " http=" + status);
		}
		::close(fd);
	}).detach();
}

// Shape/role label as shown before the Avatar learns a personal name.
// Actor::get_name() switches to the schedule name once `met` is set (which BG
// does as soon as the face appears), so call the Game_object version.
std::string appears_as_of(Actor* npc) {
	if (!npc) {
		return "someone";
	}
	std::string n = npc->Game_object::get_name();
	if (n.empty()) {
		n = "someone";
	}
	return n;
}

Actor* resolve_npc(Actor* npc, int face_shape) {
	if (npc) {
		return npc;
	}
	if (face_shape < 0) {
		return nullptr;
	}
	Game_window* gwin = Game_window::get_instance();
	if (!gwin) {
		return nullptr;
	}
	return gwin->get_npc(face_shape);
}

std::string talk_common_json() {
	std::ostringstream common;
	common << "\"npc_id\":" << current_npc_id << ",\"appears_as\":\""
		   << json_escape(current_appears_as.c_str()) << "\"";
	return common.str();
}

void emit_talk_start() {
	std::ostringstream body;
	body << "{\"type\":\"talk_start\"," << talk_common_json() << ',' << ambient_json() << '}';
	post_json(body.str());
}

void begin_talk(Actor* npc, int face_shape) {
	flush_pending_situation("talk");
	Actor* resolved = resolve_npc(npc, face_shape);
	int    id       = -1;
	if (resolved) {
		id = resolved->get_npc_num();
	} else if (face_shape >= 0) {
		id = face_shape;
	} else {
		debug_log("talk_start skipped: no npc and no face_shape");
		return;
	}

	if (talk_active.load() && current_npc_id == id) {
		return;
	}

	current_npc_id     = id;
	current_appears_as = resolved ? appears_as_of(resolved) : "someone";
	talk_active.store(true);
	emit_talk_start();
}

void ensure_talk_active() {
	if (talk_active.load()) {
		return;
	}
	flush_pending_situation("talk");
	current_npc_id     = -1;
	current_appears_as = "someone";
	talk_active.store(true);
	emit_talk_start();
}

}    // namespace

void talk_start(Actor* npc, int face_shape) {
	if (!enabled()) {
		return;
	}
	begin_talk(npc, face_shape);
}

void talk_line(const char* /*speaker*/, const char* text) {
	if (!enabled() || !text || !*text) {
		return;
	}
	ensure_talk_active();
	// No speaker name: schedule names leak personal identity. The line text is
	// the source of truth (including "You see…" and "My name is…").
	std::ostringstream body;
	body << "{\"type\":\"talk_line\"," << talk_common_json() << ",\"text\":\""
		 << json_escape(text) << "\"}";
	post_json(body.str());
}

void talk_choice(const char* choice) {
	if (!enabled() || !choice) {
		return;
	}
	ensure_talk_active();
	std::ostringstream body;
	body << "{\"type\":\"talk_choice\"," << talk_common_json() << ",\"choice\":\""
		 << json_escape(choice) << "\"}";
	post_json(body.str());
}

void talk_end() {
	if (!enabled() || !talk_active.load()) {
		return;
	}
	std::ostringstream body;
	body << "{\"type\":\"talk_end\"," << talk_common_json() << ',' << ambient_json() << '}';
	post_json(body.str());
	talk_active.store(false);
	current_npc_id = -1;
	current_appears_as.clear();
	// Footnote any micro-walk during the talk, then start a fresh stretch.
	flush_pending_situation("talk_end");
}

void book_append(const char* text) {
	if (!enabled() || !text || !*text) {
		return;
	}
	if (!book_buf.empty()) {
		book_buf.push_back('\n');
	}
	book_buf += text;
}

void book_close() {
	if (!enabled() || book_buf.empty()) {
		book_buf.clear();
		return;
	}
	flush_pending_situation("book");
	std::ostringstream body;
	body << "{\"type\":\"book_read\",\"title\":null,\"text\":\"" << json_escape(book_buf.c_str())
		 << "\"}";
	post_json(body.str());
	book_buf.clear();
}

std::string join_lines(const std::vector<std::string>& lines) {
	std::ostringstream out;
	for (size_t i = 0; i < lines.size(); ++i) {
		if (i) {
			out << '\n';
		}
		out << lines[i];
	}
	return out.str();
}

// Append a BMP code point as UTF-8.
void append_utf8(std::string& out, unsigned int cp) {
	if (cp < 0x80) {
		out += static_cast<char>(cp);
	} else if (cp < 0x800) {
		out += static_cast<char>(0xC0 | (cp >> 6));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	} else {
		out += static_cast<char>(0xE0 | (cp >> 12));
		out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	}
}

// Phonetic Britannian → Unicode Runic (same table as spectron-sidecar/src/runes.rs).
unsigned int letter_rune_cp(unsigned char ch) {
	switch (std::toupper(ch)) {
	case 'A':
		return 0x16A8;
	case 'B':
		return 0x16D2;
	case 'C':
		return 0x16B3;
	case 'D':
		return 0x16DE;
	case 'E':
		return 0x16D6;
	case 'F':
		return 0x16A0;
	case 'G':
		return 0x16B7;
	case 'H':
		return 0x16BB;
	case 'I':
		return 0x16C1;
	case 'J':
		return 0x16C3;
	case 'K':
		return 0x16B2;
	case 'L':
		return 0x16DA;
	case 'M':
		return 0x16D7;
	case 'N':
		return 0x16BE;
	case 'O':
		return 0x16DF;
	case 'P':
		return 0x16C8;
	case 'Q':
		return 0x16E9;
	case 'R':
		return 0x16B1;
	case 'S':
		return 0x16CB;
	case 'T':
		return 0x16CF;
	case 'U':
		return 0x16A2;
	case 'V':
		return 0x16A1;
	case 'W':
		return 0x16B9;
	case 'X':
		return 0x16EA;
	case 'Y':
		return 0x16A3;
	case 'Z':
		return 0x16E3;
	default:
		return 0;
	}
}

std::string raw_encoding_to_unicode(const std::string& raw) {
	std::string out;
	out.reserve(raw.size() * 3);
	for (const unsigned char ch : raw) {
		switch (ch) {
		case '(':    // TH
			append_utf8(out, 0x16A6);
			break;
		case ')':    // EE
			append_utf8(out, 0x16C7);
			break;
		case '*':    // NG
			append_utf8(out, 0x16DC);
			break;
		case '+':    // EA
			append_utf8(out, 0x16E0);
			break;
		case ',':    // ST
			append_utf8(out, 0x16E5);
			break;
		case '|':
		case ' ':
			out += ' ';
			break;
		case '\n':
			out += '\n';
			break;
		default:
			if (std::isalpha(ch)) {
				if (const unsigned int cp = letter_rune_cp(ch)) {
					append_utf8(out, cp);
				}
			} else if (std::isdigit(ch) || ch == '.' || ch == '-' || ch == '\'') {
				out += static_cast<char>(ch);
			}
			break;
		}
	}
	return out;
}

void sign_read(
		const char* gump_kind, const char* script, const std::vector<std::string>& lines_raw,
		const std::vector<std::string>& lines_latin) {
	if (!enabled() || lines_latin.empty()) {
		return;
	}
	const std::string raw   = join_lines(lines_raw);
	const std::string latin = join_lines(lines_latin);
	if (latin.empty()) {
		return;
	}
	flush_pending_situation("sign");
	const std::string runes = raw_encoding_to_unicode(raw);
	std::ostringstream body;
	body << "{\"type\":\"sign_read\",\"gump\":\"" << json_escape(gump_kind ? gump_kind : "other")
		 << "\",\"script\":\"" << json_escape(script ? script : "unknown") << "\",\"text_raw\":\""
		 << json_escape(raw.c_str()) << "\",\"text_latin\":\"" << json_escape(latin.c_str())
		 << "\",\"text_runes\":\"" << json_escape(runes.c_str()) << "\"," << ambient_json() << '}';
	post_json(body.str());
}

void examine(Game_object* obj, const char* text) {
	if (!enabled() || !text || !*text) {
		return;
	}
	flush_pending_situation("examine");
	int shape = -1;
	int frame = -1;
	int qty   = 1;
	int npc   = -1;
	if (obj) {
		shape = obj->get_shapenum();
		frame = obj->get_framenum();
		qty   = obj->get_quantity();
		if (Actor* act = obj->as_actor()) {
			npc = act->get_npc_num();
		}
	}
	std::ostringstream body;
	body << "{\"type\":\"examine\",\"text\":\"" << json_escape(text) << "\",\"shape\":" << shape
		 << ",\"frame\":" << frame << ",\"quantity\":" << qty << ",\"npc_id\":" << npc << '}';
	post_json(body.str());
}

bool container_is_party_owned(Container_game_object* cont) {
	Game_window* gwin = Game_window::get_instance();
	if (!gwin || !cont) {
		return false;
	}
	Game_object*             top   = cont;
	Container_game_object*   owner = cont->get_owner();
	while (owner) {
		top   = owner;
		owner = owner->get_owner();
	}
	if (Actor* act = top->as_actor()) {
		return act == gwin->get_main_actor() || act->is_in_party();
	}
	return false;
}

void container_opened(Container_game_object* cont) {
	if (!enabled() || !cont) {
		return;
	}
	// Avatar / companion packs are opened constantly — not “found in the world”.
	if (container_is_party_owned(cont)) {
		return;
	}

	flush_pending_situation("container");

	struct Agg {
		std::string name;
		int         quantity = 0;
		int         shape    = -1;
	};
	// Aggregate identical stackables for a readable inventory list.
	std::map<std::string, Agg> by_key;
	Game_object*               obj = nullptr;
	Object_iterator            next(cont->get_objects());
	while ((obj = next.get_next()) != nullptr) {
		std::string name = obj->get_name();
		if (name.empty()) {
			name = "something";
		}
		const int shape = obj->get_shapenum();
		const int qty   = obj->get_quantity() > 0 ? obj->get_quantity() : 1;
		std::ostringstream key;
		key << shape << '\t' << name;
		Agg& slot = by_key[key.str()];
		slot.name = name;
		slot.shape = shape;
		slot.quantity += qty;
	}

	std::ostringstream fingerprint;
	fingerprint << cont->get_shapenum() << '@' << cont->get_tile().tx << ',' << cont->get_tile().ty
				<< ',' << cont->get_tile().tz << '|';
	for (const auto& entry : by_key) {
		fingerprint << entry.second.shape << ':' << entry.second.quantity << ':' << entry.second.name
					<< ';';
	}

	static std::string last_fingerprint;
	static std::time_t last_at = 0;
	const std::time_t  now     = std::time(nullptr);
	if (fingerprint.str() == last_fingerprint && last_at != 0 && now - last_at < 8) {
		return;
	}
	last_fingerprint = fingerprint.str();
	last_at          = now;

	const std::string container_name = cont->get_name().empty() ? "container" : cont->get_name();
	std::ostringstream body;
	body << "{\"type\":\"container_opened\",\"container\":\"" << json_escape(container_name.c_str())
		 << "\",\"shape\":" << cont->get_shapenum() << ",\"empty\":" << (by_key.empty() ? "true" : "false")
		 << ",\"items\":[";
	bool first = true;
	int  listed = 0;
	constexpr int kMaxItems = 48;
	for (const auto& entry : by_key) {
		if (listed >= kMaxItems) {
			break;
		}
		if (!first) {
			body << ',';
		}
		first = false;
		body << "{\"name\":\"" << json_escape(entry.second.name.c_str()) << "\",\"quantity\":"
			 << entry.second.quantity << ",\"shape\":" << entry.second.shape << '}';
		++listed;
	}
	body << "]," << ambient_json() << '}';
	post_json(body.str());
}

void combat_changed(bool in_combat) {
	if (!enabled()) {
		return;
	}
	flush_pending_situation("combat");
	std::ostringstream body;
	body << "{\"type\":\"" << (in_combat ? "combat_start" : "combat_end") << "\"," << ambient_json() << '}';
	post_json(body.str());
}

void sleep_changed(bool sleeping) {
	if (!enabled()) {
		return;
	}
	flush_pending_situation("sleep");
	std::ostringstream body;
	body << "{\"type\":\"" << (sleeping ? "sleep_start" : "sleep_end") << "\"," << ambient_json() << '}';
	post_json(body.str());
}

void dungeon_changed(bool in_dungeon) {
	if (!enabled()) {
		return;
	}
	flush_pending_situation("dungeon");
	std::ostringstream body;
	body << "{\"type\":\"" << (in_dungeon ? "dungeon_enter" : "dungeon_leave") << "\"," << ambient_json() << '}';
	post_json(body.str());
}

void avatar_step(const Tile_coord& from, const Tile_coord& to) {
	if (!enabled()) {
		return;
	}
	record_avatar_step(from, to);
}

void barge_step(const Tile_coord& from, const Tile_coord& to, const char* mode) {
	if (!enabled()) {
		return;
	}
	record_barge_step(from, to, mode);
}

void flush_travel_before_relocate() {
	if (!enabled()) {
		return;
	}
	// Keep any walk-to-the-gate stretch; do not mix it with post-arrival steps.
	flush_pending_situation("pre_relocate");
}

void party_relocated(const Tile_coord& from, const Tile_coord& to) {
	if (!enabled()) {
		return;
	}
	// Hard reset: never continue a walk snapshot across a sudden place-change.
	reset_situation_accum();
	const int distance = from.distance(to);
	std::ostringstream body;
	body << "{\"type\":\"situation\",\"trigger\":\"relocated\",\"via\":\"teleport\",\"seen\":"
		 << seen_array_json() << ",\"mode\":\"teleport\",\"steps\":0,\"distance\":" << distance
		 << ",\"heading\":\"sudden\"," << ambient_json() << '}';
	post_json(body.str());
	reset_situation_accum();
}

void avatar_died() {
	if (!enabled()) {
		return;
	}
	flush_pending_situation("death");
	std::ostringstream body;
	body << "{\"type\":\"death\"," << ambient_json() << '}';
	post_json(body.str());
}

void sextant_reading(int latitude, const char* ns, int longitude, const char* ew) {
	if (!enabled()) {
		return;
	}
	const char* ns_s = (ns && *ns) ? ns : "?";
	const char* ew_s = (ew && *ew) ? ew : "?";
	// Attach to the open stretch (replace if already set); flush so the reading
	// is not lost if the Avatar stands still afterward.
	pending_sextant = true;
	pending_sextant_lat = latitude;
	pending_sextant_lon = longitude;
	pending_sextant_ns = ns_s;
	pending_sextant_ew = ew_s;
	emit_situation("sextant");
}

void time_reading(Game_object* source_obj, const char* text) {
	if (!enabled() || !text || !*text) {
		return;
	}
	if (!looks_like_time_reading(text)) {
		return;
	}
	flush_pending_situation("time");
	const std::string cleaned = strip_time_bark(text);
	if (cleaned.empty()) {
		return;
	}
	const std::string source = timepiece_source_label(source_obj);
	int hour   = -1;
	int minute = -1;
	if (Game_window* gwin = Game_window::get_instance()) {
		if (Game_clock* clk = gwin->get_clock()) {
			hour   = clk->get_hour();
			minute = clk->get_minute();
		}
	}
	std::ostringstream body;
	body << "{\"type\":\"time_reading\",\"text\":\"" << json_escape(cleaned.c_str())
		 << "\",\"source\":\"" << json_escape(source.c_str()) << "\",\"hour\":" << hour
		 << ",\"minute\":" << minute << ',' << ambient_json() << '}';
	post_json(body.str());
}

void companion_changed(Actor* npc, bool joined) {
	if (!enabled() || !npc) {
		return;
	}
	flush_pending_situation("companion");
	const int         npc_id = npc->get_npc_num();
	const std::string name   = npc->get_name();
	const std::string appears
			= !name.empty() ? name : (joined ? "a companion" : "a former companion");
	std::ostringstream body;
	body << "{\"type\":\"" << (joined ? "companion_join" : "companion_leave") << "\",\"npc_id\":"
		 << npc_id << ",\"appears_as\":\"" << json_escape(appears.c_str()) << "\"," << ambient_json()
		 << '}';
	post_json(body.str());
}

void party_knocked_out(Actor* npc) {
	if (!enabled() || !npc) {
		return;
	}
	flush_pending_situation("knock_out");
	Game_window*    gwin      = Game_window::get_instance();
	const bool      is_avatar = (gwin && npc == gwin->get_main_actor()) || npc->get_npc_num() == 0;
	const int       npc_id    = npc->get_npc_num();
	const std::string name    = npc->get_name();
	const std::string appears
			= is_avatar ? "the Avatar" : (!name.empty() ? name : "a companion");
	std::ostringstream body;
	body << "{\"type\":\"party_knocked_out\",\"npc_id\":" << npc_id << ",\"appears_as\":\""
		 << json_escape(appears.c_str()) << "\",\"is_avatar\":" << (is_avatar ? "true" : "false")
		 << ',' << ambient_json() << '}';
	post_json(body.str());
}

void companion_died(Actor* npc) {
	if (!enabled() || !npc) {
		return;
	}
	// Avatar true-death uses avatar_died / type "death".
	if (npc->get_npc_num() == 0) {
		return;
	}
	if (Game_window* gwin = Game_window::get_instance()) {
		if (npc == gwin->get_main_actor()) {
			return;
		}
	}
	flush_pending_situation("companion_died");
	const int         npc_id = npc->get_npc_num();
	const std::string name   = npc->get_name();
	const std::string appears = !name.empty() ? name : "a companion";
	std::ostringstream body;
	body << "{\"type\":\"companion_died\",\"npc_id\":" << npc_id << ",\"appears_as\":\""
		 << json_escape(appears.c_str()) << "\"," << ambient_json() << '}';
	post_json(body.str());
}

}    // namespace Spectron_bridge
