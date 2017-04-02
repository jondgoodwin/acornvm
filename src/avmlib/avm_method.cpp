/** Implements c-methods.
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
 */

#include "avmlib.h"
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

void methodRunC(Value th);

/* Build a new c-method value, pointing to a method written in C */
Value newCMethod(Value th, Value *dest, AcMethodp method) {
	CMethodInfo *meth = (CMethodInfo*) mem_new(th, MethEnc, sizeof(CMethodInfo));
	methodFlags(meth) = METHOD_FLG_C;
	meth->methodp = method;
	return *dest = (Value) meth;
}

/** Rather than perform invalid call, set up nulls as if they were returned */
MethodTypes invalidCall(Value th, Value *methodval, int nexpected) {
	CallInfo *ci =th(th)->curmethod;
	if (nexpected == BCVARRET)
		nexpected = 1;
	while (nexpected--)
		*methodval++ = aNull;
	th(th)->stk_top = methodval; // Mark position of last returned
	return MethodBad;
}

/** Return nulls to caller */
MethodTypes returnNulls(Value th) {
	// Copy the desired number of return values (nulls) where indicated
	CallInfo *ci =th(th)->curmethod;
	Value* to = ci->retTo;
	AintIdx want = ci->nresults; // how many caller wants
	if (want == BCVARRET)
		want = 1;
	while (want--)
		*to++ = aNull;

	// Update thread's values
	th(th)->stk_top = to; // Mark position of last returned
	ci = th(th)->curmethod = ci->previous; // Back up a frame

	// Return to 'c' method caller, if we were called from there
	if (!isMethod(ci->method) || isCMethod(ci->method))
		return MethodC;

	// For bytecode, restore info from the previous call stack frame
	if (want != BCVARRET) 
		th(th)->stk_top = ci->end; // Restore top to end for known # of returns
	return MethodBC;
}

/* Prepare call to method or closure value on stack (with parms above it). 
 * Specify how many return values to expect to find on stack.
 * Flags is 0 for normal get, 1 for set, and >>1 for retTo
 * It returns an enum indicating what to do next: bad method, bytecode, C-method.
 *
 * For c-methods, all parameters are passed, reserving 20 slots of stack space.
 * and then it is actually run.
 *
 * For byte code, parameters are massaged to ensure the expected number of
 * fixed parameters and establish a holding place for the variable parameters.
 */
MethodTypes callMorCPrep(Value th, Value *methodval, int nexpected, int flags) {

	// Capture the actual method whose code we will running
	Value realmethod;
	if (isEnc(*methodval, ArrEnc)) { // closure
		// Extract appropriate 'get' or 'set' method from closure. Be sure it is a method.
		realmethod = arrGet(th, *methodval, flags&1);
		if (!isMethod(realmethod))
			return invalidCall(th, methodval, nexpected);
	}
	else
		realmethod = *methodval; // This is the actual method

	/* Generate yielder, if that is what this method does */
	if (isYieldMeth(realmethod)) {
		// Create a new yielder on top of stack
		ThreadInfo *yielder = (ThreadInfo*) newThread(th, th(th)->stk_top, *methodval, 64, ThreadYielder);

		// Calculate number of parms to copy over, number of fill nulls
		int nparms = th(th)->stk_top - methodval - 1;
		int nulls = methodNParms(*methodval) - nparms;
		if (nulls < 0) {
			nulls = 0;
			nparms = methodNParms(*methodval);
		}
		needMoreLocal(yielder, nparms+nulls); // ensure we have room for them

		// Copy parameters/nulls over into yielder's stack
		Value *from = methodval+1;
		while (nparms--)
			*yielder->stk_top++ = *from++;
		while (nulls--)
			*yielder->stk_top++ = aNull;

		// If we expected variable number of parameters, tidy them
		if (isVarParm(*methodval)) {
			// Reset where fixed parms start (where we are about to put them)
			CallInfo *ci = yielder->curmethod;
			Value *from = ci->begin; // Capture where fixed parms are now
			ci->begin = th(yielder)->stk_top;

			// Copy fixed parms up into new frame start
			for (int i=0; i<methodNParms(*methodval); i++) {
				*th(yielder)->stk_top++ = *from;
				*from++ = aNull;  // we don't want dupes that might confuse garbage collector
			}
		}

		// Move yielder down to return value position, plus expected nulls
		methodval += flags >> 1;
		if (nexpected)
			*methodval++ = yielder;
		while (--nexpected > 0)
			*methodval++ = aNull;
		th(th)->stk_top = methodval;
		return MethodBad;
	}

	// Start and initialize a new CallInfo block
	CallInfo * ci = th(th)->curmethod = th(th)->curmethod->next ? th(th)->curmethod->next : thrGrowCI(th);
	ci->nresults = nexpected;
	ci->retTo = (ci->methodbase = methodval) + (flags>>1);  // Address of method value, varargs and return values
	ci->begin = ci->end = methodval + 1; // Parameter values are right after method value
	ci->method = realmethod;

	// C-method call - Does the whole thing: setup, call and stack clean up after return
	if (isCMethod(ci->method)) {

		// Allocate room for local variables, no parm adjustments needed
		needMoreLocal(th, STACK_MINSIZE);
		methodRunC(th); // .. and then go ahead and run it

		return MethodC; // C-method return
	}

	// Bytecode method call - Only does set-up, call done by caller
	else {
		int nparms = th(th)->stk_top - methodval - 1; // Actual number of parameters pushed
		BMethodInfo *bmethod = (BMethodInfo*) ci->method; // Capture it now before it moves

		// Initialize byte-code's call info
		ci->ip = bmethod->code; // Start with first instruction

		// Ensure sufficient data stack space.
		needMoreLocal(th, bmethod->maxstacksize); // methodval may no longer be reliable

		// If we do not have enough fixed parameters then add in nulls
		for (; nparms < methodNParms(bmethod); nparms++)
			*th(th)->stk_top++ = aNull;

		// If we expected variable number of parameters, tidy them
		if (isVarParm(bmethod)) {
			int i;

			// Reset where fixed parms start (where we are about to put them)
			Value *from = ci->begin; // Capture where fixed parms are now
			ci->begin = th(th)->stk_top;

			// Copy fixed parms up into new frame start
			for (i=0; i<methodNParms(bmethod); i++) {
				*th(th)->stk_top++ = *from;
				*from++ = aNull;  // we don't want dupes that might confuse garbage collector
			}
		}

		// Bytecode rarely uses stk_top; put it above local frame stack.
		th(th)->stk_top = ci->end;

		return MethodBC; // byte-code method return
	}
}

