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

*/
#include "pent_include.h"

#ifdef USE_TIMIDITY_MIDI

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using std::vector;

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#endif    // __GNUC__
#include "SDL.h"
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "timidity.h"
#include "timidity_common.h"
#include "timidity_instrum.h"
#include "timidity_playmidi.h"
#include "timidity_readmidi.h"
#include "timidity_output.h"
#include "timidity_controls.h"
#include "timidity_tables.h"

// we want to use Pentagram's config
#include "Configuration.h"

#ifdef NS_TIMIDITY
namespace NS_TIMIDITY {
#endif

void (*s32tobuf)(void *dp, sint32 *lp, sint32 c);
int free_instruments_afterwards=0;
static char def_instr_name[256]="";

int AUDIO_BUFFER_SIZE;
sample_t *resample_buffer=nullptr;
sint32 *common_buffer=nullptr;

#define MAXWORDS 10u

static int read_config_file(const char *name)
{
	FILE *fp;
	char tmp[1024];
	vector<char*> w;
	w.reserve(MAXWORDS);
	ToneBank *bank=nullptr;
	int line=0;
	static int rcf_count=0;

	if (rcf_count>50)
	{
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		          "Probable source loop in configuration files");
		return -1;
	}

	if (!(fp=open_file(name, 1, OF_VERBOSE)))
		return -1;

	while (fgets(tmp, sizeof(tmp), fp))
	{
		line++;
		w.clear();
		w.push_back(strtok(tmp, " \t\r\n\240"));
		if (!w[0] || (*w[0]=='#')) continue;
		while (w.size() < MAXWORDS) {
			char *token = strtok(nullptr," \t\r\n\240");
			if (token && token[0] != '#') w.push_back(token);
			else break;
		}
		if (!strcmp(w[0], "dir"))
		{
			if (w.size() < 2)
			{
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				          "%s: line %d: No directory given\n", name, line);
				close_file(fp);
				return -2;
			}
			for (unsigned i=1; i<w.size(); i++)
				add_to_pathlist(w[i]);
		}
		else if (!strcmp(w[0], "source"))
		{
			if (w.size() < 2)
			{
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				          "%s: line %d: No file name given\n", name, line);
				close_file(fp);
				return -2;
			}
			for (unsigned i=1; i<w.size(); i++)
			{
				rcf_count++;
				read_config_file(w[i]);
				rcf_count--;
			}
		}
		else if (!strcmp(w[0], "default"))
		{
			if (w.size() != 2)
			{
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				          "%s: line %d: Must specify exactly one patch name\n",
				          name, line);
				close_file(fp);
				return -2;
			}
			strncpy(def_instr_name, w[1], 255);
			def_instr_name[255]='\0';
		}
		else if (!strcmp(w[0], "drumset"))
		{
			if (w.size() < 2)
			{
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				          "%s: line %d: No drum set number given\n",
				          name, line);
				close_file(fp);
				return -2;
			}
			int i=atoi(w[1]);
			if (i<0 || i>127)
			{
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				          "%s: line %d: Drum set must be between 0 and 127\n",
				          name, line);
				close_file(fp);
				return -2;
			}
			if (!drumset[i])
			{
				drumset[i]=safe_Malloc<ToneBank>();
				memset(drumset[i], 0, sizeof(ToneBank));
			}
			bank=drumset[i];
		}
		else if (!strcmp(w[0], "bank"))
		{
			if (w.size() < 2)
			{
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				          "%s: line %d: No bank number given\n",
				          name, line);
				close_file(fp);
				return -2;
			}
			int i=atoi(w[1]);
			if (i<0 || i>127)
			{
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				          "%s: line %d: Tone bank must be between 0 and 127\n",
				          name, line);
				close_file(fp);
				return -2;
			}
			if (!tonebank[i])
			{
				tonebank[i]=safe_Malloc<ToneBank>();
				memset(tonebank[i], 0, sizeof(ToneBank));
			}
			bank=tonebank[i];
		}
		else {
			if ((w.size() < 2) || !std::isdigit(static_cast<unsigned char>(*w[0])))
			{
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				          "%s: line %d: syntax error\n", name, line);
				close_file(fp);
				return -2;
			}
			int i=atoi(w[0]);
			if (i<0 || i>127)
			{
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				          "%s: line %d: Program must be between 0 and 127\n",
				          name, line);
				close_file(fp);
				return -2;
			}
			if (!bank)
			{
				ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				          "%s: line %d: Must specify tone bank or drum set "
				          "before assignment\n",
				          name, line);
				close_file(fp);
				return -2;
			}
			if (bank->tone[i].name)
				free(bank->tone[i].name);
			strcpy((bank->tone[i].name=safe_Malloc<char>(strlen(w[1])+1)),w[1]);
			bank->tone[i].note=bank->tone[i].amp=bank->tone[i].pan=
				bank->tone[i].strip_loop=bank->tone[i].strip_envelope=
				bank->tone[i].strip_tail=-1;

			for (unsigned j=2; j<w.size(); j++)
			{
				char *cp = strchr(w[j], '=');
				if (!cp)
				{
					ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: bad patch option %s\n",
					          name, line, w[j]);
					close_file(fp);
					return -2;
				}
				*cp++=0;
				if (!strcmp(w[j], "amp"))
				{
					int k=atoi(cp);
					if ((k<0 || k>MAX_AMPLIFICATION) || !std::isdigit(static_cast<unsigned char>(*cp)))
					{
						ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
						          "%s: line %d: amplification must be between "
						          "0 and %d\n", name, line, MAX_AMPLIFICATION);
						close_file(fp);
						return -2;
					}
					bank->tone[i].amp=k;
				}
				else if (!strcmp(w[j], "note"))
				{
					int k=atoi(cp);
					if ((k<0 || k>127) || !std::isdigit(static_cast<unsigned char>(*cp)))
					{
						ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
						          "%s: line %d: note must be between 0 and 127\n",
						          name, line);
						close_file(fp);
						return -2;
					}
					bank->tone[i].note=k;
				}
				else if (!strcmp(w[j], "pan"))
				{
					int k;
					if (!strcmp(cp, "center"))
						k=64;
					else if (!strcmp(cp, "left"))
						k=0;
					else if (!strcmp(cp, "right"))
						k=127;
					else
						k=((atoi(cp)+100) * 100) / 157;
					if ((k<0 || k>127) ||
					    (k==0 && *cp!='-' && !std::isdigit(static_cast<unsigned char>(*cp))))
					{
						ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
						          "%s: line %d: panning must be left, right, "
						          "center, or between -100 and 100\n",
						          name, line);
						close_file(fp);
						return -2;
					}
					bank->tone[i].pan=k;
				}
				else if (!strcmp(w[j], "keep"))
				{
					if (!strcmp(cp, "env"))
						bank->tone[i].strip_envelope=0;
					else if (!strcmp(cp, "loop"))
						bank->tone[i].strip_loop=0;
					else
					{
						ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
						          "%s: line %d: keep must be env or loop\n", name, line);
						close_file(fp);
						return -2;
					}
				}
				else if (!strcmp(w[j], "strip"))
				{
					if (!strcmp(cp, "env"))
						bank->tone[i].strip_envelope=1;
					else if (!strcmp(cp, "loop"))
						bank->tone[i].strip_loop=1;
					else if (!strcmp(cp, "tail"))
						bank->tone[i].strip_tail=1;
					else
					{
						ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
						          "%s: line %d: strip must be env, loop, or tail\n",
						          name, line);
						close_file(fp);
						return -2;
					}
				}
				else
				{
					ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: bad patch option %s\n",
					          name, line, w[j]);
					close_file(fp);
					return -2;
				}
			}
		}
	}
	if (ferror(fp))
	{
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Can't read from %s\n", name);
		close_file(fp);
		return -2;
	}
	close_file(fp);
	return 0;
}

