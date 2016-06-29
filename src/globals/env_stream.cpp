/** Implements the $stream global
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

/** Convert passed value to Text and print out to stdout */
int env_stream_add(Value th) {
	Value val = getLocal(th,1);

	// Write string's text to stdout console
	if (isStr(val))
		fwrite(str_cstr(val), 1, str_size(val), stdout);

	return 1;
}

/** Initialize $stream global variable */
void env_stream_init(Value th) {
	Value val = pushValue(th, newPart(th, aNull));
		addCMethod(th, val, "<<", env_stream_add, "File.'<<'");
	popGlobal(th, "$stream");
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

