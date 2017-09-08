/** Mixin type methods and properties
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

/** Create a new Mixin */
int mixin_new(Value th) {
	pushMixin(th, vmlit(TypeObject), aNull, 8);
	return 1;
}

/** Initialize the Mixin type */
void core_mixin_init(Value th) {
	vmlit(TypeMixinc) = pushType(th, vmlit(TypeObject), 4);
		pushSym(th, "Mixin");
		popProperty(th, 0, "_name");
		vmlit(TypeMixinm) = pushMixin(th, vmlit(TypeObject), aNull, 4);
		popProperty(th, 0, "traits");
		pushCMethod(th, mixin_new);
		popProperty(th, 0, "New");
	popGloVar(th, "Mixin");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
