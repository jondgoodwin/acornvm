/* Implements threads, which manage execution, the stack state, and global namespace
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
	ThreadInfo *thr;
	VmInfo *vm = isThread(th)? vm(th) : (VmInfo*) th; // For main thread, we get VM instead of Thread

	// Create and initialize a thread
	MemInfo **linkp = NULL;
	thr = (ThreadInfo *) mem_new(vm, ThrEnc, sizeof(ThreadInfo), linkp, 0);
	thr->size = 0;
	thr->flags1 = 0;	// Initialize Flags1 flags

	thr->vm = vm;
	thr->global = (glo==aNull)? newGlobal(thr, GLOBAL_NEWSIZE) : glo;

	// Allocate and initialize thread's stack
	thr->stack = NULL;
	thr->size = 0;
	stkRealloc(thr, stksz);
	thr->stk_top = thr->stack;

	// initialize call stack
	CallInfo *ci = &thr->entryfn; // Initial callinfo struct is already allocated
	ci->next = ci->previous = NULL;
	ci->callstatus = 0;
	ci->nresults = 0;
	ci->stk_base = thr->stk_top;
	*thr->stk_top++ = aNull;  // default return value for this function
	ci->top = thr->stk_top + STACK_MINSIZE;
	thr->curfn = ci;

	return (Value) thr;
}

/** Return 1 if it is a Thread, else return 0 */
int isThread(Value th) {
	return isEnc(th, ThrEnc);
}

} // extern "C"
} // namespace avm


