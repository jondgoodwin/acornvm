/* Virtual Machine management
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "avmlib.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

#define memcpy_Aunit(i,val) \
	{Auint anint = (Auint) val; \
	memcpy(seedstr + i*sizeof(Auint), &anint, sizeof(Auint));}

/** Create and initialize new Virtual Machine */
AVM_API Value newVM(void) {

	// Create VM info block
	VmInfo *vm = (struct VmInfo*) mem_frealloc(NULL, sizeof(VmInfo));
	mem_init(vm); /* Initialize memory & garbage collection */
	sym_init(vm); /* Initialize symbol table */

	// Compute a randomized seed, using address space layout to increaase randomness
	char seedstr[4 * sizeof(Auint)];
	time_t timehash = time(NULL);
	memcpy_Aunit(0, vm)			// heap pointer
	memcpy_Aunit(1, timehash)	// current time in seconds
	memcpy_Aunit(2, &timehash)	// local variable pointer
	memcpy_Aunit(3, &newVM)		// public function
	vm->hashseed = hash_bytes(seedstr, sizeof(seedstr), (AHash) timehash);

	// Create main thread
	vm->main_thread = mem_gcrealloc(vm, NULL, 0, sizeof(ThreadInfo));
	struct ThreadInfo* th = th(vm->main_thread);
	th->enctyp = ThrEnc;
	th->vm = vm;

	return (Value) th;
}

/** Handle when memory manager says we have no more memory to offer */
void vm_outofmemory(void) {
	puts("Acorn VM ran out of memory");
	exit(1);
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
