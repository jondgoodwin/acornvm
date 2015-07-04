/* Implements the data stack that belongs to a thread.
 * A thread has one data stack which is an allocated array of Values, initialized to 'null'.
 *
 * The stack implementation is optimized for lean performance first, as its functions 
 * are called several times for every function or method call. Therefore, stack indices are not checked for
 * validity (except when running in debug mode, where invalid indices generate exceptions).
 *
 * A current method's area of the data stack is bounded by pointers:
 * - th(th)->curfn->stk_base points to the bottom (at 0 index)
 * - th(th)->stk_top points just above the last (top) value
 * - th(th)->curfn->top points just above last allocated value on stack for method
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
 */

#include "avmlib.h"

#include "string.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/* ****************************************
   HELPER MACROS
   ***************************************/

/** Size of the function's stack area: base to top */
#define stkSz(th) (th(th)->stk_top - th(th)->curfn->stk_base)

/** Is there room to increment stack top up by 1 */
#define stkCanIncTop(th) assert((th(th)->stk_top+1 <= th(th)->curfn->top) && "stack top overflow")

/** Point to current function's stack value at position i.
 * For a method: i=0 is self, i=1 is first parameter, etc. */
#define stkAt(th,i) (assert_exp((i)>=0 && (i)<stkSz(th) && "invalid stack index", \
	&th(th)->curfn->stk_base[i]))


/* ****************************************
   INDEX-ONLY STACK MANIPULATION
   ***************************************/

/* Retrieve the stack value at the index. Be sure 0<= idx < top.
 * Good for getting method's parameters: 0=self, 1=parm 1, etc. */
Value stkGet(Value th, AintIdx idx) {
	return *stkAt(th, idx);
}

/* Put the value on the stack at the designated position. Be sure 0<= idx < top. */
void stkSet(Value th, AintIdx idx, Value val) {
	*stkAt(th, idx) = val;
}

/* Copy the stack value at fromidx into toidx */
void stkCopy(Value th, AintIdx toidx, AintIdx fromidx) {
	*stkAt(th, toidx) = *stkAt(th, fromidx);
}

/* Remove the value at index (shifting down all values above it to top) */
void stkRemove(Value th, AintIdx idx) {
	assert(stkSz(th)>0); // Must be at least one value to remove!
	Value *p = stkAt(th, idx);
	memmove(p, p+1, sizeof(Value)*(stkSz(th)-idx-1));
	th(th)->stk_top--;
}

/* Insert the value at index (shifting up all values above it) */
void stkInsert(Value th, AintIdx idx, Value val) {
	stkCanIncTop(th); /* Check if there is room */
	Value *p = stkAt(th, idx);
	memmove(p+1, p, sizeof(Value)*(stkSz(th)-idx));
	th(th)->stk_top++;
	*p = val;
}


/* ****************************************
   TOP-BASED STACK MANIPULATION
   ***************************************/

/* Push a value on the stack's top */
void stkPush(Value th, Value val) {
	stkCanIncTop(th); /* Check if there is room */
	*th(th)->stk_top++ = val;
}

/* Push a copy of a stack's value at index onto the stack's top */
void  stkPushCopy(Value th, AintIdx idx) {
	stkCanIncTop(th); /* Check if there is room */
	*th(th)->stk_top++ = stkGet(th, idx);
}

/* Pop a value off the top of the stack */
Value stkPop(Value th) {
	assert(stkSz(th)>0); // Must be at least one value to remove!
	return *--th(th)->stk_top;
}

/* Pops the top value and writes it at idx. Often used to set return value */
void stkPopTo(Value th, AintIdx idx) {
	assert(stkSz(th)>0); // Must be at least one value to remove!
	stkSet(th, idx, *(--th(th)->stk_top));
}

/* Convert a "from top" index into a bottom-based index, where 0=top, 1 is next */
AintIdx stkFromTop(Value th, AintIdx fromtop) {
	return stkSz(th)-fromtop-1;
}

