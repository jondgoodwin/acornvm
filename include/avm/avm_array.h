/** Implements arrays: variable-sized, ordered collections of Values
 *
 * Arrays are like strings; however strings collect only bytes, whereas arrays
 * can collect values of any type. Arrays are the foundation for many Acorn types,
 * most notably List and Tuple, but also Array and all the compound types
 * (e.g., Range, Property).
 *
 * Array values are mutable; their content and size can be changed as needed.
 * Increases in size often requires allocating a larger block and copying the contents.
 * To minimize this, size it properly at creation or resize it using large increments.
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_array_h
#define avm_array_h

#include "avm/avm_memory.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

// ***********
// Array info declaration and access macros
// ***********

/** Information about an array information block. (uses MemCommonInfoT) */
typedef struct ArrInfo {
	MemCommonInfoT;
	Value *arr;		//!< Pointer to allocated array buffer
	AuintIdx avail;	//!< Allocated size of buffer
} ArrInfo;

/** Flags in flags1 */
#define ArrTuple 0x80	//!< Array is a tuple (meaningful to Acorn)

/** Point to array information, by recasting a Value pointer */
#define arr_info(val) (assert_exp(isEnc(val,ArrEnc), (ArrInfo*) val))

/** Return the number of Values stored in the array */
#define arr_size(val) (str_info(val)->size)

// ***********
// Non-API Array functions
// ***********


#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif