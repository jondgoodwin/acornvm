/** Bytecode generator for Acorn compiler
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
 */

#include "acorn.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/* Create a new bytecode method value. */
void newBMethod(Value th, Value *dest) {
	BMethodInfo *meth = (BMethodInfo*) mem_new(th, MethEnc, sizeof(BMethodInfo));
	*dest = (Value) meth;

	methodFlags(meth) = 0;
	methodNParms(meth) = 0;

	meth->code = NULL;
	meth->maxstacksize = 20;
	meth->avail = 0;
	meth->size = 0;
	meth->lits = NULL;
	meth->litsz = 0;
	meth->nbrlits = 0;
	meth->nbrexterns = 0;
	meth->locals = NULL;
	meth->localsz = 0;
	meth->nbrlocals = 0;
}

/* Put new instruction in code array */
void genPutInstr(CompInfo *comp, AuintIdx loc, Instruction i) {
	mem_growvector(comp->th, comp->method->code, loc, comp->method->avail, Instruction, INT_MAX);
	comp->method->code[loc] = i;
}

/* Append new instruction to code array */
void genAddInstr(CompInfo *comp, Instruction i) {
	mem_growvector(comp->th, comp->method->code, comp->method->size, comp->method->avail, Instruction, INT_MAX);
	comp->method->code[comp->method->size++] = i;
}

/* Add a literal and return its index */
int genAddLit(CompInfo *comp, Value val) {
	BMethodInfo* f = comp->method;

	// See if we already have it
	int i = f->nbrlits;
	while (i-- > 0)
		if (f->lits[i] == val)
			return i;

	// If not found, add it
	mem_growvector(comp->th, f->lits, f->nbrlits, f->litsz, Value, INT_MAX);
	if (isStr(val))
		str_info(val)->flags1 |= StrLiteral; // Make strings read only
	f->lits[f->nbrlits] = val;
	mem_markChk(comp->th, comp, val);
	return f->nbrlits++;
}

/* Look in reverse order for local variable, returning its register. Add if not found. */
int genLocalVar(CompInfo *comp, Value varnm) {
	BMethodInfo* f = comp->method;
	assert(isSym(varnm));

	// Look to see if local variable already defined
	if (f->nbrlocals > 0) 
		for (int reg = f->nbrlocals - 1; reg >= 0; reg--) {
			if (f->locals[reg] == varnm)
				return reg;
		}

	// Not defined: add it
	mem_growvector(comp->th, f->locals, f->nbrlocals, f->localsz, Value, INT_MAX);
	f->locals[f->nbrlocals] = varnm;
	if (f->nbrlocals+1 > f->maxstacksize)
		f->maxstacksize = f->nbrlocals + 1;
	return f->nbrlocals++;
}

/* Add a parameter */
void genAddParm(CompInfo *comp, Value varnm) {
	methodNParms(comp->method)++;
	genLocalVar(comp, varnm);
}

/* Indicate the method has a variable number of parameters */
void genVarParms(CompInfo *comp) {
	methodFlags(comp->method) = METHOD_FLG_VARPARM;
}

/* Raise method's max stack size if register is above it */
void genMaxStack(CompInfo *comp, AuintIdx reg) {
	if (comp->method->maxstacksize < reg)
		comp->method->maxstacksize = reg+1;
}

/** Get a node from an AST segment */
#define astGet(th, astseg, idx) (arrGet(th, astseg, idx))

void genExp(CompInfo *comp, Value astseg);
void genStmts(CompInfo *comp, Value astseg);

/** Return next available register to load values into */
unsigned int genNextReg(CompInfo *comp) {
	// Keep track of high-water mark for later stack allocation purposes
	if (comp->method->maxstacksize < comp->nextreg+1)
		comp->method->maxstacksize = comp->nextreg+1;
	return comp->nextreg++;
}

/** Get the destination where Jump is going */
int genGetJump(CompInfo *comp, int ip) {
	int offset = bc_j(comp->method->code[ip]);
	if (offset == BCNO_JMP)  /* point to itself represents end of list */
		return BCNO_JMP;  /* end of list */
	else
		return (ip+1)+offset;  /* turn offset into absolute position */
}

