/**
 ** Execbox.h - Execute a command-line program and display its output.
 **
 ** Written: 10/02/02 - JSF
 **/

#ifndef INCL_EXECBOX_H
#define INCL_EXECBOX_H  1

/*
Copyright (C) 2002 The Exult Team

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

class Exec_box;
class Exec_process;
// Called when child is done:
using Exec_done_fun = void (*)(int exit_code, Exec_box *box,
                               gpointer user_data);


#ifndef _WIN32

/*
 *  A class for executing a child process and capturing its output in a
 *  GTK program.
 */
class Exec_process {
public:
	// Function called when data is read
	//   from child.  If datalen == 0,
	//   child is done & exit_code is set.
	using Reader_fun = void (*)(char *data, int datalen, int exit_code,
	                            gpointer user_data);
private:
	// Pipes for talking to child:
	int child_stdin, child_stdout, child_stderr;
	int child_pid;          // Child's process ID.
	gint stdout_tag, stderr_tag;    // GDK tags for getting child's output.
	Reader_fun reader;      // Called when data read from child.
	void *reader_data;      // User data passed back.
public:
	Exec_process();
	~Exec_process();
	void kill_child();      // Kill process.
	void read_from_child(int id);   // Read and call 'reader'.
	// Execute.
	bool exec(const char *file, const char *argv[], Reader_fun rfun,
	          void *udata);
	bool check_child(int &exit_code);   // Is child still running?
};

#else
#include "ignore_unused_variable_warning.h"

class Exec_process {
public:
	using Reader_fun = void (*)(char *data, int datalen, int exit_code,
	                            gpointer user_data);
	void kill_child() {}
	void read_from_child(int id) {
		ignore_unused_variable_warning(id);
	}
	bool exec(const char *file, const char *argv[], Reader_fun rfun,
	          void *udata) {
		ignore_unused_variable_warning(file, argv, rfun, udata);
		return false;
	}
	bool check_child(int &exit_code) {
		ignore_unused_variable_warning(exit_code);
		return false;
	}
};

#endif

/*
 *  This class can execute a command-line program and display its output.
 */
class Exec_box {
	Exec_process *executor;     // Handles child process.
	GtkTextView *box;       // Where we show responses.
	GtkStatusbar *status;       // For showing status.
	guint status_ctx;       // Context for status.
	Exec_done_fun done_fun;     // Called when child has exited.
	gpointer user_data;     // Passed to done_fun.
public:
	Exec_box(GtkTextView *b, GtkStatusbar *s, Exec_done_fun dfun = nullptr,
	         gpointer udata = nullptr);
	~Exec_box();
	void show_status(const char *msg);  // Set status bar.
	// Handle data from child.
	void read_from_child(char *data, int datalen, int exit_code);
	void add_message(const char *txt);  // Add text to box.
	// Execute.
	bool exec(const char *file, const char *argv[]);
	void kill_child();      // End.
};

#endif