int Timidity_Init_Simple(int rate, int samples, sint32 encoding)
{
	std::string configfile;
	/* see if the pentagram config file specifies an alternate timidity.cfg */
	config->value("config/audio/midi/timiditycfg", configfile, CONFIG_FILE);

	if (( read_config_file(configfile.c_str()) < 0) &&
	    ((!is_system_path_defined("<BUNDLE>")) ||
	     (read_config_file(get_system_path("<BUNDLE>/" CONFIG_FILE).c_str()) < 0)) &&
	    ( read_config_file(get_system_path("<DATA>/"   CONFIG_FILE).c_str()) < 0) ) {
		return -1;
	}

	/* Check to see if the encoding is 'valid' */

	// Only 16 bit can be byte swapped
	if ((encoding & PE_BYTESWAP) && !(encoding & PE_16BIT))
		return -1;

	// u-Law can only be mono or stereo
	if ((encoding & PE_ULAW) && (encoding & ~(PE_ULAW|PE_MONO)))
		return -1;

	/* Set play mode parameters */
	play_mode->rate = rate;
	play_mode->encoding = encoding;
	switch (play_mode->encoding) {
		case 0:
		case PE_MONO:
			s32tobuf = s32tou8;
			break;

		case PE_SIGNED:
		case PE_SIGNED|PE_MONO:
			s32tobuf = s32tos8;
			break;

		case PE_ULAW:
		case PE_ULAW|PE_MONO:
			s32tobuf = s32toulaw;
			break;

		case PE_16BIT:
		case PE_16BIT|PE_MONO:
			s32tobuf = s32tou16;
			break;

		case PE_16BIT|PE_SIGNED:
		case PE_16BIT|PE_SIGNED|PE_MONO:
			s32tobuf = s32tos16;
			break;

		case PE_BYTESWAP|PE_16BIT:
		case PE_BYTESWAP|PE_16BIT|PE_MONO:
			s32tobuf = s32tou16x;
			break;

		case PE_BYTESWAP|PE_16BIT|PE_SIGNED:
		case PE_BYTESWAP|PE_16BIT|PE_SIGNED|PE_MONO:
			s32tobuf = s32tos16x;
			break;

		default:
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Unsupported audio format");
			return -1;
	}

	AUDIO_BUFFER_SIZE = samples;

	/* Allocate memory for mixing (WARNING:  Memory leak!) */
	resample_buffer = safe_Malloc<sample_t>(AUDIO_BUFFER_SIZE);
	common_buffer = safe_Malloc<sint32>(AUDIO_BUFFER_SIZE*2);

	init_tables();

	if (ctl->open(0, 0)) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Couldn't open %s\n", ctl->id_name);
		return -1;
	}

	if (!control_ratio) {
		control_ratio = play_mode->rate / CONTROLS_PER_SECOND;
		if(control_ratio<1)
			control_ratio=1;
		else if (control_ratio > MAX_CONTROL_RATIO)
			control_ratio=MAX_CONTROL_RATIO;
	}
	if (*def_instr_name)
		set_default_instrument(def_instr_name);
	return 0;
}

void Timidity_DeInit()
{
	free_instruments();

	free(resample_buffer);
	resample_buffer = nullptr;

	free(common_buffer);
	common_buffer = nullptr;
}


char timidity_error[1024] = "";
char *Timidity_Error()
{
	return timidity_error;
}

#ifdef NS_TIMIDITY
}
#endif

#endif //USE_TIMIDITY_MIDI


