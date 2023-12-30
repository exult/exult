/*

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    sdl_c.c
    Minimal control mode -- no interaction, just stores messages.
    */

#include "pent_include.h"

#ifdef USE_TIMIDITY_MIDI

#include <cstdio>
#include <cstdlib>
#include <cstdarg>

#include "timidity.h"
#include "timidity_common.h"
#include "timidity_output.h"
#include "timidity_controls.h"
#include "timidity_instrum.h"
#include "timidity_playmidi.h"
#include "ignore_unused_variable_warning.h"

#ifdef NS_TIMIDITY
namespace NS_TIMIDITY {
#endif

#ifndef ATTR_PRINTF
#ifdef __GNUC__
#define ATTR_PRINTF(x,y) __attribute__((format(printf, (x), (y))))
#else
#define ATTR_PRINTF(x,y)
#endif
#endif

static void ctl_refresh();
static void ctl_total_time(int tt);
static void ctl_master_volume(int mv);
static void ctl_file_name(char *name);
static void ctl_current_time(int ct);
static void ctl_note(int v);
static void ctl_program(int ch, int val);
static void ctl_volume(int channel, int val);
static void ctl_expression(int channel, int val);
static void ctl_panning(int channel, int val);
static void ctl_sustain(int channel, int val);
static void ctl_pitch_bend(int channel, int val);
static void ctl_reset();
static int ctl_open(int using_stdin, int using_stdout);
static void ctl_close();
static int ctl_read(sint32 *valp);
static int cmsg(int type, int verbosity_level, const char *fmt, ...) ATTR_PRINTF(3,4);

/**********************************/
/* export the interface functions */

#define ctl sdl_control_mode

ControlMode ctl=
{
	"SDL interface", 's',
	OF_NORMAL,0,0,
	ctl_open,nullptr, ctl_close, ctl_read, cmsg,
	ctl_refresh, ctl_reset, ctl_file_name, ctl_total_time, ctl_current_time,
	ctl_note,
	ctl_master_volume, ctl_program, ctl_volume,
	ctl_expression, ctl_panning, ctl_sustain, ctl_pitch_bend
};

static int ctl_open(int using_stdin, int using_stdout)
{
	ignore_unused_variable_warning(using_stdin, using_stdout);
	ctl.opened=1;
	return 0;
}

static void ctl_close()
{
	ctl.opened=0;
}

static int ctl_read(sint32 *valp)
{
	ignore_unused_variable_warning(valp);
	return TM_RC_NONE;
}

static int cmsg(int type, int verbosity_level, const char *fmt, ...)
{
	va_list ap;
	if ((type==CMSG_TEXT || type==CMSG_INFO || type==CMSG_WARNING) &&
	    ctl.verbosity<verbosity_level)
		return 0;
	va_start(ap, fmt);
	vsnprintf(timidity_error, TIMIDITY_ERROR_SIZE, fmt, ap);
	va_end(ap);
	//perr.printf ("%s\n", timidity_error);
	return 0;
}

static void ctl_refresh() { }

static void ctl_total_time(int tt) {
	ignore_unused_variable_warning(tt);
}

static void ctl_master_volume(int mv) {
	ignore_unused_variable_warning(mv);
}

static void ctl_file_name(char *name) {
	ignore_unused_variable_warning(name);
}

static void ctl_current_time(int ct) {
	ignore_unused_variable_warning(ct);
}

static void ctl_note(int v) {
	ignore_unused_variable_warning(v);
}

static void ctl_program(int ch, int val) {
	ignore_unused_variable_warning(ch, val);
}

static void ctl_volume(int channel, int val) {
	ignore_unused_variable_warning(channel, val);
}

static void ctl_expression(int channel, int val) {
	ignore_unused_variable_warning(channel, val);
}

static void ctl_panning(int channel, int val) {
	ignore_unused_variable_warning(channel, val);
}

static void ctl_sustain(int channel, int val) {
	ignore_unused_variable_warning(channel, val);
}

static void ctl_pitch_bend(int channel, int val) {
	ignore_unused_variable_warning(channel, val);
}

static void ctl_reset() {}

#ifdef NS_TIMIDITY
}
#endif

#endif //USE_TIMIDITY_MIDI
