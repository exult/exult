/*
 *  Copyright (C) 2001-2022  The Exult Team
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

#ifndef CONVERSATION_H
#define CONVERSATION_H

#include "rect.h"
#include "shapeid.h"
#include "singles.h"

#include <array>
#include <deque>
#include <memory>
#include <string>
#include <vector>

class Actor;
class Mui_dialogue;
class Npc_face_info;
class Usecode_value;
class Game_window;

class Conversation : public Game_singletons, public Paintable {
public:
	Conversation();
	~Conversation() override;

private:
	std::array<Npc_face_info*, 2> face_info{nullptr, nullptr};    // NPC's on-screen faces in convers.
	int                           num_faces       = 0;
	int                           last_face_shown = 0;               // Index of last npc face shown.
	TileRect                      avatar_face     = {0, 0, 0, 0};    // Area take by Avatar in conversation.
	TileRect*                     conv_choices    = nullptr;         // Choices during a conversation.
	int                           conv_layer      = -1;              // Overlay layer with faces/text.
	int                           conv_bg_layer   = -1;              // Overlay layer with translucent text backgrounds.
	bool                          choices_active  = false;           // Avatar choices are being shown.

	std::vector<std::string>             answers;
	std::deque<std::vector<std::string>> answer_stack;
	// Themed view (null in the classic style), resolved once per session.
	std::unique_ptr<Mui_dialogue> view;
	std::string                   style_name       = "classic";
	bool                          session_resolved = false;
	int                           usecode_depth    = 0;

public:
	inline int get_num_answers() const {
		return answers.size();
	}

	inline int get_num_faces_on_screen() const {
		return num_faces;
	}

	void init_faces();
	void show_face(int shape, int frame, int slot = -1, Actor* speaker = nullptr);
	void change_face_frame(int frame, int slot);
	void remove_face(int shape);
	void remove_slot_face(int slot);    // SI.
	void remove_last_face();            // SI.
	void show_npc_message(const char* msg);
	bool is_npc_text_pending();
	void clear_text_pending();
	void show_avatar_choices();
	void clear_avatar_choices();
	int  conversation_choice(int x, int y);

	void set_slot(int i) {
		last_face_shown = i;    // SI.
	}

	void paint() override;    // Paint entire conversation.
	void paint_faces(bool text = false);

	void add_answer(Usecode_value& val);
	void remove_answer(Usecode_value& val);
	void clear_answers();
	int  locate_answer(const char* str);

	const char* get_answer(int num) {
		return answers[num].c_str();
	}

	void push_answers();
	void pop_answers();

	bool stack_empty() const {
		return answer_stack.empty();
	}

	static bool noface;

	// Themed conversations: true while the session uses the themed view.
	bool is_themed() const {
		return view != nullptr;
	}

	Mui_dialogue* get_view() const {
		return view.get();
	}

	// Around each outermost usecode call; the session ends at depth 0.
	void enter_usecode();
	void leave_usecode();
	// Themed wait for a click / an answer (index, or -2 for Escape).
	void themed_continue();
	int  themed_choose();

	// Test support (menu test driver): the answer chosen last.
	std::string last_choice;
	void        note_choice(int index);

	// What the test driver can check about the conversation.
	struct Conv_debug {
		std::string style;          // "classic" or the theme name.
		int         active = -1;    // Slot of the face that spoke last.

		struct Slot {
			int  shape    = -1;
			int  frame    = -1;
			int  npc_num  = -1;
			bool used     = false;
			bool drawable = false;    // Has a face frame and is not hidden.
		};

		Slot                     slots[2];
		std::string              page;    // Text shown for the active face.
		std::string              wait;    // "choice", "continue" or "none".
		std::vector<std::string> answers;
		std::string              last_choice;
	};

	void describe(Conv_debug& out) const;
	// Write the conv and convbg layer buffers: "EXCV", then per layer a
	// uint32 width and height and the index bytes (0x0 if missing).
	bool dump_layers(const char* path) const;

private:
	void     set_face_rect(Npc_face_info* info, Npc_face_info* prev);
	TileRect get_conv_rect() const;
	// Create the overlay layer for faces/text; -1 if unavailable.
	int get_conv_layer();
	// Create the overlay layer for conversation text backgrounds; -1 if unavailable.
	int get_conv_bg_layer();
	// Place the layer over a fixed 320x200 rectangle centered in the game area.
	void position_conv_layer(int layer);
	// Repaint the conversation into the overlay layer.
	void repaint_conversation();
	// Rebuild the overlay layer from the current faces, text and choices.
	void render_conv_layer();
	// Draw the Avatar's face and answer choices into the current target.
	void build_avatar_choices();
	void show_avatar_choices(int num_choices, char** choices);
	void add_answer(const char* str);
	void remove_answer(const char* str);
	// Resolve the style at the first face, text or choice of a session.
	void begin_session_if_needed();
	void end_session();
	// Push faces and speaker names to the themed view.
	void sync_view();
};

#endif
