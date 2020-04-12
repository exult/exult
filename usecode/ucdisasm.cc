/*
 *  ucdisasm.cc - Disassembled usecode trace
 *
 *  Copyright (C) 1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2013  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif


#include <cstring>
#include <cstdio>
#include <iostream>

using std::strlen;
using std::cout;

#include "ucinternal.h"
#include "uctools.h"
#include "ucfunction.h"
#include "utils.h"
#include "useval.h"
#include "game.h"
#include "stackframe.h"
#include "ignore_unused_variable_warning.h"
#include "array_size.h"

int Usecode_internal::get_opcode_length(int opcode) {
	if (opcode >= 0 &&
	        static_cast<unsigned>(opcode) < array_size(opcode_table)) {

		return opcode_table[opcode].nbytes + 1;
	} else {
		return 0;
	}
}

void Usecode_internal::uc_trace_disasm(Stack_frame *frame) {
	uc_trace_disasm(frame->locals,
	                frame->num_args + frame->num_vars,
	                frame->function->statics,
	                frame->data,
	                frame->externs,
	                frame->code,
	                frame->ip);
}

void Usecode_internal::uc_trace_disasm(Usecode_value *locals, int num_locals,
                                       std::vector<Usecode_value> &locstatics,
                                       const uint8 *data, const uint8 *externals,
                                       const uint8 *code, const uint8 *ip) {
	ignore_unused_variable_warning(num_locals);
	int func_ip = static_cast<int>(ip - code);
	int opcode = *ip++;
	const uint8 *param_ip = ip;
	opcode_desc *pdesc = nullptr;

	if (opcode >= 0 && static_cast<unsigned>(opcode) < array_size(opcode_table))
		pdesc = &(opcode_table[opcode]);
	signed short immed;
	uint16 varref;
	sint16 staticref;
	unsigned short func;
	int immed32;
	int offset;

	std::printf("      %04X: ", func_ip);

	if (pdesc && pdesc->mnemonic) {
		std::printf("%s", pdesc->mnemonic);
		if (strlen(pdesc->mnemonic) < 4)
			std::printf("\t");
	} else {
		std::printf("<unknown>");
	}

	if (pdesc && pdesc->nbytes > 0) {
		switch (pdesc->type) {
		case op_byte:
			std::printf("\t%02x", *ip++);
			break;
		case op_immed:
			// Print immediate operand
			immed = Read2(ip);
			std::printf("\t%04hXH\t\t; %d", immed, immed);
			break;
		case op_immed32:
			immed32 = Read4s(ip);
			std::printf("\t%08XH\t\t; %d", immed32, immed32);
			break;
		case op_data_string:
		case op_data_string32: {
			const char *pstr;
			size_t len;
			// Print data string operand
			if (pdesc->type == op_data_string)
				offset = Read2(ip);
			else
				offset = Read4s(ip);

			pstr = reinterpret_cast<const char *>(data + offset);
			len = strlen(pstr);
			if (len > 20)
				len = 20 - 3;
			std::printf("\tL%04X\t\t; ", offset);
			for (unsigned i = 0; i < len; i++)
				std::printf("%c", pstr[i]);
			if (len < strlen(pstr))
				// String truncated
				std::printf("...");
		}
		break;
		case op_relative_jump:
			// Print jump desination
			offset = Read2(ip);
			std::printf("\t%04X",
			            (offset + func_ip + 1 + pdesc->nbytes) & 0xFFFF);
			break;
		case op_relative_jump32:
			offset = Read4s(ip);
			std::printf("\t%04X", offset + func_ip + 1 + pdesc->nbytes);
			break;
		case op_sloop:
			if (pdesc->nbytes == 11)
				ip++;
			Read2(ip);
			Read2(ip);
			Read2(ip);
			varref = Read2(ip);
			offset = Read2(ip);
			std::printf("\t[%04X], %04X\t= ", varref,
			            (offset + func_ip + 1 + pdesc->nbytes) & 0xFFFF);
			locals[varref].print(cout, true); // print value (short format)
			break;
		case op_sloop32:
			if (pdesc->nbytes == 13)
				ip++;
			Read2(ip);
			Read2(ip);
			Read2(ip);
			varref = Read2(ip);
			offset = Read4s(ip);
			std::printf("\t[%04X], %04X\t= ", varref,
			            offset + func_ip + 1 + pdesc->nbytes);
			locals[varref].print(cout, true); // print value (short format)
			break;
		case op_immed_and_relative_jump:
			immed = Read2(ip);
			offset = Read2(ip);
			std::printf("\t%04hXH, %04X", immed,
			            (offset + func_ip + 1 + pdesc->nbytes) & 0xFFFF);
			break;
		case op_immedreljump32:
			immed = Read2(ip);
			offset = Read4s(ip);
			std::printf("\t%04hXH, %04X", immed,
			            offset + func_ip + 1 + pdesc->nbytes);
			break;
		case op_call: {
			func = Read2(ip);
			immed = *ip++;
			const char **func_table = bg_intrinsic_table;
			if (Game::get_game_type() == SERPENT_ISLE) {
				if (Game::is_si_beta())
					func_table = sibeta_intrinsic_table;
				else
					func_table = si_intrinsic_table;
			}
			std::printf("\t_%s@%d\t; %04X", func_table[func], immed, func);
		}
		break;
		case op_extcall: {
			// Print extern call
			offset = Read2(ip);
			std::printf("\t[%04X]\t\t; %04XH", offset,
			            externals[2 * offset] + 256 * externals[2 * offset + 1]);
		}
		break;
		case op_varref:
			// Print variable reference
			varref = Read2(ip);
			std::printf("\t[%04X]\t\t= ", varref);
			locals[varref].print(cout, true); // print value (short)
			break;
		case op_flgref:
			// Print global flag reference
			offset = Read2(ip);
			std::printf("\tflag:[%04X]\t= ", offset);
			if (gflags[offset])
				std::printf("set");
			else
				std::printf("unset");
			break;
		case op_staticref:
			staticref = Read2(ip);
			std::printf("[%04X]\t= ", static_cast<uint16>(staticref));
			if (staticref < 0) {
				// global
				if (static_cast<unsigned>(-staticref) < statics.size())
					statics[-staticref].print(cout, true);
				else
					std::printf("<out of range>");
			} else {
				if (static_cast<unsigned>(staticref) < locstatics.size())
					locstatics[staticref].print(cout, true);
				else
					std::printf("<out of range>");
			}
			break;
		default:
			// Unknown type
			break;
		}
	}
	ip = param_ip; //restore IP back to opcode parameters

	// special cases:
	switch (opcode & 0x7f) {
	case 0x05: {    // JNE/JNE32.
		Usecode_value val;
		if (sp <= stack)
			val = Usecode_value(0);
		else
			val = *(sp - 1);

		if (val.is_false())
			std::printf("\t\t(jump taken)");
		else
			std::printf("\t\t(jump not taken)");
		break;
	}

	/*  maybe predict this?
	case 0x07:       // CMPS/CMPS32.
	{
	    int cnt = Read2(ip);    // # strings.
	    bool matched = false;

	    // only try to match if we haven't found an answer yet
	    while (!matched && !found_answer && cnt-- > 0) {
	        Usecode_value s;
	        if (sp + cnt <= stack)
	            s = Usecode_value(0);
	        else
	            s = *(sp - cnt - 1);
	        const char *str = s.get_str_value();
	        if (str && strcmp(str, user_choice) == 0) {
	            matched = true;
	            found_answer = true;
	        }
	    }
	    if (val.is_false())
	        std::printf("\t\t(jump taken)");
	    else
	        std::printf("\t\t(jump not taken)");
	    break;
	}
	break;
	*/
	}

	std::printf("\n");
}
