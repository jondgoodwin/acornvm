/** Immediate and generic Value functions, not specific to a particular momory encoding.
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
	Afloat f;	//!< float encoding of a Value
	Auint u;	//!< unsigned integer encoding of a Value
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
void setType(Value th, Value val, Value type) {
	// Do nothing if value's encoding does not support typing
	if (!isPtr(val) || ((MemInfo*)val)->enctyp < TypedEnc)
		return;

	((MemInfoT*)val)->type = type;
	mem_markChk(th, val, type);
}

/* Return the value's type (works for all values) */
Value getType(Value th, Value val) {
	// Decode the encoded Value
	switch ((Auint)val & ValMask) {
	case ValPtr:
		switch (((MemInfo*)val)->enctyp) {
		// For fixed type encodings, use its default type
		case SymEnc: return vmlit(TypeSymm);
		case ThrEnc: return vmlit(TypeThrm);
		case VmEnc: return vmlit(TypeVmm);
		case FuncEnc: return vmlit(TypeMethm);
		// Otherwise, use the type the instance claims
		default: return ((MemInfoT*)val)->type;
		}

	case ValInt:
		return vmlit(TypeIntm);
	case ValFloat:
		return vmlit(TypeFlom);
	case ValCons:
		return vmlit(val==aNull? TypeNullm : TypeBoolm);
	}
	return aNull; // Should not ever get here
}

/** Find method in part's methods or mixins. Return aNull if not found */
Value getPropR(Value th, Value type, Value methsym) {
	Value meth;

	// If this is a Type table, check its members
	if (isType(type)) {
		return tblGet(th, type, methsym);
	}
	
	// Recursively examine each type in an array
	if (isArr(type)) {
		Value *types = arr_info(type)->arr;
		AuintIdx ntypes = arr_size(type);
		while (ntypes--)
			if (aNull != (meth = tblGet(th, *types++, methsym)))
				return meth;
	}

	return aNull;
}

/* Find method in self or its type. Return aNull if not found */
Value getProperty(Value th, Value self, Value methsym) {
	Value meth;

	// If this is a Type table, search first among its members
	if (isType(self)) {
		if (aNull != (meth = tblGet(th, self, methsym)))
			return meth;
	}

	// Next, look for the method in the value's type
	if (aNull != (meth = getPropR(th, getType(th, self), methsym)))
		return meth;

	// As a last resort, look for the method in the All type
	if (aNull != (meth = tblGet(th, vmlit(TypeAll), methsym)))
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
