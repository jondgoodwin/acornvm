/** Implements c-functions and c-methods.
 *
 * c-functions are just that, functions written in C, callable by Acorn code.
 * c-methods are like c-functions, except the first parameter is always 'self'.
 *
 * A c-function always has no c-parameters, and a return type of int.
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_func_h
#define avm_func_h

#include "avm/avm_memory.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/* ************************************
   Generic macros for C and Byte-code functions
   ********************************* */

/** The common header fields for a function. */
#define MemCommonInfoF \
	MemCommonInfoGray; /**< Common info header */ \
	Value name;		/**< Function's name (a string) */ \
	Value source   /**< Function's source (Filename or Url) & anchor */

/** The generic structure for function Values */
typedef struct FuncInfo {
	MemCommonInfoF; //!< Common function object header
} FuncInfo;

/** Flags2 is for a function's number of FIXED parameters (use as lval or rval) */
#define funcNParms(val) (((FuncInfo*)val)->flags2)

/** Flags1 is for a function's flags - can use as lval or rval */
#define funcFlags(val) (((FuncInfo*)val)->flags1)

#define FUNC_FLG_C			0x80 //!< The function is coded in C (vs. Bytecode)
#define FUNC_FLG_METHOD		0x40 //!< The function is a method (vs. method)
#define FUNC_FLG_VARPARM	0x20 //!< The function accepts a variable number of parameters

/** Is the value a function? */
#define isFunc(val) (isEnc(val, FuncEnc))

/** Is the value a C function? (assumes we know it is a function) */
#define isCFunc(val) (assert_exp(isFunc(val), funcFlags(val) & FUNC_FLG_C))

/** Is the value a method? (assumes we know it is a function) */
#define isMethod(val) (assert_exp(isFunc(val), funcFlags(val) & FUNC_FLG_METHOD))

/** Is the value a method? (assumes we know it is a function) */
#define isVarParm(val) (assert_exp(isFunc(val), funcFlags(val) & FUNC_FLG_VARPARM))


/* ************************************
   C functions
   ********************************* */

/** Information about a c-function */
typedef struct CFuncInfo {
	MemCommonInfoF; //!< Common function header: see function macros in avm_memory.h

	AcFuncp funcp; //!< Address of function code
} CFuncInfo;

/** Point to c-function information, by recasting a Value pointer */
#define cfn_info(val) (assert_exp(isEnc(val,CfnEnc), (CFuncInfo*) val))


/* ************************************
   Bytecode functions
   ********************************* */

/** A bytecode instruction */
typedef uint32_t Instruction;

/* Bytecode instruction format, 32 bit wide, fields of 8 or 16 bit:
 *
 * +----+----+----+----+
 * | B  | C  | A  | OP | Format ABC
 * +----+----+----+----+
 * |    D    | A  | OP | Format AD
 * +--------------------
 * MSB               LSB
 *
 * In-memory instructions are always stored in host byte order.
*/

typedef unsigned char BCOp;		//!< Bytecode operator.
typedef unsigned char BCIns;	//!< A byte within an instruction
typedef AuintIdx BCReg;			//!< Bytecode register.

/* Operand ranges and related constants. */
#define BCMAX_A		0xff		//!< Largest value for A field
#define BCMAX_B		0xff		//!< Largest value for B field
#define BCMAX_C		0xff		//!< Largest value for C field
#define BCMAX_BX	0xffff		//!< Largest value for Bx field
#define BCBIAS_J	0x8000		//!< Bias amount for signed Jmp field
#define NO_REG		BCMAX_A		//!< Indicates no register used
#define BCNO_JMP	-1			//!< No jump value stored in instruction
#define BCVARRET    BCMAX_B		//!< Variable number of return values

