/** Virtual Machine management
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

void acn_init(Value th); // Initializer for Acorn compiler
void vmStdInit(Value th); // Initializer for standard symbols

/** Used by vm_init to build random seed */
#define memcpy_Auint(i,val) \
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

	// Create VM info block and start up memory management
	VmInfo *vm = (struct VmInfo*) mem_frealloc(NULL, sizeof(VmInfo));
	vm->enctyp = VmEnc;
	mem_init(vm); /* Initialize memory & garbage collection */
	vm->marked = vm->currentwhite & WHITEBITS;

	// Initialize safe default types for all value encodings (including Immediate)
	// These will be overridden when the core Types are loaded.
	Value* p = vm->defEncTypes = (Value*) mem_frealloc(NULL, NbrEnc*sizeof(Value));
	int i = NbrEnc;
	while (i--)
		*p++ = aNull;

	// Initialize main thread (allocated as part of VmInfo)
	Value th = (Value) (vm->main_thread = &vm->main_thr);
	((ThreadInfo*) th)->marked = vm->currentwhite & WHITEBITS;
	((ThreadInfo*) th)->enctyp = ThrEnc;
	((ThreadInfo*) th)->next = NULL;
	thrInit(&vm->main_thr, vm, aNull, STACK_NEWSIZE);

	// Compute a randomized seed, using address space layout to increaase randomness
	// Seed is used to help hash symbols
	char seedstr[4 * sizeof(Auint)];
	time_t timehash = time(NULL);
	memcpy_Auint(0, vm)			// heap pointer
	memcpy_Auint(1, timehash)	// current time in seconds
	memcpy_Auint(2, &timehash)	// local variable pointer
	memcpy_Auint(3, &newVM)		// public function
	vm->hashseed = tblCalcStrHash(seedstr, sizeof(seedstr), (AuintIdx) timehash);

	// Initialize vm-wide symbol table
	sym_init(th);
	vmStdInit(th);

	// Initialize all core types
	atyp_init(th);

	acn_init(th);

	// Start garbage collection
	mem_gcstart(th);

	return th;
}

/* Close down the virtual machine, freeing all allocated memory */
void vm_close(Value th) {
	th = vm(th)->main_thread;
	VmInfo* vm = vm(th);
	mem_freeAll(th);  /* collect all objects */
	mem_reallocvector(th, vm->stdsym, nStdSyms, 0, Value);
	sym_free(th);
	thrFreeStacks(th);
	mem_frealloc(vm(th)->defEncTypes, 0);
	assert(vm(th)->totalbytes + vm(th)->gcdebt == sizeof(VmInfo));
	mem_frealloc(vm(th), 0);  /* free main block */
}

/* Lock the Vm */
void vm_lock(Value th) {
}

/* Unlock the Vm */
void vm_unlock(Value th) {
}

/* Handle when memory manager says we have no more memory to offer */
void vm_outofmemory(void) {
	puts("Acorn VM ran out of memory");
	exit(1);
}

/* Call when we want to overflow max stack size */
void vm_outofstack(void) {
	puts("Acorn VM wants to overflow max stack size. Runaway recursive function?");
	exit(1);
}

/** Macro for generating standard symbol mappings both ways */
#define newstd(idx, str) \
	tblSet(th, stdidx, sym = aSym(th, (str)), anInt(idx)); \
	vm->stdsym[idx] = sym;

/** Initialize vm's standard symbols */
void vmStdInit(Value th) {
	// Allocate mapping tables
	Value sym;
	VmInfo* vm = vm(th);
	Value stdidx = vm->stdidx = newTbl(th, sizeof(StdSymbols));
	vm->stdsym = NULL;
	mem_reallocvector(th, vm->stdsym, 0, nStdSyms, Value);
	
	// Add standard symbols
	newstd(SymGet, "()");
	newstd(SymPut, "()=");
	newstd(SymAdd, "+=");
	newstd(SymNext, "next");
	newstd(SymPlus, "+");
	newstd(SymMinus, "-");
	newstd(SymMult, "*");
	newstd(SymDiv, "/");
	newstd(SymNeg, "-@");

	newstd(SymNull, "null");
	newstd(SymNot, "!");
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