/** Set the Jump instruction at ip to jump to dest instruction */
void genSetJump(CompInfo *comp, int ip, int dest) {
	if (ip==BCNO_JMP)
		return;
	Instruction *jmp = &comp->method->code[ip];
	int offset = dest-(ip+1);
	assert(dest != BCNO_JMP);
	if (((offset+BCBIAS_J) >> 16)!=0)
		assert(0 && "control structure too long");
	*jmp = setbc_j(*jmp, offset);
}

/* Set the jump instruction link chain starting at listip to jump to dest */
void genSetJumpList(CompInfo *comp, int listip, int dest) {
	while (listip != BCNO_JMP) {
		int next = genGetJump(comp, listip);
		genSetJump(comp, listip, dest);
		listip = next;
	}
}

/** Generate a jump that goes forward, possibly as part of an jump chain */
void genFwdJump(CompInfo *comp, int op, int reg, int *ipchain) {
	// If part of a jmp chain, add this jump to the chain
	if (*ipchain != BCNO_JMP) {
		// Find last jump in chain
		int jumpip;
		int nextip = *ipchain;
		do {
			jumpip = nextip;
			nextip = genGetJump(comp, jumpip);
		} while (nextip != BCNO_JMP);
		// Fix it to point to jump we are about to generate
		genSetJump(comp, jumpip, comp->method->size);
	}
	else
		*ipchain = comp->method->size; // New chain starts with this jump
	genAddInstr(comp, BCINS_AJ(op, reg, BCNO_JMP));
}

/** Generate conditional tests & appropriate jump(s), handled recursively for boolean operators.
	failjump is ip for first jump past the code to run on condition's success.
	passjump is ip for first jump directly to condition's success.
	notflag is true if under influence of 'not' operator: reversing jumps and and/or.
	lastjump specifies how last jump should behave: true for fail jump, false for passjump. true reverses jump condition. */
void genJumpExp(CompInfo *comp, Value astseg, int *failjump, int *passjump, bool notflag, bool lastjump) {
	Value th = comp->th;
	unsigned int svnextreg = comp->nextreg;
	Value condop = isArr(astseg)? astGet(th, astseg, 0) : astseg;
	bool revjump = notflag ^ lastjump;		// Reverse jump based on not flag and lastjump

	// Comparison ops (e.g., == or <) based on rocket operator - generation code comes later.
	int jumpop;
	if (condop == vmlit(SymLt)) jumpop = revjump? OpJGeN : OpJLt;
	else if (condop == vmlit(SymLe)) jumpop = revjump? OpJGtN : OpJLe;
	else if (condop == vmlit(SymGt)) jumpop = revjump? OpJLeN : OpJGt;
	else if (condop == vmlit(SymGe)) jumpop = revjump? OpJLtN : OpJGe;
	else if (condop == vmlit(SymEq)) jumpop = revjump? OpJNeN : OpJEq;
	else if (condop == vmlit(SymNe)) jumpop = revjump? OpJEqN : OpJNe;

	// '===' exact equivalence
	else if (condop == vmlit(SymEquiv)) {
		genExp(comp, astGet(th, astseg, 1));
		genExp(comp, astGet(th, astseg, 2));
		comp->nextreg = svnextreg;
		genFwdJump(comp, revjump? OpJDiff : OpJSame, svnextreg, lastjump? failjump : passjump);
		return;
	}

	// '~=' pattern match
	else if (condop == vmlit(SymMatchOp)) {
		genAddInstr(comp, BCINS_ABx(OpLoadLit, genNextReg(comp), genAddLit(comp, vmlit(SymMatchOp))));
		genExp(comp, astGet(th, astseg, 2)); // '~=' uses right hand value for object call
		genExp(comp, astGet(th, astseg, 1));
		genAddInstr(comp, BCINS_ABC(OpGetCall, svnextreg, comp->nextreg - svnextreg-1, 1));
		comp->nextreg = svnextreg;
		genFwdJump(comp, revjump? OpJFalse : OpJTrue, svnextreg,  lastjump? failjump : passjump);
		return;
	}

	else if (condop == vmlit(SymNot)) {
		genJumpExp(comp, astGet(th, astseg, 1), failjump, passjump, !notflag, lastjump);
		return;
	}

	else if (condop == vmlit(SymOr) || condop == vmlit(SymAnd)) {
		bool isAnd = (condop == vmlit(SymAnd)) ^ notflag; // Treat it as 'And' (or 'Or')?
		AuintIdx segi = 1;
		if (isAnd) {
			while (segi < getSize(astseg)-1) {
				genJumpExp(comp, astGet(th, astseg, segi++), failjump, passjump, notflag, true);
			}
			genJumpExp(comp, astGet(th, astseg, segi), failjump, passjump, notflag, lastjump);
			return;
		}
		else {
			int newpassjump = BCNO_JMP;
			while (segi < getSize(astseg)-1) {
				int newfailjump = BCNO_JMP;
				genJumpExp(comp, astGet(th, astseg, segi++), &newfailjump, &newpassjump, notflag, false);
				genSetJump(comp, newfailjump, comp->method->size);
			}
			genJumpExp(comp, astGet(th, astseg, segi), failjump, &newpassjump, notflag, lastjump);
			genSetJumpList(comp, newpassjump, comp->method->size); // Fix 'or' jumps to here
			return;
		}
	}

	// Otherwise, an expression to be interpreted as false/null or true (anything else)
	// (which includes explicit use of <==>)
	else {
		genExp(comp, astseg);
		comp->nextreg = svnextreg;
		genFwdJump(comp, revjump? OpJFalse : OpJTrue, svnextreg,  lastjump? failjump : passjump);
		return;
	}

	// Generate code for rocket-based comparisons
	genAddInstr(comp, BCINS_ABx(OpLoadLit, genNextReg(comp), genAddLit(comp, vmlit(SymRocket))));
	genExp(comp, astGet(th, astseg, 1));
	genExp(comp, astGet(th, astseg, 2));
	genAddInstr(comp, BCINS_ABC(OpGetCall, svnextreg, comp->nextreg - svnextreg-1, 1));
	comp->nextreg = svnextreg;
	genFwdJump(comp, jumpop, svnextreg,  lastjump? failjump : passjump);
}

