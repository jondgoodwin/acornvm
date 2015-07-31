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

/* Creates a new global namespace, extending thread's current global to an Array */
Value growGlobal(Value th, AuintIdx size) {
	Value newglo;
	Value oldglo = th(th)->global;
	// If already an Array, create a clone with inserted table at top
	if (isArr(oldglo)) {
		newglo = newArr(th, 1+getSize(oldglo));
		arrSet(th, newglo, 0, newTbl(th, size));
		arrSub(th, newglo, 1, getSize(oldglo), oldglo, 0, getSize(oldglo));
	}
	// Otherwise create an Array, with new table at top and old one beneath
	else {
		stkPush(th, newArr(th, 2)); // Keep it from being GC captured
		arrSet(th, stkFromTop(th, 0), 0, newTbl(th, size));
		arrSet(th, stkFromTop(th, 0), 1, oldglo);
		newglo=stkPop(th);
	}
	return newglo;
}

/* Retrieve a value from global namespace */
Value gloGet(Value th, Value var) {
	Value glo = th(th)->global;

	// Handle simple case when global is a table
	if (isTbl(glo))
		return tblGet(th, glo, var);

	// When global is Array, search each in turn until we get a hit
	assert(isArr(glo));
	for (AuintIdx i = 0; i<getSize(glo); i++) {
		Value val = tblGet(th, arrGet(th, glo, i), var);
		if (val != aNull)
			return val;
	}
	return aNull;
}

/* Add or change a global variable */
void gloSet(Value th, Value var, Value val) {
	Value glo = th(th)->global;

	// With Array, all changes are made only to first table
	if (isArr(glo))
		glo = arrGet(th, glo, 0);

	// Insert or alter the global variable
	assert(isTbl(glo));
	tblSet(th, glo, var, val);
}

/* Add or change a global variable */
void gloSetc(Value th, const char* var, Value val) {
	Value glo = th(th)->global;

	// With Array, all changes are made only to first table
	if (isArr(glo))
		glo = arrGet(th, glo, 0);

	// Insert or alter the global variable
	assert(isTbl(glo));
	tblSetc(th, glo, var, val);
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

