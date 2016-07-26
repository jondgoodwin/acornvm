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
void genNew(Acorn* ac) {
	mem_gccheck(ac->th);	// Incremental GC before memory allocation events
	BMethodInfo *meth = (BMethodInfo*) mem_new(ac->th, MethEnc, sizeof(BMethodInfo), NULL, 0);
	methodFlags(meth) = 0;
	methodNParms(meth) = 0;

	meth->code = NULL;
	meth->maxstacksize = 20;
	meth->size = 0;
	meth->nextInst = 0;
	meth->lits = NULL;
	meth->litsz = 0;
	meth->nbrlits = 0;
	meth->locals = NULL;
	meth->localsz = 0;
	meth->nbrlocals = 0;

	ac->method = meth;
	ac->reg_top = 0;
}

/* Put new instruction in code array */
void genPutInstr(Acorn *ac, AuintIdx loc, Instruction i) {
	mem_growvector(ac->th, ac->method->code, loc, ac->method->size, Instruction, INT_MAX);
	ac->method->code[loc] = i;
}

/* Append new instruction to code array */
void genAddInstr(Acorn *ac, Instruction i) {
	mem_growvector(ac->th, ac->method->code, ac->method->nextInst, ac->method->size, Instruction, INT_MAX);
	ac->method->code[ac->method->nextInst++] = i;
}

/* Add a literal and return its index */
int genAddLit(Acorn *ac, Value val) {
	BMethodInfo* f = ac->method;

	// See if we already have it
	int i = f->nbrlits;
	while (i-- > 0)
		if (f->lits[i] == val)
			return i;

	// If not found, add it
	mem_growvector(ac->th, f->lits, f->nbrlits, f->litsz, Value, INT_MAX);
	f->lits[f->nbrlits] = val;
	return f->nbrlits++;
}

/* Look in reverse order for local variable, returning its register. Add if not found. */
int genLocalVar(Acorn *ac, Value varnm) {
	BMethodInfo* f = ac->method;
	assert(isSym(varnm));

	// Look to see if local variable already defined
	if (f->nbrlocals > 0) 
		for (int reg = f->nbrlocals - 1; reg >= 0; reg--) {
			if (f->locals[reg] == varnm)
				return reg;
		}

	// Not defined: add it
	mem_growvector(ac->th, f->locals, f->nbrlocals, f->localsz, Value, INT_MAX);
	f->locals[f->nbrlocals] = varnm;
	if (f->nbrlocals+1 > f->maxstacksize)
		f->maxstacksize = f->nbrlocals + 1;
	return f->nbrlocals++;
}

/* Add a parameter */
void genAddParm(Acorn *ac, Value varnm) {
	methodNParms(ac->method)++;
	genLocalVar(ac, varnm);
}

/* Indicate the method has a variable number of parameters */
void genVarParms(Acorn *ac) {
	methodFlags(ac->method) = METHOD_FLG_VARPARM;
}

/* Raise method's max stack size if register is above it */
void genMaxStack(Acorn *ac, AuintIdx reg) {
	if (ac->method->maxstacksize < reg)
		ac->method->maxstacksize = reg+1;
}

/** Get the destination where Jump is going */
int genGetJump (Acorn *ac, int ip) {
	int offset = bc_j(ac->method->code[ip]);
	if (offset == BCNO_JMP)  /* point to itself represents end of list */
		return BCNO_JMP;  /* end of list */
	else
		return (ip+1)+offset;  /* turn offset into absolute position */
}

/** Set the Jump instruction at ip to jump to dest instruction */
void genSetJump (Acorn *ac, int ip, int dest) {
	Instruction *jmp = &ac->method->code[ip];
	int offset = dest-(ip+1);
	assert(dest != BCNO_JMP);
	if (((offset+BCBIAS_J) >> 16)!=0)
		assert(0 && "control structure too long");
	*jmp = setbc_j(*jmp, offset);
}

/* Set the jump instruction link chain starting at listip to jump to dest */
void genSetJumpList(Acorn *ac, int listip, int dest) {
	while (listip != BCNO_JMP) {
		int next = genGetJump(ac, listip);
		genSetJump(ac, listip, dest);
		listip = next;
	}
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