/** Generate code for some kind of property 'get' */
void genDoProp(CompInfo *comp, Value astseg, char byteop, Value rval) {
	Value th = comp->th;
	unsigned int svreg = comp->nextreg; // Save

	// <<<< optimize here by seeing if property is a std symbol and self is in register

	genExp(comp, astGet(th, astseg, 2)); // property
	genExp(comp, astGet(th, astseg, 1)); // self
	if (rval!=aNull)
		genExp(comp, rval);	// For sets

	// Load as many parameters as we have, then do property get
	for (AuintIdx i = 3; i<getSize(astseg); i++)
		genExp(comp, astGet(th, astseg, i));
	genAddInstr(comp, BCINS_ABC(byteop, svreg, comp->nextreg - svreg-1, 1));
	comp->nextreg = svreg+1;
}

/** Generate code for an assignment */
void genAssign(CompInfo *comp, Value lval, Value rval) {
	Value th = comp->th;
	Value lvalop = astGet(th, lval, 0);

	// Handle assignments that require we load rval (and other stuff) first
	unsigned int rreg = comp->nextreg; // Save where we put rvals
	if (vmlit(SymLocal) == lvalop) {
		// Optimize load straight into register, if possible
		// (But don't do it if we need a consistent register to find the value in)
		Value rvalop = astGet(th, rval, 0);
		int localreg = toAint(astGet(th, lval, 1));
		if (isSym(rval)) {
			if (vmlit(SymThis) == rval)
				genAddInstr(comp, BCINS_ABC(OpLoadReg, localreg, comp->thisreg, 0));
			else if (vmlit(SymSelf) == rval)
				genAddInstr(comp, BCINS_ABC(OpLoadReg, localreg, 0, 0));
			else if (vmlit(SymBaseurl) == rval)
				genAddInstr(comp, BCINS_ABx(OpLoadLit, localreg, genAddLit(comp, comp->lex->url)));
		} else {
			if (vmlit(SymLit) == rvalop) {
				Value litval = astGet(th, rval, 1);
				if (litval==aNull)
					genAddInstr(comp, BCINS_ABC(OpLoadPrim, localreg, 0, 0));
				else if (litval==aFalse)
					genAddInstr(comp, BCINS_ABC(OpLoadPrim, localreg, 1, 0));
				else if (litval==aTrue)
					genAddInstr(comp, BCINS_ABC(OpLoadPrim, localreg, 2, 0));
				else
					genAddInstr(comp, BCINS_ABx(OpLoadLit, localreg, genAddLit(comp, litval)));
			} else if (vmlit(SymLocal) == rvalop) {
				genAddInstr(comp, BCINS_ABC(OpLoadReg, localreg, toAint(astGet(th, rval, 1)), 0));
			} else if (vmlit(SymGlobal) == rvalop) {
				genAddInstr(comp, BCINS_ABx(OpGetGlobal, localreg, genAddLit(comp, astGet(th, rval, 1))));
			} else {
				genExp(comp, rval);
				genAddInstr(comp, BCINS_ABC(OpLoadReg, localreg, rreg, 0));
			}
		}
	} else if (vmlit(SymGlobal) == lvalop) {
		genExp(comp, rval);
		genAddInstr(comp, BCINS_ABx(OpSetGlobal, rreg, genAddLit(comp, astGet(th, lval, 1))));
	} else if (vmlit(SymActProp) == lvalop) {
		genDoProp(comp, lval, OpSetActProp, rval);
	} else if (vmlit(SymRawProp) == lvalop) {
		genDoProp(comp, lval, OpSetProp, rval);
	} else if (vmlit(SymCallProp) == lvalop) {
		genDoProp(comp, lval, OpSetCall, rval);
	}
}

