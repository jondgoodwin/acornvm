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

/** Create and initialize new Virtual Machine 
 * When a VM is started:
 * - Iit dynamically allocates the VmInfo
 *   which holds all universal information about the VM instance.
 * - Memory management and garbage collection (avm_memory.h) is managed at this level.
 *   The GC root value (the main thread) determines what allocated values to keep
 *   and which to discard.
 * - All value encodings are initialized next, including the single symbol table
 *   used across the VM.
 * - The main thread is started up, initializing its global namespace.
 * - All core types are progressively loaded, establishing the default types for 
 *   each encoding. This includes the resource types and Acorn compiler. */
AVM_API Value newVM(void) {

	// Create VM info block
	VmInfo *vm = (struct VmInfo*) mem_frealloc(NULL, sizeof(VmInfo));
	mem_init(vm); /* Initialize memory & garbage collection */
	sym_init(vm); /* Initialize symbol table */

	// Initialize safe default types for all value encodings (including Immediate)
	// These will be overridden when the core Types are loaded.
	Value* p = vm->defEncTypes = (Value*) mem_frealloc(NULL, NbrEnc*sizeof(Value));
	int i = NbrEnc;
	while (i--)
		*p++ = aNull;

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