/* Return number of values on the current function's stack */
AuintIdx stkSize(Value th) {
	return (AuintIdx) (stkSz(th));
}

/* Reset the top of the current function's stack to a new index position (padding with 'null's)
 * A negative index removes values off the top (-1 is no change, -2 pops one). */
void stkSetTop(Value th, AintIdx idx) {
	Value *base = th(th)->curfn->stk_base;

	// If positive, idx is the index of top value on stack
	if (idx >= 0) {
		assert((base + idx + 1 <= th(th)->stk_last) && "stack top overflow"); // Cannot grow past established limit
		while (th(th)->stk_top < base + idx + 1)
			*th(th)->stk_top++ = aNull; // If growing, fill with nulls
		th(th)->stk_top = base + idx + 1;
	}
	// If negative, idx is which Value from old top is new top (-1 means no change, -2 pops one)
	else {
		assert((-(idx+1) <= th(th)->stk_top - (base + 1)) && "invalid new top");
		th(th)->stk_top += idx+1;  // Adjust top using negative index
	}
}

/* ****************************************
   ALLOCATION FUNCTIONS FOR DATA STACK
   ***************************************/

/** Internal function to re-allocate stack's size */
void stkRealloc(Value th, int newsize) {
	Value *oldstack = th(th)->stack;
	int osize = th(th)->size; // size of old stack

	// Ensure we not asking for more than allowed, and that old stack's values are consistent
	assert(newsize <= STACK_MAXSIZE || newsize == STACK_ERRORSIZE);
	assert(osize==0 || th(th)->stk_last - th(th)->stack == th(th)->size - STACK_EXTRA);

	// Allocate new stack (assume success) and fill any growth with nulls
	mem_reallocvector(vm(th), th(th)->stack, th(th)->size, newsize, Value);
	for (; osize < newsize; osize++)
		th(th)->stack[osize] = aNull;

	// Correct stack values for new size
	th(th)->size = newsize;
	th(th)->stk_last = th(th)->stack + newsize - STACK_EXTRA;
}

/** Internal function to grow current method's stack area by at least n past stk_top.
   May double stack instead. May abort if beyond stack max. */
void stkGrow(Value th, AuintIdx extra) {

	// Already past max?  Abort!
	if (th(th)->size > STACK_MAXSIZE) {
		vm_outofstack();
		return;
	}

	// Calculate the max between how much we need (based on requested growth) 
	// and doubling the stack size (capped at maximum)
	AuintIdx needed = (AuintIdx)(th(th)->stk_top - th(th)->stack) + extra + STACK_EXTRA;
	AuintIdx newsize = 2 * th(th)->size;
	if (newsize > STACK_MAXSIZE) 
		newsize = STACK_MAXSIZE;
	if (newsize < needed) newsize = needed;

	// re-allocate stack (preserves contents)
    if (newsize > STACK_MAXSIZE) {
		stkRealloc(th, STACK_ERRORSIZE); // How much we give if asking for too much
    }
    else
		stkRealloc(th, newsize);
}

/* Ensure method's stack has room for 'extra' more values above top. Return 0 on failure. 
 * This may grow the stack, but never shrinks it.
 */
int stkNeeds(Value th, AuintIdx extra) {
	int success;
	CallInfo *ci = th(th)->curfn;
	vm_lock(th);

	// Check if we already have enough allocated room on stack for more values
	if ((AuintIdx)(th(th)->stk_last - th(th)->stk_top) > extra)
		success = 1;  // Sucess! Stack is already big enough
	else {
		// Will this overflow max stack size?
		if ((AuintIdx)(th(th)->stk_top - th(th)->stack) + extra + STACK_EXTRA > STACK_MAXSIZE)
			success = 0;  // Fail! - don't grow
		else {
			stkGrow(th, extra);
			success = 1;
		}
	}

	// adjust function's last allowed value upwards, as needed
	if (success && ci->top < th(th)->stk_top + extra)
		ci->top = th(th)->stk_top + extra;

	vm_unlock(th);
	return success;
}


} // extern "C"
} // namespace avm


