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

	resample.h
*/

#ifdef USE_TIMIDITY_MIDI

#	ifndef TIMIDITY_RESAMPLE_H_INCLUDED
#		define TIMIDITY_RESAMPLE_H_INCLUDED

#		include "timidity.h"

#		ifdef NS_TIMIDITY
namespace NS_TIMIDITY {
#		endif

	struct Sample;
	extern sample_t* resample_voice(int v, sint32* countptr);
	extern void      pre_resample(Sample* sp);

#		ifdef NS_TIMIDITY
}
#		endif

#	endif

#endif    // USE_TIMIDITY_MIDI
