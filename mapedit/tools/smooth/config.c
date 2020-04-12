/*
 * author: Aurelien Marchand
 * licence: GPL
 * date: 20/06/03
 *
 * This file is for reading the config files and setting up the appropriate conversions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "globals.h"
#include "config.h"
#include "plugin.h"

int close_config(FILE *f) {
	return fclose(f);
}

FILE *open_config(char *configfile) {
	FILE *f;
	if (g_statics.debug) {
		printf("\nConfig file: %s\n********************\n", configfile);
	}

	if (!strcmp(configfile, "-")) { // they match
		if (!strcmp(g_statics.filein, "-")) { // is filein also stdin?
			// IT IS!! That's no good
			fprintf(stderr, "ERROR: Already using stdin for inputimage\n");
			return NULL;
		} else {
			return stdin;
		}
	} else {
		if ((f = fopen(configfile, "ra")) == NULL) {
			fprintf(stderr, "ERROR: Couldn't open config file %s\n", configfile);
			return NULL;
		}
		return f;
	}
}

int read_config(FILE *f) {
	char line[MAX_LINE_LENGTH];
	int line_length;
	char *pluginname = NULL;
	colour_hex col;
	int idx;
	unsigned r, g, b;
	libhandle_t a_hdl = NULL;
	void *(*init)(glob_statics * g_var);

	rewind(f);
	while (!feof(f)) {
		fgets(line, MAX_LINE_LENGTH, f);
		line_length = strlen(line);
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
				pluginname = (char *)malloc((13 + line_length) * sizeof(char));
				strncpy(line, line + 1, line_length - 3); // what's between the '[' and the ']'
				line[line_length - 3] = '\0'; // and add a \0 at the end
#ifdef _WIN32
				sprintf(pluginname, "libsmooth_%s.dll", line);
#else
				sprintf(pluginname, "libsmooth_%s.so", line);
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
					// TODO: load the init function with our global stuff to initialise the plugin
					*(void**)&init = plug_load_func(a_hdl, "init_plugin");
					(*init)(&g_statics);
					hdl_list = add_handle(a_hdl, hdl_list);
				}
			} else if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == ';') {
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
				// we should use the head of hdl_list as the handle of the loaded plugin
				if (pluginname == NULL || a_hdl == NULL) {
					fprintf(stderr, "WARNING: line entered before first section. Use comments (# or ;) please.\nIgnoring line: %s", line);
				} else {
					// extract the first colour from the line, get index from palette and populate action_table at right index with hdl
					if (sscanf(line, "%6s", col) != 1 || strlen(col) != 6) { // just read 6 characters to prevent buffer overflow
						// problem
						fprintf(stderr, "ERROR: couldn't read the slave value of %s\n", line);
						fflush(stderr);
						return -1;
					} else {
						sscanf(col, "%02x%02x%02x", &r, &g, &b);
						// get index of col from palette
						idx = SDL_MapRGB(g_statics.image_in->format, r, g, b);
						// some reporting
						if (g_statics.debug > 3) {
							printf("slave is %s (idx=%d: r=%d g=%d b=%d)\n", col, idx, r, g, b);
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
		printf("********************\nEnf of config file\n\n");
		fflush(stdout);
	}

	return 0;
}

