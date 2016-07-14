/** Implements the Resource type
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

/** Create a new Resource using a url string and baseurl context */
int typ_resource_new(Value th) {
	return 1;
}

/** Initialize the File type */
Value typ_resource_init(Value th) {
	Value typ = pushType(th, vmlit(TypeType), 1);
		pushCMethod(th, typ_resource_new);
		popMember(th, 0, "new");
	popGloVar(th, "Resource");
	return typ;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

