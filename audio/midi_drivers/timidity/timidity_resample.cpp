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

    resample.c
*/

#include "pent_include.h"

#ifdef USE_TIMIDITY_MIDI

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "timidity.h"
#include "timidity_common.h"
#include "timidity_instrum.h"
#include "timidity_playmidi.h"
#include "timidity_output.h"
#include "timidity_controls.h"
#include "timidity_tables.h"
#include "timidity_resample.h"

#ifdef NS_TIMIDITY
namespace NS_TIMIDITY {
#endif

#ifdef LINEAR_INTERPOLATION
# if defined(LOOKUP_HACK) && defined(LOOKUP_INTERPOLATION)
#   define RESAMPLATION \
		v1=src[ofs>>FRACTION_BITS];\
		v2=src[(ofs>>FRACTION_BITS)+1];\
		*dest++ = static_cast<sample_t>(v1 + (iplookup[(((v2-v1)<<5) & 0x03FE0) | \
					((ofs & FRACTION_MASK) >> (FRACTION_BITS-5))]));
# else
#   define RESAMPLATION \
		v1=src[ofs>>FRACTION_BITS];\
		v2=src[(ofs>>FRACTION_BITS)+1];\
		*dest++ = static_cast<sample_t>(v1 + (((v2-v1) * (ofs & FRACTION_MASK)) >> FRACTION_BITS));
# endif
#  define INTERPVARS sample_t v1;	\
                     sample_t v2
#else
/* Earplugs recommended for maximum listening enjoyment */
#  define RESAMPLATION *dest++=src[ofs>>FRACTION_BITS];
#  define INTERPVARS
#endif

#define FINALINTERP if (ofs == le) *dest++=src[ofs>>FRACTION_BITS];
/* So it isn't interpolation. At least it's final. */

extern sample_t *resample_buffer;

/*************** resampling with fixed increment *****************/

static sample_t *rs_plain(int v, sint32 *countptr)
{

	/* Play sample until end, then free the voice. */

	INTERPVARS;
	Voice *vp=&voice[v];
	sample_t *dest=resample_buffer;
	sample_t *src=vp->sample->data;
	sint32 ofs=vp->sample_offset;
	sint32 incr=vp->sample_increment;
	sint32 le=vp->sample->data_length;
	sint32 count=*countptr;

#ifdef PRECALC_LOOPS
	sint32 i;

	if (incr<0) incr = -incr; /* In case we're coming out of a bidir loop */

	/* Precalc how many times we should go through the loop.
	 NOTE: Assumes that incr > 0 and that ofs <= le */
	i = (le - ofs) / incr + 1;

	if (i > count)
	{
		i = count;
		count = 0;
	} 
	else count -= i;

	while (i--) 
	{
		RESAMPLATION;
		ofs += incr;
	}

	if (ofs >= le) 
	{
		FINALINTERP;
		vp->status=VOICE_FREE;
		ctl->note(v);
		*countptr-=count+1;
	}

#else /* PRECALC_LOOPS */
	while (count--)
	{
		RESAMPLATION;
		ofs += incr;
		if (ofs >= le)
		{
			FINALINTERP;
			vp->status=VOICE_FREE;
			ctl->note(v);
			*countptr-=count+1;
			break;
		}
	}
#endif /* PRECALC_LOOPS */

	vp->sample_offset=ofs; /* Update offset */
	return resample_buffer;
}

static sample_t *rs_loop(Voice *vp, sint32 count)
{

	/* Play sample until end-of-loop, skip back and continue. */

	INTERPVARS;
	sint32 ofs=vp->sample_offset;
	sint32 incr=vp->sample_increment;
	sint32 le=vp->sample->loop_end;
	sint32 ll=le - vp->sample->loop_start;
	sample_t *dest=resample_buffer;
	sample_t *src=vp->sample->data;

#ifdef PRECALC_LOOPS
	sint32 i;

	while (count) 
	{
		if (ofs >= le)
			/* NOTE: Assumes that ll > incr and that incr > 0. */
			ofs -= ll;
		/* Precalc how many times we should go through the loop */
		i = (le - ofs) / incr + 1;
		if (i > count) 
		{
			i = count;
			count = 0;
		} 
		else count -= i;
		while (i--) 
		{
			RESAMPLATION;
			ofs += incr;
		}
	}
#else
	while (count--)
	{
		RESAMPLATION;
		ofs += incr;
		if (ofs>=le)
			ofs -= ll; /* Hopefully the loop is longer than an increment. */
	}
#endif

	vp->sample_offset=ofs; /* Update offset */
	return resample_buffer;
}

static sample_t *rs_bidir(Voice *vp, sint32 count)
{
	INTERPVARS;
	sint32 ofs=vp->sample_offset;
	sint32 incr=vp->sample_increment;
	sint32 le=vp->sample->loop_end;
	sint32 ls=vp->sample->loop_start;
	sample_t *dest=resample_buffer;
	sample_t *src=vp->sample->data;

#ifdef PRECALC_LOOPS
	sint32 le2 = le<<1;
	sint32 ls2 = ls<<1;
	sint32 i;
	/* Play normally until inside the loop region */

	if (ofs <= ls) 
	{
		/* NOTE: Assumes that incr > 0, which is NOT always the case
		 when doing bidirectional looping.  I have yet to see a case
		 where both ofs <= ls AND incr < 0, however. */
		i = (ls - ofs) / incr + 1;
		if (i > count) 
		{
			i = count;
			count = 0;
		} 
		else count -= i;
		while (i--) 
		{
			RESAMPLATION;
			ofs += incr;
		}
	}

	/* Then do the bidirectional looping */

	while(count) 
	{
		/* Precalc how many times we should go through the loop */
		i = ((incr > 0 ? le : ls) - ofs) / incr + 1;
		if (i > count) 
		{
			i = count;
			count = 0;
		} 
		else count -= i;
		while (i--) 
		{
			RESAMPLATION;
			ofs += incr;
		}
		if (ofs>=le) 
		{
			/* fold the overshoot back in */
			ofs = le2 - ofs;
			incr *= -1;
		} 
		else if (ofs <= ls) 
		{
			ofs = ls2 - ofs;
			incr *= -1;
		}
	}

#else /* PRECALC_LOOPS */
	/* Play normally until inside the loop region */

	if (ofs < ls)
	{
		while (count--)
		{
			RESAMPLATION;
			ofs += incr;
			if (ofs>=ls)
				break;
		}
	}

	/* Then do the bidirectional looping */

	if (count>0)
		while (count--)
	{
		RESAMPLATION;
		ofs += incr;
		if (ofs>=le)
		{
			/* fold the overshoot back in */
			ofs = le - (ofs - le);
			incr = -incr;
		}
		else if (ofs <= ls)
		{
			ofs = ls + (ls - ofs);
			incr = -incr;
		}
	}  
#endif /* PRECALC_LOOPS */
	vp->sample_increment=incr;
	vp->sample_offset=ofs; /* Update offset */
	return resample_buffer;
}

/*********************** vibrato versions ***************************/

/* We only need to compute one half of the vibrato sine cycle */
static int vib_phase_to_inc_ptr(int phase)
{
	if (phase < VIBRATO_SAMPLE_INCREMENTS/2)
		return VIBRATO_SAMPLE_INCREMENTS/2-1-phase;
	else if (phase >= 3*VIBRATO_SAMPLE_INCREMENTS/2)
		return 5*VIBRATO_SAMPLE_INCREMENTS/2-1-phase;
	else
		return phase-VIBRATO_SAMPLE_INCREMENTS/2;
}

static sint32 update_vibrato(Voice *vp, int sign)
{
	sint32 depth;
	int phase;
	int pb;
	double a;

	if (vp->vibrato_phase++ >= 2*VIBRATO_SAMPLE_INCREMENTS-1)
		vp->vibrato_phase=0;
	phase=vib_phase_to_inc_ptr(vp->vibrato_phase);

	if (vp->vibrato_sample_increment[phase])
	{
		if (sign)
			return -vp->vibrato_sample_increment[phase];
		else
			return vp->vibrato_sample_increment[phase];
	}

	/* Need to compute this sample increment. */

	depth=vp->sample->vibrato_depth<<7;

	if (vp->vibrato_sweep)
	{
		/* Need to update sweep */
		vp->vibrato_sweep_position += vp->vibrato_sweep;
		if (vp->vibrato_sweep_position >= (1<<SWEEP_SHIFT))
			vp->vibrato_sweep=0;
		else
		{
			/* Adjust depth */
			depth *= vp->vibrato_sweep_position;
			depth >>= SWEEP_SHIFT;
		}
	}

	a = FSCALE((static_cast<double>(vp->sample->sample_rate) *
	            static_cast<double>(vp->frequency)) /
	           (static_cast<double>(vp->sample->root_freq) *
	            static_cast<double>(play_mode->rate)),
	           FRACTION_BITS);

	pb=static_cast<int>((sine(vp->vibrato_phase * 
	               (SINE_CYCLE_LENGTH/(2*VIBRATO_SAMPLE_INCREMENTS)))
	          * static_cast<double>(depth) * VIBRATO_AMPLITUDE_TUNING));

	if (pb<0)
	{
		pb=-pb;
		a /= bend_fine[(pb>>5) & 0xFF] * bend_coarse[pb>>13];
	}
	else
		a *= bend_fine[(pb>>5) & 0xFF] * bend_coarse[pb>>13];

	/* If the sweep's over, we can store the newly computed sample_increment */
	if (!vp->vibrato_sweep)
		vp->vibrato_sample_increment[phase]=static_cast<sint32>(a);

	if (sign)
		a = -a; /* need to preserve the loop direction */

	return static_cast<sint32>(a);
}

static sample_t *rs_vib_plain(int v, sint32 *countptr)
{

	/* Play sample until end, then free the voice. */

	INTERPVARS;
	Voice *vp=&voice[v];
	sample_t *dest=resample_buffer;
	sample_t *src=vp->sample->data;
	sint32 le=vp->sample->data_length;
	sint32 ofs=vp->sample_offset;
	sint32 incr=vp->sample_increment;
	sint32 count=*countptr;
	int cc=vp->vibrato_control_counter;

	/* This has never been tested */

	if (incr<0) incr = -incr; /* In case we're coming out of a bidir loop */

	while (count--)
	{
		if (!cc--)
		{
			cc=vp->vibrato_control_ratio;
			incr=update_vibrato(vp, 0);
		}
		RESAMPLATION;
		ofs += incr;
		if (ofs >= le)
		{
			FINALINTERP;
			vp->status=VOICE_FREE;
			ctl->note(v);
			*countptr-=count+1;
			break;
		}
	}

	vp->vibrato_control_counter=cc;
	vp->sample_increment=incr;
	vp->sample_offset=ofs; /* Update offset */
	return resample_buffer;
}

static sample_t *rs_vib_loop(Voice *vp, sint32 count)
{

	/* Play sample until end-of-loop, skip back and continue. */

	INTERPVARS;
	sint32 ofs=vp->sample_offset;
	sint32 incr=vp->sample_increment;
	sint32 le=vp->sample->loop_end;
	sint32 ll=le - vp->sample->loop_start;
	sample_t *dest=resample_buffer;
	sample_t *src=vp->sample->data;
	int cc=vp->vibrato_control_counter;

#ifdef PRECALC_LOOPS
	sint32 i;
	int
		vibflag=0;

	while (count) 
	{
		/* Hopefully the loop is longer than an increment */
		if(ofs >= le)
			ofs -= ll;
		/* Precalc how many times to go through the loop, taking
		 the vibrato control ratio into account this time. */
		i = (le - ofs) / incr + 1;
		if(i > count) i = count;
		if(i > cc)
		{
			i = cc;
			vibflag = 1;
		} 
		else cc -= i;
		count -= i;
		while(i--) 
		{
			RESAMPLATION;
			ofs += incr;
		}
		if(vibflag) 
		{
			cc = vp->vibrato_control_ratio;
			incr = update_vibrato(vp, 0);
			vibflag = 0;
		}
	}

#else /* PRECALC_LOOPS */
	while (count--)
	{
		if (!cc--)
		{
			cc=vp->vibrato_control_ratio;
			incr=update_vibrato(vp, 0);
		}
		RESAMPLATION;
		ofs += incr;
		if (ofs>=le)
			ofs -= ll; /* Hopefully the loop is longer than an increment. */
	}
#endif /* PRECALC_LOOPS */

	vp->vibrato_control_counter=cc;
	vp->sample_increment=incr;
	vp->sample_offset=ofs; /* Update offset */
	return resample_buffer;
}

static sample_t *rs_vib_bidir(Voice *vp, sint32 count)
{
	INTERPVARS;
	sint32 ofs=vp->sample_offset;
	sint32 incr=vp->sample_increment;
	sint32 le=vp->sample->loop_end;
	sint32 ls=vp->sample->loop_start;
	sample_t *dest=resample_buffer;
	sample_t *src=vp->sample->data;
	int cc=vp->vibrato_control_counter;

#ifdef PRECALC_LOOPS
	sint32 le2=le<<1;
	sint32 ls2=ls<<1;
	sint32 i;
	int vibflag = 0;

	/* Play normally until inside the loop region */
	while (count && (ofs <= ls)) 
	{
		i = (ls - ofs) / incr + 1;
		if (i > count) i = count;
		if (i > cc) 
		{
			i = cc;
			vibflag = 1;
		} 
		else cc -= i;
		count -= i;
		while (i--) 
		{
			RESAMPLATION;
			ofs += incr;
		}
		if (vibflag) 
		{
			cc = vp->vibrato_control_ratio;
			incr = update_vibrato(vp, 0);
			vibflag = 0;
		}
	}

	/* Then do the bidirectional looping */

	while (count) 
	{
		/* Precalc how many times we should go through the loop */
		i = ((incr > 0 ? le : ls) - ofs) / incr + 1;
		if(i > count) i = count;
		if(i > cc) 
		{
			i = cc;
			vibflag = 1;
		} 
		else cc -= i;
		count -= i;
		while (i--) 
		{
			RESAMPLATION;
			ofs += incr;
		}
		if (vibflag) 
		{
			cc = vp->vibrato_control_ratio;
			incr = update_vibrato(vp, (incr < 0));
			vibflag = 0;
		}
		if (ofs >= le) 
		{
			/* fold the overshoot back in */
			ofs = le2 - ofs;
			incr *= -1;
		} 
		else if (ofs <= ls) 
		{
			ofs = ls2 - ofs;
			incr *= -1;
		}
	}

#else /* PRECALC_LOOPS */
	/* Play normally until inside the loop region */

	if (ofs < ls)
	{
		while (count--)
		{
			if (!cc--)
			{
				cc=vp->vibrato_control_ratio;
				incr=update_vibrato(vp, 0);
			}
			RESAMPLATION;
			ofs += incr;
			if (ofs>=ls)
				break;
		}
	}

	/* Then do the bidirectional looping */

	if (count>0)
		while (count--)
	{
		if (!cc--)
		{
			cc=vp->vibrato_control_ratio;
			incr=update_vibrato(vp, (incr < 0));
		}
		RESAMPLATION;
		ofs += incr;
		if (ofs>=le)
		{
			/* fold the overshoot back in */
			ofs = le - (ofs - le);
			incr = -incr;
		}
		else if (ofs <= ls)
		{
			ofs = ls + (ls - ofs);
			incr = -incr;
		}
	}
#endif /* PRECALC_LOOPS */

	vp->vibrato_control_counter=cc;
	vp->sample_increment=incr;
	vp->sample_offset=ofs; /* Update offset */
	return resample_buffer;
}

sample_t *resample_voice(int v, sint32 *countptr)
{
	sint32 ofs;
	uint8 modes;
	Voice *vp=&voice[v];

	if (!(vp->sample->sample_rate))
	{
		/* Pre-resampled data -- just update the offset and check if
		 we're out of data. */
		ofs=vp->sample_offset >> FRACTION_BITS; /* Kind of silly to use
		 FRACTION_BITS here... */
		if (*countptr >= (vp->sample->data_length>>FRACTION_BITS) - ofs)
		{
			/* Note finished. Free the voice. */
			vp->status = VOICE_FREE;
			ctl->note(v);

			/* Let the caller know how much data we had left */
			*countptr = (vp->sample->data_length>>FRACTION_BITS) - ofs;
		}
		else
			vp->sample_offset += *countptr << FRACTION_BITS;

		return vp->sample->data+ofs;
	}

	/* Need to resample. Use the proper function. */
	modes=vp->sample->modes;

	if (vp->vibrato_control_ratio)
	{
		if ((modes & MODES_LOOPING) &&
		    ((modes & MODES_ENVELOPE) ||
		     (vp->status==VOICE_ON || vp->status==VOICE_SUSTAINED)))
		{
			if (modes & MODES_PINGPONG)
				return rs_vib_bidir(vp, *countptr);
			else
				return rs_vib_loop(vp, *countptr);
		}
		else
			return rs_vib_plain(v, countptr);
	}
	else
	{
		if ((modes & MODES_LOOPING) &&
		    ((modes & MODES_ENVELOPE) ||
		     (vp->status==VOICE_ON || vp->status==VOICE_SUSTAINED)))
		{
			if (modes & MODES_PINGPONG)
				return rs_bidir(vp, *countptr);
			else
				return rs_loop(vp, *countptr);
		}
		else
			return rs_plain(v, countptr);
	}
}

void pre_resample(Sample * sp)
{
	double a;
	double xdiff;
	sint32 incr;
	sint32 ofs;
	sint32 newlen;
	sint32 count;
	sint16 *newdata;
	sint16 *dest;
	sint16 *src = sp->data;
	sint16 v1;
	sint16 v2;
	sint16 v3;
	sint16 v4;
	sint16 *vptr;
	static const char note_name[12][3] =
	{
		"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
	};

	ctl->cmsg(CMSG_INFO, VERB_NOISY, " * pre-resampling for note %d (%s%d)",
	          sp->note_to_use,
	          note_name[sp->note_to_use % 12], (sp->note_to_use & 0x7F) / 12);

	a = (static_cast<double>(sp->sample_rate) * freq_table[static_cast<int>(sp->note_to_use)]) /
		(static_cast<double>(sp->root_freq) * play_mode->rate);
	if (a <= 0) return;
	newlen = static_cast<sint32>(sp->data_length / a);
	if (newlen < 0 || (newlen >> FRACTION_BITS) > MAX_SAMPLE_SIZE) return;
	dest = newdata = safe_Malloc<sint16>(newlen >> FRACTION_BITS);

	count = (newlen >> FRACTION_BITS) - 1;
	ofs = incr = (sp->data_length - (1 << FRACTION_BITS)) / count;

	if (--count)
		*dest++ = src[0];

	/* Since we're pre-processing and this doesn't have to be done in
	 real-time, we go ahead and do the full sliding cubic interpolation. */
	while (--count)
	{
		vptr = src + (ofs >> FRACTION_BITS);
		v1 = (vptr == src) ? *vptr : *(vptr - 1);
		v2 = *vptr;
		v3 = *(vptr + 1);
		v4 = *(vptr + 2);
		xdiff = FSCALENEG(ofs & FRACTION_MASK, FRACTION_BITS);
		*dest++ = static_cast<sint16>(v2 + (xdiff / 6.0) * (-2 * v1 - 3 * v2 + 6 * v3 - v4 +
		                 xdiff * (3 * (v1 - 2 * v2 + v3) + xdiff * (-v1 + 3 * (v2 - v3) + v4))));
		ofs += incr;
	}

	if (ofs & FRACTION_MASK)
	{
		v1 = src[ofs >> FRACTION_BITS];
		v2 = src[(ofs >> FRACTION_BITS) + 1];
		*dest++ = static_cast<uint16>(v1 + (((v2 - v1) * (ofs & FRACTION_MASK)) >> FRACTION_BITS));
	}
	else
		*dest++ = src[ofs >> FRACTION_BITS];

	sp->data_length = newlen;
	sp->loop_start = static_cast<sint32>(sp->loop_start / a);
	sp->loop_end = static_cast<sint32>(sp->loop_end / a);
	free(sp->data);
	sp->data = newdata;
	sp->sample_rate = 0;
}

#ifdef NS_TIMIDITY
}
#endif

#endif //USE_TIMIDITY_MIDI
