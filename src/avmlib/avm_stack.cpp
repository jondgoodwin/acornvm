/** Implements the data stack that belongs to a thread.
 * A thread has one data stack which is an allocated array of Values, initialized to 'null'.
 *
 * The stack implementation is optimized for lean performance first, as its functions 
 * are called several times for every function or method call. Therefore, stack indices are not checked for
 * validity (except when running in debug mode, where invalid indices generate exceptions).
 *
 * A current method's area of the data stack is bounded by pointers:
 * - th(th)->curfn->begin points to the bottom (at 0 index)
 * - th(th)->stk_top points just above the last (top) value
 * - th(th)->curfn->end points just above last allocated value on stack for method
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
#define stkSz(th) (th(th)->stk_top - th(th)->curfn->begin)

/** Is there room to increment stack top up by 1 */
#define stkCanIncTop(th) assert((th(th)->stk_top+1 <= th(th)->curfn->end) && "stack top overflow")

/** Point to current function's stack value at position i.
 * For a method: i=0 is self, i=1 is first parameter, etc. */
#define stkAt(th,i) (assert_exp((i)>=0 && (i)<stkSz(th) && "invalid stack index", \
	&th(th)->curfn->begin[i]))


/* ****************************************
   INDEX-ONLY STACK MANIPULATION
   ***************************************/

/* Retrieve the stack value at the index. Be sure 0<= idx < top.
 * Good for getting method's parameters: 0=self, 1=parm 1, etc. */
Value getLocal(Value th, AintIdx idx) {
	return *stkAt(th, idx);
}

/* Put the value on the stack at the designated position. Be sure 0<= idx < top. */
void setLocal(Value th, AintIdx idx, Value val) {
	*stkAt(th, idx) = val;
	mem_markChk(th, th, val);
}

/* Copy the stack value at fromidx into toidx */
void copyLocal(Value th, AintIdx toidx, AintIdx fromidx) {
	*stkAt(th, toidx) = *stkAt(th, fromidx);
}

/* Remove the value at index (shifting down all values above it to top) */
void deleteLocal(Value th, AintIdx idx) {
	assert(stkSz(th)>0); // Must be at least one value to remove!
	Value *p = stkAt(th, idx);
	memmove(p, p+1, sizeof(Value)*(stkSz(th)-idx-1));
	th(th)->stk_top--;
}

/* Insert the popped value into index (shifting up all values above it) */
void insertLocal(Value th, AintIdx idx) {
	assert(stkSz(th)>0); // Must be at least one value to remove!
	Value *p = stkAt(th, idx);
	Value val = *(th(th)->stk_top-1);
	memmove(p+1, p, sizeof(Value)*(stkSz(th)-idx-1));
	*p = val;
}


/* ****************************************
   TOP-BASED STACK MANIPULATION
   ***************************************/

/* Push a value on the stack's top */
Value pushValue(Value th, Value val) {
	stkCanIncTop(th); /* Check if there is room */
	*th(th)->stk_top++ = val;
	mem_markChk(th, th, val); // Keep, if marked for deletion?
	return val;
}

/* Push and return the corresponding Symbol value for a 0-terminated c-string */
Value pushSym(Value th, const char *str) {
	stkCanIncTop(th); /* Check if there is room */
	return newSym(th, th(th)->stk_top++, str, strlen(str));
}

/* Push and return the corresponding Symbol value for a byte sequence of specified length */
Value pushSyml(Value th, const char *str, AuintIdx len) {
	stkCanIncTop(th); /* Check if there is room */
	return newSym(th, th(th)->stk_top++, str, len);
}

/* Push and return a new Type value */
Value pushType(Value th, Value type, AuintIdx size) {
	stkCanIncTop(th); /* Check if there is room */
	return *th(th)->stk_top++ = newType(th, type, size);
}

/* Push and return the value for a method written in C */
Value pushCMethod(Value th, AcFuncp func) {
	stkCanIncTop(th); /* Check if there is room */
	return *th(th)->stk_top++ = newCMethod(th, func);
}

/* Push and return a new List value */
Value pushList(Value th, AuintIdx size) {
	stkCanIncTop(th); /* Check if there is room */
	return newArr(th, th(th)->stk_top++, vmlit(TypeListm), size);
}

/* Put the local stack's top value into the named member of the table found at the stack's specified index */
void popMember(Value th, AintIdx tblidx, const char *mbrnm) {
	assert(stkSz(th)>0); // Must be at least one value to remove!
	Value tbl = *stkAt(th, tblidx);
	assert(isTbl(tbl));
	newSym(th, th(th)->stk_top++, mbrnm, strlen(mbrnm));
	tblSet(th, tbl, *(th(th)->stk_top-1), *(th(th)->stk_top-2));
	th(th)->stk_top -= 2; // Pop key & value after value is safely in table
}

/* Push a copy of a stack's value at index onto the stack's top */
Value pushLocal(Value th, AintIdx idx) {
	stkCanIncTop(th); /* Check if there is room */
	return *th(th)->stk_top++ = getLocal(th, idx);
}

/* Pop a value off the top of the stack */
Value popValue(Value th) {
	assert(stkSz(th)>0); // Must be at least one value to remove!
	return *--th(th)->stk_top;
}

