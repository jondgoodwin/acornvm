/** Null type methods and properties
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

/** new creates null */
int null_new(Value th) {
	pushValue(th, aNull);
	return 1;
}

/** Initialize the Null type */
void core_null_init(Value th) {
	vmlit(TypeNullc) = pushType(th, vmlit(TypeType), 4);
		pushSym(th, "Null");
		popProperty(th, 0, "_name");
		vmlit(TypeNullm) = pushMixin(th, vmlit(TypeType), aNull, 30);
			pushSym(th, "*Null");
			popProperty(th, 1, "_name");
		popProperty(th, 0, "_newtype");

		pushCMethod(th, null_new);
		popProperty(th, 0, "New");
	popGloVar(th, "Null");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif