/* Hash table value management
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

/** Calculate the hash for a sequence of bytes.
 */
AHash hash_bytes(const char *str, Auint len, AHash seed) {
	AHash hash = seed ^ (AHash) len;
	Auint l1;
	Auint step = (len >> AVM_STRHASHLIMIT) + 1;
	for (l1 = len; l1 >= step; l1 -= step)
		hash = hash ^ ((hash<<5) + (hash>>2) + str[l1 - 1]);
	return hash;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