/** Return true if the expression makes no use of any logical or comparative operators */
bool hasNoBool(Value th, Value astseg) {
	for (AuintIdx segi = 1; segi < getSize(astseg)-1; segi++) {
		Value op = astGet(th, astseg, segi);
		op = isArr(op)? astGet(th, op, 0) : op;
		if (vmlit(SymAnd)==op || vmlit(SymOr)==op || vmlit(SymNot)==op
			|| vmlit(SymEquiv) == op || vmlit(SymMatchOp) == op
			|| vmlit(SymEq)==op || vmlit(SymNe)==op
			|| vmlit(SymGt)==op || vmlit(SymGe)==op || vmlit(SymLt)==op || vmlit(SymLe)==op)
			return false;
	}
	return true;
}

/** Generate the appropriate code for something that returns a value */
void genExp(CompInfo *comp, Value astseg) {
	Value th = comp->th;
	if (isSym(astseg)) {
		if (vmlit(SymThis) == astseg)
			genAddInstr(comp, BCINS_ABC(OpLoadReg, genNextReg(comp), comp->thisreg, 0));
		else if (vmlit(SymSelf) == astseg)
			genAddInstr(comp, BCINS_ABC(OpLoadReg, genNextReg(comp), 0, 0));
		else if (vmlit(SymBaseurl) == astseg)
			genAddInstr(comp, BCINS_ABx(OpLoadLit, genNextReg(comp), genAddLit(comp, comp->lex->url)));
	} else {
		Value op = astGet(th, astseg, 0);
		if (vmlit(SymLit) == op) {
			Value litval = astGet(th, astseg, 1);
			if (litval==aNull)
				genAddInstr(comp, BCINS_ABC(OpLoadPrim, genNextReg(comp), 0, 0));
			else if (litval==aFalse)
				genAddInstr(comp, BCINS_ABC(OpLoadPrim, genNextReg(comp), 1, 0));
			else if (litval==aTrue)
				genAddInstr(comp, BCINS_ABC(OpLoadPrim, genNextReg(comp), 2, 0));
			else
				genAddInstr(comp, BCINS_ABx(OpLoadLit, genNextReg(comp), genAddLit(comp, litval)));
		} else if (vmlit(SymExt) == op) {
			genAddInstr(comp, BCINS_ABx(OpLoadLit, genNextReg(comp), toAint(astGet(th, astseg, 1))));
		} else if (vmlit(SymLocal) == op) {
			genAddInstr(comp, BCINS_ABC(OpLoadReg, genNextReg(comp), toAint(astGet(th, astseg,1)), 0));
		} else if (vmlit(SymGlobal) == op) {
			genAddInstr(comp, BCINS_ABx(OpGetGlobal, genNextReg(comp), genAddLit(comp, astGet(th, astseg, 1))));
		} else if (vmlit(SymAssgn) == op) {
			genAssign(comp, astGet(th, astseg, 1), astGet(th, astseg, 2));
		} else if (vmlit(SymActProp) == op) {
			genDoProp(comp, astseg, OpGetActProp, aNull);
		} else if (vmlit(SymCallProp) == op) {
			genDoProp(comp, astseg, OpGetCall, aNull);
		} else if (vmlit(SymThisBlock) == op) {
			unsigned int svthis = comp->thisreg;
			Value svthisop = comp->thisop;
			int thisreg = comp->nextreg;
			genExp(comp, astGet(th, astseg, 1));
			comp->thisreg = thisreg;
			comp->thisop = astGet(th, astseg, 2);
			genStmts(comp, astGet(th, astseg, 3));
			comp->thisop = svthisop;
			comp->thisreg = svthis;
		} else if (vmlit(SymQuestion) == op) { // Ternary
			int svnextreg = comp->nextreg;
			int failjump = BCNO_JMP;
			int passjump = BCNO_JMP;
			genJumpExp(comp, astGet(th, astseg, 1), &failjump, NULL, false, true);
			int nextreg = genNextReg(comp);
			comp->nextreg = svnextreg;
			genExp(comp, astGet(th, astseg, 2));
			genFwdJump(comp, OpJump, 0, &passjump);
			genSetJumpList(comp, failjump, comp->method->size);
			comp->nextreg = svnextreg;
			genExp(comp, astGet(th, astseg, 3));
			genSetJumpList(comp, passjump, comp->method->size);
		} else if ((vmlit(SymOr)==op || vmlit(SymAnd)==op) && hasNoBool(th, astseg)) {
			// 'Pure' and/or conditional processing
			int svnextreg = comp->nextreg;
			int jumpip = BCNO_JMP;
			AuintIdx segi;
			for (segi = 1; segi < getSize(astseg)-1; segi++) {
				genExp(comp, astGet(th, astseg, segi));
				comp->nextreg = svnextreg;
				genFwdJump(comp, op==vmlit(SymOr)? OpJTrue : OpJFalse, svnextreg, &jumpip);
			}
			genExp(comp, astGet(th, astseg, segi));
			genSetJumpList(comp, jumpip, comp->method->size);
		} else if (vmlit(SymAnd)==op || vmlit(SymOr)==op || vmlit(SymNot)==op
			|| vmlit(SymEquiv) == op || vmlit(SymMatchOp) == op
			|| vmlit(SymEq)==op || vmlit(SymNe)==op
			|| vmlit(SymGt)==op || vmlit(SymGe)==op || vmlit(SymLt)==op || vmlit(SymLe)==op)
		{
			// Conditional/boolean expression, resolved to 'true' or 'false'
			int failjump = BCNO_JMP;
			genJumpExp(comp, astseg, &failjump, NULL, false, true);
			int nextreg = genNextReg(comp);
			genAddInstr(comp, BCINS_ABC(OpLoadPrim, nextreg, 2, 0));
			genAddInstr(comp, BCINS_AJ(OpJump, 0, 1));
			genSetJumpList(comp, failjump, comp->method->size);
			genAddInstr(comp, BCINS_ABC(OpLoadPrim, nextreg, 1, 0));
		}
	}
}

