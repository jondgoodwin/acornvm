/** List type methods and properties
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

/** Create a new List. Parameters fill the List */
int list_new(Value th) {
	int arrsz = getTop(th)-1;
	Value arr = pushArray(th, vmlit(TypeListm), arrsz);
	int idx = 0;
	while (arrsz--) {
		arrSet(th, arr, idx, getLocal(th, idx+1));
		idx++;
	}
	return 1;
}

/** Add to a List */
int list_add(Value th) {
	arrAdd(th, getLocal(th,0), getLocal(th,1));
	return 0;
}

/** Get next item from a List */
int list_next(Value th) {
	Value arr = getLocal(th,0);
	AintIdx sz = arr_size(arr);
	AintIdx pos = (getLocal(th,1)==aNull)? 0 : toAint(getLocal(th,1));
	pushValue(th, pos>=sz? aNull : arrGet(th, arr, pos));
	setLocal(th, 1, pos>=sz? aNull : anInt((Aint)pos+1));
	return 2;
}

/** Initialize the List type */
void core_list_init(Value th) {
	vmlit(TypeListc) = pushType(th, vmlit(TypeType), 4);
		pushSym(th, "List");
		popProperty(th, 0, "_name");
		vmlit(TypeListm) = pushMixin(th, vmlit(TypeType), aNull, 20);
			pushSym(th, "*List");
			popProperty(th, 1, "_name");
			pushCMethod(th, list_add);
			popProperty(th, 1, "<<");
			pushCMethod(th, list_next);
			popProperty(th, 1, "next");
		popProperty(th, 0, "_newtype");
		pushCMethod(th, list_new);
		popProperty(th, 0, "New");
	popGloVar(th, "List");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
