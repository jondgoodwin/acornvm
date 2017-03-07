/** Closure type methods and properties
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

/** Create a new Closure. Parameters fill the Closure */
int clo_new(Value th) {
	Value traits = pushProperty(th, 0, "traits"); popValue(th);
	if (getTop(th)==1)
		pushArray(th, traits, 2);
	else {
		int arrsz = getTop(th)-1;
		Value arr = pushArray(th, traits, arrsz);
		int idx = 0;
		while (arrsz--) {
			arrSet(th, arr, idx, getLocal(th, idx+1));
			idx++;
		}
	}
	return 1;
}

/** Get array element at specified integer position */
int clo_get(Value th) {
	if (getTop(th)<2 || !isInt(getLocal(th, 1)))
		return 0;
	AintIdx idx = toAint(getLocal(th,1))+2;
	pushValue(th, arrGet(th, getLocal(th,0), idx));
	return 1;
}

/** Set array element at specified integer position */
int clo_set(Value th) {
	if (getTop(th)<3 || !isInt(getLocal(th, 2)))
		return 0;
	AintIdx idx = toAint(getLocal(th,2))+2;
	arrSet(th, getLocal(th,0), idx, getLocal(th, 1));
	return 0;
}

/** Return 'get' method */
int clo_getmethod(Value th) {
	pushValue(th, arrGet(th, getLocal(th,0), 0));
	return 1;
}

/** Return 'set' method */
int clo_setmethod(Value th) {
	pushValue(th, arrGet(th, getLocal(th,0), 1));
	return 1;
}

/** Return number of elements in closure */
int clo_getsize(Value th) {
	pushValue(th, anInt(getSize(getLocal(th, 0))-2));
	return 1;
}

/** Initialize the Closure type */
void core_clo_init(Value th) {
	vmlit(TypeCloc) = pushType(th, vmlit(TypeType), 4);
		pushSym(th, "Closure");
		popProperty(th, 0, "_name");
		vmlit(TypeClom) = pushMixin(th, vmlit(TypeType), aNull, 8);
			pushSym(th, "*Closure");
			popProperty(th, 1, "_name");
			pushCMethod(th, clo_get);
			pushCMethod(th, clo_set);
			pushClosure(th, 2);
			popProperty(th, 1, "[]");
			pushCMethod(th, clo_getmethod);
			pushValue(th, aNull);
			pushClosure(th, 2);
			popProperty(th, 1, "getmethod");
			pushCMethod(th, clo_setmethod);
			pushValue(th, aNull);
			pushClosure(th, 2);
			popProperty(th, 1, "setmethod");
			pushCMethod(th, clo_getsize);
			pushValue(th, aNull);
			pushClosure(th, 2);
			popProperty(th, 1, "size");
		popProperty(th, 0, "traits");
		pushCMethod(th, clo_new);
		popProperty(th, 0, "New");
	popGloVar(th, "Closure");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
