/** Defines Value, corresponding C data types, and various validation and casting functions.
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_value_h
#define avm_value_h

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
AVM_INLINE int isSame(Value a, Value b) {
	return a==b;
}

/** What type of data is encoded within the value, as established by the last 2 bits.
 * Because of 32/64-bit allocation alignment, pointers always have 0 in last 2 bits.
 * Thus, non-zero bits can be used to indicate a non-pointer Value. */
enum ValBits {
	ValPtr = 0,		/*! Value points to a compound value's header */
	ValInt = 1,  	/*! Value is a signed integer */
	ValFloat = 2,	/*! Value is a floating-point number */
	ValSym = 3		/*! Value is a symbol or special value (null, false, true) */
};
/** The mask used to isolate the value's ValBits info */
const int ValMask = 0x3;
/** How many bits to shift a Value to remove or make space for ValBits info */
const int ValShift = 2;

/* *******************************************************
   Integer value functions
   **************************************************** */

/** Is v an Integer? */
AVM_INLINE int isInt(Value v) {
	return ((Auint) v & ValMask)==ValInt;
}

/** Cast c-integer n into an Integer value
 * This loses top two-bits of integer precision.
 * If integer is too large, this could result in an unexpected value and change of sign. */
AVM_INLINE Value anInt(Aint n) {
	assert(n == (n << ValShift) >> ValShift); // Enforce we lost nothing (during debug)
	return (Value) ((n << ValShift)+ValInt);
}

/** Cast an Integer value into a c-integer 
 * Note: It assumes (and won't verify) that v is an Integer */
AVM_INLINE Aint toAint(Value v) {
	assert(isInt(v)); // Enforce v is an integer (during debug)
	return ((Aint) v) >> ValShift; // shift will keep integer's sign
}


/* *******************************************************
   Float value functions
   **************************************************** */

/** Helps cast between float and unsigned without any conversion */
union Fcaster {
	Afloat f;
	Auint u;
};

/** Is v a Float? */
AVM_INLINE int isFloat(Value v) {
	return ((Auint) v & ValMask)==ValFloat;
}

/** Cast c-float n into a Float value
 * This loses bottom two-bits of Float mantissa precision. */
AVM_INLINE Value aFloat(Afloat n) {
	union Fcaster x;
	x.f = n;
	return (Value) ((x.u & ~ValMask) + ValFloat); // replace bottom 2-bits with float marker
}

/** Cast an Float value into a c-float 
 * Note: It assumes (and won't verify) that v is an Float */
AVM_INLINE Afloat toAfloat(Value v) {
	union Fcaster x;
	assert(isFloat(v)); // Enforce v is a float (during debug)
	x.u = ((Auint) v & ~ValMask); // eliminate float marker bits
	return x.f; 
}

/* *******************************************************
   null, false and true values and functions.
   (they are encoded in the impossible space for a symbol pointer
   **************************************************** */

/** The null value */
#define aNull ((Value) ((0 << ValShift) + ValSym))
/** The false value */
#define aFalse ((Value) ((1 << ValShift) + ValSym))
/** The true value */
#define aTrue ((Value) ((2 << ValShift) + ValSym))
/** The minimum possible value for a true symbol */
#define SymMinValue ((Value) ((3 << ValShift) + ValSym))

/** Is value null? */
AVM_INLINE int isNull(Value v) {
	return v==aNull;
}

/** Is value false or null? */
AVM_INLINE int isFalse(Value v) {
	return v==aFalse || v==aNull;
}

/** Is value true or false? */
AVM_INLINE int isBool(Value v) {
	return v==aTrue || v==aFalse;
}

/* *******************************************************
   Symbol functions.
   **************************************************** */

/** Is value a symbol? */
AVM_INLINE int isSym(Value v) {
	return ((Auint)v & ValMask)==ValSym && v>=SymMinValue;
}

#endif