/** Generate all if/elif/else blocks */
void genIf(CompInfo *comp, Value astseg) {
	Value th = comp->th;

	int jumpEndIp = BCNO_JMP;	// Instruction pointer to first jump to end of if

	// Process all condition/block pairs in astseg
	AuintIdx ifindx = 1;		// Index into astseg for each pair
	do {
		// Generate conditional jump for bypassing block on condition failure
		Value condast = astGet(th, astseg, ifindx);
		int jumpNextIp = BCNO_JMP;	// Instruction pointer to jump to next elif/else block
		if (condast != vmlit(SymElse))
			genJumpExp(comp, condast, &jumpNextIp, NULL, false, true);
		genStmts(comp, astGet(th, astseg, ifindx+1)); // Generate block
		// Generate/fix jumps after clause's block
		if (condast != vmlit(SymElse)) {
			if (ifindx+2 < getSize(astseg))
				genFwdJump(comp, OpJump, 0, &jumpEndIp);
			genSetJumpList(comp, jumpNextIp, comp->method->size); // Fix jumps to next elif/else block
		}
		ifindx += 2;
	} while (ifindx < getSize(astseg));
	genSetJumpList(comp, jumpEndIp, comp->method->size); // Fix jumps to end of 'if' 
}

/** Generate while block */
void genWhile(CompInfo *comp, Value astseg) {
	Value th = comp->th;
	int svJumpBegIp = comp->whileBegIp;
	int svJumpEndIp = comp->whileEndIp;
	comp->whileBegIp = comp->method->size;
	comp->whileEndIp = BCNO_JMP;
	genJumpExp(comp, astGet(th, astseg, 1), &comp->whileEndIp, NULL, false, true);
	genStmts(comp, astGet(th, astseg, 2)); // Generate block
	genAddInstr(comp, BCINS_AJ(OpJump, 0, comp->whileBegIp - comp->method->size-1));
	genSetJumpList(comp, comp->whileEndIp, comp->method->size); // Fix jumps to end of 'while' block
	comp->whileBegIp = svJumpBegIp;
	comp->whileEndIp = svJumpEndIp;
}