/** A tailcall is effectively a call that does not increase the call stack size (a goto).
 * It replaces the current call frame state with state for the called method.
 * This function is only used at the end of a bytecode method and never from a C method.
 * Specify address where method value is store and how many return values expected on stack.
 * It returns an enum indicating what to do next: bad method, bytecode, C-method.
 * We can assume the call is for a method or a closure.
 *
 * For c-methods, all parameters are passed, reserving 20 slots of stack space.
 * and then it is actually run and the return values are managed.
 *
 * For byte code, parameters are massaged to ensure the expected number of
 * fixed parameters and establish a holding place for the variable parameters.
 */
MethodTypes tailcallMorCPrep(Value th, Value *methodval, int getset) {

	// Capture the actual method whose code we will running
	Value realmethod;
	if (isEnc(*methodval, ArrEnc)) { // closure
		// Extract appropriate 'get' or 'set' method from closure. Be sure it is a method.
		realmethod = arrGet(th, *methodval, getset);
		if (!isMethod(realmethod))
			return returnNulls(th);
	}
	else
		realmethod = *methodval; // This is the actual method

	// Alter current call frame to specify method we are calling and parms
	CallInfo * ci = th(th)->curmethod;
	// Capture method whose code we are running, extracting from closure if needed
	ci->method = realmethod;
	int nparms = th(th)->stk_top - methodval - 1;
	memmove(ci->methodbase, methodval, (nparms+1)*sizeof(Value)); // Move new parms down over old ones
	th(th)->stk_top = ci->methodbase + nparms + 1;
	ci->retTo = ci->methodbase;  // Address of method value, varargs and return values
	ci->begin = ci->end = ci->methodbase + 1; // Parameter values are right after method value
	// Do not alter ci->nresults, as it is still correct for method we will return to

	// C-method call - Does the whole thing: setup, call and stack clean up after return
	if (isCMethod(ci->method)) {

		// Allocate room for local variables, no parm adjustments needed
		needMoreLocal(th, STACK_MINSIZE);
		methodRunC(th); // and then run it

		return isCMethod(th(th)->curmethod->method)? MethodC : MethodBC;
	}

	// Bytecode method call - Only does set-up, call done by caller
	else {
		BMethodInfo *bmethod = (BMethodInfo*) ci->method; // Capture it now before it moves

		// Initialize byte-code's call info
		ci->ip = bmethod->code; // Start with first instruction

		// Ensure sufficient data stack space.
		needMoreLocal(th, bmethod->maxstacksize); // methodval may no longer be reliable

		// If we do not have enough fixed parameters then add in nulls
		for (; nparms < methodNParms(bmethod); nparms++)
			*th(th)->stk_top++ = aNull;

		// If we expected variable number of parameters, tidy them
		if (isVarParm(bmethod)) {
			int i;

			// Reset where fixed parms start (where we are about to put them)
			Value *from = ci->begin; // Capture where fixed parms are now
			ci->begin = th(th)->stk_top;

			// Copy fixed parms up into new frame start
			for (i=0; i<methodNParms(bmethod); i++) {
				*th(th)->stk_top++ = *from;
				*from++ = aNull;  // we don't want dupes that might confuse garbage collector
			}
		}

		// Bytecode rarely uses stk_top; put it above local frame stack.
		th(th)->stk_top = ci->end;

		return MethodBC; // byte-code method return
	}
}

