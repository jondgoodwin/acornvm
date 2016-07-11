/** Implements c-methods.
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
 */

#include "avmlib.h"
#include <string.h>

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/* Build a new c-method value, pointing to a method written in C */
Value aCMethod(Value th, AcMethodp method, const char* name, const char* src) {
	// Put on stack to keep safe from GC
	pushValue(th, newStr(th, name));
	pushValue(th, newStr(th, src));

	mem_gccheck(th);	// Incremental GC before memory allocation events
	CMethodInfo *meth = (CMethodInfo*) mem_new(th, MethEnc, sizeof(CMethodInfo), NULL, 0);
	methodFlags(meth) = METHOD_FLG_C;
	meth->methodp = method;
	meth->source = popValue(th);
	meth->name = popValue(th);

	return meth;
}

/* Build a new c-method value, pointing to a method written in C */
Value newCMethod(Value th, AcMethodp method) {
	mem_gccheck(th);	// Incremental GC before memory allocation events
	CMethodInfo *meth = (CMethodInfo*) mem_new(th, MethEnc, sizeof(CMethodInfo), NULL, 0);
	methodFlags(meth) = METHOD_FLG_C;
	meth->methodp = method;
	return meth;
}

/** Return codes from methodCallPrep */
enum MethodTypes {
	MethodBad,	//!< Not a valid method (probably unknown method)
	MethodBC,		//!< Byte-code method
	MethodC		//!< C-method
};

/** Prepare call to method value on stack (with parms above it). 
 * Specify how many return values to expect to find on stack.
 * Returns 0 if bad method, 1 if bytecode, 2 if C-method
 *
 * For c-methods, all parameters are passed, reserving 20 slots of stack space.
 *
 * For byte code, parameters are massaged to ensure the expected number of
 * fixed parameters and establish a holding place for the variable parameters.
 */
