/** Range type methods and properties
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

/** Create a new range */
int range_new(Value th) {
	if (getTop(th)<3)
		return 0;
	Value rng = pushArray(th, vmlit(TypeRangem), 3);
	arrSet(th, rng, 0, getLocal(th, 1));
	arrSet(th, rng, 1, getLocal(th, 2));
	arrSet(th, rng, 2, getTop(th)>3? getLocal(th, 3) : aNull);
	return 1;
}

/** Get the 'from' value out of the range */
int range_from_get(Value th) {
	pushValue(th, arrGet(th, getLocal(th, 0), 0));
	return 1;
}

/** Set the 'from' value out of the range */
int range_from_set(Value th) {
	arrSet(th, getLocal(th, 0), 0, getLocal(th, 1));
	setTop(th, 1);
	return 1;
}

/** Get the 'to' value out of the range */
int range_to_get(Value th) {
	pushValue(th, arrGet(th, getLocal(th, 0), 1));
	return 1;
}

/** Set the 'to' value out of the range */
int range_to_set(Value th) {
	arrSet(th, getLocal(th, 0), 1, getLocal(th, 1));
	setTop(th, 1);
	return 1;
}

/** Get the 'step' value out of the range */
int range_step_get(Value th) {
	pushValue(th, arrGet(th, getLocal(th, 0), 2));
	return 1;
}

/** Set the 'step' value out of the range */
int range_step_set(Value th) {
	arrSet(th, getLocal(th, 0), 2, getLocal(th, 1));
	setTop(th, 1);
	return 1;
}

/** Initialize the Range type */
void core_range_init(Value th) {
	vmlit(TypeRangec) = pushType(th, vmlit(TypeType), 4);
		pushSym(th, "Range");
		popProperty(th, 0, "_name");
		vmlit(TypeRangem) = pushMixin(th, vmlit(TypeType), aNull, 30);
			pushSym(th, "*Range");
			popProperty(th, 1, "_name");
			pushCMethod(th, range_from_get);
			pushCMethod(th, range_from_set);
			pushClosure(th, 2);
			popProperty(th, 1, "from");
			pushCMethod(th, range_to_get);
			pushCMethod(th, range_to_set);
			pushClosure(th, 2);
			popProperty(th, 1, "to");
			pushCMethod(th, range_step_get);
			pushCMethod(th, range_step_set);
			pushClosure(th, 2);
			popProperty(th, 1, "step");
		popProperty(th, 0, "_newtype");

		pushCMethod(th, range_new);
		popProperty(th, 0, "New");
	popGloVar(th, "Range");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