/* Macros to get instruction fields. */
#define bc_op(i)	((BCOp)((i)&0xff))			//!< Get instruction's op code
#define bc_a(i)		((BCReg)(((i)>>8)&0xff))	//!< Get instruction's A field
#define bc_ax(i)	((BCReg)(((i)>>8)))			//!< Get all but op code
#define bc_b(i)		((BCReg)((i)>>24))			//!< Get instruction's B field
#define bc_c(i)		((BCReg)(((i)>>16)&0xff))	//!< Get instruction's C field
#define bc_bx(i)		((uint16_t)((i)>>16))		//!< Get instruction's Bx field
#define bc_j(i)		((bc_bx(i)-BCBIAS_J))	//!< Get instruction's jump field

/** Byte Code Op Code Instructions */
enum ByteCodeOps {
	OpLoadReg,
	OpLoadRegs,
	OpLoadLit,
	OpLoadLitx,
	OpExtraArg,
	OpLoadPrim,
	OpLoadNulls,
	OpLoadVararg,
	OpGetGlobal,
	OpSetGlobal,
	OpJump,
	OpJNull,
	OpJNNull,
	OpJTrue,
	OpJFalse,
	OpJEq,
	OpJNe,
	OpJLt,
	OpJLe,
	OpJGt,
	OpJGe,
	OpJSame,
	OpJDiff,
	OpLoadStd,
	OpCall,
	OpReturn,
	OpTailCall,
	OpForPrep,
	OpRptPrep,
	OpRptCall
};

/** Information about a bytecode function */
typedef struct BFuncInfo {
	MemCommonInfoF;			//!< Common function header
	Instruction *code;		//!< Array of bytecode instructions (size is its size)
	Value *lits;			//!< Array of literals used by this function
	Value *locals;			//!< Array of local variables (& parms) used by function
	AuintIdx litsz;			//!< Allocated size of literal list
	AuintIdx nbrlits;		//!< Number of literals in lits
	AuintIdx localsz;		//!< Allocated size of local list
	AuintIdx nbrlocals;		//!< Number of local variables in locals
	AuintIdx maxstacksize;	//!< Maximum size of stack needed to parms+locals
} BFuncInfo;

/** Mark all Func values for garbage collection 
 * Increments how much allocated memory the func uses. */
#define funcMark(th, f) \
	{mem_markobj(th, (f)->name); \
	mem_markobj(th, (f)->source); \
	if (!isCFunc(f) && ((BFuncInfo*)f)->nbrlits>0) \
		for (AuintIdx i=0; i<((BFuncInfo*)f)->nbrlits; i++) \
			mem_markobj(th, ((BFuncInfo*)f)->lits[i]); \
	if (!isCFunc(f) && ((BFuncInfo*)f)->nbrlocals>0) \
		for (AuintIdx i=0; i<((BFuncInfo*)f)->nbrlocals; i++) \
			mem_markobj(th, ((BFuncInfo*)f)->locals[i]); \
	vm(th)->gcmemtrav += isCFunc(f)? sizeof(CFuncInfo) : sizeof(BFuncInfo);}

/** Free all of an part's allocated memory */
#define funcFree(th, f) \
	{if (isCFunc(f)) mem_free(th, (CFuncInfo*)(f)); \
	else {\
		BFuncInfo* bf = (BFuncInfo*)f; \
		if (bf->code) mem_freearray(th, bf->code, bf->size); \
		if (bf->lits) mem_freearray(th, bf->lits, bf->litsz); \
		if (bf->locals) mem_freearray(th, bf->locals, bf->localsz); \
		mem_free(th, bf); \
	}}

/** Acorn compilation state */
typedef struct Acorn {
	Acorn* prev;	//!< Previous compile state in chain
	Acorn* next;	//!< Next compile state in chain
	Value th;		//!< Current thread
	BFuncInfo* func; //!< Function being built
	AuintIdx ip;	//!< Instruction pointer into func->code
	AuintIdx reg_top; //!< Top of function's data stack
} Acorn;

// ***********
// Non-API C-Function functions
// ***********

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif