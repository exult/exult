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

   filter.c: written by Vincent Pagel ( pagel@loria.fr ) 
 
   implements fir antialiasing filter : should help when setting sample
   rates as low as 8Khz.

   April 95
      - first draft

   22/5/95
      - modify "filter" so that it simulate leading and trailing 0 in the buffer
   */

#include "pent_include.h"

#ifdef USE_TIMIDITY_MIDI

#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include "timidity.h"
#include "timidity_common.h"
#include "timidity_controls.h"
#include "timidity_instrum.h"
#include "timidity_filter.h"

#ifdef NS_TIMIDITY
namespace NS_TIMIDITY {
#endif

/*  bessel  function   */
static float ino(float x)
{
	float y;
	float de;
	float e;
	float sde;
	int i;

	y = x / 2;
	e = 1.0;
	de = 1.0;
	i = 1;
	do {
		de = de * y / static_cast<float>(i);
		sde = de * de;
		e += sde;
	} while (!( (e * 1.0e-08 - sde > 0) || (i++ > 25) ));
	return e;
}	

/* Kaiser Window (symetric) */
static void kaiser(float *w,int n,float beta)
{
	float xind;
	float xi;
	int i;

	xind = static_cast<float>((2*n - 1) * (2*n - 1));
	for (i =0; i<n ; i++) 
	{
		xi = static_cast<float>(i + 0.5);
		w[i] = ino(static_cast<float>(beta * sqrt(1. - 4 * xi * xi / xind))) / ino(beta);
	}
}

/*
 * fir coef in g, cuttoff frequency in fc
 */
static void designfir(float *g , float fc)
{
	int i;
	float xi;
	float omega;
	float att;
	float beta ;
	float w[ORDER2];

	for (i =0; i < ORDER2 ;i++) 
	{
		xi = static_cast<float>(i + 0.5);
		omega = static_cast<float>(PI * xi);
		g[i] = static_cast<float>(sin( static_cast<double>(omega * fc)) / omega);
	}

	att = 40.; /* attenuation  in  db */
	beta = static_cast<float>(exp(log(0.58417 * (att - 20.96)) * 0.4) + 0.07886 
	                          * (att - 20.96));
	kaiser( w, ORDER2, beta);

	/* Matrix product */
	for (i =0; i < ORDER2 ; i++)
		g[i] = g[i] * w[i];
}

/*
 * FIR filtering -> apply the filter given by coef[] to the data buffer
 * Note that we simulate leading and trailing 0 at the border of the 
 * data buffer
 */
static void filter(sample_t *result, const sample_t *data, sint32 length, const float coef[])
{
	sint32 sample;
	sint32 i;
	sint32 sample_window;
	sint16 peak = 0;
	float sum;

	/* Simulate leading 0 at the begining of the buffer */
	for (sample = 0; sample < ORDER2 ; sample++ )
	{
		sum = 0.0;
		sample_window= sample - ORDER2;

		for (i = 0; i < ORDER ;i++) 
			sum += static_cast<float>(coef[i] *
			                          ((sample_window<0)? 0.0 : data[sample_window++])) ;

		/* Saturation ??? */
		if (sum> 32767.) { sum=32767.; peak++; }
		if (sum< -32768.) { sum=-32768; peak++; }
		result[sample] = static_cast<sample_t>(sum);
	}

	/* The core of the buffer  */
	for (sample = ORDER2; sample < length - ORDER + ORDER2 ; sample++ )
	{
		sum = 0.0;
		sample_window= sample - ORDER2;

		for (i = 0; i < ORDER ;i++) 
			sum += data[sample_window++] * coef[i];

		/* Saturation ??? */
		if (sum> 32767.) { sum=32767.; peak++; }
		if (sum< -32768.) { sum=-32768; peak++; }
		result[sample] = static_cast<sample_t>(sum);
	}

	/* Simulate 0 at the end of the buffer */
	for (sample = length - ORDER + ORDER2; sample < length ; sample++ )
	{
		sum = 0.0;
		sample_window= sample - ORDER2;

		for (i = 0; i < ORDER ;i++) 
			sum += static_cast<float>(coef[i] *
			                          ((sample_window>=length)? 0.0 : data[sample_window++])) ;

		/* Saturation ??? */
		if (sum> 32767.) { sum=32767.; peak++; }
		if (sum< -32768.) { sum=-32768; peak++; }
		result[sample] = static_cast<sample_t>(sum);
	}

	if (peak)
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, 
		          "Saturation %2.3f %%.", 100.0*peak/ static_cast<float>(length));
}

/***********************************************************************/
/* Prevent aliasing by filtering any freq above the output_rate        */
/*                                                                     */
/* I don't worry about looping point -> they will remain soft if they  */
/* were already                                                        */
/***********************************************************************/
void antialiasing(Sample *sp, sint32 output_rate )
{
	sample_t *temp;
	int i;
	float fir_symetric[ORDER];
	float fir_coef[ORDER2];
	float freq_cut;  /* cutoff frequency [0..1.0] FREQ_CUT/SAMP_FREQ*/


	ctl->cmsg(CMSG_INFO, VERB_NOISY, "Antialiasing: Fsample=%iKHz",
	          sp->sample_rate);

	/* No oversampling  */
	if (output_rate>=sp->sample_rate)
		return;

	freq_cut= static_cast<float>(output_rate) / sp->sample_rate;
	ctl->cmsg(CMSG_INFO, VERB_NOISY, "Antialiasing: cutoff=%f%%",
	          freq_cut*100.);

	designfir(fir_coef,freq_cut);

	/* Make the filter symetric */
	for (i = 0 ; i<ORDER2 ;i++) 
		fir_symetric[ORDER-1 - i] = fir_symetric[i] = fir_coef[ORDER2-1 - i];

	/* We apply the filter we have designed on a copy of the patch */
	temp = safe_Malloc<sample_t>(sp->data_length);
	memcpy(temp,sp->data,sp->data_length);

	filter(sp->data,temp,sp->data_length/sizeof(sample_t),fir_symetric);

	free(temp);
}

#ifdef NS_TIMIDITY
}
#endif

#endif //USE_TIMIDITY_MIDI
