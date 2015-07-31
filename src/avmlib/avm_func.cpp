/** Implements c-functions and c-methods.
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

/* Build a new c-function value, pointing to a function written in C */
Value aCFunc(Value th, AcFuncp func, const char* name, const char* src) {
	// Put on stack to keep safe from GC
	stkPush(th, newStr(th, name));
	stkPush(th, newStr(th, src));

	mem_gccheck(th);	// Incremental GC before memory allocation events
	CFuncInfo *fn = (CFuncInfo*) mem_new(th, FuncEnc, sizeof(CFuncInfo), NULL, 0);
	funcFlags(fn) = FUNC_FLG_C;
	fn->funcp = func;
	fn->name = stkFromTop(th,1);
	fn->source = stkFromTop(th,0);
	stkSetSize(th, -2); // Pop values off stack

	return fn;
}

/* Build a new c-method value, pointing to a function written in C */
Value aCMethod(Value th, AcFuncp func, const char* name, const char* src) {
	// Put on stack to keep safe from GC
	stkPush(th, newStr(th, name));
	stkPush(th, newStr(th, src));

	mem_gccheck(th);	// Incremental GC before memory allocation events
	CFuncInfo *fn = (CFuncInfo*) mem_new(th, FuncEnc, sizeof(CFuncInfo), NULL, 0);
	funcFlags(fn) = FUNC_FLG_C | FUNC_FLG_METHOD;
	fn->funcp = func;
	fn->name = stkFromTop(th,1);
	fn->source = stkFromTop(th,0);
	stkSetSize(th, -2); // Pop values off stack

	return fn;
}

/* Execute byte-code program pointed at by thread's current call frame */
void bfnRun(Value th) {
	CallInfo *ci = ((ThreadInfo*)th)->curfn;
	BFuncInfo* func = (BFuncInfo*) (*ci->funcbase);
	Value *lits = func->lits; 
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
			*rega = *(lits + bc_d(i));
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
			AuintIdx nbrvar = stkbeg - ci->funcbase - funcNParms(func) - 1;
			AuintIdx cnt = bc_b(i);
			if (cnt==BCVARRET) {
				cnt = nbrvar;  /* get all var. arguments */
				stkNeeds(th, nbrvar);
				rega = stkbeg + bc_a(i);  /* previous call may change the stack */
				th(th)->stk_top = rega + nbrvar;
			}
			for (AuintIdx j = 0; j < cnt; j++)
				*rega++ = (j < nbrvar)? *(stkbeg+j-nbrvar) : aNull;
			} break;

		// OpGetGlobal: R(A) := Globals(Literals(D))
		case OpGetGlobal:
			*rega = gloGet(th, *(lits + bc_d(i)));
			break;

		// OpSetGlobal: Globals(Literals(D)) := R(A)
		case OpSetGlobal:
			gloSet(th, *(lits + bc_d(i)), *rega);
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
			*rega = ss(th, bc_c(i));
			break;

		// OpCall: R(A .. A+C-1) := R(A+1).R(A)(R(A+1) .. A+B-1))
		// if (B == 0xFF) then B = top. If (C == 0xFF), then top is set to last_result+1, 
		// so next open instruction (CALL, RETURN, SETLIST) may use top.
		case OpCall: {
			int b = bc_b(i); // nbr of parms
			// Reset frame top for fixed parms (var already has it adjusted)
			if (b != BCVARRET) 
				th(th)->stk_top = rega+b+1;
			int nresults = bc_c(i); // number of results expected
			*rega = findMethod(th, *(rega+1), *rega);
			if (thrCalli(th, rega, nresults)) {
				ci = th(th)->curfn;
				ci->callstatus |= CIST_REENTRY;
				func = (BFuncInfo*) (*ci->funcbase);
				lits = func->lits; // literals
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
			Value* to = th(th)->curfn->retTo;
			AintIdx have = bc_b(i); // return values we have
			if (have == BCVARRET) // have variable # of return values?
				have = th(th)->stk_top - rega; // Get all up to top
			AintIdx want = th(th)->curfn->nresults; // how many caller wants
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
			th(th)->curfn = ci->previous; // Back up a frame

			// Return to 'c' function caller, if we were called from there
			if (!(ci->callstatus & CIST_REENTRY))  // 'ci' still the called one
				return;

			// For bytecode, restore info from the previous call stack frame
			ci = th(th)->curfn;
			assert(ci->callstatus & CIST_BYTECODE);
			if (want != BCVARRET) 
				th(th)->stk_top = ci->end; // Restore top to end for known # of returns
			func = (BFuncInfo*) (*ci->funcbase);
			lits = func->lits; 
			stkbeg = ci->begin;
			}
			break;

		// OpTailCall: return R(A)(R(A+1), ... ,R(A+B-1))
		// if (B == 0xFF) then B = top. If (C == 0xFF), then top is set to last_result+1, 
		// so next open instruction (CALL, RETURN, SETLIST) may use top.
		case OpTailcall: {
			int b = bc_b(i); // nbr of parms
			// Reset frame top for fixed parms (var already has it adjusted)
			if (b != BCVARRET) 
				th(th)->stk_top = rega+b+1;
			assert(bc_c(i) == BCVARRET);
			*rega = findMethod(th, *(rega+1), *rega);
			if (thrCalli(th, rega, BCVARRET)) {
				// byte-code tail call: put called frame (n) in place of caller one (o)
				CallInfo *nci = th(th)->curfn;
				CallInfo *oci = nci->previous;
				Value *nfunc = nci->funcbase;
				Value *ofunc = oci->funcbase;

				/* Move var and fixed parms down to old stack frame */
				Value *lim = nci->begin + funcNParms(*nfunc);
				for (AuintIdx aux = 0; nfunc + aux < lim; aux++)
					*(ofunc + aux) = *(nfunc + aux);

				// Correct old frame values
				oci->begin = ofunc + (nci->begin - nfunc);
				oci->end = th(th)->stk_top = ofunc + (th(th)->stk_top - nfunc);
				oci->ip = nci->ip;
				oci->callstatus |= CIST_TAIL;  /* function was tail called */
				ci = th(th)->curfn = oci;  /* remove new frame */
				assert(th(th)->stk_top == oci->begin + ((BFuncInfo*)(*ofunc))->maxstacksize);

				// Load up local variables
				func = (BFuncInfo*) (*ci->funcbase);
				lits = func->lits; // literals
				stkbeg = ci->begin;
			} else
				// We did full C-function call
				stkbeg = ci->begin;
			} break;

		// OpForPrep: R(A+1) := R(B); R(A) = R(A+1).StdMeth(C); R(A+2):=null
		case OpForPrep:
			*(rega+1) = *(stkbeg + bc_b(i));
			*rega = findMethod(th, *(rega+1), ss(th, bc_c(i)));
			*(rega+2) = aNull;
			break;

		// OpRptPrep: R(A+1) := R(B); R(A) = R(A+1).StdMeth(C)
		case OpRptPrep:
			*(rega+1) = *(stkbeg + bc_b(i));
			*rega = findMethod(th, *(rega+1), ss(th, bc_c(i)));
			break;

		// Should never reach here
		default:
			assert(0);
			break;
	}
  }
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif


