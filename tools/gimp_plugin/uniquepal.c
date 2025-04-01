/**
 ** Transform an U7 palette file so that the colours are all unique.
 **     This seems to be necessary to be able to properly select colours in
 **     the Gimp.
 **
 **     syntax: uniquepal inputfile outputfile
 **/

/*
Copyright (C) 2001-2022  The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <stdio.h>
#include <stdlib.h>

unsigned char pal[768];

int coleq(int c1, int c2) {
	return (pal[c1 * 3] == pal[c2 * 3]) && (pal[c1 * 3 + 1] == pal[c2 * 3 + 1])
		   && (pal[c1 * 3 + 2] == pal[c2 * 3 + 2]);
}

int unique(int c1) {
	int c2;
	int equal = 0;
	for (c2 = 0; c2 < 256 && !equal; c2++) {
		if (c1 != c2 && coleq(c1, c2)) {
			equal = 1;
		}
	}
	return !equal;
}

int main(int argc, char** argv) {
	FILE* fp;
	int   occ[343]; /* 7*7*7 */
	int   i;
	int   tmp;
	int   c1;
	int   c2;
	int   found;
	int   diff;
	int   d1;
	int   d2;
	int   d3;
	int   md2;
	int   md3;

	if (argc < 3) {
		printf("You need to specify input and output file.\n");
		exit(1);
	}

	fp = fopen(argv[1], "r");
	if (!fp) {
		printf("Error opening file %s\n", argv[1]);
		exit(1);
	}

	for (i = 0; i < 768; i++) {
		tmp = getc(fp);
		if (tmp == EOF) {
			printf("Error reading file. Not a palette file?\n");
			fclose(fp);
			exit(1);
		}
		pal[i] = (unsigned char)tmp;
	}

	fclose(fp);

	for (c1 = 255; c1 >= 0; c1--) {
		/*    printf("%i: %i %i %i\n", c1, pal[c1*3], pal[c1*3+1],
		 * pal[c1*3+2]);*/
		if (!unique(c1)) {
			printf("colour %i not unique\n", c1);
			/* alter c1 slightly to a unique value */
			found = 0;
			for (diff = 1; diff < 16 && !found; diff++) {
				for (d1 = -diff; d1 <= diff && !found; d1++) {
					if ((int)pal[c1 * 3] + d1 < 0
						|| (int)pal[c1 * 3] + d1 > 63) {
						continue;
					}
					/* printf("d1=%i ", d1); */
					md2 = abs(diff - abs(d1));
					for (d2 = -md2; d2 <= md2 && !found; d2++) {
						if ((int)pal[c1 * 3 + 1] + d2 < 0
							|| (int)pal[c1 * 3 + 1] + d2 > 63) {
							continue;
						}
						/* printf("d2=%i ", d2); */
						d3 = -abs(diff - abs(d1) - abs(d2));
						/* printf("d3=%i ", d3); */
						if (((int)pal[c1 * 3 + 2]) + d3 >= 0
							&& ((int)pal[c1 * 3 + 2]) + d3 <= 63) {
							pal[c1 * 3]
									= (unsigned char)((int)pal[c1 * 3] + d1);
							pal[c1 * 3 + 1]
									= (unsigned char)((int)pal[c1 * 3 + 1]
													  + d2);
							pal[c1 * 3 + 2]
									= (unsigned char)((int)pal[c1 * 3 + 2]
													  + d3);
							printf("trying %i: %i %i %i\n", c1, pal[c1 * 3],
								   pal[c1 * 3 + 1], pal[c1 * 3 + 2]);
							found = unique(c1);
							if (!found) {
								pal[c1 * 3] = (unsigned char)((int)pal[c1 * 3]
															  - d1);
								pal[c1 * 3 + 1]
										= (unsigned char)((int)pal[c1 * 3 + 1]
														  - d2);
								pal[c1 * 3 + 2]
										= (unsigned char)((int)pal[c1 * 3 + 2]
														  - d3);
							}
						}

						if (!found) {
							d3 = abs(diff - abs(d1) - abs(d2));
							/* printf("d3=%i ", d3); */
							if ((int)pal[c1 * 3 + 2] + d3 >= 0
								&& (int)pal[c1 * 3 + 2] + d3 <= 63) {
								pal[c1 * 3] = (unsigned char)((int)pal[c1 * 3]
															  + d1);
								pal[c1 * 3 + 1]
										= (unsigned char)((int)pal[c1 * 3 + 1]
														  + d2);
								pal[c1 * 3 + 2]
										= (unsigned char)((int)pal[c1 * 3 + 2]
														  + d3);
								printf("trying %i: %i %i %i\n", c1, pal[c1 * 3],
									   pal[c1 * 3 + 1], pal[c1 * 3 + 2]);
								found = unique(c1);
								if (!found) {
									pal[c1 * 3]
											= (unsigned char)((int)pal[c1 * 3]
															  - d1);
									pal[c1 * 3 + 1]
											= (unsigned char)((int)
																	  pal[c1 * 3
																		  + 1]
															  - d2);
									pal[c1 * 3 + 2]
											= (unsigned char)((int)
																	  pal[c1 * 3
																		  + 2]
															  - d3);
								}
							}
						}
					}
				}
			}
			if (!found) {
				printf("oops... ran out of colours... this shouldn't have "
					   "happened\n");
				exit(1);
			}
		}
	}

	fp = fopen(argv[2], "w");

	for (i = 0; i < 768; i++) {
		putc(pal[i], fp);
	}

	fclose(fp);

	printf("Done!\n");
	return 0;
}
