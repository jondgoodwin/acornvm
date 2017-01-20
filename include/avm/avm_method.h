/** Implements methods built in C and Acorn (compiled to bytecode).
 *
 * Methods are callable procedures whose first implicit parameter is 'self'.
 * They always return at least one value.
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

/** The generic structure for method Values */
typedef struct MethodInfo {
	MemCommonInfoMeth; //!< Common method object header
} MethodInfo;

/** Flags2 is for a method's number of FIXED parameters (use as lval or rval) */
#define methodNParms(val) (((MethodInfo*)val)->flags2)

/** Flags1 is for a method's flags - can use as lval or rval */
#define methodFlags(val) (((MethodInfo*)val)->flags1)

// 0x80 reserved for Locked
#define METHOD_FLG_C			0x40 //!< The method is coded in C (vs. Bytecode)
#define METHOD_FLG_VARPARM		0x20 //!< The method accepts a variable number of parameters

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
	OpGetClosure,
	OpSetClosure,
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
	OpJEqN,
	OpJNeN,
	OpJLtN,
	OpJLeN,
	OpJGtN,
	OpJGeN,
	OpJSame,
	OpJDiff,
	OpLoadStd,
	OpGetProp,
	OpSetProp,
	OpGetActProp,
	OpSetActProp,
	OpGetCall,
	OpSetCall,
	OpReturn,
	OpTailCall,
	OpForPrep,
	OpRptPrep,
	OpRptCall
};

/** Information about a bytecode method */
typedef struct BMethodInfo {
	MemCommonInfoMeth;			//!< Common method header
	Instruction *code;		//!< Array of bytecode instructions (size is nbr of instructions)
	Value *lits;			//!< Array of literals used by this method
	AuintIdx avail;			//!< nbr of Instructions code is allocated for
	AuintIdx litsz;			//!< Allocated size of literal list
	AuintIdx nbrlits;		//!< Number of literals in lits
	AuintIdx nbrexterns;	//!< Number of externals in lits
	AuintIdx nbrlocals;		//!< Number of local variables in locals
	AuintIdx maxstacksize;	//!< Maximum size of stack needed to parms+locals
} BMethodInfo;

/** Mark all Func values for garbage collection 
 * Increments how much allocated memory the func uses. */
#define methodMark(th, m) \
	{if (!isCMethod(m) && ((BMethodInfo*)m)->nbrlits>0) \
		for (AuintIdx i=0; i<((BMethodInfo*)m)->nbrlits; i++) \
			mem_markobj(th, ((BMethodInfo*)m)->lits[i]); \
	}

/** Free all of an part's allocated memory */
#define methodFree(th, m) \
	{if (isCMethod(m)) mem_free(th, (CMethodInfo*)(m)); \
	else {\
		BMethodInfo* bm = (BMethodInfo*)m; \
		if (bm->code) mem_freearray(th, bm->code, bm->avail); \
		if (bm->lits) mem_freearray(th, bm->lits, bm->litsz); \
		mem_free(th, bm); \
	}}

// ***********
// Non-API C-method functions
// ***********

/** Build a new c-method value, pointing to a method written in C */
Value newCMethod(Value th, Value *dest, AcMethodp method);

/** Return codes from callPrep */
enum MethodTypes {
	MethodBad,	//!< Not a valid method (probably unknown method)
	MethodBC,	//!< Byte-code method
	MethodC,	//!< C-method
	MethodRet	//!< Bad method - but we want to do a C-return for tailcall
};

/** Prepare call to method value on stack (with parms above it). 
 * Specify how many return values to expect to find on stack.
 * Flags is 0 for normal get, 1 for set, and 2 for repeat get
 * Returns 0 if bad method, 1 if bytecode, 2 if C-method
 *
 * For c-methods, all parameters are passed, reserving 20 slots of stack space.
 * and then it is actually run.
 *
 * For byte code, parameters are massaged to ensure the expected number of
 * fixed parameters and establish a holding place for the variable parameters.
 */
MethodTypes callPrep(Value th, Value *methodval, int nexpected, int flags);

/** Execute byte-code method pointed at by thread's current call frame */
void methodRunBC(Value th);

/** Serialize an method's bytecode contents to indented text */
void methSerialize(Value th, Value str, int indent, Value arr);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif