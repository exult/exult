/*
 *  Copyright (C) 2001  The Exult Team
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
#include "singles.h"
#include "shapeid.h"
#include <deque>
#include <vector>
#include <string>

class Npc_face_info;
class Usecode_value;
class Game_window;

class Conversation : public Game_singletons, public Paintable {
public:
	~Conversation() override;

private:
	Npc_face_info *face_info[2] = {nullptr, nullptr};    // NPC's on-screen faces in convers.
	int num_faces = 0;
	int last_face_shown = 0;        // Index of last npc face shown.
	Rectangle avatar_face = {0, 0, 0, 0};      // Area take by Avatar in conversation.
	Rectangle *conv_choices = nullptr;    // Choices during a conversation.

	std::vector<std::string> answers;
	std::deque< std::vector<std::string> > answer_stack;

public:
	inline int get_num_answers() const {
		return answers.size();
	}
	inline int get_num_faces_on_screen() const {
		return num_faces;
	}
	void init_faces();
	void show_face(int shape, int frame, int slot = -1);
	void change_face_frame(int frame, int slot);
	void remove_face(int shape);
	void remove_slot_face(int slot); // SI.
	void remove_last_face();    // SI.
	void show_npc_message(const char *msg);
	bool is_npc_text_pending();
	void clear_text_pending();
	void show_avatar_choices();
	void clear_avatar_choices();
	int conversation_choice(int x, int y);
	void set_slot(int i) {
		last_face_shown = i;    // SI.
	}
	void paint() override;           // Paint entire conversation.
	void paint_faces(bool text = false);

	void add_answer(Usecode_value &val);
	void remove_answer(Usecode_value &val);
	void clear_answers();
	int locate_answer(const char *str);
	const char *get_answer(int num) {
		return answers[num].c_str();
	}

	void push_answers();
	void pop_answers();
	bool stack_empty() const {
		return answer_stack.empty();
	}

private:
	void set_face_rect(Npc_face_info *info, Npc_face_info *prev, int screenw, int screenh);
	void show_avatar_choices(int num_choices, char **choices);
	void add_answer(const char *str);
	void remove_answer(const char *str);
};

#endif
