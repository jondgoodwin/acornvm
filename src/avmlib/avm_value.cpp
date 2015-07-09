/* Immediate and generic Value functions, not specific to a particular momory encoding.
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

/* Set the type used by a value (if encoding allows it to change) */
void setType(Value val, Value type) {
	// Do nothing if value's encoding does not support typing
	if (!isPtr(val) || ((MemInfo*)val)->enctyp < TypedEnc)
		return;

	((MemInfoT*)val)->type = type;
}

/* Return the value's type (works for all values) */
Value getType(Value th, Value val) {
	// Decode the encoded Value
	switch ((Auint)val & ValMask) {
	case ValPtr:
		// For fixed type encodings, use its default type
		if (((MemInfo*)val)->enctyp < TypedEnc)
			return vm(th)->defEncTypes[((MemInfo*)val)->enctyp];

		// Otherwise, get type from the value
		return ((MemInfoT*)val)->type;

	case ValInt:
		return vm(th)->defEncTypes[IntEnc];
	case ValFloat:
		return vm(th)->defEncTypes[FloatEnc];
	case ValCons:
		return vm(th)->defEncTypes[val==aNull? NullEnc : BoolEnc];
	}
	return aNull; // Should not ever get here
}

/* Find method in part's methods or mixins. Return aNull if not found */
Value findMethodR(Value th, Value part, Value methsym) {
	Value meth;
	// Check methods first
	if (part_info(part)->methods != aNull && aNull != (meth = tblGet(th, part_info(part)->methods, methsym)))
		return meth;
	
	// Recursively examine each mixin
	if (part_info(part)->mixins != aNull) {
		Value *mixin = arr_info(part_info(part)->mixins)->arr;
		AuintIdx mixsz = arr_size(part_info(part)->mixins);
		while (mixsz--)
			if (aNull != (meth = tblGet(th, *mixin, methsym)))
				return meth;
	}

	return aNull;
}

/* Find method in self or its type. Return aNull if not found */
Value findMethod(Value th, Value self, Value methsym) {
	Value meth;

	// If it is a non-Type part, look for method first in self's methods and mixins
	if (isPart(self) && !(part_info(self)->flags1 & PartType)) {
		if (aNull != (meth = findMethodR(th, self, methsym)))
			return meth;
	}

	// Next, look for the method in the value's type
	if (aNull != (meth = findMethodR(th, getType(th, self), methsym)))
		return meth;

	// As a last resort, look for the method in the All type
	if (aNull != (meth = findMethodR(th, vm(th)->defEncTypes[AllEnc], methsym)))
		return meth;

	return aNull;
}

/* Return the size of a symbol, string, array, hash or other collection. Any other value type returns 0 */
Auint getSize(Value val) {
	if (isPtr(val))
		return ((MemInfo*)val)->size;
	return 0;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
