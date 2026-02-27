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

	output.h

*/

#ifdef USE_TIMIDITY_MIDI

#	ifndef TIMIDITY_OUTPUT_H_INCLUDED
#		define TIMIDITY_OUTPUT_H_INCLUDED

#		include "timidity.h"

#		ifdef NS_TIMIDITY
namespace NS_TIMIDITY {
#		endif

	struct PlayMode {
		sint32      rate, encoding;
		const char* id_name;
	};

	extern PlayMode *play_mode_list[], *play_mode;
	extern int       init_buffers(int kbytes);

	/* Conversion functions -- These overwrite the sint32 data in *lp with
	   data in another format */

	/* The size of the output buffers */
	extern int AUDIO_BUFFER_SIZE;

	/* Actual copy function */
	extern void (*s32tobuf)(void* dp, sint32* lp, sint32 c);

	/* 8-bit signed and unsigned*/
	extern void s32tos8(void* dp, sint32* lp, sint32 c);
	extern void s32tou8(void* dp, sint32* lp, sint32 c);

	/* 16-bit */
	extern void s32tos16(void* dp, sint32* lp, sint32 c);
	extern void s32tou16(void* dp, sint32* lp, sint32 c);

	/* byte-exchanged 16-bit */
	extern void s32tos16x(void* dp, sint32* lp, sint32 c);
	extern void s32tou16x(void* dp, sint32* lp, sint32 c);

	/* uLaw (8 bits) */
	extern void s32toulaw(void* dp, sint32* lp, sint32 c);

/* little-endian and big-endian specific */
#		ifdef TIMIDITY_LITTLE_ENDIAN
#			define s32tou16l s32tou16
#			define s32tou16b s32tou16x
#			define s32tos16l s32tos16
#			define s32tos16b s32tos16x
#		else
#			define s32tou16l s32tou16x
#			define s32tou16b s32tou16
#			define s32tos16l s32tos16x
#			define s32tos16b s32tos16
#		endif

#		ifdef NS_TIMIDITY
}
#		endif

#	endif

#endif    // USE_TIMIDITY_MIDI
