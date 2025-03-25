/*

	TiMidity -- Experimental MIDI to WAVE converter
	Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	 common.c

	 */

#include "pent_include.h"

#ifdef USE_TIMIDITY_MIDI

#	include "ignore_unused_variable_warning.h"
#	include "timidity.h"
#	include "timidity_common.h"
#	include "timidity_controls.h"
#	include "timidity_output.h"

#	include <cerrno>
#	include <cstdio>
#	include <cstdlib>
#	include <cstring>

#	ifdef NS_TIMIDITY
namespace NS_TIMIDITY {
#	endif

/* I guess "rb" should be right for any libc */
#	define OPEN_MODE "rb"

	char current_filename[1024];

#	ifdef DEFAULT_TIMIDITY_PATH
	/* The paths in this list will be tried whenever we're reading a file */
	static PathList  defaultpathlist = {DEFAULT_TIMIDITY_PATH, nullptr};
	static PathList* pathlist = &defaultpathlist; /* This is a linked list */
#	else
static PathList* pathlist = nullptr;
#	endif

	/*	Try to open a file for reading. If the filename ends in one of the
	 defined compressor extensions, pipe the file through the decompressor */
	static FILE* try_to_open(char* name, int decompress, int noise_mode) {
		ignore_unused_variable_warning(decompress, noise_mode);
		FILE* fp;

		fp = fopen(name, OPEN_MODE); /* First just check that the file exists */

		if (!fp) {
			return nullptr;
		}

#	ifdef DECOMPRESSOR_LIST
		if (decompress) {
			int          l, el;
			static char *decompressor_list[] = DECOMPRESSOR_LIST, **dec;
			char         tmp[1024], tmp2[1024], *cp, *cp2;
			/* Check if it's a compressed file */
			l = strlen(name);
			for (dec = decompressor_list; *dec; dec += 2) {
				el = strlen(*dec);
				if ((el >= l) || (strcmp(name + l - el, *dec))) {
					continue;
				}

				/* Yes. Close the file, open a pipe instead. */
				fclose(fp);

				/* Quote some special characters in the file name */
				cp  = name;
				cp2 = tmp2;
				while (*cp) {
					switch (*cp) {
					case '\'':
					case '\\':
					case ' ':
					case '`':
					case '!':
					case '"':
					case '&':
					case ';':
						*cp2++ = '\\';
					}
					*cp2++ = *cp++;
				}
				*cp2 = 0;

				snprintf(tmp, sizeof(tmp), *(dec + 1), tmp2);
				return popen(tmp, "r");
			}
		}
#	endif

		return fp;
	}

	/* This is meant to find and open files for reading, possibly piping
	 them through a decompressor. */
	FILE* open_file(const char* name, int decompress, int noise_mode) {
		if (!name || !(*name)) {
			ctl->cmsg(
					CMSG_ERROR, VERB_NORMAL,
					"Attempted to open nameless file.");
			return nullptr;
		}

		/* First try the given name */

		strncpy(current_filename, name, 1023);
		current_filename[1023] = '\0';

		ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Trying to open %s", current_filename);
		FILE* fp;
		if ((fp = try_to_open(current_filename, decompress, noise_mode))) {
			return fp;
		}

		if (noise_mode && (errno != ENOENT)) {
			ctl->cmsg(
					CMSG_ERROR, VERB_NORMAL, "%s: %s", current_filename,
					strerror(errno));
			return nullptr;
		}

#	ifndef SDL_PLATFORM_WIN32
		if (name[0] != PATH_SEP) {
#	else
	if (name[0] != '\\' && name[0] != '/' && name[1] != ':') {
#	endif
			PathList* plp = pathlist;
			while (plp) /* Try along the path then */
			{
				*current_filename = 0;
				int l             = static_cast<int>(strlen(plp->path));
				if (l) {
					strcpy(current_filename, plp->path);
#	ifndef SDL_PLATFORM_WIN32
					if (current_filename[l - 1] != PATH_SEP)
#	else
				if (current_filename[l - 1] != '\\'
					&& current_filename[l - 1] != '/')
#	endif
						strcat(current_filename, PATH_STRING);
				}
				strcat(current_filename, name);
				ctl->cmsg(
						CMSG_INFO, VERB_DEBUG, "Trying to open %s",
						current_filename);
				if ((fp
					 = try_to_open(current_filename, decompress, noise_mode))) {
					return fp;
				}
				if (noise_mode && (errno != ENOENT)) {
					ctl->cmsg(
							CMSG_ERROR, VERB_NORMAL, "%s: %s", current_filename,
							strerror(errno));
					return nullptr;
				}
				plp = plp->next;
			}
		}

		/* Nothing could be opened. */

		*current_filename = 0;

		if (noise_mode >= 2) {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s", name, strerror(errno));
		}

		return nullptr;
	}

	/* This closes files opened with open_file */
	void close_file(FILE* fp) {
#	ifdef DECOMPRESSOR_LIST
		if (pclose(fp)) /* Any better ideas? */
#	endif
			fclose(fp);
	}

	/* This is meant for skipping a few bytes in a file or fifo. */
	void skip(FILE* fp, size_t len) {
		while (len > 0) {
			size_t c = len;
			if (c > 1024) {
				c = 1024;
			}
			len -= c;
			char tmp[1024];
			if (c != fread(tmp, 1, c, fp)) {
				ctl->cmsg(
						CMSG_ERROR, VERB_NORMAL, "%s: skip: %s",
						current_filename, strerror(errno));
			}
		}
	}

	/* This'll allocate memory or die. */
	void* safe_malloc(size_t count) {
		void* p;
		if (count > (1 << 21)) {
			ctl->cmsg(
					CMSG_FATAL, VERB_NORMAL,
					"Strange, I feel like allocating %u bytes. This must be a "
					"bug.",
					static_cast<unsigned>(count));
		} else if ((p = malloc(count))) {
			return p;
		} else {
			ctl->cmsg(
					CMSG_FATAL, VERB_NORMAL, "Sorry. Couldn't malloc %u bytes.",
					static_cast<unsigned>(count));
		}

		ctl->close();
		exit(10);
		return nullptr;
	}

	/* This adds a directory to the path list */
	void add_to_pathlist(char* s) {
		auto* plp  = safe_Malloc<PathList>();
		char* path = safe_Malloc<char>(strlen(s) + 1);
		strcpy(path, s);
		plp->path = path;
		plp->next = pathlist;
		pathlist  = plp;
	}

#	ifdef NS_TIMIDITY
}
#	endif

#endif    // USE_TIMIDITY_MIDI
