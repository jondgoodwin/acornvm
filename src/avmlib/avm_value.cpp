/* Value functions
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#include <avmlib.h>

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/** Helps cast between float and unsigned without any conversion */
union Fcaster {
	Afloat f;
	Auint u;
};

/** Cast c-float n into a Float value
 * This loses bottom two-bits of Float mantissa precision. */
Value aFloat(Afloat n) {
	union Fcaster x;
	x.f = n;
	return (Value) ((x.u & ~ValMask) + ValFloat); // replace bottom 2-bits with float marker
}

/** Cast an Float value into a c-float 
 * Note: It assumes (and won't verify) that v is an Float */
Afloat toAfloat(Value v) {
	union Fcaster x;
	assert(isFloat(v)); // Enforce v is a float (during debug)
	x.u = ((Auint) v & ~ValMask); // eliminate float marker bits
	return x.f; 
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
