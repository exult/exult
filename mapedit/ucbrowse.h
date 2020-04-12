/**
 ** Ucbrowse.h - Browse usecode functions.
 **
 ** Written: Nov. 19, 2006 - JSF
 **/

/*
Copyright (C) 2001-2006 The Exult Team

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

#ifndef INCL_UCBROWSE
#define INCL_UCBROWSE

#include <string>
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wparentheses"
#if !defined(__llvm__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wuseless-cast"
#else
#pragma GCC diagnostic ignored "-Wunneeded-internal-declaration"
#endif
#endif  // __GNUC__
#include <gtk/gtk.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__
/*
 *  The 'Usecode browser' window:
 */
class Usecode_browser {
	GtkWidget *win;         // Main window.
	GtkWidget *tree;        // The tree-view.
	std::string choice;     // Set when window is closed.
	GtkTreeStore *model;
public:
	Usecode_browser();
	~Usecode_browser();
	GtkWidget *get_win() {
		return win;
	}
	void show(bool tf);     // Show/hide.
	// Configure when created/resized.
	const char *get_choice() const {
		return choice.c_str();
	}
	void okay();
	void cancel() {
		choice = "";
		show(false);
	}
	void setup_list();
};

#endif
