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

	tables.h
*/

#ifdef USE_TIMIDITY_MIDI

#	ifndef TIMIDITY_TABLES_H_INCLUDED
#		define TIMIDITY_TABLES_H_INCLUDED

#		include "timidity.h"

#		include <cmath>

#		ifdef NS_TIMIDITY
namespace NS_TIMIDITY {
#		endif

#		ifdef LOOKUP_SINE
	extern float sine(int x);
#		else
#			define sine(x) (std::sin((2 * PI / 1024.0) * (x)))
#		endif

#		define SINE_CYCLE_LENGTH 1024
	extern sint32 freq_table[];
	extern double vol_table[];
	extern double bend_fine[];
	extern double bend_coarse[];
	extern uint8* _l2u;    /* 13-bit PCM to 8-bit u-law */
	extern uint8  _l2u_[]; /* used in LOOKUP_HACK */
#        ifdef LOOKUP_HACK
	extern sint16  _u2l[];
	extern sint32* mixup;
#			ifdef LOOKUP_INTERPOLATION
	extern sint8* iplookup;
#			endif
#		endif

	extern void init_tables();

#		ifdef NS_TIMIDITY
}
#		endif

#	endif

#endif    // USE_TIMIDITY_MIDI
