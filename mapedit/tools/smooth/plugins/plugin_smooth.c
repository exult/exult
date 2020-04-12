#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "../globals.h"

#define PLUGIN_NAME "Smooth"

glob_statics my_g_stat;
glob_variables my_g_var;

// global variables
char col[256][15][7]; // colour has 6 char + \0, 13 possible choice + trigger + slave and max of 256 colours that could be subject to transformation
int glob_idx = 0;

/*****************************************************************************/
// NO CHANGES TO MAKE HERE

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
		printf("Checking the global stats for %s\n", PLUGIN_NAME);
		printf("\tdebug = %d\n", my_g_stat.debug);
		printf("\tfilein = %s\n", my_g_stat.filein);
		printf("\tfileout = %s\n", my_g_stat.fileout);
		printf("\tconfig_file = %s\n", my_g_stat.config_file);
	}
}

void deinit_plugin() {
	// required since it is called specifically at unload time
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

	// in this case we know we should receive 18 parameters
	// 1 for slave, 1 for master and 13 for the resulting colour

	for (i = 0; i < size; i++) {
		sscanf(line, "%c", &c);
		line++;
		if (c != ' ' && c != '\t' && c != '\n' && c != '\r') { // skip spaces, tabs and enters
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

	if (my_g_stat.debug > 3) {
		printf("Parsed line checking:\n");
		for (unsigned j = 0; j <= idx; j++) {
			printf("Value(%d): %s\n", glob_idx, col[glob_idx][j]);
		}
	}

	// mark the end of the record
	//  col[glob_idx][idx][0]='\0';

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
	// used to find a trigger colour around the chunk we are transforming

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
	//  printf("checking if %ld = %ld (%s): %d\n",a,val,col_name,a == val);

	return a == val || b == val || c == val || d == val || e == val || f == val || g == val || h == val;
}
// UP TO HERE
/*****************************************************************************/

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
	unsigned short int a, b, c, d;

	my_g_var.global_x = g_var->global_x;
	my_g_var.global_y = g_var->global_y;
	my_g_var.image_out = g_var->image_out;

	col_num = my_getpixel(my_g_stat.image_in, my_g_var.global_x, my_g_var.global_y);

	// find the colour in big table
	while (strncasecmp(col[loc_idx][0], colour, 6) && loc_idx < glob_idx) {
		//    printf("I look for %s and I got %s instead!\n",colour,col[loc_idx][0]);
		loc_idx++;
	}

	if (loc_idx >= glob_idx) {
		fprintf(stderr, "WARNING: loc_idx >= glob_idx. This should never happen\n");
		return colour; // colour is not in table, so we don't treat it. This should never happen.
	} else {
		//    fprintf(stderr,"found an entry in col for %s at %d\n",colour,loc_idx);
		//    fprintf(stderr,"col[%d][1] = %s\n",loc_idx,col[loc_idx][1]);

	}

	if (strncasecmp(col[loc_idx][1], a_star, 1) == 0 || has_around(col[loc_idx][1])) {
		//    printf("trig is around!\n");
		// this is the main part. Trigger is * or trigger is around the chunk to change.
		a = calculate(col_num, my_g_var.global_x - 1, my_g_var.global_y);
		b = calculate(col_num, my_g_var.global_x, my_g_var.global_y + 1);
		c = calculate(col_num, my_g_var.global_x + 1, my_g_var.global_y);
		d = calculate(col_num, my_g_var.global_x, my_g_var.global_y - 1);

		if (!((a || c) && (b || d))) {
			// this combinaison is not authorised for modifying.
			// we must have at least one on North or South or both AND one on East or West or both
			if (my_g_stat.debug > 3) printf("trigger present but not authorised combinaison a=%d b=%d c=%d d=%d\n", a, b, c, d);
			return colour;
		}

		calc_value = (int)(1 + (2.4 * a) + (-2.1 * b) + (1.1 * c) + (3.1 * d) + 0.5);

		if (my_g_stat.debug > 3) printf("calc_value is %d at (%d,%d) -- a=%d b=%d c=%d d=%d\n", calc_value, my_g_var.global_x, my_g_var.global_y, a, b, c, d);

		if (calc_value != 6) {
			// not the center chunk with a trigger on one of the corners
			return col[loc_idx][calc_value + 2]; // the first 2 cells are taken by slave and trigger
		} else {
			// in this case, the chunk is a center chunk with a trigger on one corner
			unsigned short int i, j, k, l;
			// need to know trigger's index.
			//      Uint8 idx_trigger = SDL_MapRGB(my_g_stat.image_in->format,16*(int)col[loc_idx][1][0]+(int)col[loc_idx][1][1],16*(int)col[loc_idx][1][2]+(int)col[loc_idx][1][3],16*(int)col[loc_idx][1][4]+(int)col[loc_idx][1][5]);
			Uint8 idx_trigger = 0;
			i = calculate(idx_trigger, my_g_var.global_x - 1, my_g_var.global_y - 1); // NW corner
			j = calculate(idx_trigger, my_g_var.global_x + 1, my_g_var.global_y - 1); // NE corner
			k = calculate(idx_trigger, my_g_var.global_x + 1, my_g_var.global_y + 1); // SE corner
			l = calculate(idx_trigger, my_g_var.global_x - 1, my_g_var.global_y + 1); // SW corner

			if (i + j + k + l != 1) {
				// not exactly one trigger at the corners. This is not allowed
				return colour;
			} else {
				calc_value = 8 + 1 * i + 2 * j + 3 * k + 4 * l;
				return col[loc_idx][calc_value + 2];
			}
		}
	} else {
		// there is no trigger around this chunk
		//    printf("no trig around\n");
		return colour;
	}
}
