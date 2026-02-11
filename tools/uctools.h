#ifndef UCTOOLS_H
#define UCTOOLS_H

#include <array>
#include <string_view>

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
	op_sloop              = 10,
	op_pop                = 11,
	op_immed32            = 12,
	op_data_string32      = 13,
	op_relative_jump32    = 14,
	op_immedreljump32     = 15,
	op_sloop32            = 16,
	op_staticref          = 17,
	op_classvarref        = 18,
	op_immed_pair         = 19,
	op_argnum_reljump     = 20,
	op_argnum_reljump32   = 21,
	op_argnum             = 22,
	op_funid              = 23,
	op_funcall            = 24,
	op_clsfun             = 25,
	op_clsfun_vtbl        = 26,
	op_static_sloop       = 27,
	op_clsid              = 28,
	op_unconditional_jump = 29,
	op_uncond_jump32      = 30,
	op_funid32            = 31
};

// Opcode descriptor
struct opcode_desc {
	// Mnemonic - nullptr if not known yet
	const char* mnemonic;
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
constexpr const std::array opcode_table{
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 00
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 01
		opcode_desc{              "loop", 10,                   op_sloop, 0, 0}, // 02
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 03
		opcode_desc{         "startconv",  2,           op_relative_jump, 0, 0}, // 04
		opcode_desc{               "jne",  2,           op_relative_jump, 1, 0}, // 05
		opcode_desc{               "jmp",  2,      op_unconditional_jump, 0, 0}, // 06
		opcode_desc{              "cmps",  4,          op_argnum_reljump, 0, 0}, // 07 JSF
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 08
		opcode_desc{               "add",  0,                          0, 2, 1}, // 09
		opcode_desc{               "sub",  0,                          0, 2, 1}, // 0a
		opcode_desc{               "div",  0,                          0, 2, 1}, // 0b
		opcode_desc{               "mul",  0,                          0, 2, 1}, // 0c
		opcode_desc{               "mod",  0,                          0, 2, 1}, // 0d
		opcode_desc{               "and",  0,                          0, 2, 1}, // 0e
		opcode_desc{                "or",  0,                          0, 2, 1}, // 0f
		opcode_desc{               "not",  0,                          0, 1, 1}, // 10
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 11
		opcode_desc{               "pop",  2,                  op_varref, 1, 0}, // 12
		opcode_desc{        "push\ttrue",  0,                          0, 0, 1}, // 13
		opcode_desc{       "push\tfalse",  0,                          0, 0, 1}, // 14
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 15
		opcode_desc{             "cmpgt",  0,                          0, 2, 1}, // 16
		opcode_desc{             "cmplt",  0,                          0, 2, 1}, // 17
		opcode_desc{             "cmpge",  0,                          0, 2, 1}, // 18
		opcode_desc{             "cmple",  0,                          0, 2, 1}, // 19
		opcode_desc{             "cmpne",  0,                          0, 2, 1}, // 1a
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 1b
		opcode_desc{             "addsi",  2,             op_data_string, 0, 0}, // 1c
		opcode_desc{             "pushs",  2,             op_data_string, 0, 1}, // 1d
		opcode_desc{              "arrc",  2,                  op_argnum, 0, 1}, // 1e
		opcode_desc{             "pushi",  2,                   op_immed, 0, 1}, // 1f
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 20
		opcode_desc{              "push",  2,                  op_varref, 0, 1}, // 21
		opcode_desc{             "cmpeq",  0,                          0, 2, 1}, // 22
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 23
		opcode_desc{              "call",  2,                 op_extcall, 0, 0}, // 24
		opcode_desc{               "ret",  0,                          0, 0, 0}, // 25
		opcode_desc{              "aidx",  2,                  op_varref, 1, 1}, // 26
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 27
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 28
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 29
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 2a
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 2b
		opcode_desc{              "ret2",  0,                          0, 0, 0}, // 2c
		opcode_desc{              "retv",  0,                          0, 1, 0}, // 2d
		opcode_desc{          "initloop",  0,                          0, 0, 0}, // 2e
		opcode_desc{             "addsv",  2,                  op_varref, 0, 0}, // 2f
		opcode_desc{                "in",  0,                          0, 2, 1}, // 30
		opcode_desc{    "conv_something",  4, op_immed_and_relative_jump, 0, 0}, // 31
		opcode_desc{              "retz",  0,                          0, 0, 0}, // 32
		opcode_desc{               "say",  0,                          0, 0, 0}, // 33
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 34
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 35
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 36
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 37
		opcode_desc{            "callis",  3,                    op_call, 0, 1}, // 38
		opcode_desc{             "calli",  3,                    op_call, 0, 0}, // 39
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 3a
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 3b
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 3c
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 3d
		opcode_desc{     "push\titemref",  0,                          0, 0, 1}, // 3e
		opcode_desc{              "abrt",  0,                          0, 0, 0}, // 3f
		opcode_desc{           "endconv",  0,                          0, 0, 0}, // 40
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 41
		opcode_desc{             "pushf",  2,                  op_flgref, 0, 1}, // 42
		opcode_desc{              "popf",  2,                  op_flgref, 1, 0}, // 43
		opcode_desc{             "pushb",  1,                    op_byte, 0, 1}, // 44
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 45
		opcode_desc{      "setarrayelem",  2,                   op_immed, 2, 0}, // 46
		opcode_desc{             "calle",  2,                   op_funid, 1, 0}, // 47
		opcode_desc{     "push\teventid",  0,                          0, 0, 1}, // 48
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 49
		opcode_desc{              "arra",  0,                          0, 2, 1}, // 4a
		opcode_desc{      "pop\teventid",  0,                          0, 1, 0}, // 4b
		opcode_desc{           "dbgline",  2,                   op_immed, 0, 0}, // 4c
		opcode_desc{           "dbgfunc",  4,             op_data_string, 0, 0}, // 4d
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 4e - 4f
		opcode_desc{      "push\tstatic",  2,               op_staticref, 0, 1}, // 50
		opcode_desc{       "pop\tstatic",  2,               op_staticref, 1, 0}, // 51
		opcode_desc{             "callo",  2,                   op_funid, 1, 1}, // 52
		opcode_desc{           "callind",  0,                 op_funcall, 2, 0}, // 53
		opcode_desc{      "push\tclsvar",  2,             op_classvarref, 0, 1}, // 54
		opcode_desc{       "pop\tclsvar",  2,             op_classvarref, 1, 0}, // 55
		opcode_desc{             "callm",  2,                  op_clsfun, 0, 0}, // 56
		opcode_desc{            "callms",  4,             op_clsfun_vtbl, 0, 0}, // 57
		opcode_desc{         "clscreate",  2,                   op_clsid, 0, 0}, // 58
		opcode_desc{          "classdel",  0,                          0, 1, 0}, // 59
		opcode_desc{             "aidxs",  2,               op_staticref, 1, 1}, // 5a
		opcode_desc{"setstaticarrayelem",  2,                   op_immed, 2, 0}, // 5b
		opcode_desc{        "staticloop", 10,            op_static_sloop, 0, 0}, // 5c
		opcode_desc{        "aidxclsvar",  2,             op_classvarref, 1, 1}, // 5d
		opcode_desc{"setclsvararrayelem",  2,                   op_immed, 2, 0}, // 5e
		opcode_desc{        "clsvarloop", 10,                   op_sloop, 0, 0}, // 5f
		opcode_desc{      "push\tchoice",  0,                          0, 0, 1}, // 60
		opcode_desc{          "starttry",  2,           op_relative_jump, 0, 0}, // 61
		opcode_desc{            "endtry",  0,                          0, 0, 0}, // 62
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 63
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 64-67
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 68-6b
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 6c-6f
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 70-73
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 74-77
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 78-7b
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0},
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 7c-7f
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 80
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 81
		opcode_desc{            "loop32", 12,                 op_sloop32, 0, 0}, // 82
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 83
		opcode_desc{       "startconv32",  4,         op_relative_jump32, 0, 0}, // 84
		opcode_desc{             "jne32",  4,         op_relative_jump32, 1, 0}, // 85
		opcode_desc{             "jmp32",  4,           op_uncond_jump32, 0, 0}, // 86
		opcode_desc{            "cmps32",  6,        op_argnum_reljump32, 0, 0}, // 87
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 88
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 89
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 8a
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 8b
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 8c
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 8d
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 8e
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 8f
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 90
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 91
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 92
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 93
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 94
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 95
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 96
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 97
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 98
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 99
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 9a
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 9b
		opcode_desc{           "addsi32",  4,           op_data_string32, 0, 0}, // 9c
		opcode_desc{           "pushs32",  4,           op_data_string32, 0, 1}, // 9d
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // 9e
		opcode_desc{           "pushi32",  4,                 op_immed32, 0, 1}, // 9f
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // a0
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // a1
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // a2
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // a3
		opcode_desc{            "call32",  4,                 op_funid32, 0, 0}, // a4
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // a5
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // a6
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // a7
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // a8
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // a9
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // aa
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // ab
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // ac
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // ad
		opcode_desc{        "initloop32",  0,                          0, 0, 0}, // ae
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // af
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // b0
		opcode_desc{  "conv_something32",  6,          op_immedreljump32, 0, 0}, // b1
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // b2
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // b3
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // b4
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // b5
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // b6
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // b7
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // b8
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // b9
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // ba
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // bb
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // bc
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // bd
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // be
		opcode_desc{             "throw",  0,                          0, 1, 0}, // bf
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // c0
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // c1
		opcode_desc{          "pushfvar",  0,                          0, 1, 1}, // c2
		opcode_desc{           "popfvar",  0,                          0, 2, 0}, // c3
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // c4
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // c5
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // c6
		opcode_desc{           "calle32",  4,                 op_funid32, 1, 0}, // c7
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // c8
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // c9
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // ca
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // cb
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // cc
		opcode_desc{         "dbgfunc32",  8,           op_data_string32, 0, 0}, // cd
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // ce
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // cf
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // d0
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // d1
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // d2
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // d3
		opcode_desc{         "callindex",  1,                    op_byte, 0, 0}, // d4
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // d5
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // d6
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // d7
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // d8
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // d9
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // da
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // db
		opcode_desc{      "staticloop32", 12,                 op_sloop32, 0, 0}, // dc
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // dd
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // de
		opcode_desc{      "clsvarloop32", 12,                 op_sloop32, 0, 0}, // df
		opcode_desc{             nullptr,  0,                          0, 0, 0}, // e0
		opcode_desc{        "starttry32",  4,         op_relative_jump32, 0, 0}  // e1
};

// Embedded function table

/*
 *  Tables of usecode intrinsics:
 */
#define USECODE_INTRINSIC_PTR(NAME) std::string_view(#NAME)

constexpr const std::array bg_intrinsic_table{
#include "bgintrinsics.h"
};
constexpr const std::array si_intrinsic_table{
#include "siintrinsics.h"
};

constexpr const std::array sibeta_intrinsic_table{
#include "sibetaintrinsics.h"
};

#endif
