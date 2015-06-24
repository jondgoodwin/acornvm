/** Defines Value, corresponding C data types, and various validation and casting functions.
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_value_h
#define avm_value_h

#include <assert.h>
#include <limits.h>
#include <stddef.h>

#include "avm_env.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

// A convenience macro for assert(), establishing the conditions expected to be true, before returning expression e
#define assert_exp(c,e)	(assert(c), (e))

/* *******************************************************
   Define Value and C-types.
   We want all our Value-based types sized the same, 
   according to the architecture (e.g., all 32-bit or all 64-bit).
   **************************************************** */

/** A signed integer, whose size matches Value */
typedef ptrdiff_t Aint;
/** An unsigned integer, whose size matches Value */
typedef size_t Auint;
/** A float, whose size matches Value (see avm_env.h) */
typedef ptrdiff_float Afloat;

/** A fixed-sized, self-typed encoded value which holds any kind of data.
 * It can be passed to or returned from Acorn or C-functions.
 * Never manipulate a Value directly; always use an AcornVM api function.
 *
 * Its size is that of a full address-space pointer (32- or 64-bits). 
 * It holds either an immediate value (null, true, false, integer, float, symbol)
 * or a pointer to a compound/complex value's header.
 */
typedef void *Value;

/** Quick, exact equivalence check between two values ('===')
 * Great for null, false, true, integers and symbols.
 * Less suitable for floats (no epsilon) and comparing contents of containers (e.g., strings).
 * Is fast because it avoids using type-specific methods. */
#define isSame(a,b) (a==b)

/** What type of data is encoded within the value, as established by the last 2 bits.
 * Because of 32/64-bit allocation alignment, pointers always have 0 in last 2 bits.
 * Thus, non-zero bits can be used to indicate a non-pointer Value. */
enum ValBits {
	ValPtr = 0,		/*! Value points to a compound value's header */
	ValInt = 1,  	/*! Value is a signed integer */
	ValFloat = 2,	/*! Value is a floating-point number */
	ValCons = 3		/*! Value is a constant (null, false, true) */
};
/** The mask used to isolate the value's ValBits info */
const int ValMask = 0x3;
/** How many bits to shift a Value to remove or make space for ValBits info */
const int ValShift = 2;

/* *******************************************************
   Integer value functions
   **************************************************** */

/** Is v an Integer? */
#define isInt(v) (((Auint) v & ValMask)==ValInt)

/** Cast c-integer n into an Integer value
 * This loses top two-bits of integer precision.
 * If integer is too large, this could result in an unexpected value and change of sign. */
#define anInt(n) ((Value) ((n << ValShift)+ValInt))

/** Cast an Integer value into a c-integer 
 * Note: It assumes (and won't verify) that v is an Integer */
#define toAint(v) (((Aint) v) >> ValShift)

/* *******************************************************
   Float value functions
   **************************************************** */

/** Is v a Float? */
#define isFloat(v) (((Auint) v & ValMask)==ValFloat)

/** Cast c-float n into a Float value
 * This loses bottom two-bits of Float mantissa precision. */
AVM_API Value aFloat(Afloat n);

/** Cast an Float value into a c-float 
 * Note: It assumes (and won't verify) that v is an Float */
AVM_API Afloat toAfloat(Value v);

/* *******************************************************
   null, false and true values and functions.
   (they are encoded in the impossible space for a symbol pointer
   **************************************************** */

/** The null value */
#define aNull ((Value) ((0 << ValShift) + ValCons))
/** The false value */
#define aFalse ((Value) ((1 << ValShift) + ValCons))
/** The true value */
#define aTrue ((Value) ((2 << ValShift) + ValCons))

/** Is value null? */
#define isNull(v) (v==aNull)

/** Is value false or null? */
#define isFalse(v) (v<=aFalse)

/** Is value true or false? */
#define isBool(v) (v>=aFalse)

/* *******************************************************
   Pointer functions.
   **************************************************** */

/** Is value a pointer? */
#define isPtr(v) (((Auint)v & ValMask)==ValPtr)

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif