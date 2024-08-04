#include "istring.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

using namespace std;

#ifndef ATTR_NORET
#	ifdef __GNUC__
#		define ATTR_NORET __attribute__((noreturn))
#	else
#		define ATTR_NORET
#	endif
#endif

void rebuild() ATTR_NORET;

void rebuild() {
	FILE* fi = fopen("index", "r");
	if (fi == nullptr) {
		printf("Can't open index file.\n");
		exit(0);
	}
	FILE* fo = fopen("usecode", "wb");
	if (fo == nullptr) {
		printf("Can't create usecode file.\n");
		exit(0);
	}
	while (!feof(fi)) {
		char  s[10];
		char* err = fgets(s, 10, fi);
		assert(err != nullptr);

		char filename[18];
		strcpy(filename, s);
		char* pos = strchr(filename, '\n');
		if (pos) {
			*pos = '\0';
		}
		strcat(filename, ".uco");
		if (!feof(fi)) {
			s[strlen(s) - 1] = 0;
			printf("Writing function: %s... ", s);
			FILE* fi2 = fopen(filename, "rb");
			if (fi2 == nullptr) {
				printf("Can't open file %s\n", filename);
				exit(0);
			}
			while (!feof(fi2)) {
				const unsigned c = fgetc(fi2);
				if (!feof(fi2)) {
					fputc(c, fo);
				}
			}
			fclose(fi2);
			printf("done\n");
		}
	}
	exit(0);
}

int main(int argc, char* argv[]) {
	printf("Wody's Rip v0.005\nCopyright (c) 1999 Wody Dragon (a.k.a. Wouter "
		   "Dijkslag)\n");
	if (argc < 2 || (Pentagram::iequals(argv[1], "put") && argc != 3)) {
		printf("Syntax: rip <number>\tGets function <number> out of usecode "
			   "(and"
			   " index)\n\trip all\t\tGets all functions out of usecode (and "
			   "index)\n"
			   "\trip glue\tRecreate usecode file (needs all functions)\n"
			   "\trip index\tOnly get index\n\trip put <nr>\tInserts function"
			   " <nr> into the usecode file\n");
		exit(0);
	}

	constexpr const unsigned all_functions
			= std::numeric_limits<unsigned>::max();
	constexpr const unsigned only_index
			= std::numeric_limits<unsigned>::max() - 1u;

	unsigned number;
	bool     put = false;
	if (Pentagram::iequals(argv[1], "all")) {
		number = all_functions;
	} else if (Pentagram::iequals(argv[1], "glue")) {
		rebuild();    // note: this doesn't return
	} else if (Pentagram::iequals(argv[1], "index")) {
		number = only_index;
	} else if (Pentagram::iequals(argv[1], "put")) {
		sscanf(argv[2], "%x", &number);
		put = true;
	} else {
		sscanf(argv[1], "%x", &number);
	}

	FILE* fi = fopen("usecode", "rb+");
	if (fi == nullptr) {
		printf("Can't open usecode file.\n");
		exit(0);
	}
	FILE* fo2 = fopen("index", "w");
	if (fo2 == nullptr) {
		printf("Can't create index file.\n");
		exit(0);
	}
	while (true) {
		unsigned short fn;
		if (fread(&fn, 2, 1, fi) != 1) {
			break;
		}
		unsigned fs;
		bool     extended;
		if (fn == 0xFFFF) {
			extended   = true;
			size_t err = fread(&fn, 2, 1, fi);
			assert(err == 1);
			err = fread(&fs, 4, 1, fi);
			assert(err == 1);
		} else {
			extended = false;
			unsigned short temp;
			const size_t   err = fread(&temp, 2, 1, fi);
			assert(err == 1);
			fs = temp;
		}

		char s[10];
		char filename[18];
		if (number == all_functions || number == only_index || number == fn) {
			snprintf(s, sizeof(s), "%04X", fn);
			strcpy(filename, s);
			strcat(filename, ".uco");
			fprintf(fo2, "%s\n", s);
		}
		if (number == all_functions || number == fn) {
			if (!put) {
				printf("Writing function: %s... ", s);
				FILE* fo = fopen(filename, "wb");
				if (fo == nullptr) {
					printf("Can't open file %s\n", filename);
					exit(0);
				}

				if (extended) {
					unsigned short temp;
					temp = 0xFFFF;
					fwrite(&temp, 2, 1, fo);
					fwrite(&fn, 2, 1, fo);
					fwrite(&fs, 4, 1, fo);
				} else {
					fwrite(&fn, 2, 1, fo);
					unsigned short temp = fs;
					fwrite(&temp, 2, 1, fo);
				}

				for (unsigned i = 0; i < fs; i++) {
					fputc(fgetc(fi), fo);
				}
				fclose(fo);
				printf("done\n");
			} else {
				printf("Reading function: %s... ", s);
				FILE* fo = fopen(filename, "rb");
				if (fo == nullptr) {
					printf("Can't open file %s\n", filename);
					exit(0);
				}
				unsigned short fnc;
				size_t         err = fread(&fnc, 2, 1, fo);
				assert(err == 1);

				unsigned fsc;
				if (fnc == 0xFFFF) {
					if (extended == 0) {
						printf("Wrong header (u7) in object\n");
						exit(0);
					}
					err = fread(&fnc, 2, 1, fo);
					assert(err == 1);
					err = fread(&fsc, 4, 1, fo);
					assert(err == 1);
				} else {
					if (extended == 1) {
						printf("Wrong header (extended) in object\n");
						exit(0);
					}
					unsigned short temp;
					err = fread(&temp, 2, 1, fo);
					assert(err == 1);
					fsc = temp;
				}
				if (fnc != fn) {
					printf("Wrong function in object\n");
					exit(0);
				}
				if (fsc != fs) {
					printf("Wrong size in object\n");
					exit(0);
				}
				fseek(fi, ftell(fi), SEEK_SET); /* These two fseeks force my */
				for (unsigned i = 0; i < fs;
					 i++) {               /* Borland C++ 5.02 to read */
					fputc(fgetc(fo), fi); /* write. Without them they */
				}
				fclose(fo);                     /* don't work as I think */
				fseek(fi, ftell(fi), SEEK_SET); /* they should (the writing */
				printf("done\n");               /* doesn't work) */
			}
		} else { /* Skip function */
			fseek(fi, fs, SEEK_CUR);
		}
	}
	fclose(fi);
	fclose(fo2);
	printf("All done\n");
	return 0;
}
