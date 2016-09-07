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

void vm_litinit(Value th); // Initializer for literals
void vm_stdinit(Value th); // Initializer for standard symbols
void core_init(Value th); // Initialize all core types

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

	logInfo(AVM_RELEASE " started.");

	// Create VM info block and start up memory management
	VmInfo *vm = (struct VmInfo*) mem_frealloc(NULL, sizeof(VmInfo));
	vm->enctyp = VmEnc;
	mem_init(vm); /* Initialize memory & garbage collection */

	// VM is GC Root: Never marked or collected. Black will trigger write barrier
	vm->marked = bitmask(BLACKBIT); 

	// Initialize main thread (allocated as part of VmInfo)
	Value th = (Value) (vm->main_thread = &vm->main_thr);
	((ThreadInfo*) th)->marked = vm->currentwhite;
	((ThreadInfo*) th)->enctyp = ThrEnc;
	((ThreadInfo*) th)->next = NULL;
	thrInit(&vm->main_thr, vm, STACK_NEWSIZE);
	vm->threads = (MemInfo*) &vm->main_thr;

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
	mem_markChk(th, vm, vm->global);
	((ThreadInfo*) th)->global = vm->global; // For now, main thread needs global too
	vm_litinit(th); // Load reserved and standard symbols into literal list
	core_init(th); // Load up global table and literal list with core types
	setType(th, vm->global, vmlit(TypeIndexm)); // Fix up type info for global table

	// Initialize byte-code standard methods and the Acorn compiler
	vm_stdinit(th);

	// Start garbage collection
	mem_gcstart(th);

	return th;
}

/* Close down the virtual machine, freeing all allocated memory */
void vmClose(Value th) {
	th = vm(th)->main_thread;
	VmInfo* vm = vm(th);
	mem_freeAll(th);  /* collect all objects */
	mem_reallocvector(th, vm->stdsym, nStdSym, 0, Value);
	sym_free(th);
	thrFreeStacks(th);
	assert(vm(th)->totalbytes == sizeof(VmInfo));
	mem_frealloc(vm(th), 0);  /* free main block */
	logInfo(AVM_RELEASE " ended.");
}

/* Lock the Vm */
void vm_lock(Value th) {
}

/* Unlock the Vm */
void vm_unlock(Value th) {
}

#include <stdarg.h>
/* Log a message to the logfile */
void vmLog(const char *msg, ...) {
	// Start line with timestamp
	time_t ltime;
	char timestr[80];
	ltime=time(NULL);
	strftime (timestr, sizeof(timestr), "%X %x  ", localtime(&ltime));
	fputs(timestr, stderr);

	// Do a formatted output, passing along all parms
	va_list argptr;
	va_start(argptr, msg);
	vfprintf(stderr, msg, argptr);
	va_end(argptr);
	fputs("\n", stderr);

	// Ensure log file gets it
	fflush(stderr);
}

/** Mapping structure correlating a VM literal symbol's number with its name */
struct vmLitSymEntry {
	int litindex;		//!< Literal symbol's number
	const char *symnm;	//!< Literal symbol's string
};

/** Constant array that identifies and maps all VM literal symbols */
const struct vmLitSymEntry vmLitSymTable[] = {
	// Compiler reserved names
	{SymNull, "null"},
	{SymFalse, "false"},
	{SymTrue, "true"},
	{SymAnd, "and"},
	{SymAsync, "async"},
	{SymBaseurl, "baseurl"},
	{SymBreak, "break"},
	{SymContinue, "continue"},
	{SymDo, "do"},
	{SymEach, "each"},
	{SymElse, "else"},
	{SymElif, "elif"},
	{SymIf, "if"},
	{SymIn, "in"},
	{SymMatch, "match"},
	{SymNot, "not"},
	{SymOr, "or"},
	{SymReturn, "return"},
	{SymSelf, "self"},
	{SymThis, "this"},
	{SymTo, "to"},
	{SymVar, "var"},
	{SymWait, "wait"},
	{SymWhile, "while"},
	{SymWorker, "worker"},
	{SymYield, "yield"},
	{SymLBrace, "{"},
	{SymRBrace, "}"},
	{SymSemicolon, ";"},

	// Compiler symbols that are also methods
	{SymAppend, "<<"},
	{SymPlus, "+"},
	{SymMinus, "-"},
	{SymMult, "*"},
	{SymDiv, "/"},

	// Methods that are not compiler symbols
	{SymNew, "New"},
	{SymParas, "()"},
	{SymNeg, "-@"},
	{SymNext, "next"},

	{SymFinalizer, "_finalizer"},
	{SymName, "_name"},

	// AST symbols
	{SymMethod, "method"},
	{SymAssgn, "="},
	{SymColon, ":"},
	{SymThisBlock, "thisblock"},
	{SymCallProp, "callprop"},
	{SymActProp, "activeprop"},
	{SymRawProp, "rawprop"},
	{SymGlobal, "global"},
	{SymLocal, "local"},
	{SymLit, "lit"},
	{SymResource, "Resource"},

	// End of literal table
	{0, NULL}
};

/** Initialize vm's literals. */
void vm_litinit(Value th) {
	// Allocate untyped array for literal storage
	VmInfo* vm = vm(th);
	newArr(th, &vm->literals, aNull, nVmLits);
	mem_markChk(th, vm, vm->literals);
	arrSet(th, vm->literals, nVmLits-1, aNull);  // Ensure it is full with nulls

	Value *vmlits = arr_info(vm->literals)->arr;
	vmlits[TypeType] = aNull;

	// Load up literal symbols from table
	const struct vmLitSymEntry *vmlittblp = &vmLitSymTable[0];
	while (vmlittblp->symnm) {
		newSym(th, &vmlits[vmlittblp->litindex], vmlittblp->symnm, strlen(vmlittblp->symnm));
		vmlittblp++;
	}
}

/** Map byte-code's standard symbols to VM's literals (max. number at 256) */
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
	mem_markChk(th, vm, vm->stdidx);
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

void core_null_init(Value th);
void core_bool_init(Value th);
void core_int_init(Value th);
void core_float_init(Value th);
void core_list_init(Value th);
void core_index_init(Value th);
void core_type_init(Value th);

void core_resource_init(Value th);
void core_method_init(Value th);
void core_file_init(Value th);

/** Initialize all core types */
void core_init(Value th) {

	core_type_init(th); // Type must be first, so other types can use this as their type
	vmlit(TypeAll) = pushType(th, aNull, 0);
	popGloVar(th, "All");

	core_null_init(th);
	core_bool_init(th);
	core_int_init(th);
	core_float_init(th);
	core_list_init(th);
	core_index_init(th);

	// Load resource before the types it uses
	core_resource_init(th);
	core_method_init(th);
	core_file_init(th);
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