/* Pops the top value and writes it at idx. Often used to set return value */
void popLocal(Value th, AintIdx idx) {
	assert(stkSz(th)>0); // Must be at least one value to remove!
	assert(idx>=0 && idx < stkSz(th)-1 && "invalid local index");
	setLocal(th, idx, *(th(th)->stk_top-1));
	--th(th)->stk_top; // Pop after value is safely in Global
}

/* Retrieve the stack value at the index from top. Be sure 0<= idx < top. */
Value getFromTop(Value th, AintIdx idx) {
	return *stkAt(th, stkSz(th) - idx - 1);
}

/* Return number of values on the current function's stack */
AuintIdx getTop(Value th) {
	return (AuintIdx) (stkSz(th));
}

/* When index is positive, this indicates how many Values are on the function's stack.
 * This can shrink the stack or grow it (padding with 'null's).
 * A negative index removes that number of values off the top. */
void setTop(Value th, AintIdx idx) {
	Value *base = th(th)->curfn->begin;

	// If positive, idx is the index of top value on stack
	if (idx >= 0) {
		assert((base + idx <= th(th)->stk_last) && "stack top overflow"); // Cannot grow past established limit
		while (th(th)->stk_top < base + idx)
			*th(th)->stk_top++ = aNull; // If growing, fill with nulls
		th(th)->stk_top = base + idx;
	}
	// If negative, idx is which Value from old top is new top (-1 means no change, -2 pops one)
	else {
		assert((-(idx) <= th(th)->stk_top - base) && "invalid new top");
		th(th)->stk_top += idx;  // Adjust top using negative index
	}
}

/* ****************************************
   GLOBAL VARIABLE ACCESS
   ***************************************/

/* Push and return the symbolically-named global variable's value */
Value pushGlobal(Value th, const char *var) {
	stkCanIncTop(th); /* Check if there is room */
	assert(isTbl(th(th)->global));
	Value val = newSym(th, th(th)->stk_top++, var, strlen(var));
	mem_markChk(th, th, val); /* Mark it if needed */
	return *(th(th)->stk_top-1) = tblGet(th, th(th)->global, val);
}

/* Alter the symbolically-named global variable to have the value popped off the local stack */
void popGlobal(Value th, const char *var) {
	assert(stkSz(th)>0); // Must be at least one value to remove!
	assert(isTbl(th(th)->global));
	Value val = newSym(th, th(th)->stk_top++, var, strlen(var));
	tblSet(th, th(th)->global, *(th(th)->stk_top-1), *(th(th)->stk_top-2));
	th(th)->stk_top -= 2; // Pop key & value after value is safely in Global
}

/* Push the value of the current process thread's global variable table. */
Value pushGlobalTbl(Value th) {
	stkCanIncTop(th); /* Check if there is room */
	return *th(th)->stk_top++ = th(th)->global;
}

/* ****************************************
   ALLOCATION FUNCTIONS FOR DATA STACK
   ***************************************/

/** Internal function to re-allocate stack's size */
void stkRealloc(Value th, int newsize) {
	mem_gccheck(th);	// Incremental GC before memory allocation events
	Value *oldstack = th(th)->stack;
	int osize = th(th)->size; // size of old stack

	// Ensure we not asking for more than allowed, and that old stack's values are consistent
	assert(newsize <= STACK_MAXSIZE || newsize == STACK_ERRORSIZE);
	assert(osize==0 || th(th)->stk_last - th(th)->stack == th(th)->size - STACK_EXTRA);

	// Allocate new stack (assume success) and fill any growth with nulls
	mem_reallocvector(th, th(th)->stack, th(th)->size, newsize, Value);
	for (; osize < newsize; osize++)
		th(th)->stack[osize] = aNull;

	// Correct stack values for new size
	th(th)->size = newsize;
	th(th)->stk_last = th(th)->stack + newsize - STACK_EXTRA;

	// Correct all data stack pointers, given that data stack may have moved in memory
	if (oldstack) {
		CallInfo *ci;
		AintIdx shift = th(th)->stack - oldstack;
		th(th)->stk_top = th(th)->stk_top + shift;
		for (ci = th(th)->curfn; ci != NULL; ci = ci->previous) {
			ci->end += shift;
			ci->funcbase += shift;
			ci->retTo += shift;
			ci->begin += shift;
		}
	}
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

/* Ensure method's stack has room for 'needed' values above top. Return 0 on failure. 
 * This may grow the stack, but never shrinks it.
 */
int needMoreLocal(Value th, AuintIdx needed) {
	int success;
	CallInfo *ci = th(th)->curfn;
	vm_lock(th);

	// Check if we already have enough allocated room on stack for more values
	if ((AuintIdx)(th(th)->stk_last - th(th)->stk_top) > needed + STACK_EXTRA)
		success = 1;  // Success! Stack is already big enough
	else {
		// Will this overflow max stack size?
		if ((AuintIdx)(th(th)->stk_top - th(th)->stack) > STACK_MAXSIZE - needed - STACK_EXTRA)
			success = 0;  // Fail! - don't grow
		else {
			stkGrow(th, needed);
			success = 1;
		}
	}

	// adjust function's last allowed value upwards, as needed
	if (success && ci->end < th(th)->stk_top + needed)
		ci->end = th(th)->stk_top + needed;

	vm_unlock(th);
	return success;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

