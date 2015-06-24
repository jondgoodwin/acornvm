/** Implements symbols, immutable and permanently stored byte-sequences.
 * Two separately-created symbols with the same sequence of bytes will have the same single Value.
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_hash_h
#define avm_hash_h

#include "avm/avm_memory.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

AHash hash_bytes(const char *str, Auint len, AHash seed);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif