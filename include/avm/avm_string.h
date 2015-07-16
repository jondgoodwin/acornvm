/** Implements strings, mutable byte-sequences.
 *
 * A string is a sequence of bytes with a specified length.
 * Although strings are often character text, they can be used to encode any data
 * (including unicode, control characters and '\0').
 *
 * String values are mutable; their content and size can be changed as needed.
 * Increases in size often requires allocating a larger block and copying the contents.
 * To minimize this, size it properly at creation or resize it using large increments.
 *
 * A string encoding may be the foundation for several Types, such as Acorn's
 * Bytes (1 byte per character) and Text (Unicode UTF-8 multi-byte characters) types.
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_string_h
#define avm_string_h

#include "avm/avm_memory.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

// ***********
// String info declaration and access macros
// ***********

/** Information about a string information block. (uses MemCommonInfoT) */
typedef struct StrInfo {
	MemCommonInfoT;
	char *str;		//!< Pointer to allocated buffer for bytes
	AuintIdx avail;	//!< Allocated size of character buffer
} StrInfo;

/** Free all of a string's allocated memory */
#define strFree(th, s) \
	mem_gcrealloc(th, (s)->str, (s)->avail + 1, 0); \
	mem_free(th, (s));

#define str_memsize(val) (sizeof(StrInfo) + (str_info(val)->avail) + 1)

/** Mark all in-use string values for garbage collection 
 * Increments how much allocated memory the string uses. */
#define strMark(th, s) \
	{mem_markobj(th, (s)->type); \
	vm(th)->gcmemtrav += str_memsize(s);}

/** Point to string information, by recasting a Value pointer */
#define str_info(val) (assert_exp(isEnc(val,StrEnc), (StrInfo*) val))

/** Point to the string's 0-terminated c-string value */
#define str_cstr(val) ((char*) (str_info(val)->str))

/** Return the length of the string's bytes (without 0-terminator) */
#define str_size(val) (str_info(val)->size)

// ***********
// Non-API String functions
// ***********

AuintIdx str_hash(Value th, Value val);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif