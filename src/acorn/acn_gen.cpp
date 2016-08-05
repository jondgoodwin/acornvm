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
	mem_gccheck(th);	// Incremental GC before memory allocation events
	BMethodInfo *meth = (BMethodInfo*) mem_new(th, MethEnc, sizeof(BMethodInfo), NULL, 0);
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
	f->lits[f->nbrlits] = val;
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

/** Get the destination where Jump is going */
int genGetJump (CompInfo *comp, int ip) {
	int offset = bc_j(comp->method->code[ip]);
	if (offset == BCNO_JMP)  /* point to itself represents end of list */
		return BCNO_JMP;  /* end of list */
	else
		return (ip+1)+offset;  /* turn offset into absolute position */
}

/** Set the Jump instruction at ip to jump to dest instruction */
void genSetJump (CompInfo *comp, int ip, int dest) {
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

	// Handle simple rvals: register to register

	// Handle assignments that require we load rval (and other stuff) first
	unsigned int rreg = comp->nextreg; // Save where we put rvals
	if (vmlit(SymGlobal) == lvalop) {
		genExp(comp, rval);
		genAddInstr(comp, BCINS_ABx(OpSetGlobal, rreg, genAddLit(comp, astGet(th, lval, 1))));
	} else if (vmlit(SymActProp) == lvalop) {
		genDoProp(comp, lval, OpSetActProp, rval);
	} else if (vmlit(SymCallProp) == lvalop) {
		genDoProp(comp, lval, OpSetCall, rval);
	}
	comp->nextreg = rreg;
}

/** Generate the appropriate code for something that returns a value */
void genExp(CompInfo *comp, Value astseg) {
	Value th = comp->th;
	if (isSym(astseg)) {
		if (vmlit(SymThis) == astseg)
			genAddInstr(comp, BCINS_ABC(OpLoadReg, genNextReg(comp), comp->thisreg, 0));
		else if (vmlit(SymSelf) == astseg)
			genAddInstr(comp, BCINS_ABC(OpLoadReg, genNextReg(comp), 0, 0));
	} else {
		Value op = astGet(th, astseg, 0);
		if (vmlit(SymLit) == op) {
			genAddInstr(comp, BCINS_ABx(OpLoadLit, genNextReg(comp), genAddLit(comp, astGet(th, astseg, 1))));
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
			comp->thisreg = comp->nextreg;
			comp->thisop = astGet(th, astseg, 2);
			genExp(comp, astGet(th, astseg, 1));
			genStmts(comp, astGet(th, astseg, 3));
			comp->thisop = svthisop;
			comp->thisreg = svthis;
		}
	}
}

/** Generate a sequence of statements */
void genStmts(CompInfo *comp, Value astseg) {
	Value th = comp->th;
	unsigned int svnextreg = comp->nextreg;
	for (AuintIdx i=1; i<getSize(astseg); i++) {
		// Do append/prepend for all statements, if requested
		if (comp->thisop != aNull) {
			genAddInstr(comp, BCINS_ABx(OpLoadLit, genNextReg(comp), genAddLit(comp, comp->thisop)));
			genAddInstr(comp, BCINS_ABC(OpLoadReg, genNextReg(comp), comp->thisreg, 0));			
		}
		genExp(comp, astGet(comp->th, astseg, i));
		if (comp->thisop != aNull)
			genAddInstr(comp, BCINS_ABC(OpGetCall, svnextreg, comp->nextreg - svnextreg-1, 0));
		comp->nextreg = svnextreg;
	}
}

/* Generate a complete byte-code method by walking the 
 * Abstract Syntax Tree generated by the parser */
void genBMethod(CompInfo *comp) {
	// Initialize generation state
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

