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

/* Create a new bytecode method value. */
void genNew(CompInfo *comp) {
	mem_gccheck(comp->th);	// Incremental GC before memory allocation events
	BMethodInfo *meth = (BMethodInfo*) mem_new(comp->th, MethEnc, sizeof(BMethodInfo), NULL, 0);
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

	comp->method = meth;
	comp->reg_top = 0;
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

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

