/** All type methods and properties
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

/** <=> */
int all_compare(Value th) {
	if (getTop(th)>1 && getLocal(th,0)==getLocal(th,1)) {
		pushValue(th, anInt(0));
		return 1;
	}
	return 0;
}

/** =~ */
int all_match(Value th) {
	if (getTop(th)>1 && getLocal(th,0)==getLocal(th,1)) {
		pushValue(th, aTrue);
		return 1;
	}
	return 0;
}

/** executable? */
int all_isexec(Value th) {
	pushValue(th, isCallable(getLocal(th,0))? aTrue : aFalse);
	return 1;
}

/** type */
int all_type(Value th) {
	pushValue(th, getType(th, getLocal(th,0)));
	return 1;
}

/** property */
int all_property(Value th) {
	if (getTop(th)>1) {
		pushValue(th, getProperty(th, getLocal(th, 0), getLocal(th, 1)));
		return 1;
	}
	return 0;
}

/** Initialize the Null type */
void core_all_init(Value th) {
	vmlit(TypeAll) = pushType(th, vmlit(TypeType), 8);
		pushSym(th, "All");
		popProperty(th, 0, "_name");
		pushCMethod(th, all_compare);
		popProperty(th, 0, "<=>");
		pushCMethod(th, all_match);
		popProperty(th, 0, "=~");
		pushCMethod(th, all_match);
		popProperty(th, 0, "executable?");
		pushCMethod(th, all_property);
		popProperty(th, 0, "property");
		pushCMethod(th, all_type);
		popProperty(th, 0, "type");
	popGloVar(th, "All");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
