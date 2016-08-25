/** Immediate and generic Value functions, not specific to a particular momory encoding.
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#include "avmlib.h"
#include <stdio.h>
#include <string.h>

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
	//mem_markChk(th, val, type);
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
		case MethEnc: return vmlit(TypeMethm);
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

/* Append serialized val to end of str. */
void serialize(Value th, Value str, int indent, Value val) {
	// Decode the encoded Value
	switch ((Auint)val & ValMask) {
	case ValCons:
		if (val==aNull) strAppend(th, str, "null", 4);
		else if (val==aFalse) strAppend(th, str, "false", 5);
		else if (val==aTrue) strAppend(th, str, "true", 4);
		return;
	case ValInt:
		{char istr[15];
		sprintf(istr, "%d", toAint(val));
		strAppend(th, str, istr, strlen(istr)); 
		return;
		}
	case ValFloat:
		{char fstr[30];
		sprintf(fstr, "%#.15g", toAfloat(val));
		strAppend(th, str, fstr, strlen(fstr)); 
		return;
		}
	case ValPtr:
		switch (((MemInfo*)val)->enctyp) {
		case SymEnc: 
			{strAppend(th, str, "'", 1);
			strAppend(th, str, sym_cstr(val), sym_size(val)); 
			strAppend(th, str, "'", 1);
			return;}
		case StrEnc: 
			if (isCData(val))
				strAppend(th, str, "+CData", 6);
			else {
				strAppend(th, str, "\"", 1);
				if (str_cstr(val))
					strAppend(th, str, str_cstr(val), strlen(str_cstr(val))); 
				strAppend(th, str, "\"", 1);
			}
			return;
		case ArrEnc: 
			arrSerialize(th, str, indent, val);
			return;
		case TblEnc: 
			tblSerialize(th, str, indent, val);
			return;
		case MethEnc: 
			if (isCMethod(val))
				{strAppend(th, str, "CMethod", 7); return;}
			else
				{methSerialize(th, str, indent, val); return;}
		case ThrEnc: strAppend(th, str, "Thread", 6); return;
		case VmEnc: strAppend(th, str, "Vm", 2); return;
		default: {
			MemInfo *mi = (MemInfo*) val;
			vmLog("Serialize: %p >>%s<< (%d)",mi, toStr(str), getSize(str));
			assert(0); }
			return;
		}
	}
	return; // Should not ever get here
}

/** Find method in part's methods or mixins. Return aNull if not found */
Value *getPropR(Value type, Value methsym) {
	Value *meth;

	// If this is a Type table, check its properties
	if (isType(type)) {
		if (NULL != (meth = tblGetp(type, methsym)))
			return meth;
		// Or else try inherited properties
		if (NULL != (meth = getPropR(tbl_info(type)->inheritype, methsym)))
			return meth;
	}
	
	// Recursively examine each type in an array
	else if (isArr(type)) {
		Value *types = arr_info(type)->arr;
		AuintIdx ntypes = arr_size(type);
		while (ntypes--) {
			if (NULL != (meth = tblGetp(*types, methsym)))
				return meth;
			// Or else try inherited properties
/*			if (NULL != (meth = getPropR(tbl_info(*types++)->inheritype, methsym)))
				return meth;*/
			types++;
		}
	}

	return NULL;
}

/* Find value's property in self or from its type. Return aNull if not found */
Value getProperty(Value th, Value self, Value methsym) {
	Value *meth;

	// If this is a ProtoType table, search first among its value's own properties
	if (isPrototype(self)) {
		if (NULL != (meth = tblGetp(self, methsym)))
			return *meth;
	}

	// Next, look for the method in the value's type
	if (NULL != (meth = getPropR(getType(th, self), methsym)))
		return *meth;

	// As a last resort, look for the method in the All type
	if (NULL != (meth = tblGetp(vmlit(TypeAll), methsym)))
		return *meth;

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
