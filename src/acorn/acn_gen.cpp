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

/* Create a new bytecode function/method value. */
void genNew(Acorn* ac, int ismeth, Value name, Value src) {
	mem_gccheck(ac->th);	// Incremental GC before memory allocation events
	BFuncInfo *fn = (BFuncInfo*) mem_new(ac->th, FuncEnc, sizeof(BFuncInfo), NULL, 0);
	funcFlags(fn) = (ismeth)? FUNC_FLG_METHOD : 0;
	funcNParms(fn) = 0;

	fn->name = name;
	fn->source = src;
	fn->code = NULL;
	fn->maxstacksize = 20;
	fn->size = 0;
	fn->lits = NULL;
	fn->litsz = 0;
	fn->nbrlits = 0;
	fn->locals = NULL;
	fn->localsz = 0;
	fn->nbrlocals = 0;

	ac->func = fn;
	ac->ip = 0;
	ac->reg_top = 0;
}

/* Put new instruction in code array */
void genPutInstr(Acorn *ac, AuintIdx loc, Instruction i) {
	mem_growvector(ac->th, ac->func->code, loc, ac->func->size, Instruction, INT_MAX);
	ac->func->code[loc] = i;
}

/* Append new instruction to code array */
void genAddInstr(Acorn *ac, Instruction i) {
	mem_growvector(ac->th, ac->func->code, ac->ip, ac->func->size, Instruction, INT_MAX);
	ac->func->code[ac->ip++] = i;
}

/* Add a literal and return its index */
int genAddLit(Acorn *ac, Value val) {
	BFuncInfo* f = ac->func;

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
	BFuncInfo* f = ac->func;
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
	funcNParms(ac->func)++;
	genLocalVar(ac, varnm);
}

/* Indicate the function has a variable number of parameters */
void genVarParms(Acorn *ac) {
	funcFlags(ac->func) = FUNC_FLG_VARPARM;
}

/* Raise function's max stack size if register is above it */
void genMaxStack(Acorn *ac, AuintIdx reg) {
	if (ac->func->maxstacksize < reg)
		ac->func->maxstacksize = reg+1;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

