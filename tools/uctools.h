#ifndef UCTOOLS_H
#define UCTOOLS_H

#include "array_size.h"

// Opcode flags
enum Opcode_flags {
    // Just a 16bit word
    op_immed = 0,
    // Will print a part of data string as comment
    op_data_string = 1,
    // Will add command offset before printing
    op_relative_jump = 2,
    // Will print third byte as decimal after comma
    op_call = 3,
    // Will print in square brackets
    op_varref = 4,
    // Will print in square brackets with "flag:" prefix
    op_flgref = 5,
    // Call of usecode function using extern table
    op_extcall = 6,
    // An immediate op. and then a rel. jmp address:  JSF
    op_immed_and_relative_jump = 7,
    // Just x bytes
    op_push = 8,
    // Just a byte
    op_byte = 9,
    // variable + relative jump (for the 'sloop')
    op_sloop = 10,
    op_pop = 11,
    op_immed32 = 12,
    op_data_string32 = 13,
    op_relative_jump32 = 14,
    op_immedreljump32 = 15,
    op_sloop32 = 16,
    op_staticref = 17,
    op_classvarref = 18,
    op_immed_pair = 19,
    op_argnum_reljump = 20,
    op_argnum_reljump32 = 21,
    op_argnum = 22,
    op_funid = 23,
    op_funcall = 24,
    op_clsfun = 25,
    op_clsfun_vtbl = 26,
    op_static_sloop = 27,
    op_clsid = 28,
    op_unconditional_jump = 29,
    op_uncond_jump32 = 30,
	op_funid32 = 31
};

// Opcode descriptor
struct opcode_desc {
	// Mnemonic - nullptr if not known yet
	const char *mnemonic;
	// Number of operand bytes
	int nbytes;
	// Type flags
	unsigned int type;
	// Number of elements poped from stack
	int num_pop;
	// Number of elements pushed into stack
	int num_push;
};

