/** Manage an execution thread
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_thread_h
#define avm_thread_h

#include "avm/avm_memory.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

	typedef struct ThreadInfo {
		MemCommonInfo;
		VmInfo *vm;		/**< Virtual machine that thread is part of */
		Value global;	/**< global namespace hash table */
	} ThreadInfo;

	/** Turn the thread value into a pointer */
	#define th(th) ((ThreadInfo*) th)

	/** Point to thread's vm info */
	#define vm(th) (((ThreadInfo*)th)->vm)

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif