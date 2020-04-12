#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "../globals.h"

#define PLUGIN_NAME "Randomize"

// global variables
char col[256][256][7];
int glob_idx = 0;
glob_statics my_g_stat;

void init_plugin(glob_statics *g_stat) {
	// required
	// update the local glob_statics so that other functions can access it as well.
	my_g_stat.debug = g_stat->debug;
	my_g_stat.filein = g_stat->filein;
	my_g_stat.fileout = g_stat->fileout;
	my_g_stat.config_file = g_stat->config_file;
	my_g_stat.image_in = g_stat->image_in;

	if (my_g_stat.debug) {
		printf("Loading %s\n", PLUGIN_NAME);
	}

	if (my_g_stat.debug > 3) {
		printf("Checking the global stats\n");
		printf("\tdebug = %d\n", my_g_stat.debug);
		printf("\tfilein = %s\n", my_g_stat.filein);
		printf("\tfileout = %s\n", my_g_stat.fileout);
		printf("\tconfig_file = %s\n", my_g_stat.config_file);
	}
}

void deinit_plugin() {
	// optional
	// not required but it can be useful to inform the plugin is unloading
	if (my_g_stat.debug) {
		printf("Unloading %s\n", PLUGIN_NAME);
	}
}

int plugin_parse(char *line) {
	// required
	// will prepare the plugin to know what to send back when receiving a colour_hex in plugin_apply

	char c;
	int i, size = strlen(line);
	int newrec = 1;
	int let = 0;
	unsigned int idx = -1;

	for (i = 0; i < size; i++) {
		sscanf(line, "%c", &c);
		line++;
		if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
			if (newrec) {
				newrec = 0;
				idx++;
			}
			if (let < 6) {
				col[glob_idx][idx][let] = c;
			}
			let++;
		} else {
			col[glob_idx][idx][let] = '\0';
			newrec = 1;
			let = 0;
		}
	}

	// for debugging purposes only
	//  for(i=0;i<=idx;i++){
	//    printf("Value(%d): %s\n",glob_idx,col[glob_idx][i]);
	//  }
	// mark the end of the
	col[glob_idx][i][0] = '\0';

	glob_idx++;

	return 0;
}

int get_random(int max) {
	unsigned long int timer = SDL_GetTicks();
	int i;
	int temp = rand() % 10;
	srand(timer * rand());

	for (i = 0; i < temp; i++) rand();
	return rand() % max;
}

char *plugin_apply(char colour[6]) {
	// required
	// function called to transform a colour_hex into another one. plugin_parse must have been called previously to know what to return.
	// the implementation is left to the plugin writer
	// both plugin_parse and plugin_apply are obviously plugin-specific
	// colour is the colour at point (i,j) -- (i,j) are external variables from smooth

	int loc_idx = 0;
	int max_num = 0;
	int my_rand;

	// find the colour in big table
	while (strncasecmp(col[loc_idx][0], colour, 6) && loc_idx < glob_idx) {
		//    printf("I look for %s and I got %s instead!\n",colour,col[loc_idx][0]);
		loc_idx++;
	}

	if (loc_idx >= glob_idx) {
		return colour;
	}

	// find the max number of elements
	while (col[loc_idx][max_num][0] != '\0') max_num++;
	max_num = max_num - 1;
	//  printf("there are %d element to take from:",max_num);
	my_rand = get_random(max_num);
	my_rand++;
	//  printf(" taking %d\ (%s)\n",my_rand,col[loc_idx][my_rand]);

	return col[loc_idx][my_rand];
}
