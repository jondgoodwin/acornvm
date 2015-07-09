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

#ifndef avm_cfunc_h
#define avm_cfunc_h

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
	MemCommonInfo; \
	Value name;		/**< Function's name (a string) */ \
	Value source   /**< Function's source (Filename or Url) & anchor */

/** The generic structure for function Values */
typedef struct FuncInfo {
	MemCommonInfoF;
} FuncInfo;

/* Flags2 is for a function's number of FIXED parameters (use as lval or rval) */
#define funcNParms(val) (((FuncInfo*)val)->flags2)

/* Flags1 is for a function's flags - can use as lval or rval */
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
	MemCommonInfoF; // Common function header: see function macros in avm_memory.h

	AcFuncp funcp; //!< Address of function code
} CFuncInfo;

/** Point to c-function information, by recasting a Value pointer */
#define cfn_info(val) (assert_exp(isEnc(val,CfnEnc), (CFuncInfo*) val))


/* ************************************
   Bytecode functions
   ********************************* */

/** A bytecode instruction */
typedef uint32_t Instruction;

/** Information about a bytecode function */
typedef struct BFuncInfo {
	MemCommonInfoF;
	Instruction *code;
	AuintIdx maxstacksize;
} BFuncInfo;

// ***********
// Non-API C-Function functions
// ***********

/** Execute byte-code program pointed at by thread's current call frame */
void bfnRun(Value th);


#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif