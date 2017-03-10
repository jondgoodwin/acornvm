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

/** Recursively determine whether valtype uses type anywhere */
bool type_matchR(Value type, Value valtype) {
	if (isType(valtype))
		return valtype==type || type_matchR(type, tbl_info(valtype)->inheritype);
	
	// Recursively examine each type in an array
	else if (isArr(valtype)) {
		Value *valtypes = arr_info(valtype)->arr;
		AuintIdx ntypes = arr_size(valtype);
		while (ntypes--) {
			if (*valtypes==type || type_matchR(type, tbl_info(*valtypes)->inheritype))
				return true;
			valtypes++;
		}
	}
	return false;
}

/* Match whether passed value uses this type's trait */
int type_match(Value th) {
	if (getTop(th)<2)
		return 0;
	Value val = getLocal(th, 1);

	// Get self's traits
	Value self = getLocal(th, 0);
	Value traits = pushProperty(th, 0, "traits");
	popValue(th);
	if (traits == aNull)
		traits = self;

	// See if we find traits in inheritance list
	pushValue(th, (isPrototype(traits)&&traits==val)||self==vmlit(TypeAll)||type_matchR(traits,getType(th, val))? aTrue : aFalse);
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
		pushCMethod(th, type_match);
		popProperty(th, 0, "~~");
	popGloVar(th, "Type");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
