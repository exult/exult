#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "uctools.h"

static opcode_desc push_table[] = {
	{"true", 0, 0x13, 0, 0},
	{"false", 0, 0x14, 0, 0},
	{"itemref", 0, 0x3e, 0, 0},
	{"eventid", 0, 0x48, 0, 0}
};

static opcode_desc pop_table[] = {
	{"eventid", 0, 0x4b, 0, 0}
};

static const char *compiler_table[] = {
	".argc",
	".extern",
	".externsize",
	".localc"
};

#define MAX_LABELS 3500
#define TOKEN_LENGTH 25600

char token[TOKEN_LENGTH], *token2, curlabel[256], indata;
int pass,offset;
unsigned byte, word, funcnum, datasize, codesize;
int extended;

char labels[MAX_LABELS][10];
int offsets[MAX_LABELS];
unsigned lindex;

FILE *fo, *fi;

void emit_byte(unsigned i) {
	if (pass > 0) {
		fputc(i, fo);
		if (indata) datasize++;
	}
	offset++;
	codesize++;
}

void emit_word(unsigned i) {
	emit_byte(i & 0xff);
	emit_byte((i >> 8) & 0xff);
}

void emit_dword(unsigned i) {
	emit_byte(i & 0xff);
	emit_byte((i >> 8) & 0xff);
	emit_byte((i >> 16) & 0xff);
	emit_byte((i >> 24) & 0xff);
}

inline void emit_byte(int i) {
	emit_byte(static_cast<unsigned>(i));
}

inline void emit_word(int i) {
	emit_word(static_cast<unsigned>(i));
}

inline void emit_dword(int i) {
	emit_dword(static_cast<unsigned>(i));
}

void add_label() {
	unsigned i;
	if (token[strlen(token) - 1] == ':')
		token[strlen(token) - 1] = 0;
	if (lindex >= MAX_LABELS) {
		printf("Too many labels.\n");
		exit(0);
	}
	for (i = 0; i < lindex; i++) {
		if (!strcasecmp(token, labels[i])) {
			printf("Warning: label '%s' already exists.\n", token);
			return;
		}
	}
	strcpy(labels[lindex], token);
	offsets[lindex++] = offset;
}

int get_label(void) {
	unsigned i;
	for (i = 0; i < lindex; i++) {
		if (!strcasecmp(token, labels[i]))
			return(offsets[i]);
	}
	printf("Warning: label '%s' does not exist.\n", token);
	if (token[0] == 'L')
		sscanf(token, "L%x", &word);
	else
		sscanf(token, "%x", &word);
	return(word);
}

void check_jump_label_16(int label) {
	if (label < -32768 || label > 32767)
		printf("Warning: offset too big for 16 bit at label %s!\n", curlabel);
}

void check_data_label_16(int label) {
	if (label > 65535)
		printf("Warning: offset too big for 16 bit at label %s!\n", curlabel);
}

int find_intrinsic(const char **func_table, unsigned funsize, const char *name) {
	unsigned i;
	for (i = 0; i < funsize; i++) {
		if (!strcasecmp(name, func_table[i]))
			return(i);
	}
	printf("Warning: intrinsic '%s' does not exist.\n", name);
	return(0);
}

void read_token(FILE *fi) {
	int i = 0, c = 32;
	while (((c == ' ') || (c == '\t') || (c == '\n') || (c == ',')) && (!feof(fi)))
		c = fgetc(fi);
	while (!((c == ' ') || (c == '\t') || (c == '\n') || (c == ',')) && (!feof(fi))) {
		if (i >= TOKEN_LENGTH - 1) {
			fprintf(stderr, "Error: token too long!\n");
			exit(-1);
		}
		token[i++] = c;
		if (c == ';') {
			while ((c = fgetc(fi)) != '\n') /* do nothing */ ;
			i = 0;
		}
		if (c == 39) {
			while ((c = fgetc(fi)) != '\n') {
				if (i >= TOKEN_LENGTH - 1) {
					fprintf(stderr, "Error: token too long!\n");
					exit(-1);
				}
				token[i++] = c;
			}
			ungetc(c, fi);
			i--;
		}
		c = fgetc(fi);
	}

	if (i >= TOKEN_LENGTH - 1) {
		fprintf(stderr, "Error: token too long!\n");
		exit(-1);
	}
	token[i] = 0;
}