/* Prepare call to yielder value on stack (with parms above it). 
 * Specify how many return values to expect to find on stack.
 * Flags>>1 is retTo displacement
 * Returns MethodY
 *
 * Parameters are massaged to ensure the expected number of
 * fixed parameters and establish a holding place for the variable parameters.
 */
MethodTypes callYielderPrep(Value th, Value *methodval, int nexpected, int flags) {
	assert(isYielder(*methodval));
	ThreadInfo *yielder = (ThreadInfo*)*methodval;

	// A completed thread is no longer executable. Return nulls instead.
	if (yielder->flags1 & ThreadDone)
		return invalidCall(th, methodval, nexpected);

	// Copy parameters over to top of yielder's stack
	CallInfo *ycf = yielder->curmethod;
	Value *from = methodval+2; // skip 'self'
	int nparms = th(th)->stk_top - methodval - 2; // Actual number of parameters pushed
	int nulls = 0;
	if (ycf->nresults != BCVARRET) {
		if ((nulls = ycf->nresults - nparms) < 0) {
			nparms = ycf->nresults;
			nulls = 0;
		}
	}
	while (nparms-- > 0)
		*yielder->stk_top++ = *from++;
	while (nulls--)
		*yielder->stk_top++ = aNull;

	// Set return info in yielder's existing call frame, which is mostly set up correctly
	ycf->nresults = nexpected;
	ycf->retTo = methodval + (flags>>1);  // Address of method value, varargs and return values
	yielder->yieldTo = th;  // Preserve the current thread we will yield back to

	// Bytecode rarely uses stk_top; put it above local frame stack.
	th(yielder)->stk_top = ycf->end;
	return MethodY; // byte-code method return
}

/** Execute C method (and do return) at thread's current call frame 
 * Call and data stack already set up by callMorCPrep. */
void methodRunC(Value th) {
	CallInfo *ci =th(th)->curmethod;

	// Perform C method, capturing number of return values
	AintIdx have = (((CMethodInfo*)(ci->method))->methodp)(th);
	
	// Calculate how any we have to copy down and how many nulls for padding
	Value *from = th(th)->stk_top-have;
	Value* to = ci->retTo;
	AintIdx want = ci->nresults; // how many caller wants
	AintIdx nulls = 0; // how many nulls for padding
	if (have != want && want!=BCVARRET) { // performance penalty only to mismatches
		if (have > want) have = want; // cap down to what we want
		else /* if (have < want) */ nulls = want - have; // need some null padding
	}

	// Copy return values into previous frame's data stack
	while (have--)
		*to++ = *from++;
	while (nulls--)
		*to++ = aNull;  // Fill rest with nulls

	// Update thread's values
	th(th)->stk_top = to; // Mark position of last returned
	th(th)->curmethod = ci->previous; // Back up a frame
	return;
}

/** macro to make method calls consistent easier to read in methodRunBC */
#define methCall(firstreg, nexpected, flags) \
	switch (canCallMorC(*firstreg)? callMorCPrep(th, firstreg, nexpected, flags) \
		: isYielder(*firstreg)? callYielderPrep(th, firstreg, nexpected, flags) \
		: invalidCall(th, firstreg, nexpected)) { \
	case MethodY: \
		th = *rega; \
	case MethodBC: \
		ci = th(th)->curmethod; \
		meth = (BMethodInfo*) (ci->method); \
		lits = meth->lits; \
		stkbeg = ci->begin; \
	}