/** Generate a statement */
void genStmt(CompInfo *comp, Value aststmt) {
	Value th = comp->th;
	unsigned int svnextreg = comp->nextreg;

	// Do append/prepend of value generated by the statement, if 'this' block requested this
	if (comp->thisop != aNull) {
		genAddInstr(comp, BCINS_ABx(OpLoadLit, genNextReg(comp), genAddLit(comp, comp->thisop)));
		genAddInstr(comp, BCINS_ABC(OpLoadReg, genNextReg(comp), comp->thisreg, 0));			
	}

	// Handle various kinds of statements
	Value op = isArr(aststmt)? astGet(th, aststmt, 0) : aststmt;
	if (op==vmlit(SymIf)) genIf(comp, aststmt); 
	else if (op==vmlit(SymWhile)) genWhile(comp, aststmt);
	else if (op==vmlit(SymBreak) && comp->whileBegIp!=-1)
		genFwdJump(comp, OpJump, 0, &comp->whileEndIp);
	else if (op==vmlit(SymContinue) && comp->whileBegIp!=-1)
		genAddInstr(comp, BCINS_AJ(OpJump, 0, comp->whileBegIp - comp->method->size-1));
	else genExp(comp, aststmt);

	// Finish append/prepend
	if (comp->thisop != aNull)
		genAddInstr(comp, BCINS_ABC(OpGetCall, svnextreg, comp->nextreg - svnextreg-1, 0));

	comp->nextreg = svnextreg;
}

/** Generate one or a sequence of statements */
void genStmts(CompInfo *comp, Value astseg) {
	Value th = comp->th;
	if (isArr(astseg) && astGet(comp->th, astseg, 0)==vmlit(SymSemicolon)) {
		for (AuintIdx i=1; i<getSize(astseg); i++) {
			genStmt(comp, astGet(comp->th, astseg, i));
		}
	}
	else
		genStmt(comp, astseg);
}

/* Generate a complete byte-code method by walking the 
 * Abstract Syntax Tree generated by the parser */
void genBMethod(CompInfo *comp) {
	// Initialize generation state
	comp->method->nbrexterns = comp->method->nbrlits;
	comp->nextreg = comp->method->maxstacksize = comp->method->nbrlocals;
	comp->thisreg = 0; // Starts with 'self'
	comp->thisop = aNull;

	// Root node is always 'method', its second node is always statements
	genAddInstr(comp, BCINS_ABC(OpLoadNulls, comp->method->nbrlocals, 0, 0));
	genStmts(comp, astGet(comp->th, comp->ast, 1));
	genAddInstr(comp, BCINS_ABC(OpReturn, comp->method->nbrlocals, 1, 0));
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