MethodTypes methodCallPrep(Value th, Value *methodval, int nexpected) {

	// Return if we do not have a valid method to call
	if (!isMethod(*methodval))
		return MethodBad;

	// Start and initialize a new CallInfo block
	CallInfo * ci = th(th)->curmethod = th(th)->curmethod->next ? th(th)->curmethod->next : thrGrowCI(th);
	ci->nresults = nexpected;
	ci->retTo = ci->methodbase = methodval;  // Address of method value, varargs and return values
	ci->begin = ci->end = methodval + 1; // Parameter values are right after method value

	// C-method call - Does the whole thing: setup, call and stack clean up after return
	if (isCMethod(*methodval)) {
		// ci->callstatus=0;

		// Allocate room for local variables, no parm adjustments needed
		needMoreLocal(th, STACK_MINSIZE);

		return MethodC; // C-method return
	}

	// Bytecode method call - Only does set-up, call done by caller
	else {
		int nparms = th(th)->stk_top - methodval - 1; // Actual number of parameters pushed
		BMethodInfo *bmethod = (BMethodInfo*) *methodval; // Capture it now before it moves

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
 *
 * For byte code, parameters are massaged to ensure the expected number of
 * fixed parameters and establish a holding place for the variable parameters.
 */
MethodTypes methodTailCallPrep(Value th, Value *methodval, int nexpected) {

	// Return if we do not have a valid method to call
	if (!isMethod(*methodval))
		return MethodBad;

	// Re-initialize current CallInfo frame
	CallInfo * ci = th(th)->curmethod;
	int nparms = th(th)->stk_top - methodval - 1;
	memmove(ci->methodbase, methodval, (nparms+1)*sizeof(Value)); // Move down parms
	th(th)->stk_top = ci->methodbase + nparms + 1;
	ci->nresults = nexpected;
	ci->retTo = ci->methodbase;  // Address of method value, varargs and return values
	ci->begin = ci->end = ci->methodbase + 1; // Parameter values are right after method value

	// C-method call - Does the whole thing: setup, call and stack clean up after return
	if (isCMethod(*methodval)) {
		// ci->callstatus=0;

		// Allocate room for local variables, no parm adjustments needed
		needMoreLocal(th, STACK_MINSIZE);

		return MethodC; // C-method return
	}

	// Bytecode method call - Only does set-up, call done by caller
	else {
		BMethodInfo *bmethod = (BMethodInfo*) *ci->methodbase; // Capture it now before it moves

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

/** Return nulls instead of doing an invalid method call */
void methodRetNulls(Value th) {

	// Copy the desired number of return values (nulls) where indicated
	CallInfo *ci =th(th)->curmethod;
	Value* to = ci->retTo;
	AintIdx want = ci->nresults; // how many caller wants
	while (want--)
		*to++ = aNull;
	return;
}

/** Execute C method (and do return) at thread's current call frame 
 * Call and data stack already set up by methodCallPrep. */
void methodRunC(Value th) {
	CallInfo *ci =th(th)->curmethod;

	// Perform C method, capturing number of return values
	AintIdx have = (((CMethodInfo*)(*ci->methodbase))->methodp)(th);
	
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

/** Execute byte-code method pointed at by thread's current call frame */
void methodRunBC(Value th) {
	CallInfo *ci = ((ThreadInfo*)th)->curmethod;
	BMethodInfo* meth = (BMethodInfo*) (*ci->methodbase);
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
			// Clone a string literal, as program might change it
			if (isStr(*rega))
				*rega = newStrl(th, str_cstr(*rega), str_size(*rega)); 
			break;

		// OpLoadLitX: R(A) := Literals(extra arg) {+ EXTRAARG(Ax)}
		case OpLoadLitx:
			assert(bc_op(*ci->ip) == OpExtraArg);
			*rega = *(lits + bc_ax(i));
			// Clone a string literal, as program might change it
			if (isStr(*rega))
				*rega = newStrl(th, str_cstr(*rega), str_size(*rega));
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

		// OpCall: R(A .. A+C-1) := R(A+1).R(A)(R(A+1) .. A+B-1))
		// if (B == 0xFF) then B = top. If (C == 0xFF), then top is set to last_result+1, 
		// so next open instruction (CALL, RETURN, VAR) may use top.
		case OpCall: {
			int b = bc_b(i); // nbr of parms
			// Reset frame top for fixed parms (var already has it adjusted)
			if (b != BCVARRET) 
				th(th)->stk_top = rega+b+1;

			// Prepare call frame and stack, then perform the call
			*rega = getProperty(th, *(rega+1), *rega);
			switch (methodCallPrep(th, rega, bc_c(i))) {
			case MethodBad: 
				methodRetNulls(th); 
				break;
			case MethodC:
				methodRunC(th);
				break;
			case MethodBC:
				ci = th(th)->curmethod;
				meth = (BMethodInfo*) (*ci->methodbase);
				lits = meth->lits; // literals
				stkbeg = ci->begin;
			} }
			break;

		// OpCall: R(A .. A+C-1) := R(A+1).R(A)(R(A+1) .. A+B-1))
		// if (B == 0xFF) then B = top. If (C == 0xFF), then top is set to last_result+1, 
		// so next open instruction (CALL, RETURN, VAR) may use top.
		case OpTailCall: {
			int b = bc_b(i); // nbr of parms
			// Reset frame top for fixed parms (var already has it adjusted)
			if (b != BCVARRET) 
				th(th)->stk_top = rega+b+1;

			// Prepare call frame and stack, then perform the call
			*rega = getProperty(th, *(rega+1), *rega);
			switch (methodTailCallPrep(th, rega, bc_c(i))) {
			case MethodBad: 
				methodRetNulls(th); 
				break;
			case MethodC:
				methodRunC(th);
				break;
			case MethodBC:
				ci = th(th)->curmethod;
				meth = (BMethodInfo*) (*ci->methodbase);
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
			MethodTypes ftyp = methodCallPrep(th, rega, bc_c(i));
			th(th)->curmethod->retTo += 2;
			switch (ftyp) {
			case MethodBad: 
				methodRetNulls(th); 
				break;
			case MethodC:
				methodRunC(th);
				break;
			case MethodBC:
				ci = th(th)->curmethod;
				meth = (BMethodInfo*) (*ci->methodbase);
				lits = meth->lits; // literals
				stkbeg = ci->begin;
			} }
			break;

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
			if (!isMethod(*ci->methodbase) || isCMethod(*ci->methodbase))
				return;

			// For bytecode, restore info from the previous call stack frame
			if (want != BCVARRET) 
				th(th)->stk_top = ci->end; // Restore top to end for known # of returns
			meth = (BMethodInfo*) (*ci->methodbase);
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

/* Call a method value placed on stack (with nparms above it). 
 * Indicate how many return values to expect to find on stack. */
void methodCall(Value th, int nparms, int nexpected) {
	Value* methodpos = th(th)->stk_top - nparms - 1;

	// If "method" is a symbol, treat it as a property to lookup
	if (isSym(*methodpos))
		*methodpos = getProperty(th, *(methodpos+1), *methodpos);

	// Prepare call frame and stack, then perform the call
	switch (methodCallPrep(th, methodpos, nexpected)) {
	case MethodBad: 
		methodRetNulls(th);
		break;
	case MethodC:
		methodRunC(th);
		break;
	case MethodBC:
		methodRunBC(th);
		break;
	}
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif


