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

/* Return 1 if callable: a method or closure */
int isCallable(Value val) {
	return (isPtr(val) && (
		((MemInfo*) val)->enctyp==MethEnc
		|| (((MemInfo*) val)->enctyp==ArrEnc && arr_info(val)->flags1 & TypeClo)));
}

/* Prepare call to method value on stack (with parms above it). 
 * Specify how many return values to expect to find on stack.
 * Flags is 0 for normal get, 1 for set, and 2 for repeat get
 * Returns 0 if bad method, 1 if bytecode, 2 if C-method
 *
 * For c-methods, all parameters are passed, reserving 20 slots of stack space.
 * and then it is actually run.
 *
 * For byte code, parameters are massaged to ensure the expected number of
 * fixed parameters and establish a holding place for the variable parameters.
 */
MethodTypes callPrep(Value th, Value *methodval, int nexpected, int flags) {

	// Do not call if we do not have a valid method to call - return nulls or set val instead
	if (!isCallable(*methodval)) {
		// Copy the desired number of return values (nulls) where indicated
		CallInfo *ci =th(th)->curmethod;
		Value* to = ci->retTo + (flags&2);
		AintIdx want = ci->nresults; // how many caller wants
		if (flags==1) {
			*to++ = *(methodval+2); // For set call, return set value by default
			want--;
		}
		while (want--)
			*to++ = aNull;
		return MethodBad;
	}

	// Start and initialize a new CallInfo block
	CallInfo * ci = th(th)->curmethod = th(th)->curmethod->next ? th(th)->curmethod->next : thrGrowCI(th);
	ci->nresults = nexpected;
	ci->retTo = ci->methodbase = methodval + (flags&2);  // Address of method value, varargs and return values
	ci->begin = ci->end = methodval + 1; // Parameter values are right after method value

	// Capture the actual method whose code we are running
	if (isEnc(*methodval, ArrEnc))
		ci->method = arrGet(th, *methodval, flags&1); // extract from closure variables
	else
		ci->method = *methodval; // We already have the method

	// C-method call - Does the whole thing: setup, call and stack clean up after return
	if (isCMethod(ci->method)) {
		// ci->callstatus=0;

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

/** Prepare tailcall to method value on stack (with parms above it). 
 * Specify how many return values to expect to find on stack.
 * Returns 0 if bad method, 1 if bytecode, 2 if C-method
 *
 * For c-methods, all parameters are passed, reserving 20 slots of stack space.
 * and then it is actually run
 *
 * For byte code, parameters are massaged to ensure the expected number of
 * fixed parameters and establish a holding place for the variable parameters.
 */
MethodTypes tailcallPrep(Value th, Value *methodval, int nexpected, int getset) {

	// Return if we do not have a valid method to call
	// Respond to call failure with a return of setval or nulls
	if (!isMethod(*methodval) && !isClosure(*methodval)) {
		// Copy the desired number of return values (nulls) where indicated
		CallInfo *ci =th(th)->curmethod;
		Value* to = ci->retTo;
		AintIdx want = ci->nresults; // how many caller wants
		if (getset==1) {
			*to++ = *(methodval+2); // For set call, return set value by default
			want--;
		}
		while (want--)
			*to++ = aNull;

		// Update thread's values
		th(th)->stk_top = to; // Mark position of last returned
		ci = th(th)->curmethod = ci->previous; // Back up a frame

		// Return to 'c' method caller, if we were called from there
		if (!isMethod(ci->method) || isCMethod(ci->method))
			return MethodRet;

		// For bytecode, restore info from the previous call stack frame
		if (want != BCVARRET) 
			th(th)->stk_top = ci->end; // Restore top to end for known # of returns
		return MethodBad;
	}

	// Re-initialize current CallInfo frame
	CallInfo * ci = th(th)->curmethod;
	int nparms = th(th)->stk_top - methodval - 1;
	memmove(ci->methodbase, methodval, (nparms+1)*sizeof(Value)); // Move down parms
	th(th)->stk_top = ci->methodbase + nparms + 1;
	ci->nresults = nexpected;
	ci->retTo = ci->methodbase;  // Address of method value, varargs and return values
	ci->begin = ci->end = ci->methodbase + 1; // Parameter values are right after method value

	// Capture the actual method whose code we are running
	if (isEnc(*methodval, ArrEnc))
		ci->method = arrGet(th, *methodval, getset); // extract from closure variables
	else
		ci->method = *methodval; // We already have the method

	// C-method call - Does the whole thing: setup, call and stack clean up after return
	if (isCMethod(ci->method)) {
		// ci->callstatus=0;

		// Allocate room for local variables, no parm adjustments needed
		needMoreLocal(th, STACK_MINSIZE);
		methodRunC(th); // and then run it

		return MethodC; // C-method return
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

/** Execute C method (and do return) at thread's current call frame 
 * Call and data stack already set up by callPrep. */
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
	switch (callPrep(th, firstreg, nexpected, flags)) { \
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
			*rega = arrGet(th, ci->methodbase, bc_bx(i));
			break;

		// OpSetClosure: Closure(D) := R(A)
		case OpSetClosure:
			arrSet(th, ci->methodbase, bc_bx(i), *rega);
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

		// OpJSame: if R(A)!===R(B) then ip++. Jump follows.
		case OpJSame:
			if (!isSame(*rega, *(stkbeg+bc_b(i)))) ci->ip++; break;

		// OpJDiff: if R(A)===R(B) then ip++. Jump follows.
		case OpJDiff:
			if (isSame(*rega, *(stkbeg+bc_b(i)))) ci->ip++; break;

		// OpJEq: if R(A)==0 or not Integer then ip+= sBx.
		case OpJEq:
			if (!isInt(*rega) || *rega == anInt(0)) ci->ip += bc_j(i); break;

		// OpJNe: if R(A)!=0 or not Integer then ip+= sBx.
		case OpJNe:
			if (!isInt(*rega) || *rega != anInt(0)) ci->ip += bc_j(i); break;

		// OpJLt: if R(A)<0 or not Integer then ip+= sBx.
		case OpJLt:
			if (!isInt(*rega) || toAint(*rega) < 0) ci->ip += bc_j(i); break;

		// OpJLe: if R(A)<=0 or not Integer then ip+= sBx.
		case OpJLe:
			if (!isInt(*rega) || toAint(*rega) <= 0) ci->ip += bc_j(i); break;

		// OpJGt: if R(A)>0 or not Integer then ip+= sBx.
		case OpJGt:
			if (!isInt(*rega) || toAint(*rega) > 0) ci->ip += bc_j(i); break;

		// OpJGe: if R(A)>=0 or not Integer then ip+= sBx.
		case OpJGe:
			if (!isInt(*rega) || toAint(*rega) >= 0) ci->ip += bc_j(i); break;

		// LoadStd: R(A+1) := R(B); R(A) = StdMeth(C)
		case OpLoadStd:
			*(rega+1) = *(stkbeg + bc_b(i));
			*rega = vmStdSym(th, bc_c(i));
			break;

		// OpForPrep: R(A+1) := R(B); R(A) = R(A+1).StdMeth(C); R(A+2):=null
		case OpForPrep:
			*(rega+1) = *(stkbeg + bc_b(i));
			*rega = getProperty(th, *(rega+1), vmStdSym(th, bc_c(i)));
			*(rega+2) = aNull;
			break;

		// OpRptPrep: R(A+1) := R(B); R(A) = R(A+1).StdMeth(C)
		case OpRptPrep:
			*(rega+1) = *(stkbeg + bc_b(i));
			*rega = getProperty(th, *(rega+1), vmStdSym(th, bc_c(i)));
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
			if (isCallable(*rega)) {
				th(th)->stk_top = rega+2; // Set fixed top for 2 values
				methCall(rega, bc_c(i), 0);
			}
			} break;

		// OpSetActProp: R(A) := R(A).R(A+1)=R(A+2)
		// Set active property value
		case OpSetActProp: {
			Value propval = getProperty(th, *(rega+1), *rega);
			if (isCallable(propval)) {
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

		// OpGetCall: R(A .. A+C-1) := R(A+1).R(A)(R(A+1) .. A+B-1))
		// if (B == 0xFF) then B = top. If (C == 0xFF), then top is set to last_result+1, 
		// so next open instruction (CALL, RETURN, VAR) may use top.
		case OpGetCall: {
			// Get property value. If executable, we can do call
			*rega = getProperty(th, *(rega+1), *rega); // Find executable
			if (!isCallable(*rega)) {
				// If not callable, set up an indexed get/set
				*(rega+1) = *rega;
				*rega = getProperty(th, *(rega+1), vmlit(SymParas));
			}

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
			Value propcall = getProperty(th, *(rega+1), *rega); // Find executable
			if (isCallable(propcall)) 
				*rega = propcall;
			else {
				// If not callable, do an indexed get/set
				*(rega+1) = *rega;
				*rega = getProperty(th, *(rega+1), vmlit(SymParas));
			}

			// Reset frame top for fixed parms (var already has it adjusted)
			int b = bc_b(i); // nbr of parms
			if (b != BCVARRET) 
				th(th)->stk_top = rega+b+1;

			// Prepare call frame and stack, then perform the call
			methCall(rega, bc_c(i), 1);
			} break;

		// OpGetCall: R(A .. A+C-1) := R(A+1).R(A)(R(A+1) .. A+B-1))
		// if (B == 0xFF) then B = top. If (C == 0xFF), then top is set to last_result+1, 
		// so next open instruction (CALL, RETURN, VAR) may use top.
		case OpTailCall: {
			int b = bc_b(i); // nbr of parms
			// Reset frame top for fixed parms (var already has it adjusted)
			if (b != BCVARRET) 
				th(th)->stk_top = rega+b+1;

			// Prepare call frame and stack, then perform the call
			*rega = getProperty(th, *(rega+1), *rega);
			switch (tailcallPrep(th, rega, bc_c(i), 0)) {
			case MethodRet:
				return; // Return to C caller on bad tailcall attempt
			case MethodBad:
			case MethodBC:
				ci = th(th)->curmethod;
				meth = (BMethodInfo*) (ci->method);
				lits = meth->lits; // literals
				stkbeg = ci->begin;
			} }
			break;

		// OpRptCall: R(A+2 .. A+C+1) := R(A)(R(A+1) .. A+B-1))
		// if (B == 0xFF) then B = top. If (C == 0xFF), then top is set to last_result+1, 
		// so next open instruction (CALL, RETURN, VAR) may use top.
		case OpRptCall: {
			int b = bc_b(i); // nbr of parms
			// Reset frame top for fixed parms (var already has it adjusted)
			if (b != BCVARRET) 
				th(th)->stk_top = rega+b+1;

			// Prepare call frame and stack, then perform the call
			methCall(rega, bc_c(i), 2);
			} break;

		// OpReturn: retTo(0 .. wanted) := R(A .. A+B-1); Return
	    //	if (B == 0xFF), it uses Top-1 (resets Top) rather than A+B-1 (Top=End)
		//	Caller sets retTo and wanted (if 0xFF, all return values)
		//  Return restores previous frame
		case OpReturn:
		{
			// Calculate how any we have to copy down and how many nulls for padding
			Value* to = th(th)->curmethod->retTo;
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
			while (have--)
				*to++ = *rega++;
			while (nulls--)
				*to++ = aNull;  // Fill rest with nulls

			// Update thread's values
			th(th)->stk_top = to; // Mark position of last returned
			ci = th(th)->curmethod = ci->previous; // Back up a frame

			// Return to 'c' method caller, if we were called from there
			if (!isMethod(ci->method) || isCMethod(ci->method))
				return;

			// For bytecode, restore info from the previous call stack frame
			if (want != BCVARRET) 
				th(th)->stk_top = ci->end; // Restore top to end for known # of returns
			meth = (BMethodInfo*) (ci->method);
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
	if (!isCallable(*methodpos)) {
		*methodpos = getProperty(th, *(methodpos+1), *methodpos);
		// If property's value is not callable, set up an indexed get/set using '()' property
		if (!isCallable(*methodpos)) {
			*(methodpos+1) = *methodpos;	// property value we got becomes 'self'
			*methodpos = getProperty(th, *(methodpos+1), vmlit(SymParas)); // look up '()' method
		}
	}

	// Prepare call frame and stack, then perform the call
	switch (callPrep(th, methodpos, nexpected, 0)) {
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
	if (!isCallable(*methodpos)) {
		*methodpos = getProperty(th, *(methodpos+1), *methodpos);
		// If property's value is not callable, set up an indexed get/set using '()' property
		if (!isCallable(*methodpos)) {
			*(methodpos+1) = *methodpos;	// property value we got becomes 'self'
			*methodpos = getProperty(th, *(methodpos+1), vmlit(SymParas)); // look up '()' method
		}
	}

	// Prepare call frame and stack, then perform the call
	switch (callPrep(th, methodpos, nexpected, 1)) {
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
		case OpLoadStd: methABSSerialize(th, str, "LoadStd ", i, vmStdSym(th, bc_c(i))); break;
		case OpForPrep: methABSSerialize(th, str, "ForPrep ", i, vmStdSym(th, bc_c(i))); break;
		case OpRptPrep: methABSSerialize(th, str, "RptPrep ", i, vmStdSym(th, bc_c(i))); break;
		case OpGetProp: methABCSerialize(th, str, "GetProp ", i); break;
		case OpSetProp: methABCSerialize(th, str, "SetProp ", i); break;
		case OpGetActProp:  methABCSerialize(th, str, "GetActProp ", i); break;
		case OpSetActProp:  methABCSerialize(th, str, "SetActProp ", i); break;
		case OpGetCall:  methABCSerialize(th, str, "GetCall ", i); break;
		case OpSetCall:  methABCSerialize(th, str, "SetCall ", i); break;
		case OpTailCall:  methABCSerialize(th, str, "TailCall ", i); break;
		case OpRptCall:  methABCSerialize(th, str, "RptCall ", i); break;
		case OpReturn: methABCSerialize(th, str, "Return ", i); break;
		default: strAppend(th, str, "Unknown Opcode", 14);
		}
	}
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif


