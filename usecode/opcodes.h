/**
 ** Opcodes.h - Opcode definitions for Usecode.
 **
 ** Written: 1/1/01 - JSF
 **/

#ifndef INCL_OPCODES
#define INCL_OPCODES 1

enum UsecodeOps {
	UC_INVALID = -1,
	UC_EXTOPCODE = 0x80,
	UC_LOOPTOP = 0x02,
	UC_CONVERSE = 0x04,
	UC_JNE = 0x05,
	UC_JMP = 0x06,
	UC_CMPS = 0x07,
	UC_ADD = 0x09,
	UC_SUB = 0x0a,
	UC_DIV = 0x0b,
	UC_MUL = 0x0c,
	UC_MOD = 0x0d,
	UC_AND = 0x0e,
	UC_OR = 0x0f,
	UC_NOT = 0x10,
	UC_POP = 0x12,
	UC_PUSHTRUE = 0x13,
	UC_PUSHFALSE = 0x14,
	UC_CMPGT = 0x16,
	UC_CMPLT = 0x17,
	UC_CMPGE = 0x18,
	UC_CMPLE = 0x19,
	UC_CMPNE = 0x1a,
	UC_ADDSI = 0x1c,
	UC_PUSHS = 0x1d,
	UC_ARRC = 0x1e,
	UC_PUSHI = 0x1f,
	UC_PUSH = 0x21,
	UC_CMPEQ = 0x22,
	UC_CALL = 0x24,
	UC_RET = 0x25,           // No return value.
	UC_AIDX = 0x26,
	UC_RET2 = 0x2c,          // Identical to UC_RET; UCC never emits this.
	UC_RETV = 0x2d,          // Returns value from stack.
	UC_LOOP = 0x2e,
	UC_ADDSV = 0x2f,
	UC_IN = 0x30,
	UC_CONVSMTH = 0x31,      // Audition opcode; UCC never emits this.
	UC_RETZ = 0x32,          // Returns zero.
	UC_SAY = 0x33,
	UC_CALLIS = 0x38,
	UC_CALLI = 0x39,
	UC_PUSHITEMREF = 0x3e,
	UC_ABRT = 0x3f,
	UC_THROW = 0xbf,         // Like abrt, but accepts an expression to be sent up.
	UC_CONVERSELOC = 0x40,   // CONVERSE jmps here.
	UC_PUSHF = 0x42,         // PUSH global flag.
	UC_POPF = 0x43,          // POP global flag.
	UC_PUSHB = 0x44,         // Push byte that follows.
	UC_POPARR = 0x46,        // Pop into array elem.
	UC_CALLE = 0x47,
	UC_PUSHEVENTID = 0x48,
	UC_ARRA = 0x4a,
	UC_POPEVENTID = 0x4b,
	UC_DBGLINE = 0x4c,       // SI debug opcode; UCC never emits this.
	UC_DBGFUNC = 0x4d,       // SI debug opcode; UCC never emits this.
	// Added for Exult:
	UC_PUSHSTATIC = 0x50,    // Push static.
	UC_POPSTATIC = 0x51,     // Pop static.
	UC_CALLO = 0x52,         // Call original.
	UC_CALLIND = 0x53,       // Call indirect.  Addr. on stack.
	UC_PUSHTHV = 0x54,       // Push this->var.
	UC_POPTHV = 0x55,        // Pop this->var.
	UC_CALLM = 0x56,         // Call method (index is param, class on stack).
	UC_CALLMS = 0x57,        // Call method (index is param, and so is vtable).
	UC_CLSCREATE = 0x58,     // Create class instance.
	UC_CLASSDEL = 0x59,      // Delete class instance.
	UC_AIDXS = 0x5a,         // Pop static array elem.
	UC_POPARRS = 0x5b,       // Pop into static array elem.
	UC_LOOPTOPS = 0x5c,      // Loop with static array
	UC_AIDXTHV = 0x5d,       // Pop this->var array elem.
	UC_POPARRTHV = 0x5e,     // Pop this->var array elem.
	UC_LOOPTOPTHV = 0x5f,    // Loop with this->var array.
	UC_PUSHCHOICE = 0x60,    // Pushes last selected user choice.
	UC_TRYSTART = 0x61,      // TRY/CATCH block start.
	UC_TRYEND = 0x62,        // TRY/CATCH block end.
	UC_PUSHFVAR = 0xc2,      // PUSH global flag using stack value.
	UC_POPFVAR = 0xc3,       // POP global flag using stack value.
	UC_CALLINDEX_OLD = 0xd3, // Call indirect; UCC never emits this.
	UC_CALLINDEX = 0xd4,     // Call indirect.  Addr. on stack. with arguments

	// 32-bit usecode
	UC_LOOPTOP32 = 0x82,
	UC_CONVERSE32 = 0x84,
	UC_JNE32 = 0x85,
	UC_JMP32 = 0x86,
	UC_CMPS32 = 0x87,
	UC_ADDSI32 = 0x9c,
	UC_PUSHS32 = 0x9d,
	UC_PUSHI32 = 0x9f,
	UC_CALL32 = 0xa4,
	UC_LOOP32 = 0xae,        // 32-bit version of UC_LOOP; UCC never emits this.
	UC_CONVSMTH32 = 0xb1,    // 32-bit audition opcode; UCC never emits this.
	UC_CALLE32 = 0xc7,
	UC_DBGFUNC32 = 0xcd,     // 32-bit version of SI debug opcode; UCC never emits this.
	UC_LOOPTOPS32 = 0xdc,    // 32-bit loop with static array.
	UC_LOOPTOPTHV32 = 0xdf,  // 32-bit loop with this->var array.
	UC_TRYSTART32 = 0xe1     // TRY/CATCH block, 32-bit version.
};

inline UsecodeOps &operator|=(UsecodeOps &lhs, int rhs) {
	lhs = static_cast<UsecodeOps>(static_cast<int>(lhs) | rhs);
	return lhs;
}

inline UsecodeOps &operator&=(UsecodeOps &lhs, int rhs) {
	lhs = static_cast<UsecodeOps>(static_cast<int>(lhs) & rhs);
	return lhs;
}

inline UsecodeOps operator|(UsecodeOps const &lhs, int rhs) {
	UsecodeOps ret = lhs;
	return ret |= rhs;
}

inline UsecodeOps operator&(UsecodeOps const &lhs, int rhs) {
	UsecodeOps ret = lhs;
	return ret &= rhs;
}

#endif
