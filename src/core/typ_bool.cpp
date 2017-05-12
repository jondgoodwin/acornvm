/** Bool type methods and properties
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

/** new creates true or false */
int bool_new(Value th) {
	pushValue(th, isFalse(getLocal(th, 0))? aFalse : aTrue);
	return 1;
}

/** <=> for boolean */
int bool_compare(Value th) {
	if (getTop(th)>1 && getLocal(th, 1)==getLocal(th,0)) {
		pushValue(th, anInt(0));
		return 1;
	}
	return 0;
}

/** Initialize the Bool type */
void core_bool_init(Value th) {
	vmlit(TypeBoolc) = pushType(th, vmlit(TypeObject), 4);
		pushSym(th, "Bool");
		popProperty(th, 0, "_name");
		vmlit(TypeBoolm) = pushMixin(th, vmlit(TypeObject), aNull, 30);
			pushSym(th, "*Bool");
			popProperty(th, 1, "_name");
			pushCMethod(th, bool_compare);
			popProperty(th, 1, "<=>");
		popProperty(th, 0, "traits");

		pushCMethod(th, bool_new);
		popProperty(th, 0, "New");
	popGloVar(th, "Bool");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
