/* General purpose c-API functions
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#include <string.h>
#include "avmlib.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/* Return a read-only pointer into a C-string encoded by a symbol or string-oriented Value. 
 * It is guaranteed to have a 0-terminating character just after its full length. 
 * Anything other value type returns NULL.
 */
const char* toStr(Value val) {
	if (isSym(val))
		return (const char*) sym_cstr(val);
	if (isStr(val))
		return str_cstr(val);
	return 0;
}

/* Return 1 if the symbol or string value's characters match the zero-terminated c-string, otherwise 0. */
int strEq(Value val, const char* str) {
	if (isSym(val))
		return sym_size(val)==strlen(str) && !strcmp(sym_cstr(val), str);
	if (isStr(val))
		return str_size(val)==strlen(str) && !strcmp(str_cstr(val), str);
	return 0;
}

/* Return the size of a symbol, string, array, hash or other collection. Any other value type returns 0 */
Auint getSize(Value val) {
	if (isPtr(val))
		return ((MemInfo*)val)->size;
	return 0;
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


#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