// Opcode table
static opcode_desc opcode_table[] = {
	{ nullptr, 0, 0, 0, 0 },                       // 00
	{ nullptr, 0, 0, 0, 0 },                       // 01
	{ "loop", 10, op_sloop, 0, 0 },     // 02
	{ nullptr, 0, 0, 0, 0 },                       // 03
	{ "startconv", 2, op_relative_jump, 0, 0 }, // 04
	{ "jne", 2, op_relative_jump, 1, 0 },               // 05
	{ "jmp", 2, op_unconditional_jump, 0, 0 },              // 06
	{ "cmps", 4, op_argnum_reljump, 0, 0 },         // 07 JSF
	{ nullptr, 0, 0, 0, 0 },                       // 08
	{ "add", 0, 0, 2, 1 },                  // 09
	{ "sub", 0, 0, 2, 1 },                  // 0a
	{ "div", 0, 0, 2, 1 },                  // 0b
	{ "mul", 0, 0, 2, 1 },                  // 0c
	{ "mod", 0, 0, 2, 1 },                  // 0d
	{ "and", 0, 0, 2, 1 },                  // 0e
	{ "or", 0, 0, 2, 1 },                       // 0f
	{ "not", 0, 0, 1, 1 },                  // 10
	{ nullptr, 0, 0, 0, 0 },                       // 11
	{ "pop", 2, op_varref, 1, 0 },                  // 12
	{ "push\ttrue", 0, 0, 0, 1 },                   // 13
	{ "push\tfalse", 0, 0, 0, 1 },              // 14
	{ nullptr, 0, 0, 0, 0 },                       // 15
	{ "cmpgt", 0, 0, 2, 1 },                    // 16
	{ "cmplt", 0, 0, 2, 1 },                 // 17
	{ "cmpge", 0, 0, 2, 1 },                    // 18
	{ "cmple", 0, 0, 2, 1 },                    // 19
	{ "cmpne", 0, 0, 2, 1 },                    // 1a
	{ nullptr, 0, 0, 0, 0 },                       // 1b
	{ "addsi", 2, op_data_string, 0, 0 },               // 1c
	{ "pushs", 2, op_data_string, 0, 1 },               // 1d
	{ "arrc", 2, op_argnum, 0, 1 },                 // 1e
	{ "pushi", 2, op_immed, 0, 1 },                 // 1f
	{ nullptr, 0, 0, 0, 0 },                       // 20
	{ "push", 2, op_varref, 0, 1 },                 // 21
	{ "cmpeq", 0, 0, 2, 1 },                    // 22
	{ nullptr, 0, 0, 0, 0 },                       // 23
	{ "call", 2, op_extcall, 0, 0 },                    // 24
	{ "ret", 0, 0, 0, 0 },                  // 25
	{ "aidx", 2, op_varref, 1, 1 },                 // 26
	{ nullptr, 0, 0, 0, 0 },                       // 27
	{ nullptr, 0, 0, 0, 0 },                       // 28
	{ nullptr, 0, 0, 0, 0 },                       // 29
	{ nullptr, 0, 0, 0, 0 },                       // 2a
	{ nullptr, 0, 0, 0, 0 },                       // 2b
	{ "ret2", 0, 0, 0, 0 },                 // 2c
	{ "retv", 0, 0, 1, 0 },                 // 2d
	{ "initloop", 0, 0, 0, 0 },             // 2e
	{ "addsv", 2, op_varref, 0, 0 },                // 2f
	{ "in", 0, 0, 2, 1 },                       // 30
	{ "conv_something", 4, op_immed_and_relative_jump, 0, 0 },          // 31
	{ "retz", 0, 0, 0, 0 },                  // 32
	{ "say", 0, 0, 0, 0 },                  // 33
	{ nullptr, 0, 0, 0, 0 },                       // 34
	{ nullptr, 0, 0, 0, 0 },                       // 35
	{ nullptr, 0, 0, 0, 0 },                       // 36
	{ nullptr, 0, 0, 0, 0 },                       // 37
	{ "callis", 3, op_call, 0, 1 },                 // 38
	{ "calli", 3, op_call, 0, 0 },                  // 39
	{ nullptr, 0, 0, 0, 0 },                       // 3a
	{ nullptr, 0, 0, 0, 0 },                       // 3b
	{ nullptr, 0, 0, 0, 0 },                       // 3c
	{ nullptr, 0, 0, 0, 0 },                       // 3d
	{ "push\titemref", 0, 0, 0, 1 },                // 3e
	{ "abrt", 0, 0, 0, 0 },                 // 3f
	{ "endconv", 0, 0, 0, 0 },                  // 40
	{ nullptr, 0, 0, 0, 0 },                       // 41
	{ "pushf", 2, op_flgref, 0, 1 },                    // 42
	{ "popf", 2, op_flgref, 1, 0 },                 // 43
	{ "pushb", 1, op_byte, 0, 1 },                  // 44
	{ nullptr, 0, 0, 0, 0 },                       // 45
	{ "setarrayelem", 2, op_immed, 2, 0 },          // 46
	{ "calle", 2, op_funid, 1, 0 },                 // 47
	{ "push\teventid", 0, 0, 0, 1 },                // 48
	{ nullptr, 0, 0, 0, 0 },                       // 49
	{ "arra", 0, 0, 2, 1 },                 // 4a
	{ "pop\teventid", 0, 0, 1, 0 },                 // 4b
	{ "dbgline", 2, op_immed, 0, 0 },                  // 4c
	{ "dbgfunc", 4, op_data_string, 0, 0 },            // 4d
	{ nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },           // 4e - 4f
	{ "push\tstatic", 2, op_staticref, 0, 1 },          // 50
	{ "pop\tstatic", 2, op_staticref, 1, 0 },           // 51
	{ "callo", 2, op_funid, 1, 1 },             // 52
	{ "callind", 0, op_funcall, 2, 0 },         // 53
	{ "push\tclsvar", 2, op_classvarref, 0, 1 },            // 54
	{ "pop\tclsvar", 2, op_classvarref, 1, 0 },         // 55
	{ "callm", 2, op_clsfun, 0, 0 },                    // 56
	{ "callms", 4, op_clsfun_vtbl, 0, 0 },  // 57
	{ "clscreate", 2, op_clsid, 0, 0 }, // 58
	{ "classdel", 0, 0, 1, 0 }, // 59
	{ "aidxs", 2, op_staticref, 1, 1 }, // 5a
	{ "setstaticarrayelem", 2, op_immed, 2, 0 },    // 5b
	{ "staticloop", 10, op_static_sloop, 0, 0 },    // 5c
	{ "aidxclsvar", 2, op_classvarref, 1, 1 }, // 5d
	{ "setclsvararrayelem", 2, op_immed, 2, 0 },    // 5e
	{ "clsvarloop", 10, op_sloop, 0, 0 },    // 5f
	{ "push\tchoice", 0, 0, 0, 1 },                   // 60
	{ "starttry", 2, op_relative_jump, 0, 0 },              // 61
	{ "endtry", 0, 0, 0, 0 },              // 62
	{ nullptr, 0, 0, 0, 0 },   // 63
	{ nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   // 64-67
	{ nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   // 68-6b
	{ nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   // 6c-6f
	{ nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   // 70-73
	{ nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   // 74-77
	{ nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   // 78-7b
	{ nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   { nullptr, 0, 0, 0, 0 },   // 7c-7f
	{ nullptr, 0, 0, 0, 0 },                       // 80
	{ nullptr, 0, 0, 0, 0 },                       // 81
	{ "loop32", 12, op_sloop32, 0, 0 },       // 82
	{ nullptr, 0, 0, 0, 0 },                       // 83
	{ "startconv32", 4, op_relative_jump32, 0, 0 }, // 84
	{ "jne32", 4, op_relative_jump32, 1, 0 },       // 85
	{ "jmp32", 4, op_uncond_jump32, 0, 0 },     // 86
	{ "cmps32", 6, op_argnum_reljump32, 0, 0 }, // 87
	{ nullptr, 0, 0, 0, 0 },                       // 88
	{ nullptr, 0, 0, 0, 0 },                   // 89
	{ nullptr, 0, 0, 0, 0 },                   // 8a
	{ nullptr, 0, 0, 0, 0 },                   // 8b
	{ nullptr, 0, 0, 0, 0 },                   // 8c
	{ nullptr, 0, 0, 0, 0 },                   // 8d
	{ nullptr, 0, 0, 0, 0 },                   // 8e
	{ nullptr, 0, 0, 0, 0 },                       // 8f
	{ nullptr, 0, 0, 0, 0 },                   // 90
	{ nullptr, 0, 0, 0, 0 },                       // 91
	{ nullptr, 0, 0, 0, 0 },               // 92
	{ nullptr, 0, 0, 0, 0 },               // 93
	{ nullptr, 0, 0, 0, 0 },           // 94
	{ nullptr, 0, 0, 0, 0 },                       // 95
	{ nullptr, 0, 0, 0, 0 },                   // 96
	{ nullptr, 0, 0, 0, 0 },                   // 97
	{ nullptr, 0, 0, 0, 0 },                   // 98
	{ nullptr, 0, 0, 0, 0 },                   // 99
	{ nullptr, 0, 0, 0, 0 },                   // 9a
	{ nullptr, 0, 0, 0, 0 },                       // 9b
	{ "addsi32", 4, op_data_string32, 0, 0 },       // 9c
	{ "pushs32", 4, op_data_string32, 0, 1 },       // 9d
	{ nullptr, 0, 0, 0, 0 },               // 9e
	{ "pushi32", 4, op_immed32, 0, 1 },             // 9f
	{ nullptr, 0, 0, 0, 0 },                       // a0
	{ nullptr, 0, 0, 0, 0 },               // a1
	{ nullptr, 0, 0, 0, 0 },                   // a2
	{ nullptr, 0, 0, 0, 0 },                       // a3
	{ "call32", 4, op_funid32, 0, 0 },     // a4
	{ nullptr, 0, 0, 0, 0 },                   // a5
	{ nullptr, 0, 0, 0, 0 },               // a6
	{ nullptr, 0, 0, 0, 0 },                       // a7
	{ nullptr, 0, 0, 0, 0 },                       // a8
	{ nullptr, 0, 0, 0, 0 },                       // a9
	{ nullptr, 0, 0, 0, 0 },                       // aa
	{ nullptr, 0, 0, 0, 0 },                       // ab
	{ nullptr, 0, 0, 0, 0 },                   // ac
	{ nullptr, 0, 0, 0, 0 },                   // ad
	{ "initloop32", 0, 0, 0, 0 },              // ae
	{ nullptr, 0, 0, 0, 0 },               // af
	{ nullptr, 0, 0, 0, 0 },                       // b0
	{ "conv_something32", 6, op_immedreljump32, 0, 0 },         // b1
	{ nullptr, 0, 0, 0, 0 },                   // b2
	{ nullptr, 0, 0, 0, 0 },                   // b3
	{ nullptr, 0, 0, 0, 0 },                       // b4
	{ nullptr, 0, 0, 0, 0 },                       // b5
	{ nullptr, 0, 0, 0, 0 },                       // b6
	{ nullptr, 0, 0, 0, 0 },                       // b7
	{ nullptr, 0, 0, 0, 0 },                   // b8
	{ nullptr, 0, 0, 0, 0 },                   // b9
	{ nullptr, 0, 0, 0, 0 },                       // ba
	{ nullptr, 0, 0, 0, 0 },                       // bb
	{ nullptr, 0, 0, 0, 0 },                       // bc
	{ nullptr, 0, 0, 0, 0 },                       // bd
	{ nullptr, 0, 0, 0, 0 },               // be
	{ "throw", 0, 0, 1, 0 },                 // bf
	{ nullptr, 0, 0, 0, 0 },                   // c0
	{ nullptr, 0, 0, 0, 0 },                       // c1
	{ "pushfvar", 0, 0, 1, 1 },                 // c2
	{ "popfvar", 0, 0, 2, 0 },                  // c3
	{ nullptr, 0, 0, 0, 0 },                   // c4
	{ nullptr, 0, 0, 0, 0 },                       // c5
	{ nullptr, 0, 0, 0, 0 },           // c6
	{ "calle32", 4, op_funid32, 1, 0 },         // c7
	{ nullptr, 0, 0, 0, 0 },               // c8
	{ nullptr, 0, 0, 0, 0 },                       // c9
	{ nullptr, 0, 0, 0, 0 },                   // ca
	{ nullptr, 0, 0, 0, 0 },                   // cb
	{ nullptr, 0, 0, 0, 0 },                   // cc
	{ "dbgfunc32", 8, op_data_string32, 0, 0 },    // cd
	{ nullptr, 0, 0, 0, 0 },               // ce
	{ nullptr, 0, 0, 0, 0 },                       // cf
	{ nullptr, 0, 0, 0, 0 },                   // d0
	{ nullptr, 0, 0, 0, 0 },                   // d1
	{ nullptr, 0, 0, 0, 0 },                   // d2
	{ nullptr, 0, 0, 0, 0 },                   // d3
	{ "callindex", 1, op_byte, 0, 0 },      // d4
	{ nullptr, 0, 0, 0, 0 },                   // d5
	{ nullptr, 0, 0, 0, 0 },                   // d6
	{ nullptr, 0, 0, 0, 0 },                   // d7
	{ nullptr, 0, 0, 0, 0 },               // d8
	{ nullptr, 0, 0, 0, 0 },                       // d9
	{ nullptr, 0, 0, 0, 0 },                   // da
	{ nullptr, 0, 0, 0, 0 },                   // db
	{ "staticloop32", 12, op_sloop32, 0, 0 },       // dc
	{ nullptr, 0, 0, 0, 0 },                       // dd
	{ nullptr, 0, 0, 0, 0 },                   // de
	{ "clsvarloop32", 12, op_sloop32, 0, 0 },      // df
	{ nullptr, 0, 0, 0, 0 },               // e0
	{ "starttry32", 4, op_relative_jump32, 0, 0 }              // e1

};

// Embedded function table

/*
 *  Tables of usecode intrinsics:
 */
#define USECODE_INTRINSIC_PTR(NAME) #NAME

const char *bg_intrinsic_table[] = {
#include "bgintrinsics.h"
};
const int bg_intrinsic_size = array_size(bg_intrinsic_table);
const char *si_intrinsic_table[] = {
#include "siintrinsics.h"
};
const int si_intrinsic_size = array_size(si_intrinsic_table);

const char *sibeta_intrinsic_table[] = {
#include "sibetaintrinsics.h"
};
const int sibeta_intrinsic_size = array_size(sibeta_intrinsic_table);

#endif

