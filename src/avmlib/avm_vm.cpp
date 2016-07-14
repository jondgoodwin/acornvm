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

void acn_init(Value th); // Initializer for Acorn compile
void vm_litinit(Value th); // Initializer for literals
void vm_stdinit(Value th); // Initializer for standard symbols

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

	// Initialize main thread (allocated as part of VmInfo)
	Value th = (Value) (vm->main_thread = &vm->main_thr);
	((ThreadInfo*) th)->marked = vm->currentwhite & WHITEBITS;
	((ThreadInfo*) th)->enctyp = ThrEnc;
	((ThreadInfo*) th)->next = NULL;
	thrInit(&vm->main_thr, vm, STACK_NEWSIZE);

	// Compute a randomized seed, using address space layout to increaase randomness
	// Seed is used to help calculate randomly distributed symbol hashes
	char seedstr[4 * sizeof(Auint)];
	time_t timehash = time(NULL);
	memcpy_Auint(0, vm)			// heap pointe
	memcpy_Auint(1, timehash)	// current time in seconds
	memcpy_Auint(2, &timehash)	// local variable pointe
	memcpy_Auint(3, &newVM)		// public function
	vm->hashseed = tblCalcStrHash(seedstr, sizeof(seedstr), (AuintIdx) timehash);

	// Initialize vm-wide symbol table, global table and literals
	sym_init(th); // Initialize hash table for symbols
	newTbl(th, &vm->global, aNull, GLOBAL_NEWSIZE); // Create global hash table
	((ThreadInfo*) th)->global = vm->global; // For now, main thread needs global too
	vm_litinit(th); // Load reserved and standard symbols into literal list
	glo_init(th); // Load up global table and literal list with core types
	setType(th, vm->global, vmlit(TypeIndexm)); // Fix up type info for global table

	// Initialize byte-code standard methods and the Acorn compiler
	vm_stdinit(th);
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
	mem_reallocvector(th, vm->stdsym, nStdSym, 0, Value);
	sym_free(th);
	thrFreeStacks(th);
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
	puts("Acorn VM wants to overflow max stack size. Runaway recursive method?");
	exit(1);
}

struct vmLitSymEntry {
	int litindex;
	const char *symnm;
};

const struct vmLitSymEntry vmLitSymTable[] = {
	// Compiler symbols that are also methods
	{SymNew, "new"},
	{SymAppend, "<<"},
	{SymPlus, "+"},
	{SymMinus, "-"},
	{SymMult, "*"},
	{SymDiv, "/"},

	// Methods that are not compiler symbols
	{SymParas, "()"},
	{SymNeg, "-@"},
	{SymNext, "next"},

	{SymFinalizer, "_finalizer"},

	// End of literal table
	{0, NULL}
};

/** Initialize vm's literals. */
void vm_litinit(Value th) {
	// Allocate untyped array for literal storage
	VmInfo* vm = vm(th);
	newArr(th, &vm->literals, aNull, nVmLits);
	Value *vmlits = arr_info(vm->literals)->arr;

	// Load up literal symbols from table
	const struct vmLitSymEntry *vmlittblp = &vmLitSymTable[0];
	while (vmlittblp->symnm) {
		newSym(th, &vmlits[vmlittblp->litindex], vmlittblp->symnm, strlen(vmlittblp->symnm));
		vmlittblp++;
	}
}

// Map byte-code's standard symbols to VM's literals (max. number at 256)
const int stdTblMap[] = {
	// Commonly-called methods
	SymNew,		// 'new'
	SymParas,	// '()'
	SymAppend,	// '<<'
	SymPlus,	// '+'
	SymMinus,	// '-'
	SymMult,	// '*'
	SymDiv,		// '/'
	SymNeg,		// '-@'

	SymNext,	// 'next'
	-1
};

/** Initialize vm's standard symbols */
void vm_stdinit(Value th) {
	// Allocate mapping tables
	VmInfo* vm = vm(th);
	Value stdidx =  newTbl(th, &vm->stdidx, aNull, nStdSym);
	vm->stdsym = NULL;
	mem_reallocvector(th, vm->stdsym, 0, nStdSym, Value);

	// Populate the mapping tables with the corresponding VM literals
	const int *mapp = &stdTblMap[0];
	int idx = 0;
	while (*mapp >= 0 && idx<nStdSym) {
		tblSet(th, stdidx, vmlit(*mapp), anInt(idx));
		vm->stdsym[idx] = vmlit(*mapp);
		idx++;
		mapp++;
	}
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
