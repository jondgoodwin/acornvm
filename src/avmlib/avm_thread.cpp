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
	ci->callstatus = 0;
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

/** Restore call and data stack after call, copying return values down 
 * nreturned is how many values the called function actually returned */
void thrReturn(Value th, int nreturned) {
	CallInfo *ci = th(th)->curfn;

	// Copy down returned results (capped no higher than expected)
	Value *from = th(th)->stk_top-nreturned;
	Value *to = ci->retTo;
	for (int n = ci->nresults < nreturned ? ci->nresults : nreturned; n; n--)
		*to++ = *from++;
	th(th)->stk_top = to;

	// Add nulls as needed to pad up to expected number of results
	int n = ci->nresults - nreturned;
	while (n-- > 0)
		stkPush(th, aNull);

	// Back up call chain
	th(th)->curfn = ci->previous;
}

/** Call a function value placed on stack (with its parms above it).
 * It returns 1 if it is bytecode calling bytecode, meaning the call was 
 * prepared, but not performed (because it returns to the interpreter which
 * continues execution at the called function.
 * A return of 0 means the function was called.
 *
 * nexpected is how many results function is expected to return. 
 *
 * For c-functions, the parameters are passed untouched with 20 slots of local working space.
 *
 * For byte code, the parameters are massaged to provide the expected number of
 * fixed parameters and a special holding place for the variable parameters.
 */
bool thrCalli(Value th, Value *funcval, int nexpected) {
	assert(isFunc(*funcval));

	// Start and initialize a new CallInfo block
	CallInfo * ci = th(th)->curfn = th(th)->curfn->next ? th(th)->curfn->next : thrGrowCI(th);
	ci->nresults = nexpected;
	ci->funcbase = ci->retTo = funcval;  // Address of function value, varargs and return values
	ci->begin = funcval + 1; // Parameter values are right after function value
	ci->end = funcval + 1; // Will be fixed by stkNeeds

	// C-function call - Does the whole thing: setup, call and stack clean up after return
	if (isCFunc(*funcval)) {
		CFuncInfo* cfunc = (CFuncInfo*) *funcval; // Capture it now before it moves
		ci->callstatus=0;

		// We do no parameter adjustments for c-functions
		// Allocate room for local variables
		stkNeeds(th, STACK_MINSIZE);
		// Do the actual call and then restore stack
		thrReturn(th, ((cfunc)->funcp)(th));
	}

	// Bytecode function call - Only does set-up, call done by caller
	else {
		int nparms = th(th)->stk_top - funcval - 1; // Actual number of parameters pushed
		BFuncInfo *bfunc = (BFuncInfo*) *funcval; // Capture it now before it moves

		// Initialize byte-code's call info
		ci->callstatus = CIST_BYTECODE;
		ci->ip = bfunc->code; // Start with first instruction

		// Ensure sufficient data stack space.
		stkNeeds(th, bfunc->maxstacksize); // funcval may no longer be reliable

		// If we do not have at least enough fixed parameters then fill up with nulls
		for (; nparms < funcNParms(bfunc); nparms++)
			*th(th)->stk_top++ = aNull;

		// If we expected variable number of parameters, we better tidy them
		if (isVarParm(bfunc)) {
			int i;

			// Reset where fixed parms start (where we are about to put them)
			Value *from = ci->begin; // Capture where fixed parms are now
			ci->begin = th(th)->stk_top;

			// Copy fixed parms up into new frame start
			for (i=0; i<funcNParms(bfunc); i++) {
				*th(th)->stk_top++ = *from;
				*from++ = aNull;  // we don't want dupes that might confuse garbage collector
			}
		}

		// Bytecode won't be using stk_top; put it above local frame stack.
		th(th)->stk_top = ci->end;

		// No reason to execute if already in byte-code interpretation
		if (th(th)->curfn->previous->callstatus & CIST_BYTECODE)
			return 1;

		// Start interpretation of op-codes (interpreter will handle return)
		bfnRun(th);
	}

	return 0;	
}

/** Call a function value placed on stack (with nparms above it). 
 * Indicate how many return values to expect to find on stack. */
void thrCall(Value th, int nparms, int nexpected) {
	thrCalli(th, th(th)->stk_top - nparms - 1, nexpected);
}

/* Replace self+method on stack with function+self, then call */
bool thrCallMethod(Value th, int nparms, int nexpected) {
	Value *methnm = th(th)->stk_top-nparms-1;

	// Look up method's function, then do swap
	*methnm = findMethod(th, *(methnm+1), *methnm);

	return thrCalli(th, th(th)->stk_top - nparms - 1, nexpected);
}


#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

