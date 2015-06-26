/** Implements symbols, immutable byte-sequences.
 *
 * A symbol is similar to a string, as it is a collection of characters. However, symbols are
 * immutable and indexed in a symbol table so that any two symbols with identical characters
 * will be represented only once. This speeds up comparing whether two symbols are the same, 
 * making them excellent for hash table keys.
 *
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

/** Information about a symbol memory block. (uses MemCommonInfo) */
typedef struct SymInfo {
	MemCommonInfo;
	AHash hash; //!< Calculated hash
	// The symbol characters follow here: char sym[len+1];
} SymInfo;

/** Point to symbol information, by recasting a Value pointer */
#define sym_info(val) (assert_exp(isEnc(val,SymEnc), (SymInfo*) val))

/** Point to the symbol's 0-terminated c-string value */
#define sym_cstr(val) ((char*) (sym_info(val)+1))

/** Return the length of the symbol's string (without 0-terminator) */
#define sym_size(val) (sym_info(val)->size)

// ***********
// Non-API Symbol functions
// ***********

void sym_init(VmInfo* vm);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif