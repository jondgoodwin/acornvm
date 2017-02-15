/** Type type methods and properties
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

/** Create a new Type */
int type_new(Value th) {
	Value type = pushType(th, getLocal(th, 0), 4);
	return 1;
}

/** Lookup a value from type's named property */
int type_get(Value th) {
	pushValue(th, getTop(th)>=2? tblGet(th, getLocal(th,0), getLocal(th,1)) : aNull);
	return 1;
}

/** Change a value for type's named property */
int type_set(Value th) {
	if (getTop(th)>=3)
		tblSet(th, getLocal(th,0), getLocal(th,2), getLocal(th,1));
	setTop(th, 1);
	return 1;
}

/** Initialize the Type type, used to create other types */
void core_type_init(Value th) {
	vmlit(TypeType) = pushType(th, aNull, 12);
		pushSym(th, "Type");
		popProperty(th, 0, "_name");
		pushCMethod(th, type_new);
		popProperty(th, 0, "New");
		pushCMethod(th, type_get);
		pushCMethod(th, type_set);
		pushClosure(th, 2);
		popProperty(th, 0, "[]");
	popGloVar(th, "Type");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
