/** Implements symbols, immutable byte-sequences.
 * Two separately-created symbols with the same sequence of bytes will have the same Value.
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_symbol_h
#define avm_symbol_h

#include "avm/avm_memory.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

// ***********
// Symbol info declaration and access macros
// ***********

/** Information about a symbol memory block. */
typedef struct SymInfo {
	MemCommonInfo;
	AHash hash; //!< Calculated hash
	Auint len;  //!< number of characters
	// The symbol characters follow here: char sym[len+1];
} SymInfo;

/** Point to symbol information, by recasting a Value pointer */
#define sym_info(val) (assert_exp(isEnc(val,SymEnc), (SymInfo*) val))

/** Point to the symbol's 0-terminated c-string value */
#define sym_cstr(val) ((char*) (sym_info(val)+1))

/** Return the length of the symbol's string (without 0-terminator) */
#define sym_size(val) (sym_info(val)->len)

// ***********
// Non-API Symbol functions
// ***********

void sym_init(VmInfo* vm);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif