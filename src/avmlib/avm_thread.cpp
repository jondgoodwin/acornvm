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
Value newThread(Value th, Value glo, AuintIdx stksz) {
	ThreadInfo *nthr;
	mem_gccheck(th);	// Incremental GC before memory allocation events

	// Create and initialize a thread
	MemInfo **linkp = NULL;
	nthr = (ThreadInfo *) mem_new(th, ThrEnc, sizeof(ThreadInfo), linkp, 0);
	thrInit(nthr, vm(th), glo, stksz);
	return (Value)nthr;
}

/* Initialize a thread.
 * We do this separately, as Vm allocates main thread as part of VmInfo */
void thrInit(ThreadInfo* thr, VmInfo* vm, Value glo, AuintIdx stksz) {

	thr->vm = vm;
	thr->size = 0;
	thr->flags1 = 0;	// Initialize Flags1 flags

	// Allocate and initialize thread's stack
	thr->stack = NULL;
	thr->size = 0;
	stkRealloc(thr, stksz);
	thr->stk_top = thr->stack;

	// initialize call stack
	CallInfo *ci = &thr->entryfn; // Initial callinfo struct is already allocated
	ci->next = ci->previous = NULL;
	// ci->callstatus = 0;
	ci->nresults = 0;
	ci->funcbase = ci->retTo = thr->stk_top;
	*thr->stk_top++ = aNull;  // Place for non-existent function
	ci->begin = thr->stk_top;
	ci->end = thr->stk_top + STACK_MINSIZE;
	thr->curfn = ci;

	thr->global = (glo==aNull)? newGlobal(thr, GLOBAL_NEWSIZE) : glo;
}

/** Return 1 if it is a Thread, else return 0 */
int isThread(Value th) {
	return isEnc(th, ThrEnc);
}

/** Internal routine to allocate and append a new CallInfo structure to end of call stack */
CallInfo *thrGrowCI(Value th) {
	CallInfo *ci = (CallInfo *) mem_gcrealloc(th, NULL, 0, sizeof(CallInfo));
	assert(th(th)->curfn->next == NULL);
	th(th)->curfn->next = ci;
	ci->previous = th(th)->curfn;
	ci->next = NULL;
	return ci;
}

/** Free all allocated CallInfo blocks in thread */
void thrFreeCI(Value th) {
	CallInfo *ci = th(th)->curfn;
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
	th(th)->curfn = &th(th)->entryfn;
	thrFreeCI(th);
	// Free data stack
	mem_freearray(th, th(th)->stack, th(th)->size);  /* free stack array */
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

