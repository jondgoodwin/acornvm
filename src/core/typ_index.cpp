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
	Value type = pushTbl(th, getLocal(th, 0), 4);
	return 1;
}

/** Initialize the Index type, used to create other types */
void core_index_init(Value th) {
	vmlit(TypeType) = pushType(th, aNull, 1);
		pushSym(th, "Index");
		popProperty(th, 0, "_name");
		pushCMethod(th, index_new);
		popProperty(th, 0, "New");
	popGloVar(th, "Index");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