/* Execute byte-code method pointed at by thread's current call frame */
void methodRunBC(Value th) {
	CallInfo *ci = ((ThreadInfo*)th)->curmethod;
	BMethodInfo* meth = (BMethodInfo*) (ci->method);
	Value *lits = meth->lits; 
	Value *stkbeg = ci->begin;

	// main loop of interpreter
	for (;;) {
		Instruction i = *(ci->ip++);
		Value *rega = stkbeg + bc_a(i);
		assert(stkbeg == ci->begin);
		assert(stkbeg <= ((ThreadInfo*)th)->stk_top && ((ThreadInfo*)th)->stk_top < ((ThreadInfo*)th)->stack + ((ThreadInfo*)th)->size);
		switch (bc_op(i)) {

		// OpLoadReg: R(A) := R(B)
		case OpLoadReg:
			*rega = *(stkbeg + bc_b(i));
			break;

		// OpLoadRegs: R(A .. A+C-1) := R(B .. B+C-1)
		case OpLoadRegs: 
			memmove(rega, stkbeg+bc_b(i), bc_c(i)*sizeof(Value));
			break;

		// OpLoadLit: R(A) := Literals(D)
		case OpLoadLit:
			*rega = *(lits + bc_bx(i));
			break;

		// OpLoadLitX: R(A) := Literals(extra arg) {+ EXTRAARG(Ax)}
		case OpLoadLitx:
			assert(bc_op(*ci->ip) == OpExtraArg);
			*rega = *(lits + bc_ax(i));
			break;

		// OpLoadPrim: R(A) := B==0? null, B==1? false, B==2? true
		case OpLoadPrim:
			*rega = (Value)((((Auint)bc_b(i)) << ValShift) + ValCons);
			break;

		// OpLoadNulls: R(A), R(A+1), ..., R(A+B) := null
		case OpLoadNulls:
			{BCReg b = bc_b(i);
			do {
				*rega++ = aNull;
			} while (b--);}
			break;

		// OpLoadContext: R(A) := B==0? th, B==1? ci->methodbase
		case OpLoadContext:
			*rega = bc_b(i)==0? th : *(ci->methodbase);
			break;

		// OpLoadVararg: R(A), R(A+1), ..., R(A+B-1) := vararg
		// if (B == 0xFF), use actual number of varargs and set top
		case OpLoadVararg: {
			AuintIdx nbrvar = stkbeg - ci->methodbase - methodNParms(meth) - 1;
			AuintIdx cnt = bc_b(i);
			if (cnt==BCVARRET) {
				cnt = nbrvar;  /* get all var. arguments */
				needMoreLocal(th, nbrvar);
				rega = stkbeg + bc_a(i);  /* previous call may change the stack */
				th(th)->stk_top = rega + nbrvar;
			}
			for (AuintIdx j = 0; j < cnt; j++)
				*rega++ = (j < nbrvar)? *(stkbeg+j-nbrvar) : aNull;
			} break;

		// OpGetGlobal: R(A) := Globals(Literals(D))
		case OpGetGlobal:
			*rega = gloGet(th, *(lits + bc_bx(i)));
			break;

		// OpSetGlobal: Globals(Literals(D)) := R(A)
		case OpSetGlobal:
			gloSet(th, *(lits + bc_bx(i)), *rega);
			break;

		// OpGetClosure: R(A) := Closure(D)
		case OpGetClosure:
			if (isArr(*ci->methodbase))
				*rega = arrGet(th, *ci->methodbase, bc_b(i));
			break;

		// OpSetClosure: Closure(D) := R(A)
		case OpSetClosure:
			if (isArr(*ci->methodbase))
				arrSet(th, *ci->methodbase, bc_b(i), *rega);
			break;

		// OpJump: ip += sBx
		case OpJump:
			ci->ip += bc_j(i);
			break;

		// OpJNull: if R(A)===null then ip += sBx
		case OpJNull:
			if (*rega==aNull) ci->ip += bc_j(i); break;

		// OpJTrue: if R(A)!==null then ip += sBx
		case OpJNNull:
			if (*rega!=aNull) ci->ip += bc_j(i); break;

		// OpJTrue: if R(A) then ip += sBx
		case OpJTrue:
			if (!isFalse(*rega)) ci->ip += bc_j(i); break;

		// OpJFalse: if !R(A) then ip += sBx
		case OpJFalse:
			if (isFalse(*rega)) ci->ip += bc_j(i); break;

		// OpJSame: if R(A)===R(A+1) then ip += sBx.
		case OpJSame:
			if (isSame(*rega, *(rega+1))) ci->ip += bc_j(i); break;

		// OpJDiff: if R(A)!===R(A+1) then ip += sBx
		case OpJDiff:
			if (!isSame(*rega, *(rega+1))) ci->ip += bc_j(i); break;

		// OpJEq: if R(A)==0 then ip+= sBx.
		case OpJEq:
			if (*rega == anInt(0)) ci->ip += bc_j(i); break;

		// OpJEqN: if R(A)==0 or null then ip+= sBx.
		case OpJEqN:
			if (*rega == anInt(0) || *rega == aNull) ci->ip += bc_j(i); break;

		// OpJNe: if R(A)!=0 or not Integer then ip+= sBx.
		case OpJNe:
			if (*rega != anInt(0)) ci->ip += bc_j(i); break;

		// OpJNeN: if R(A)!=0 or not Integer then ip+= sBx.
		case OpJNeN:
			if (*rega != anInt(0) || *rega == aNull) ci->ip += bc_j(i); break;

		// OpJLt: if R(A)<0 then ip+= sBx.
		case OpJLt:
			if (isInt(*rega) && toAint(*rega) < 0) ci->ip += bc_j(i); break;

		// OpJLtN: if R(A)<0 or null then ip+= sBx.
		case OpJLtN:
			if (!isInt(*rega) || toAint(*rega) < 0) ci->ip += bc_j(i); break;

		// OpJLe: if R(A)<=0 then ip+= sBx.
		case OpJLe:
			if (isInt(*rega) && toAint(*rega) <= 0) ci->ip += bc_j(i); break;

		// OpJLeN: if R(A)<=0 or null then ip+= sBx.
		case OpJLeN:
			if (!isInt(*rega) || toAint(*rega) <= 0) ci->ip += bc_j(i); break;

		// OpJGt: if R(A)>0 then ip+= sBx.
		case OpJGt:
			if (isInt(*rega) && toAint(*rega) > 0) ci->ip += bc_j(i); break;

		// OpJGtN: if R(A)>0 or null then ip+= sBx.
		case OpJGtN:
			if (!isInt(*rega) || toAint(*rega) > 0) ci->ip += bc_j(i); break;

		// OpJGe: if R(A)>=0 then ip+= sBx.
		case OpJGe:
			if (isInt(*rega) && toAint(*rega) >= 0) ci->ip += bc_j(i); break;

		// OpJGeN: if R(A)>=0 or null then ip+= sBx.
		case OpJGeN:
			if (!isInt(*rega) || toAint(*rega) >= 0) ci->ip += bc_j(i); break;

		// LoadStd: R(A+1) := R(B); R(A) = StdMeth(C)
		case OpLoadStd:
			*(rega+1) = *(stkbeg + bc_b(i));
			*rega = vmStdSym(th, bc_c(i));
			break;

		// OpEachPrep: R(A) := R(B).Each
		case OpEachPrep:
			*rega = *(stkbeg + bc_b(i));
			if (isMethod(*rega)) {
				*(rega+1) = *(ci->begin); // self
				th(th)->stk_top = rega+2;
				methCall(rega, 1, 1);
			}
			else if (!canCall(*rega)) {
				*(rega+1) = *rega;
				*rega = getProperty(th, *rega, vmlit(SymEachMeth));
				th(th)->stk_top = rega+2;
				methCall(rega, 1, 1);
			}
			break;

		// OpEachSplat: R(A+1) := R(A)/null, R(A+2) := ...[R(A)], R(A):=R(A)+1
		case OpEachSplat: {
			AuintIdx nbrvar = stkbeg - ci->methodbase - methodNParms(meth) - 1;
			AuintIdx j = toAint(*rega);
			if (j<nbrvar) {
				*(rega+1) = *rega;
				*(rega+2) = *(stkbeg+j-nbrvar);
				*rega = anInt(j+1);
			}
			else {
				*(rega+1) = aNull;
				*(rega+2) = aNull;
			}
			} break;

		// OpGetMeth: R(A) := R(A).*R(A+1)
		// Get property's method value (unless property is an executable method)
		case OpGetMeth:
			if (!canCall(*rega))
				*rega = getProperty(th, *(rega+1), *rega); // Find executable
			break;

		// OpGetProp: R(A) := R(A).*R(A+1)
		// Get property value
		case OpGetProp:
			*rega = getProperty(th, *(rega+1), *rega); // Find executable
			break;

		// OpSetProp: R(A) := R(A).*R(A+1)=R(A+2)
		// Set self's member value, if this is a table or type
		case OpSetProp:
			if (isTbl(*(rega+1)))
				tblSet(th, *(rega+1), *rega, *(rega+2));
			*rega = *(rega+2);
			break;

		// OpGetActProp: R(A .. A+C-1) := R(A).R(A+1)
		// Get active property value
		case OpGetActProp: {
			*rega = getProperty(th, *(rega+1), *rega);
			if (canCall(*rega)) {
				th(th)->stk_top = rega+2; // Set fixed top for 2 values
				methCall(rega, bc_c(i), 0);
			}
			} break;

		// OpSetActProp: R(A) := R(A).R(A+1)=R(A+2)
		// Set active property value
		case OpSetActProp: {
			Value propval = getProperty(th, *(rega+1), *rega);
			if (canCall(propval)) {
				*rega = propval;
				th(th)->stk_top = rega+3; // Set fixed top for 2 values
				methCall(rega, 1, 1);
			}
			else {
				if (isType(*(rega+1)))
					tblSet(th, *(rega+1), *rega, *(rega+2));
				*rega = *(rega+2);
			}
			} break;

		// OpEachCall: R(A .. A+C-1) := R(A+1).R(A)(R(A+1) .. A+B-1))
		// if (B == 0xFF) then B = top. If (C == 0xFF), then top is set to last_result+1, 
		// so next open instruction (CALL, RETURN, VAR) may use top.
		case OpEachCall: {
			// Get property value. If executable, we can do call
			if (canCall(*rega)) {
				// Reset frame top for fixed parms (var already has it adjusted)
				int b = bc_b(i); // nbr of parms
				if (b != BCVARRET) 
					th(th)->stk_top = rega+b+1;

				// Prepare call frame and stack, then perform the call
				methCall(rega, bc_c(i), 2);
			}
			else {
				*(rega+1) = aNull;
			}
			} break;

		// OpGetCall: R(A .. A+C-1) := R(A+1).R(A)(R(A+1) .. A+B-1))
		// if (B == 0xFF) then B = top. If (C == 0xFF), then top is set to last_result+1, 
		// so next open instruction (CALL, RETURN, VAR) may use top.
		case OpGetCall: {
			// Get property value. If executable, we can do call
			if (!canCall(*rega))
				*rega = getProperty(th, *(rega+1), *rega); // Find executable

			// Reset frame top for fixed parms (var already has it adjusted)
			int b = bc_b(i); // nbr of parms
			if (b != BCVARRET) 
				th(th)->stk_top = rega+b+1;

			// Prepare call frame and stack, then perform the call
			methCall(rega, bc_c(i), 0);
			} break;

		// OpSetCall: R(A .. A+C-1) := R(A+1).R(A)(R(A+1) .. A+B-1))
		// if (B == 0xFF) then B = top. If (C == 0xFF), then top is set to last_result+1, 
		// so next open instruction (CALL, RETURN, VAR) may use top.
		case OpSetCall: {
			// Get property value. If executable, we can do call
			if (!canCall(*rega))
				*rega = getProperty(th, *(rega+1), *rega); // Find executable

			// Reset frame top for fixed parms (var already has it adjusted)
			int b = bc_b(i); // nbr of parms
			if (b != BCVARRET) 
				th(th)->stk_top = rega+b+1;

			// Prepare call frame and stack, then perform the call
			methCall(rega, bc_c(i), 1);
			} break;

		// OpTailCall: R(A .. A+C-1) := R(A+1).R(A)(R(A+1) .. A+B-1))
		// if (B == 0xFF) then B = top. C was set by who called us.
		case OpTailCall: {
			int b = bc_b(i); // nbr of parms
			// Reset frame top for fixed parms (var already has it adjusted)
			if (b != BCVARRET) 
				th(th)->stk_top = rega+b+1;

			// Cannot do a tailcall if the return would switch threads
			// In this situation, we must do a normal call then a return afterwards
			if (isYielder(th) && ci==&th(th)->entrymethod) {
				getCall(th, b, bc_c(i)); // Call

				// Return: Calculate how any we have to copy down and how many nulls for padding
				AintIdx have = th(th)->stk_top - rega; // Get all up to top
				AintIdx want = th(th)->curmethod->nresults; // how many caller wants
				AintIdx nulls = 0; // how many nulls for padding
				if (have != want && want!=BCVARRET) { // performance penalty only to mismatches
					if (have > want) have = want; // cap down to what we want
					else /* if (have < want) */ nulls = want - have; // need some null padding
				}

				// Copy return values into previous frame's data stack
				Value *from = rega;
				Value *to = th(th)->curmethod->retTo;
				while (have--)
					*to++ = *from++;
				while (nulls--)
					*to++ = aNull;  // Fill rest with nulls

				// Back up for thread switching
				// Mark that yielder is finished and cannot be used further
				th(th)->flags1 |= ThreadDone;

				// Switch current thread to caller
				th = th(th)->yieldTo;
				ci = th(th)->curmethod;
				th(th)->stk_top = to; // Mark position of last returned

				// Return to 'c' method caller, if we were called from there
				meth = (BMethodInfo*) ci->method;
				if (!isMethod(meth) || isCMethod(meth))
					return;

				// For bytecode, restore info from the previous call stack frame
				if (want != BCVARRET)
					th(th)->stk_top = ci->end; // Restore top to end for known # of returns
			}
			else {
				// Prepare call frame and stack, then perform the call
				if (!canCall(*rega))
					*rega = getProperty(th, *(rega+1), *rega);
				switch (canCallMorC(*rega)? tailcallMorCPrep(th, rega, 0)
					: isYielder(*rega)? callYielderPrep(th, rega, bc_c(i), 0)
					: returnNulls(th)) {
				case MethodC:
					return; // Return to C caller on bad tailcall attempt
				case MethodBC:
					ci = th(th)->curmethod;
					meth = (BMethodInfo*) (ci->method);
				}
			}
			lits = meth->lits;
			stkbeg = ci->begin;
			} break;

		// OpReturn: retTo(0 .. wanted) := R(A .. A+B-1); Return
	    //	if (B == 0xFF), it uses Top-1 (resets Top) rather than A+B-1 (Top=End)
		//	Caller sets retTo and wanted (if 0xFF, all return values)
		//  Return restores previous frame
		case OpReturn:
		{
			// Calculate how any we have to copy down and how many nulls for padding
			AintIdx have = bc_b(i); // return values we have
			if (have == BCVARRET) // have variable # of return values?
				have = th(th)->stk_top - rega; // Get all up to top
			AintIdx want = th(th)->curmethod->nresults; // how many caller wants
			AintIdx nulls = 0; // how many nulls for padding
			if (have != want && want!=BCVARRET) { // performance penalty only to mismatches
				if (have > want) have = want; // cap down to what we want
				else /* if (have < want) */ nulls = want - have; // need some null padding
			}

			// Copy return values into previous frame's data stack
			Value *from = rega;
			Value *to = th(th)->curmethod->retTo;
			while (have--)
				*to++ = *from++;
			while (nulls--)
				*to++ = aNull;  // Fill rest with nulls

			// Back up differently for thread switching vs. frame rollback
			if (isYielder(th) && ci==&th(th)->entrymethod) {
				// Mark that yielder is finished and cannot be used further
				th(th)->flags1 |= ThreadDone;

				// Switch current thread to caller
				th = th(th)->yieldTo;
				ci = th(th)->curmethod;
			}
			else
				// Back up a frame
				ci = th(th)->curmethod = ci->previous;
			th(th)->stk_top = to; // Mark position of last returned

			// Return to 'c' method caller, if we were called from there
			meth = (BMethodInfo*) ci->method;
			if (!isMethod(meth) || isCMethod(meth))
				return;

			// For bytecode, restore info from the previous call stack frame
			if (want != BCVARRET) 
				th(th)->stk_top = ci->end; // Restore top to end for known # of returns
			lits = meth->lits; 
			stkbeg = ci->begin;
			}
			break;

		// OpYield: retTo(0 .. wanted) := R(A .. A+B-1); Return
	    //	if (B == 0xFF), it uses Top-1 (resets Top) rather than A+B-1 (Top=End)
		//	Caller sets retTo and wanted (if 0xFF, all return values)
		//  Return restores previous thread
		case OpYield:
		{
			// Calculate how many return values we have to copy down and how many nulls for padding
			AintIdx have = bc_b(i); // return values we have
			if (have == BCVARRET) // have variable # of return values?
				have = th(th)->stk_top - rega; // Get all up to top
			AintIdx want = th(th)->curmethod->nresults; // how many caller wants
			AintIdx nulls = 0; // how many nulls for padding
			if (have != want && want!=BCVARRET) { // performance penalty only to mismatches
				if (have > want) have = want; // cap down to what we want
				else /* if (have < want) */ nulls = want - have; // need some null padding
			}

			// Copy return values into previous frame's data stack
			Value *from = rega;
			Value *to = th(th)->curmethod->retTo;
			while (have--)
				*to++ = *from++;
			while (nulls--)
				*to++ = aNull;  // Fill rest with nulls

			// Fix yielder stack pointer for callback
			th(th)->stk_top = rega;

			// Switch current thread/frame to caller
			th = th(th)->yieldTo;
			ci = th(th)->curmethod;

			// Mark position of last returned
			th(th)->stk_top = to; // Mark position of last returned

			// Return to 'c' method caller, if we were called from there
			meth = (BMethodInfo*) ci->method;
			if (!isMethod(meth) || isCMethod(meth))
				return;

			// For bytecode, restore info from the previous call stack frame
			if (want != BCVARRET) 
				th(th)->stk_top = ci->end; // Restore top to end for known # of returns

			// Restore info from the call stack frame
			lits = meth->lits; 
			stkbeg = ci->begin;
			}
			break;

		// Should never reach here
		default:
			assert(0 && "Invalid byte code");
			break;
	}
  }
}

