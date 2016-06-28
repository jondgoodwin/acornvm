/** Implements the global namespace that belongs to a thread.
 * This inheritance implementation allows us to keep a thread's global namespace
 * From being polluted by another, but still share variables, if desired.
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
 */

#include "avmlib.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/* Create a new global namespace */
Value newGlobal(Value th, AuintIdx size) {
	return newTbl(th, size); // A simple hashed table
}

/* Retrieve a value from global namespace */
Value gloGet(Value th, Value var) {
	Value glo = th(th)->global;
	assert(isTbl(glo));
	return tblGet(th, glo, var);
}

/* Add or change a global variable */
void gloSet(Value th, Value var, Value val) {
	Value glo = th(th)->global;
	assert(isTbl(glo));
	tblSet(th, glo, var, val);
}

/* Add or change a global variable */
void gloSetc(Value th, const char* var, Value val) {
	Value glo = th(th)->global;
	assert(isTbl(glo));
	tblSetc(th, glo, var, val);
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

