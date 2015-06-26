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

/* Return 1 if the value is a Symbol, otherwise 0 */
int isSym(Value sym) {
	return isEnc(sym, SymEnc);
}

/* Return 1 if the value is a String, otherwise 0 */
int isStr(Value str) {
	return isEnc(str, StrEnc);
}

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

/* Return the byte size of a symbol or string. Any other value type returns 0 */
Auint strSz(Value val) {
	if (isSym(val))
		return sym_size(val);
	if (isStr(val))
		return str_size(val);
	return 0;
}


#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
