/** Integer type methods and properties
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

/** Add two integers */
int int_plus(Value th) {
	if (!isInt(getLocal(th, 1)))
		setLocal(th, 1, anInt(0));
	pushValue(th, anInt(toAint(getLocal(th,0)) + toAint(getLocal(th,1))));
	return 1;
}

/** Subtract two integers */
int int_minus(Value th) {
	if (!isInt(getLocal(th, 1)))
		setLocal(th, 1, anInt(0));
	pushValue(th, anInt(toAint(getLocal(th,0)) - toAint(getLocal(th,1))));
	return 1;
}

/** Multiply two integers */
int int_mult(Value th) {
	if (!isInt(getLocal(th, 1)))
		setLocal(th, 1, anInt(0));
	pushValue(th, anInt(toAint(getLocal(th,0)) * toAint(getLocal(th,1))));
	return 1;
}

/** Initialize the Integer type */
void core_int_init(Value th) {
	vmlit(TypeIntc) = pushType(th, vmlit(TypeType), 4);
		pushSym(th, "Integer");
		popProperty(th, 0, "_name");
		vmlit(TypeIntm) = pushMixin(th, vmlit(TypeType), aNull, 30);
			pushSym(th, "*Integer");
			popProperty(th, 1, "_name");
			pushCMethod(th, int_plus);
			popProperty(th, 1, "+");
			pushCMethod(th, int_minus);
			popProperty(th, 1, "-");
			pushCMethod(th, int_mult);
			popProperty(th, 1, "*");
		popProperty(th, 0, "_newtype");
	popGloVar(th, "Integer");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
