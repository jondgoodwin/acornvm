/** Implements methods, particularly those built in C.
 *
 * Methods are callable procedures whose first implicit parameter is 'self'
 *
 * A method implemented in C is only passed the current thread (th).
 * Its actual parameters are placed on the thread's stack.
 * It can make use of the C API functions to interact with the VM.
 * It returns an int specifying how values it returns to its caller.
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_method_h
#define avm_method_h

#include "avm/avm_memory.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/* ************************************
   Generic macros for methods
   ********************************* */

/** The common header fields for a method. */
#define MemCommonInfoMeth \
	MemCommonInfoGray; /**< Common info header */ \
	Value name;		/**< Method's name (a string) */ \
	Value source   /**< Method's source (Filename or Url) & anchor */

/** The generic structure for method Values */
typedef struct MethodInfo {
	MemCommonInfoMeth; //!< Common method object header
} MethodInfo;

/** Flags2 is for a method's number of FIXED parameters (use as lval or rval) */
#define methodNParms(val) (((MethodInfo*)val)->flags2)

/** Flags1 is for a method's flags - can use as lval or rval */
#define methodFlags(val) (((MethodInfo*)val)->flags1)

#define METHOD_FLG_C			0x80 //!< The method is coded in C (vs. Bytecode)
#define METHOD_FLG_VARPARM		0x40 //!< The method accepts a variable number of parameters

/** Is the value a method? */
#define isMethod(val) (isEnc(val, MethEnc))

/** Is the value a C method? (assumes we know it is a method) */
#define isCMethod(val) (assert_exp(isMethod(val), methodFlags(val) & METHOD_FLG_C))

/** Does the method have variable parameters? (assumes we know it is a method) */
#define isVarParm(val) (assert_exp(isMethod(val), methodFlags(val) & METHOD_FLG_VARPARM))


/* ************************************
   C methods
   ********************************* */

/** Information about a c-method */
typedef struct CMethodInfo {
	MemCommonInfoMeth; //!< Common method header

	AcMethodp methodp; //!< Address of method code
} CMethodInfo;


/* ************************************
   Bytecode methods
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

/** Information about a bytecode method */
typedef struct BMethodInfo {
	MemCommonInfoMeth;			//!< Common method header
	Instruction *code;		//!< Array of bytecode instructions (size is its size)
	Value *lits;			//!< Array of literals used by this method
	Value *locals;			//!< Array of local variables (& parms) used by method
	AuintIdx litsz;			//!< Allocated size of literal list
	AuintIdx nbrlits;		//!< Number of literals in lits
	AuintIdx localsz;		//!< Allocated size of local list
	AuintIdx nbrlocals;		//!< Number of local variables in locals
	AuintIdx maxstacksize;	//!< Maximum size of stack needed to parms+locals
} BMethodInfo;

/** Mark all Func values for garbage collection 
 * Increments how much allocated memory the func uses. */
#define methodMark(th, m) \
	{mem_markobj(th, (m)->name); \
	mem_markobj(th, (m)->source); \
	if (!isCMethod(m) && ((BMethodInfo*)m)->nbrlits>0) \
		for (AuintIdx i=0; i<((BMethodInfo*)m)->nbrlits; i++) \
			mem_markobj(th, ((BMethodInfo*)m)->lits[i]); \
	if (!isCMethod(m) && ((BMethodInfo*)m)->nbrlocals>0) \
		for (AuintIdx i=0; i<((BMethodInfo*)m)->nbrlocals; i++) \
			mem_markobj(th, ((BMethodInfo*)m)->locals[i]); \
	vm(th)->gcmemtrav += isCMethod(m)? sizeof(CMethodInfo) : sizeof(BMethodInfo);}

/** Free all of an part's allocated memory */
#define methodFree(th, m) \
	{if (isCMethod(m)) mem_free(th, (CMethodInfo*)(m)); \
	else {\
		BMethodInfo* bm = (BMethodInfo*)m; \
		if (bm->code) mem_freearray(th, bm->code, bm->size); \
		if (bm->lits) mem_freearray(th, bm->lits, bm->litsz); \
		if (bm->locals) mem_freearray(th, bm->locals, bm->localsz); \
		mem_free(th, bm); \
	}}

/** Acorn compilation state */
typedef struct Acorn {
	Acorn* prev;	//!< Previous compile state in chain
	Acorn* next;	//!< Next compile state in chain
	Value th;		//!< Current thread
	BMethodInfo* method; //!< Method being built
	AuintIdx ip;	//!< Instruction pointer into method->code
	AuintIdx reg_top; //!< Top of method's data stack
} Acorn;

// ***********
// Non-API C-method functions
// ***********

/** Build a new c-method value, pointing to a method written in C */
Value newCMethod(Value th, AcMethodp method);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif