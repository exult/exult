#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "../globals.h"

#define PLUGIN_NAME "Stream"

glob_statics my_g_stat;
glob_variables my_g_var;

// global variables
char col[256][18][7];
int glob_idx = 0;


Uint8 my_getpixel(SDL_Surface *surface, int x, int y) {
	int bpp = surface->format->BytesPerPixel;
	Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
	/* Here p is the address to the pixel we want to retrieve */

	switch (bpp) {
	case 1:
		return *p;
	default:
		return 0;       /* shouldn't happen, but avoids warnings */
	}
}

void init_plugin(glob_statics *g_stat) {
	// required since it is called specifically at load time
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
	// required since it is called specifically at unload time
	if (my_g_stat.debug) {
		printf("Unloading Stream\n");
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

	// in this case we know we should receive 18 parameters
	// 1 for slave, 1 for master and 16 for the resulting colour

	if (my_g_stat.debug > 3) {
		printf("Parsing %s\n", line);
	}

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
			if (newrec == 0) {
				col[glob_idx][idx][let] = '\0';
			}
			newrec = 1;
			let = 0;
		}
	}


	// for debugging purposes only
	//  for(i=0;i<=idx;i++){
	//    printf("Value(%d): %s\n",glob_idx,col[glob_idx][i],col[glob_idx][i]);
	//  }

	// mark the end of the record
	col[glob_idx][i][0] = '\0';

	glob_idx++;

	return 0;
}

int calculate(Uint8 col_num, unsigned int my_x, unsigned int my_y) {
	// this function helps to identify which colour to return
	// it returns either 1 or 0 based on whether the pixel at (x,y) is of colour col_num
	// it also checks boundaries and return 0 if out of boundaries
	int ret;

	if (my_x > 191 || my_y > 191) {
		//    printf("out of bounds\n");
		return 0;
	} else {
		ret = my_getpixel(my_g_stat.image_in, my_x, my_y);
		return ret == col_num;
	}
}

int has_around(char *col_name) {
	// this checks if there is a chunk around (i,j) of colour col_name
	// we check the 8 directions

	long int a0 = my_getpixel(my_g_stat.image_in, (192 + my_g_var.global_x - 1) % 192, (192 + my_g_var.global_y - 1) % 192);
	long int a = my_g_stat.image_in->format->palette->colors[a0].r * 256 * 256 + my_g_stat.image_in->format->palette->colors[a0].g * 256 + my_g_stat.image_in->format->palette->colors[a0].b;
	long int b0 = my_getpixel(my_g_stat.image_in, (192 + my_g_var.global_x) % 192, (192 + my_g_var.global_y - 1) % 192);
	long int b = my_g_stat.image_in->format->palette->colors[b0].r * 256 * 256 + my_g_stat.image_in->format->palette->colors[b0].g * 256 + my_g_stat.image_in->format->palette->colors[b0].b;
	long int c0 = my_getpixel(my_g_stat.image_in, (192 + my_g_var.global_x + 1) % 192, (192 + my_g_var.global_y - 1) % 192);
	long int c = my_g_stat.image_in->format->palette->colors[c0].r * 256 * 256 + my_g_stat.image_in->format->palette->colors[c0].g * 256 + my_g_stat.image_in->format->palette->colors[c0].b;
	long int d0 = my_getpixel(my_g_stat.image_in, (192 + my_g_var.global_x - 1) % 192, (192 + my_g_var.global_y) % 192);
	long int d = my_g_stat.image_in->format->palette->colors[d0].r * 256 * 256 + my_g_stat.image_in->format->palette->colors[d0].g * 256 + my_g_stat.image_in->format->palette->colors[d0].b;
	long int e0 = my_getpixel(my_g_stat.image_in, (192 + my_g_var.global_x + 1) % 192, (192 + my_g_var.global_y) % 192);
	long int e = my_g_stat.image_in->format->palette->colors[e0].r * 256 * 256 + my_g_stat.image_in->format->palette->colors[e0].g * 256 + my_g_stat.image_in->format->palette->colors[e0].b;
	long int f0 = my_getpixel(my_g_stat.image_in, (192 + my_g_var.global_x - 1) % 192, (192 + my_g_var.global_y + 1) % 192);
	long int f = my_g_stat.image_in->format->palette->colors[f0].r * 256 * 256 + my_g_stat.image_in->format->palette->colors[f0].g * 256 + my_g_stat.image_in->format->palette->colors[f0].b;
	long int g0 = my_getpixel(my_g_stat.image_in, (192 + my_g_var.global_x) % 192, (192 + my_g_var.global_y + 1) % 192);
	long int g = my_g_stat.image_in->format->palette->colors[g0].r * 256 * 256 + my_g_stat.image_in->format->palette->colors[g0].g * 256 + my_g_stat.image_in->format->palette->colors[g0].b;
	long int h0 = my_getpixel(my_g_stat.image_in, (192 + my_g_var.global_x + 1) % 192, (192 + my_g_var.global_y + 1) % 192);
	long int h = my_g_stat.image_in->format->palette->colors[h0].r * 256 * 256 + my_g_stat.image_in->format->palette->colors[h0].g * 256 + my_g_stat.image_in->format->palette->colors[h0].b;

	long int val = strtol(col_name, (char **)NULL, 16);
	//  printf("checking if %d = %d: %d\n",a,val,a == val);

	return a == val || b == val || c == val || d == val || e == val || f == val || g == val || h == val;
}

char *plugin_apply(char colour[6], glob_variables *g_var) {
	// required
	// function called to transform a colour_hex into another one. plugin_parse must have been called previously to know what to return.
	// the implementation is left to the plugin writer
	// both plugin_parse and plugin_apply are obviously plugin-specific
	// colour is the colour at point (i,j) -- (i,j) are external variables from smooth

	int loc_idx = 0;
	unsigned int calc_value;
	Uint8 col_num = 4;
	char *a_star = "*";

	my_g_var.global_x = g_var->global_x;
	my_g_var.global_y = g_var->global_y;
	my_g_var.image_out = g_var->image_out;

	col_num = my_getpixel(my_g_stat.image_in, my_g_var.global_x, my_g_var.global_y);

	// find the colour in big table
	while (strncasecmp(col[loc_idx][0], colour, 6) && loc_idx < glob_idx) {
		loc_idx++;
	}

	if (loc_idx >= glob_idx) {
		fprintf(stderr, "WARNING: loc_idx >= glob_idx. This should never happen\n");
		return colour; // colour is not in table, so we don't treat it. This should never happen.
	}

	if (strncasecmp(col[loc_idx][1], a_star, 1) == 0 || has_around(col[loc_idx][1])) {
		calc_value = (1 * calculate(col_num, my_g_var.global_x, (my_g_var.global_y - 1))) + (2 * calculate(col_num, (my_g_var.global_x + 1), my_g_var.global_y)) + (4 * calculate(col_num, my_g_var.global_x, (my_g_var.global_y + 1))) + (8 * calculate(col_num, (my_g_var.global_x - 1), my_g_var.global_y));

		if (my_g_stat.debug > 4) printf("calc_value is %u at (%d,%d) -- col_num = %u\n", calc_value, my_g_var.global_x, my_g_var.global_y, col_num);
		return col[loc_idx][calc_value + 2]; // the first 2 cells are taken by slave and trigger
	} else {
		return colour;
	}
}
