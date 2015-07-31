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
#define setbc_byte(p, x, ofs) \
  ((uint8_t *)(p))[LJ_ENDIAN_SELECT(ofs, 3-ofs)] = (uint8_t)(x)
#define setbc_op(p, x)	setbc_byte(p, (x), 0)
#define setbc_a(p, x)	setbc_byte(p, (x), 1)
#define setbc_b(p, x)	setbc_byte(p, (x), 3)
#define setbc_c(p, x)	setbc_byte(p, (x), 2)
#define setbc_d(p, x) \
  ((uint16_t *)(p))[LJ_ENDIAN_SELECT(1, 0)] = (uint16_t)(x)
#define setbc_j(p, x)	setbc_d(p, (BCPos)((int32_t)(x)+BCBIAS_J))

/* Macros to compose instructions. */
#define BCINS_ABC(o, a, b, c) \
  (((BCIns)(o))|((BCIns)(a)<<8)|((BCIns)(b)<<24)|((BCIns)(c)<<16))
#define BCINS_ABx(o, a, bx) \
  (((BCIns)(o))|((BCIns)(a)<<8)|((uint16_t)(bx)<<16))
#define BCINS_AJ(o, a, j)	BCINS_ABx(o, a, ((int16_t)(j)+BCBIAS_J))

// ***********
// Non-API C-Function functions
// ***********

/** Create a new bytecode function/method value. */
void genNew(Acorn *ac, int ismeth, Value name, Value src);
/** Put new byte-code instruction to code array */
void genPutInstr(Acorn *ac, AuintIdx loc, Instruction i);
/** Append new instruction to code array */
void genAddInstr(Acorn *ac, Instruction i);
/** Add a literal and return its index */
int genAddLit(Acorn *ac, Value val);
/** Look in reverse order for local variable, returning its register. Add if not found. */
int genLocalVar(Acorn *ac, Value varnm);
/** Add a parameter */
void genAddParm(Acorn *ac, Value sym);
/** Indicate the function has a variable number of parameters */
void genVarParms(Acorn *ac);
/** Raise function's max stack size if register is above it */
void genMaxStack(Acorn *ac, AuintIdx reg);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif