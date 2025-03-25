#include "../globals.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLUGIN_NAME "Randomize"

// global variables
#define MAX_RANDOM 256
colour_hex col[256]
			  [MAX_RANDOM + 2];    // # randoms, source color, random colors
int          glob_idx = 0;
glob_statics my_g_stat;

void init_plugin(glob_statics* g_stat) {
	// required
	// update the local glob_statics so that other functions can access it as
	// well.
	my_g_stat.debug       = g_stat->debug;
	my_g_stat.filein      = g_stat->filein;
	my_g_stat.fileout     = g_stat->fileout;
	my_g_stat.config_file = g_stat->config_file;
	my_g_stat.image_in    = g_stat->image_in;

	if (my_g_stat.debug) {
		printf("Loading %s\n", PLUGIN_NAME);
	}

	if (my_g_stat.debug > 3) {
		printf("Checking the global stats for %s\n", PLUGIN_NAME);
		printf("\tdebug = %d\n", my_g_stat.debug);
		printf("\tfilein = %s\n", my_g_stat.filein);
		printf("\tfileout = %s\n", my_g_stat.fileout);
		printf("\tconfig_file = %s\n", my_g_stat.config_file);
	}
}

void deinit_plugin(void) {
	// optional
	// not required but it can be useful to inform the plugin is unloading
	if (my_g_stat.debug) {
		printf("Unloading %s\n", PLUGIN_NAME);
	}
}

int plugin_parse(char* line) {
	// required
	// will prepare the plugin to know what to send back when receiving a
	// colour_hex in plugin_apply

	const int    size     = strlen(line);
	int          newrec   = 1;
	int          let      = 0;
	unsigned int idx      = 0;
	char         color[7] = {0};
	colour_hex   r        = 0;
	colour_hex   g        = 0;
	colour_hex   b        = 0;

	if (my_g_stat.debug > 3) {
		printf("Parsing %s\n", line);
	}

	col[glob_idx][0] = 0;

	for (int i = 0; i <= size; i++) {
		char c = (i == size ? '\n' : line[i]);
		if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
			if (newrec) {
				newrec = 0;
				idx++;
			}
			if (let < 6) {
				color[let] = c;
			}
			let++;
		} else {
			if (newrec == 0) {
				color[let] = '\0';
				sscanf(color, "%02x%02x%02x", &r, &g, &b);
				col[glob_idx][idx] = ((r << 16) | (g << 8) | (b));
				col[glob_idx][0]   = idx - 1;
			}
			newrec = 1;
			let    = 0;
		}
	}

	glob_idx++;

	return 0;
}

int get_random(int max) {
	unsigned long int timer = SDL_GetTicks();
	int               temp  = rand() % 10;
	srand(timer * rand());

	for (int i = 0; i < temp; i++) {
		(void)rand();
	}
	return rand() % max;
}

colour_hex plugin_apply(colour_hex colour) {
	// required
	// function called to transform a colour_hex into another one. plugin_parse
	// must have been called previously to know what to return. the
	// implementation is left to the plugin writer both plugin_parse and
	// plugin_apply are obviously plugin-specific colour is the colour at point
	// (i,j) -- (i,j) are external variables from smooth

	int loc_idx = 0;

	// find the colour in big table
	while (loc_idx < glob_idx && col[loc_idx][1] != colour) {
		// printf("I look for %06x and I got %06x at %d instead!\n",
		//        colour, col[loc_idx][1], loc_idx+1);
		loc_idx++;
	}

	if (loc_idx >= glob_idx) {
		printf("I look for %06x, not found\n", colour);
		return colour;
	}

	// find the max number of elements
	unsigned max_num = col[loc_idx][0];
	// printf("There are %d elements to take for %06x :", max_num, colour);
	int my_rand = get_random(max_num) + 1;
	// printf(" taking %d (%06x)\n", my_rand, col[loc_idx][my_rand+1]);

	return col[loc_idx][my_rand + 1];
}
