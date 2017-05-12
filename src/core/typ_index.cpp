/** Index type methods and properties
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

/** Create a new Index */
int index_new(Value th) {
	Value traits = pushProperty(th, 0, "traits"); popValue(th);
	Value type = pushTbl(th, traits, getTop(th)>1 && isInt(getLocal(th,1))? toAint(getLocal(th,1)) : 4);
	return 1;
}

/** Return true if index has no elements */
int index_isempty(Value th) {
	pushValue(th, tbl_size(getLocal(th, 0))==0? aTrue : aFalse);
	return 1;
}

/** Get element at specified key */
int index_get(Value th) {
	if (getTop(th)<2)
		return 0;
	pushValue(th, tblGet(th, getLocal(th,0), getLocal(th,1)));
	return 1;
}

/** Set element at specified key */
int index_set(Value th) {
	if (getTop(th)<3)
		return 0;
	tblSet(th, getLocal(th,0), getLocal(th,2), getLocal(th, 1));
	return 0;
}

/** Remove element from specified key */
int index_remove(Value th) {
	if (getTop(th)<2)
		return 0;
	tblRemove(th, getLocal(th,0), getLocal(th,1));
	setTop(th, 1);
	return 1;
}

/** Return number of elements in Index */
int index_getsize(Value th) {
	pushValue(th, anInt(getSize(getLocal(th, 0))));
	return 1;
}

/** Closure iterator that retrieves Index's next value */
int index_each_get(Value th) {
	Value self    = pushCloVar(th, 2); popValue(th);
	Value current = pushCloVar(th, 3); popValue(th);
	Value next = tblNext(self, current);
	if (next == aNull)
		return 0;
	pushValue(th, next);
	popCloVar(th, 3);
	pushValue(th, next);
	pushValue(th, tblGet(th, self, next));
	return 2;
}

/** Return a get/set closure that iterates over the Index */
int index_each(Value th) {
	Value self = pushLocal(th, 0);
	pushCMethod(th, index_each_get);
	pushValue(th, aNull);
	pushValue(th, self);
	pushValue(th, aNull);
	pushClosure(th, 4);
	return 1;
}

/** Initialize the Index type, used to create other types */
void core_index_init(Value th) {
	vmlit(TypeIndexc) = pushType(th, aNull, 4);
		pushSym(th, "Index");
		popProperty(th, 0, "_name");
		vmlit(TypeIndexm) = pushMixin(th, vmlit(TypeObject), aNull, 16);
			pushSym(th, "*Index");
			popProperty(th, 1, "_name");
			pushCMethod(th, index_isempty);
			popProperty(th, 1, "empty?");
			pushCMethod(th, index_get);
			pushCMethod(th, index_set);
			pushClosure(th, 2);
			popProperty(th, 1, "[]");
			pushCMethod(th, index_remove);
			popProperty(th, 1, "Remove");
			pushCMethod(th, index_getsize);
			pushValue(th, aNull);
			pushClosure(th, 2);
			popProperty(th, 1, "size");
			pushCMethod(th, index_each);
			popProperty(th, 1, "Each");
		popProperty(th, 0, "traits");
		pushCMethod(th, index_new);
		popProperty(th, 0, "New");
	popGloVar(th, "Index");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
