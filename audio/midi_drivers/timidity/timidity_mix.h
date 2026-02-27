/*

	TiMidity -- Experimental MIDI to WAVE converter
	Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

	In case you haven't heard, this program is free software;
	you can redistribute it and/or modify it under the terms of the
	GNU General Public License as published by the Free Software
	Foundation; either version 2 of the License, or (at your option)
	any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	mix.h

*/

#ifdef USE_TIMIDITY_MIDI

#	ifndef TIMIDITY_MIX_H_INCLUDED
#		define TIMIDITY_MIX_H_INCLUDED

#		include "timidity.h"

#		ifdef NS_TIMIDITY
namespace NS_TIMIDITY {
#		endif

	extern void mix_voice(sint32* buf, int v, sint32 c);
	extern int  recompute_envelope(int v);
	extern void apply_envelope_to_amp(int v);

#		ifdef NS_TIMIDITY
}
#		endif

#	endif

#endif    // USE_TIMIDITY_MIDI
