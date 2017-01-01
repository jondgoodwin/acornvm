/** VM type methods and properties
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
 */

#include "avmlib.h"
#include <stdio.h>

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/** Print the Text string */
int vm_print(Value th) {
	if (getTop(th)>1 && isStr(getLocal(th,1))) {
		printf(toStr(getLocal(th,1)));
	}
	return 0;
}

/** Log the Text string */
int vm_log(Value th) {
	if (getTop(th)>1 && isStr(getLocal(th,1))) {
		vmLog(toStr(getLocal(th,1)));
	}
	return 0;
}

/** Initialize the List type */
void core_vm_init(Value th) {
	vmlit(TypeListc) = pushType(th, vmlit(TypeType), 4);
		pushSym(th, "Vm");
		popProperty(th, 0, "_name");
		pushCMethod(th, vm_print);
		popProperty(th, 0, "Print");
		pushCMethod(th, vm_log);
		popProperty(th, 0, "Log");
	popGloVar(th, "Vm");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
