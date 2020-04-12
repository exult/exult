/*
Copyright (C) 2000 The Exult Team

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

#ifndef SPELLBOOK_GUMP_H
#define SPELLBOOK_GUMP_H

#include "Gump.h"

class Spellbook_object;
class Bookmark_button;

template <typename T>
int sign(T val) {
	return (T(0) < val) - (val < T(0));
}

/*
 *  Abstract base class for spellbook, spell-scrolls:
 */
class Spelltype_gump : public Gump {
public:
	Spelltype_gump(int shnum) : Gump(nullptr, shnum) {  }
	// Perform spell.
	virtual void do_spell(int spell) = 0;
	// Set bookmark.
	virtual void select_spell(int spell) = 0;
};

/*
 *  Open spellbook.  The spells are drawn in the object area.
 */
class Spellbook_gump : public Spelltype_gump {
	int page;           // Starting with 0 (= circle #).
	int turning_page;       // 1 if turning forward, -1 backward,
	//   0 if not turning.
	int turning_frame;      // Next frame to show.
	short avail[9 * 8];     // For each spell, # which the
	//   available reagents make possible.
	Spellbook_object *book;     // Book this shows.
	Game_object *book_owner;    // Top-owner of book.
	// Page turners:
	Gump_button *leftpage, *rightpage;
	Bookmark_button *bookmark;
	Gump_button *spells[9 * 8]; // ->spell 'buttons'.
	int spwidth, spheight;      // Dimensions of a spell shape.
	void set_avail();       // Set up counts.
public:
	friend class Bookmark_button;
	Spellbook_gump(Spellbook_object *b);
	~Spellbook_gump() override;
	void do_spell(int spell) override;   // Perform spell.
	void change_page(int delta);    // Page forward/backward.
	void select_spell(int spell) override;   // Set bookmark.
	Game_object *get_owner() override;// Get object this belongs to.
	// Is a given point on a button?
	Gump_button *on_button(int mx, int my) override;
	// Paint button.
	void paint_button(Gump_button *btn);
	// Paint it and its contents.
	void paint() override;
	bool handle_kbd_event(void *ev) override;
};

/*
 *  Open spell-scroll in Serpent Isle.
 */
class Spellscroll_gump : public Spelltype_gump {
	Game_object *scroll;        // Scroll clicked on.
	Gump_button *spell;
	int spwidth, spheight;      // Dimensions of a spell shape.
public:
	Spellscroll_gump(Game_object *s);
	~Spellscroll_gump() override;
	void do_spell(int spell) override;   // Perform spell.
	void select_spell(int) override
	{  }
	Game_object *get_owner() override;// Get object this belongs to.
	// Is a given point on a button?
	Gump_button *on_button(int mx, int my) override;
	// Paint button.
	void paint_button(Gump_button *btn);
	// Paint it and its contents.
	void paint() override;
};

#endif
