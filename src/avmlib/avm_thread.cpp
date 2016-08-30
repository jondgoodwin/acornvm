/** Implements threads, which manage execution, the stack state, and global namespace
 * Look to avm_stack.cpp for stack code and avm_global.cpp for the namespace code.
 *
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

/* Return a new Thread with a starter namespace and stack. */
Value newThread(Value th, Value *dest, AuintIdx stksz) {
	ThreadInfo *newth;

	// Create and initialize a thread
	newth = (ThreadInfo *) mem_new(th, ThrEnc, sizeof(ThreadInfo), NULL, 0);
	thrInit(newth, vm(th), stksz);
	return *dest = (Value)newth;
}

/* Initialize a thread.
 * We do this separately, as Vm allocates main thread as part of VmInfo */
void thrInit(ThreadInfo* thr, VmInfo* vm, AuintIdx stksz) {

	thr->vm = vm;
	thr->size = 0;
	thr->flags1 = 0;	// Initialize Flags1 flags

	// Allocate and initialize thread's stack
	thr->stack = NULL;
	thr->size = 0;
	stkRealloc(thr, stksz);
	thr->stk_top = thr->stack;

	// initialize call stack
	CallInfo *ci = &thr->entrymethod; // Initial callinfo struct is already allocated
	ci->next = ci->previous = NULL;
	// ci->callstatus = 0;
	ci->nresults = 0;
	ci->methodbase = ci->retTo = thr->stk_top;
	*thr->stk_top++ = aNull;  // Place for non-existent function
	ci->begin = thr->stk_top;
	ci->end = thr->stk_top + STACK_MINSIZE;
	ci->method = aNull;
	thr->curmethod = ci;

	thr->global = aNull;
}

/** Return 1 if it is a Thread, else return 0 */
int isThread(Value th) {
	return isEnc(th, ThrEnc);
}

/** Internal routine to allocate and append a new CallInfo structure to end of call stack */
CallInfo *thrGrowCI(Value th) {
	CallInfo *ci = (CallInfo *) mem_gcrealloc(th, NULL, 0, sizeof(CallInfo));
	assert(th(th)->curmethod->next == NULL);
	th(th)->curmethod->next = ci;
	ci->previous = th(th)->curmethod;
	ci->next = NULL;
	return ci;
}

/** Free all allocated CallInfo blocks in thread */
void thrFreeCI(Value th) {
	CallInfo *ci = th(th)->curmethod;
	CallInfo *next = ci->next;
	ci->next = NULL;
	while ((ci = next) != NULL) {
		next = ci->next;
		mem_free(th, ci);
	}
}

/* Free everything allocated for thread */
void thrFreeStacks(Value th) {
	if (th(th)->stack == NULL)
		return;  /* stack not completely built yet */
	// Free call stack
	th(th)->curmethod = &th(th)->entrymethod;
	thrFreeCI(th);
	// Free data stack
	mem_freearray(th, th(th)->stack, th(th)->size);  /* free stack array */
}


/* Retrieve a value from global namespace */
Value gloGet(Value th, Value var) {
	assert(isTbl(th(th)->global));
	return tblGet(th, th(th)->global, var);
}

/* Add or change a global variable */
void gloSet(Value th, Value var, Value val) {
	assert(isTbl(th(th)->global));
	tblSet(th, th(th)->global, var, val);
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

