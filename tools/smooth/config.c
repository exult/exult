/*
 * author: Aurelien Marchand
 * licence: GPL
 * date: 20/06/03
 *
 * This file is for reading the config files and setting up the appropriate
 * conversions
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "globals.h"
#include "plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int close_config(FILE* f) {
	return fclose(f);
}

FILE* open_config(const char* configfile) {
	if (g_statics.debug) {
		printf("\nConfig file: %s\n********************\n", configfile);
	}

	if (!strcmp(configfile, "-")) {              // they match
		if (!strcmp(g_statics.filein, "-")) {    // is filein also stdin?
			// IT IS!! That's no good
			fprintf(stderr, "ERROR: Already using stdin for inputimage\n");
			return NULL;
		} else {
			return stdin;
		}
	} else {
		FILE* f = fopen(configfile, "ra");
		if (f == NULL) {
			fprintf(stderr, "ERROR: Couldn't open config file %s\n",
					configfile);
			return NULL;
		}
		return f;
	}
}

int read_config(FILE* f) {
	char*       pluginname = NULL;
	libhandle_t a_hdl      = NULL;

	rewind(f);
	while (!feof(f)) {
		char line[MAX_LINE_LENGTH];
		fgets(line, MAX_LINE_LENGTH, f);
		int line_length = strlen(line);
		if (g_statics.debug > 2) {
			printf("length of line: %d\n", line_length);
			fflush(stdout);
		}

		if (line_length == (MAX_LINE_LENGTH - 1)) {
			// too many characters in the line. We might have missed some
			fprintf(stderr, "ERROR: Too many characters in line %s\n", line);
			return -1;
		}
		if (line_length != 0) {
			if (line[0] == '[') {
				// new plugin section
				if (pluginname != NULL) {
					if (g_statics.debug > 2) {
						printf("freeing %s\n", pluginname);
						fflush(stdout);
					}
					free(pluginname);
				}
				if (g_statics.debug) {
					printf("\nsection %s", line);
					fflush(stdout);
				}
				const size_t namelen = (16 + line_length) * sizeof(char);
				pluginname           = (char*)malloc(namelen);
				// what's between the '[' and the ']'
				memmove(line, line + 1, line_length - 3);
				line[line_length - 3] = '\0';    // and add a \0 at the end
#ifdef _WIN32
				snprintf(pluginname, namelen, "libsmooth_%s.dll", line);
#else
				snprintf(pluginname, namelen, "libsmooth_%s.so", line);
#endif
				if (g_statics.debug > 2) {
					printf("Plugin to load: %s\n", pluginname);
				}
				if ((a_hdl = plug_load(pluginname)) == NULL) {
					return -1;
				} else {
					if (g_statics.debug > 2) {
						printf("Adding %s to list\n", pluginname);
					}
					// TODO: load the init function with our global stuff to
					// initialise the plugin
					void* (*init)(glob_statics* g_var);
					*(void**)&init = plug_load_func(a_hdl, "init_plugin");
					(*init)(&g_statics);
					hdl_list = add_handle(a_hdl, hdl_list);
				}
			} else if (
					line[0] == '#' || line[0] == '\n' || line[0] == '\r'
					|| line[0] == ';') {
				if (g_statics.debug > 3) {
					printf("skipping: %s", line);
					fflush(stdout);
				}
			} else {
				if (g_statics.debug > 1) {
					printf("line read: %s", line);
					fflush(stdout);
				}
				// send the line to the plugin_process_line from pluginname
				// we should use the head of hdl_list as the handle of the
				// loaded plugin
				if (pluginname == NULL || a_hdl == NULL) {
					fprintf(stderr,
							"WARNING: line entered before first section. Use "
							"comments (# or ;) please.\nIgnoring line: %s",
							line);
				} else {
					// extract the first colour from the line, get index from
					// palette and populate action_table at right index with hdl
					unsigned r;
					unsigned g;
					unsigned b;
					if (sscanf(line, "%02x%02x%02x", &r, &g, &b) != 3) {
						// prevent buffer overflow problem
						fprintf(stderr,
								"ERROR: couldn't read the slave value of %s\n",
								line);
						fflush(stderr);
						return -1;
					} else {
						// get index of (r,g,b) from palette
						int idx = SDL_MapRGB(
								g_statics.image_in_format,
								g_statics.image_in_palette, r, g, b);
						// some reporting
						if (g_statics.debug > 3) {
							printf("slave is %02x%02x%02x "
								   "(idx=%d: r=%u g=%u b=%u)\n",
								   r, g, b, idx, r, g, b);
							fflush(stdout);
						}
						// teach the plugin how to convert what
						add_plugin_parse(line, a_hdl);

						// and add hdl to action_table[index]
						add_plugin_apply(idx, a_hdl);
					}
				}
			}
		}
	}
	// clear up the last pluginname loaded
	if (pluginname != NULL) {
		if (g_statics.debug > 2) {
			printf("freeing %s\n", pluginname);
			fflush(stdout);
		}
		free(pluginname);
	}
	if (g_statics.debug) {
		printf("********************\nEnd of config file\n\n");
		fflush(stdout);
	}

	return 0;
}
