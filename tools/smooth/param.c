/*
 * author: Aurelien Marchand
 * licence: GPL
 * date: 20/06/03
 *
 * This file is for parsing the command line and setting up the appropriate
 * variables
 */

#include "param.h"

#include "globals.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void show_help(char* prog_name) {
	printf("Usage: %s [-h] [-c configfile] [-i inputimage] [-o "
		   "outputimage.bmp] [-d debuglevel]\n\n",
		   prog_name);
	printf("-h\t\t\tShows this help\n");
	printf("-c configfile\t\tSpecify config file for conversion\n");
	printf("\t\t\t\t(default: smooth.conf)\n");
	printf("-i inputimage\t\tImage to convert from. Any indexed format "
		   "supported by SDL_Image\n");
	printf("\t\t\t\t(default: rough.bmp)\n");
	printf("-o outputimage.bmp\tImage to convert to. The image will be in "
		   "BMP.\n");
	printf("\t\t\t\t(default: smoothed.bmp)\n");
	printf("-d debuglevel\t\tInteger value for debug level; the higher, the "
		   "more debug information\n");
	printf("\t\t\t\t(default: 0, max effect: 4)\n");
	fflush(stdout);
}

int is_switch(char* val) {
	// function return true if the parameter has exactly 2 characters and starts
	// with '-'
	return !strncmp(val, "-", 1) && strlen(val) == 2;
}

int read_param(int argc, char** argv) {
	int i;

	g_statics.config_file = "smooth.conf";
	g_statics.filein      = "rough.bmp";
	g_statics.fileout     = "smoothed.bmp";

	for (i = 1; i < argc; i++) {
		if (is_switch(argv[i])) {    // argv[i] starts with - but has more than
									 // 1 char
			switch (argv[i][1]) {
			case 'h': {
				show_help(argv[0]);
				exit(0);
			}
			case 'd': {
				if (i + 1 < argc && !is_switch(argv[i + 1])) {
					printf("debug value: %d\n", atoi(argv[i + 1]));
					g_statics.debug = atoi(argv[i + 1]);
				} else {
					printf("missing value for switch 'd'\n");
					return -1;
				}
				break;
			}
			case 'c': {
				if (i + 1 < argc && !is_switch(argv[i + 1])) {
					// printf("config file: %s\n",argv[i+1]);
					g_statics.config_file = argv[i + 1];
				} else {
					printf("missing value for switch 'c'\n");
					return -1;
				}
				break;
			}
			case 'o': {
				if (i + 1 < argc && !is_switch(argv[i + 1])) {
					g_statics.fileout = argv[i + 1];
				} else {
					printf("missing value for switch 'o'\n");
					return -1;
				}
				break;
			}
			case 'i': {
				if (i + 1 < argc && !is_switch(argv[i + 1])) {
					g_statics.filein = argv[i + 1];
				} else {
					printf("missing value for switch 'i'\n");
					return -1;
				}
				break;
			}
			default: {
				show_help(argv[0]);
				return -1;
			}
			}
			i++;
		} else {
			show_help(argv[0]);
			return -1;
		}
	}

	return 0;
}