/* Get a value's property using indexing parameters. Will call a method if found.
 * The stack holds the property symbol followed by nparms parameters (starting with self).
 * nexpected specifies how many return values to expect to find on stack.*/
void getCall(Value th, int nparms, int nexpected) {
	Value* methodpos = th(th)->stk_top - nparms - 1;

	// If "method" is not a method (likely a symbol), treat it as a property to lookup
	if (!canCall(*methodpos))
		*methodpos = getProperty(th, *(methodpos+1), *methodpos);

	// Prepare call frame and stack, then perform the call
	switch (canCallMorC(*methodpos)? callMorCPrep(th, methodpos, nexpected, 0)
		: isYielder(*methodpos)? callYielderPrep(th, methodpos, nexpected, 0)
		: invalidCall(th, methodpos, nexpected)) {
	case MethodY:
		methodRunBC(*methodpos);
		break;
	case MethodBC:
		methodRunBC(th);
		break;
	}
}

/* Set a value's property using indexing parameters. Will call a closure's set method if found.
 * The stack holds the property symbol followed by nparms parameters (starting with self).
 * The first value after self is the value to set.
 * nexpected specifies how many return values to expect to find on stack.*/
void setCall(Value th, int nparms, int nexpected) {
	Value* methodpos = th(th)->stk_top - nparms - 1;

	// If "method" is a symbol, treat it as a property to lookup
	if (!canCall(*methodpos))
		*methodpos = getProperty(th, *(methodpos+1), *methodpos);

	// Prepare call frame and stack, then perform the call
	switch (canCallMorC(*methodpos)? callMorCPrep(th, methodpos, nexpected, 1)
		: isYielder(*methodpos)? callYielderPrep(th, methodpos, nexpected, 1)
		: invalidCall(th, methodpos, nexpected)) {
	case MethodBC:
		methodRunBC(th);
		break;
	}
}

