/** Bytecode generator for Acorn compiler
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef acn_gen_h
#define acn_gen_h

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/* Macros to set instruction fields. */

/** Set a specific byte position in the byte-code instruction */
#define setbc_byte(p, x, ofs) \
  ((uint8_t *)(p))[LJ_ENDIAN_SELECT(ofs, 3-ofs)] = (uint8_t)(x)
#define setbc_op(p, x)	setbc_byte(p, (x), 0) //!< Change the instruction's op code
#define setbc_a(p, x)	setbc_byte(p, (x), 1) //!< Change the instruction's A byte
#define setbc_b(p, x)	setbc_byte(p, (x), 3) //!< Change the instruction's B byte
#define setbc_c(p, x)	setbc_byte(p, (x), 2) //!< Change the instruction's C byte
#define setbc_bx(p, x) \
	((p&0xffff) | (((uint16_t)(x))<<16))	//!< Change the instruction's bx field
#define setbc_j(p, x)	setbc_bx(p, ((int16_t)(x)+BCBIAS_J))  //!< Change the instruction's jump field

/* Macros to compose instructions. */
#define BCINS_ABC(o, a, b, c) \
  (((BCIns)(o))|((BCIns)(a)<<8)|((BCIns)(b)<<24)|((BCIns)(c)<<16)) //!< Build an ABC-based instruction
#define BCINS_ABx(o, a, bx) \
  (((BCIns)(o))|((BCIns)(a)<<8)|((uint16_t)(bx)<<16)) //!< Build an ABx-based instruction
#define BCINS_AJ(o, a, j)	BCINS_ABx(o, a, ((int16_t)(j)+BCBIAS_J)) //!< Build a jump instruction

// ***********
// Non-API C-Method functions
// ***********

/** Create a new bytecode method value. */
void genNew(CompInfo *comp);
/** Put new byte-code instruction to code array */
void genPutInstr(CompInfo *comp, AuintIdx loc, Instruction i);
/** Append new instruction to code array */
void genAddInstr(CompInfo *comp, Instruction i);
/** Add a literal and return its index */
int genAddLit(CompInfo *comp, Value val);
/** Look in reverse order for local variable, returning its register. Add if not found. */
int genLocalVar(CompInfo *comp, Value varnm);
/** Add a parameter */
void genAddParm(CompInfo *comp, Value sym);
/** Indicate the method has a variable number of parameters */
void genVarParms(CompInfo *comp);
/** Raise method's max stack size if register is above it */
void genMaxStack(CompInfo *comp, AuintIdx reg);
/** Set the jump instruction link chain starting at listip to jump to dest */
void genSetJumpList(CompInfo *comp, int listip, int dest);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif