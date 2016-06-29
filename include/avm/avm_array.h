/** Implements arrays: variable-sized, ordered collections of Values
 *
 * Arrays are like strings; however strings collect only bytes, whereas arrays
 * can collect values of any type. Arrays are the foundation for many Acorn types,
 * most notably List, but also Array and all the compound types
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
	MemCommonInfoT;	//!< Common typed object header
	Value *arr;		//!< Pointer to allocated array buffer
	AuintIdx avail;	//!< Allocated size of buffer
} ArrInfo;

/** Mark all in-use array values for garbage collection 
 * Increments how much allocated memory the array uses. */
#define arrMark(th, a) \
	{mem_markobj(th, (a)->type); \
	if ((a)->size > 0) \
		for (Value *arrp = &(a)->arr[(a)->size-1]; arrp >= (a)->arr; arrp--) \
			mem_markobj(th, *arrp); \
	vm(th)->gcmemtrav += sizeof(ArrInfo) + sizeof(Value) * (a)->avail;}

/** Free all of an array's allocated memory */
#define arrFree(th, a) \
	{mem_freearray(th, (a)->arr, (a)->avail); \
	mem_free(th, (a));}

/** Point to array information, by recasting a Value pointer */
#define arr_info(val) (assert_exp(isEnc(val,ArrEnc), (ArrInfo*) val))

/** Return the number of Values stored in the array */
#define arr_size(val) (arr_info(val)->size)

// ***********
// Non-API Array functions
// ***********

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif
