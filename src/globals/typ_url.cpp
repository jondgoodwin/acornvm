/** Implements the Url type
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

/** Create a new Url using a url string and Url context */
int typ_url_new(Value th) {
	return 1;
}

/** Initialize the File type */
Value typ_url_init(Value th) {
	Value typ = pushType(th, vmlit(TypeType), 1);
		pushCMethod(th, typ_url_new);
		popMember(th, 0, "new");
	popGlobal(th, "Url");
	return typ;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