/** Serialize a,b,c for an op code */
void methABCSerialize(Value th, Value str, const char *op, Instruction i) {
	strAppend(th, str, op, strlen(op));
	serialize(th, str, 0, anInt(bc_a(i)));
	strAppend(th, str, ", ", 2);
	serialize(th, str, 0, anInt(bc_b(i)));
	strAppend(th, str, ", ", 2);
	serialize(th, str, 0, anInt(bc_c(i)));
}

/** Serialize a+value for an op code */
void methALSerialize(Value th, Value str, const char *op, Instruction i, Value lit) {
	strAppend(th, str, op, strlen(op));
	serialize(th, str, 0, anInt(bc_a(i)));
	strAppend(th, str, ", ", 2);
	serialize(th, str, 0, lit);
}

/** Serialize a,value,b for an op code */
void methABSSerialize(Value th, Value str, const char *op, Instruction i, Value lit) {
	strAppend(th, str, op, strlen(op));
	serialize(th, str, 0, anInt(bc_a(i)));
	strAppend(th, str, ", ", 2);
	serialize(th, str, 0, lit);
	strAppend(th, str, ", ", 2);
	serialize(th, str, 0, anInt(bc_b(i)));
}

/* Serialize an method's bytecode contents to indented text */
void methSerialize(Value th, Value str, int indent, Value method) {
	BMethodInfo* meth = (BMethodInfo*) (method);
	Value *lits = meth->lits;
	int ind;
	strAppend(th, str, "+Method", 7);
	for (AuintIdx ip=0; ip<meth->size; ip++) {
		Instruction i = meth->code[ip];
		strAppend(th, str, "\n", 1);
		ind = indent+1;
		while (ind--)
			strAppend(th, str, "\t", 1);
		serialize(th, str, indent+1, anInt(ip));
		strAppend(th, str, ": ", 2);
		switch (bc_op(i)) {
		case OpLoadReg: methABCSerialize(th, str, "LoadReg ", i); break;
		case OpLoadRegs: methABCSerialize(th, str, "LoadRegs ", i); break;
		case OpLoadLit: methALSerialize(th, str, "LoadLit ", i, *(lits + bc_bx(i))); break;
		case OpLoadLitx: methALSerialize(th, str, "LoadLitx ", i, *(lits + bc_ax(i))); ip++; break;
		case OpLoadPrim: methALSerialize(th, str, "LoadPrim ", i, (Value)((((Auint)bc_b(i)) << ValShift) + ValCons)); break;
		case OpLoadNulls: methABCSerialize(th, str, "LoadNulls ", i); break;
		case OpLoadContext: methABCSerialize(th, str, "LoadContext ", i); break;
		case OpLoadVararg: methABCSerialize(th, str, "LoadVararg ", i); break;
		case OpGetGlobal: methALSerialize(th, str, "GetGlobal ", i, *(lits + bc_bx(i))); break;
		case OpSetGlobal: methALSerialize(th, str, "SetGlobal ", i, *(lits + bc_bx(i))); break;
		case OpGetClosure: methABCSerialize(th, str, "GetClosure ", i); break;
		case OpSetClosure: methABCSerialize(th, str, "SetClosure ", i); break;
		case OpJump: methALSerialize(th, str, "Jump ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJNull: methALSerialize(th, str, "JNull ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJNNull: methALSerialize(th, str, "JNNull ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJTrue: methALSerialize(th, str, "JTrue ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJFalse: methALSerialize(th, str, "JFalse ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJSame: methALSerialize(th, str, "JSame ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJDiff: methALSerialize(th, str, "JDiff ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJEq: methALSerialize(th, str, "JEq ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJNe: methALSerialize(th, str, "JNe ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJLt: methALSerialize(th, str, "JLt ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJLe: methALSerialize(th, str, "JLe ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJGt: methALSerialize(th, str, "JGt ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJGe: methALSerialize(th, str, "JGe ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJEqN: methALSerialize(th, str, "JEqN ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJNeN: methALSerialize(th, str, "JNeN ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJLtN: methALSerialize(th, str, "JLtN ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJLeN: methALSerialize(th, str, "JLeN ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJGtN: methALSerialize(th, str, "JGtN ", i, anInt(ip+bc_j(i)+1)); break;
		case OpJGeN: methALSerialize(th, str, "JGeN ", i, anInt(ip+bc_j(i)+1)); break;
		case OpLoadStd: methABSSerialize(th, str, "LoadStd ", i, vmStdSym(th, bc_c(i))); break;
		case OpEachPrep: methABCSerialize(th, str, "EachPrep ", i); break;
		case OpEachSplat: methABCSerialize(th, str, "EachSplat ", i); break;
		case OpEachCall: methABCSerialize(th, str, "EachCall ", i); break;
		case OpGetMeth: methABCSerialize(th, str, "GetMeth ", i); break;
		case OpGetProp: methABCSerialize(th, str, "GetProp ", i); break;
		case OpSetProp: methABCSerialize(th, str, "SetProp ", i); break;
		case OpGetActProp:  methABCSerialize(th, str, "GetActProp ", i); break;
		case OpSetActProp:  methABCSerialize(th, str, "SetActProp ", i); break;
		case OpGetCall:  methABCSerialize(th, str, "GetCall ", i); break;
		case OpSetCall:  methABCSerialize(th, str, "SetCall ", i); break;
		case OpTailCall:  methABCSerialize(th, str, "TailCall ", i); break;
		case OpReturn: methABCSerialize(th, str, "Return ", i); break;
		case OpYield: methABCSerialize(th, str, "Yield ", i); break;
		default: strAppend(th, str, "Unknown Opcode", 14);
		}
	}
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif


