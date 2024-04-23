/*
 * smooth
 * converts a 8 bpp indexed image file into another 8 bpp BMP file
 * applying some transformation in the way
 * so that less work is left to create a u7map
 *
 * author: Aurelien Marchand
 * licence: GPL
 * date: 20/06/03
 *
 * syntax: smooth [-i image_in] [-o image_out.bmp] [-c config_file]
 * image_in must be in indexed image with less than 256 colours (whatever type:
 * BMP, GIF, PNG) and of resolution 192x192 if image_out exists, it will be
 * erased default values: image_in = rough.bmp image_out = smoothed.bmp
 * config_file = smooth.conf
 */

#include <stdio.h>
#include <stdlib.h>

#if !defined(MAIN)
#	define MAIN
#endif

#include "config.h"

#include "globals.h"
#include "image.h"
#include "linked.h"
#include "param.h"
#include "plugin.h"
#include "smooth.h"

void clean_up(int ret) {
	// clean up everything that might be loaded
	int i;

	// clean up list holding handles
	if (hdl_list != NULL) {
		delete_list(hdl_list);
	}

	// clean up action_table
	for (i = 0; i < MAX_COLOURS; i++) {
		if (action_table[i] != NULL) {
			delete_list(action_table[i]);
		}
	}

	// clean up image_in
	if (g_statics.image_in != NULL) {
		SDL_DestroySurface(g_statics.image_in);
	}
	// clean up image_out
	if (g_variables.image_out != NULL) {
		SDL_DestroySurface(g_variables.image_out);
	}

	// quit
	exit(ret);
}

int main(int argc, char** argv) {
	FILE* config;
	int   r;

	// initialise some variables
	hdl_list = NULL;
	for (r = 0; r < MAX_COLOURS; r++) {
		action_table[r] = NULL;
	}

	// make sure we can understand the command line
	if (read_param(argc, argv) < 0) {
		fprintf(stderr, "ERROR: command line invalid\n");
		exit(-1);
	}

	// initialise SDL
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		printf("Couldn't initialise SDL: %s\n", SDL_GetError());
		exit(-1);
	}

	atexit(SDL_Quit);

	// make sure the source image is readable
	if (img_read(g_statics.filein) < 0) {
		//    fprintf(stderr,"ERROR: problem with image %s\n",filein);
		clean_up(-1);
	}

	// make sure the config file is readable
	if ((config = open_config(g_statics.config_file)) == NULL) {
		clean_up(-1);
	}

	// make sure it has been read properly
	if (read_config(config) < 0) {
		clean_up(-1);
	}
	// make sure we can close it properly
	if (close_config(config)
		== EOF) {    // shouldn't happen, but you never know
		perror("");
		clean_up(-1);
	}

	// time to apply the transformation
	if (process_image() < 0) {
		// problem
		clean_up(-1);
	}

	// make sure we are able to write the destination image
	if (img_write(g_statics.fileout) < 0) {
		//    fprintf(stderr,"ERROR: problem with image %s\n",filein);
		clean_up(-1);
	}

	// all is good, we can quit.
	SDL_Quit();
	printf("Done!\n");
	clean_up(0);
	return 0;    // to avoid warnings
}