int main(int argc,char *argv[]) {
	unsigned i, opsize = sizeof(opcode_table) / sizeof(opcode_desc),
	       pushsize = sizeof(push_table) / sizeof(opcode_desc),
	       popsize = sizeof(pop_table) / sizeof(opcode_desc),
	       compsize = sizeof(compiler_table) / sizeof(char *);
	int label;
	const char **func_table = bg_intrinsic_table;
	int funsize = bg_intrinsic_size;
	int findex = 1;         // Index in argv of 1st filename.
	unsigned int opcodetype;
	indata = codesize = datasize = 0;
	/*  printf("Wody's Usecode Compiler v0.009\nCopyright (c) 1999 Wody "
	    "Dragon (a.k.a. Wouter Dijkslag)\n");*/
	if (argc < 3) {
		printf("syntax: %s [-s|-b] infile outfile\n", argv[0]);
		exit(0);
	}
	// Serpent Isle?
	if (strcmp(argv[1], "-s") == 0) {
		findex++;
		func_table = si_intrinsic_table;
		funsize = si_intrinsic_size;
	} else if (strcmp(argv[1], "-b") == 0) {
		findex++;
		func_table = sibeta_intrinsic_table;
		funsize = sibeta_intrinsic_size;
	}


	lindex = 0;
	for (pass = 0; pass < 2; pass++) {
		//          printf("Pass %d\n",pass+1);
		if ((fi = fopen(argv[findex], "r")) == NULL) {
			printf("Can't open infile for reading\n");
			exit(0);
		}
		if ((fo = fopen(argv[findex + 1], "wb")) == NULL) {
			printf("Can't open outfile for writing\n");
			exit(0);
		}
		while (!feof(fi)) {
			read_token(fi);
			if (strlen(token) > 1 && token[strlen(token) - 1] == ':') {
				token[strlen(token) - 1] = 0; // remove trailing ':'
				if (pass == 0)
					add_label();
				strcpy(curlabel, token);
			} else if (!strcmp(token, ".code")) {
				indata = 0;
				offset = 0;
			} else if (!strcmp(token, ".data")) {
				if (extended == 0) {
					emit_word(funcnum);
					emit_word(0);
					emit_word(0);
					codesize = 2;
				} else {
					emit_word(-1);
					emit_word(funcnum);
					emit_dword(0);
					emit_dword(0);
					codesize = 4;
				}

				indata = 1;
				offset = 0;

			} else if (!strcmp(token, ".funcnumber")) {
				read_token(fi);
				sscanf(token, "%x", &funcnum);
				printf("Function %04X\n", funcnum);
				// codesize=2;
				extended = 0;
			} else if (!strcmp(token, ".ext32")) {
				extended = 1;
			} else if (!strcmp(token, ".msize") || !strcmp(token, ".dsize")) {
				// Ignore either of these.
			} else if (token[0] == '.') {
				indata = 0;
				for (i = 0; i < compsize; i++)
					if (!strcasecmp(compiler_table[i], token)) {
						read_token(fi);
						sscanf(token, "%x", &word);
						emit_word(word);
					}
			} else if (!strcmp(token, "db")) {
				read_token(fi);
				if (token[0] == 39)
					for (i = 1; i < strlen(token); i++)
						emit_byte(token[i]);
				else {
					sscanf(token, "%x", &byte);
					emit_byte(byte);
				}
			} else if (!strcasecmp(token, "dw")) {
				read_token(fi);
				sscanf(token, "%x", &word);
				emit_word(word);
			} else
				for (i = 0; i < opsize; i++) {
					if (!opcode_table[i].mnemonic) continue;
					if (!strcasecmp(opcode_table[i].mnemonic, token)) {
						if (opcode_table[i].nbytes == 0 && opcode_table[i].type == 0)
							emit_byte(i);
						else {
							opcodetype = opcode_table[i].type;
							if (i == 0x21) opcodetype = op_push;
							if (i == 0x12) opcodetype = op_pop;
							switch (opcodetype) {
							case op_byte:
								emit_byte(i);
								read_token(fi);
								sscanf(token, "%x", &word);
								emit_byte(word);
								break;
							case op_call:
								emit_byte(i);
								read_token(fi);
								if ((token2 = strchr(token, '@')) != NULL) {
									*token2++ = 0;
									if (token[0] != '_')
										word = find_intrinsic(func_table, funsize, token);
									else {
										read_token(fi);
										sscanf(token, "(%x)", &word);
									}
									emit_word(word);
									sscanf(token2, "%u", &word);
								} else {
									sscanf(token, "%x", &word);
									emit_word(word);
									read_token(fi);
									sscanf(token, "%u", &word);
								}
								emit_byte(word);
								break;
							case op_data_string:
								emit_byte(i);
								read_token(fi);
								label = get_label();
								check_data_label_16(label);
								emit_word(label);
								break;
							case op_data_string32:
								emit_byte(i);
								read_token(fi);
								emit_dword(get_label());
								break;
							case op_extcall:
							case op_varref:
								emit_byte(i);
								read_token(fi);
								if (!memcmp(token, "extern:", sizeof("extern:")-1))
									sscanf(token, "extern:[%x]", &word);
								else
									sscanf(token, "[%x]", &word);
								emit_word(word);
								break;
							case op_flgref:
								emit_byte(i);
								read_token(fi);
								sscanf(token, "flag:[%x]", &word);
								emit_word(word);
								break;
							case op_push:
								read_token(fi);
								for (i = 0; i < pushsize; i++) {
									if (!strcasecmp(push_table[i].mnemonic, token)) {
										emit_byte(push_table[i].type);
										break;
									}
								}
								if (i == pushsize) {
									emit_byte(0x21);
									sscanf(token, "[%x]", &word);
									emit_word(word);
								}
								break;
							case op_pop:
								read_token(fi);
								for (i = 0; i < popsize; i++) {
									if (!strcasecmp(pop_table[i].mnemonic, token)) {
										emit_byte(pop_table[i].type);
										break;
									}
								}
								if (i == popsize) {
									emit_byte(0x12);
									sscanf(token, "[%x]", &word);
									emit_word(word);
								}
								break;
							case op_immed:
							case op_argnum:
							case op_funid:
							case op_clsfun:
							case op_clsid:
								emit_byte(i);
								read_token(fi);
								sscanf(token, "%x", &word);
								emit_word(word);
								break;
							case op_immed32:
							case op_funid32:
								emit_byte(i);
								read_token(fi);
								sscanf(token, "%x", &word);
								emit_dword(word);
								break;
							case op_relative_jump:
							case op_unconditional_jump:
								emit_byte(i);
								read_token(fi);
								if (pass == 1) {
									// printf("%x, %x, %x\n", get_label(), offset, get_label() - offset-2);
									label = get_label() - offset - 2;
									check_jump_label_16(label);
									emit_word(label);
								} else
									emit_word(-1);
								break;
							case op_relative_jump32:
							case op_uncond_jump32:
								emit_byte(i);
								read_token(fi);
								if (pass == 1) {
									// printf("%x, %x, %x\n", get_label(), offset, get_label() - offset-2);
									emit_dword(get_label() - offset - 4);
								} else
									emit_dword(-1);
								break;
							case op_immed_and_relative_jump:
							case op_argnum_reljump:
								emit_byte(i);
								read_token(fi);
								sscanf(token, "%x", &word);
								emit_word(word);
								read_token(fi);
								if (pass == 1) {
									label = get_label() - offset - 2;
									check_jump_label_16(label);
									emit_word(label);
								} else
									emit_word(-1);
								break;
							case op_immedreljump32:
							case op_argnum_reljump32:
								emit_byte(i);
								read_token(fi);
								sscanf(token, "%x", &word);
								emit_word(word);
								read_token(fi);
								if (pass == 1)
									emit_dword(get_label() - offset - 4);
								else
									emit_dword(-1);
								break;
							case op_sloop:
							case op_static_sloop:
#if 0
								emit_byte(0x2E);
								if (pass == 0) {
									sscanf(curlabel, "%x:", &word);
									sprintf(token, "%04X:", word + 1);
									printf("adding sloop label %s (curlabel=%s)\n", token, curlabel);
									add_label();
								}
#endif
								emit_byte(0x02);
								read_token(fi);
								sscanf(token, "[%x]", &word);
								emit_word(word);
								read_token(fi);
								sscanf(token, "[%x]", &word);
								emit_word(word);
								read_token(fi);
								sscanf(token, "[%x]", &word);
								emit_word(word);
								read_token(fi);
								sscanf(token, "[%x]", &word);
								emit_word(word);
								read_token(fi);
								sscanf(token, "%x", &word);
								if (pass == 1) {
									label = get_label() - offset - 2;
									check_jump_label_16(label);
									emit_word(label);
								} else
									emit_word(-1);
								break;
							case op_sloop32:
#if 0
								emit_byte(0xAE);
								if (pass == 0) {
									sscanf(curlabel, "%x:", &word);
									sprintf(token, "%04X:", word + 1);
									printf("adding sloop label %s (curlabel=%s)\n", token, curlabel);
									add_label();
								}
#endif
								emit_byte(0x82);
								read_token(fi);
								sscanf(token, "[%x]", &word);
								emit_word(word);
								read_token(fi);
								sscanf(token, "[%x]", &word);
								emit_word(word);
								read_token(fi);
								sscanf(token, "[%x]", &word);
								emit_word(word);
								read_token(fi);
								sscanf(token, "[%x]", &word);
								emit_word(word);
								read_token(fi);
								sscanf(token, "%x", &word);
								if (pass == 1)
									emit_dword(get_label() - offset - 2);
								else
									emit_dword(-1);
								break;
							default:
								break;
							}
						}
					}

				}
		}

		if (extended == 0) {
			fseek(fo, 2, SEEK_SET);
			indata = 0;
			i = codesize;

			if (codesize > 65535) {
				printf("Error: code size > 64Kb and not in ext32 mode!\n");
			}
			emit_word(i);

			if (datasize > 65535) {
				printf("Error: data size > 64Kb and not in ext32 mode!\n");
			}

			emit_word(datasize);
		} else {
			fseek(fo, 4, SEEK_SET);
			indata = 0;
			i = codesize;
			emit_dword(i);
			emit_dword(datasize);
		}
		fclose(fo);
		fclose(fi);
	}
	return 0;
}
