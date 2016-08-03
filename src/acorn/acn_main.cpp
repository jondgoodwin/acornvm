/** Main module for Acorn compiler
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

/* Return a new CompInfo value, compiler state for an Acorn method */
Value newCompiler(Value th, Value *dest, Value src, Value url) {
	CompInfo *comp;
	mem_gccheck(th);	// Incremental GC before memory allocation events

	// Create an compiler context (this block of code can be gc-locked as atomic)
	MemInfo **linkp = NULL;
	comp = (CompInfo *) mem_new(th, CompEnc, sizeof(CompInfo), linkp, 0);
	*dest = (Value) comp;
	comp->th = th;
	comp->lex = NULL;
	comp->ast = (Value) NULL;
	comp->method = NULL;

	newLex(th, (Value *) &comp->lex, src, url);
	newArr(th, &comp->ast, aNull, 2);
	newBMethod(th, (Value *)&comp->method);

	comp->reg_top = 0;

	return *dest;
}

//! Get index for a standard symbol identified using its VM literal name
#define ss(lit) (toAint(tblGet(th, vm(th)->stdidx, vmlit(lit))))

/** Generate bytecode test programs */
Value genTestPgm(Value th, int pgm) {
	int saveip = 0;
	Value src = pushString(th, aNull, "");
	CompInfo* ac = (CompInfo*) pushCompiler(th, src, aNull);
	pushValue(th, ac->method);
	Value self = pushSym(th, "self");
	switch (pgm) {

	// Test the Load instructions
	case 0: {
		Value glosym = pushSym(th, "$g");
		genAddParm(ac, self);
		genAddInstr(ac, BCINS_ABC(OpLoadReg, 1, genLocalVar(ac, self), 0)); // load self into 1
		genAddInstr(ac, BCINS_ABC(OpLoadPrim, 2, 2, 0)); // Load primitive true
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 3, genAddLit(ac, aFloat(3.14f))));  // Load literal
		genAddInstr(ac, BCINS_ABx(OpSetGlobal, 3, genAddLit(ac, glosym)));
		genAddInstr(ac, BCINS_ABx(OpGetGlobal, 4, genAddLit(ac, glosym)));
		genAddInstr(ac, BCINS_ABC(OpReturn, 1, 4, 0));
		popValue(th);
			} break;

	// Test variable parameters
	case 1:
		genAddParm(ac, self);
		genVarParms(ac);
		genAddInstr(ac, BCINS_ABC(OpLoadReg, 1, genLocalVar(ac, self), 0)); // load self into 1
		genAddInstr(ac, BCINS_ABC(OpLoadVararg, 2, BCVARRET, 0)); // Load var args
		genAddInstr(ac, BCINS_ABC(OpReturn, 1, BCVARRET, 0));
		break;

	// Test call and jump instructions with a fibonacci calculation
	case 2:
		genAddParm(ac, self);
		genMaxStack(ac, 6);
		genAddInstr(ac, BCINS_AJ(OpJNNull, 0, 2));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 1, genAddLit(ac, anInt(5))));
		genAddInstr(ac, BCINS_AJ(OpJump, 0, 1));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 1, genAddLit(ac, anInt(1))));
		genAddInstr(ac, BCINS_AJ(OpJTrue, 0, 1));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 0, genAddLit(ac, anInt(10))));
		genAddInstr(ac, BCINS_ABC(OpLoadReg, 2, 1, 0));
		genAddInstr(ac, BCINS_AJ(OpJLe, 0, 10));
		genAddInstr(ac, BCINS_ABC(OpLoadReg, 3, 2, 0));
		genAddInstr(ac, BCINS_ABC(OpLoadStd, 4, 1, ss(SymPlus)));
		genAddInstr(ac, BCINS_ABC(OpLoadReg, 6, 2, 0));
		genAddInstr(ac, BCINS_ABC(OpGetCall, 4, 2, 1));
		genAddInstr(ac, BCINS_ABC(OpLoadRegs, 1, 3, 2));
		genAddInstr(ac, BCINS_ABC(OpLoadStd, 3, 0, ss(SymMinus)));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 5, genAddLit(ac, anInt(1))));
		genAddInstr(ac, BCINS_ABC(OpGetCall, 3, 2, 1));
		genAddInstr(ac, BCINS_ABC(OpLoadReg, 0, 3, 0));
		genAddInstr(ac, BCINS_AJ(OpJump, 0, -11));
		genAddInstr(ac, BCINS_ABC(OpReturn, 1, 1, 0));
		break;

	// Test tailcall with a factorial method
	case 3: {
		Value a = pushSym(th, "a");
		Value fact = pushSym(th, "fact");
		genAddParm(ac, self);
		genAddParm(ac, a);
		genMaxStack(ac, 6);
		genAddInstr(ac, BCINS_AJ(OpJGt, 0, 1));
		genAddInstr(ac, BCINS_ABC(OpReturn, 1, 1, 0));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 2, genAddLit(ac, fact)));
		genAddInstr(ac, BCINS_ABC(OpLoadStd, 3, 0, ss(SymMinus)));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 5, genAddLit(ac, anInt(1))));
		genAddInstr(ac, BCINS_ABC(OpGetCall, 3, 2, 1));
		genAddInstr(ac, BCINS_ABC(OpLoadStd, 4, 0, ss(SymMult)));
		genAddInstr(ac, BCINS_AJ(OpJFalse, 1, 2));
		genAddInstr(ac, BCINS_ABC(OpLoadReg, 6, 1, 0));
		genAddInstr(ac, BCINS_AJ(OpJump, 0, 1));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 6, genAddLit(ac, anInt(1))));
		genAddInstr(ac, BCINS_ABC(OpGetCall, 4, 2, 1));
		genAddInstr(ac, BCINS_ABC(OpTailCall, 2, 2, BCVARRET));

		tblSet(th, vmlit(TypeIntm), fact, ac->method);

		popValue(th);
		popValue(th);
			} break;

	// Test repetitive preps and calls, doing a build and for loop on a list, summing its integers
	case 4: {
		Value list = pushSym(th, "List");
		genAddParm(ac, self);
		genMaxStack(ac, 9);
		genAddInstr(ac, BCINS_ABC(OpLoadStd, 3, 0, ss(SymNew)));
		genAddInstr(ac, BCINS_ABx(OpGetGlobal, 4, genAddLit(ac, list)));
		genAddInstr(ac, BCINS_ABC(OpGetCall, 3, 1, 1));
		genAddInstr(ac, BCINS_ABC(OpRptPrep, 2, 3, ss(SymAppend)));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 4, genAddLit(ac, anInt(5))));
		genAddInstr(ac, BCINS_ABC(OpRptCall, 2, 2, 0));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 4, genAddLit(ac, anInt(7))));
		genAddInstr(ac, BCINS_ABC(OpRptCall, 2, 2, 0));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 4, genAddLit(ac, anInt(8))));
		genAddInstr(ac, BCINS_ABC(OpRptCall, 2, 2, 0));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 1, genAddLit(ac, anInt(0))));
		genAddInstr(ac, BCINS_ABC(OpForPrep, 2, 3, ss(SymNext)));
		genAddInstr(ac, BCINS_ABC(OpRptCall, 2, 2, 2));
		saveip = ac->method->size; // Save loc of jump
		genAddInstr(ac, BCINS_AJ(OpJNull, 4, BCNO_JMP)); // Will calculate to 5
		genAddInstr(ac, BCINS_ABC(OpLoadStd, 6, 1, ss(SymPlus)));
		genAddInstr(ac, BCINS_ABC(OpLoadReg, 8, 5, 0));
		genAddInstr(ac, BCINS_ABC(OpGetCall, 6, 2, 1));
		genAddInstr(ac, BCINS_ABC(OpLoadReg, 1, 6, 0));
		genAddInstr(ac, BCINS_AJ(OpJump, 0, -7));
		genSetJumpList(ac, saveip, ac->method->size); // Correct the jump to go to here
		genAddInstr(ac, BCINS_ABC(OpReturn, 1, 1, 0));
		popValue(th);
			} break;

	// Test File and URL stuff
	case 5: {
		Value fil = pushSym(th, "Resource");
		Value get = pushSym(th, "()");
		Value testacn = pushString(th, vmlit(TypeTextm), "file://./test.acn");
		genAddParm(ac, self);
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 3, genAddLit(ac, get)));
		genAddInstr(ac, BCINS_ABx(OpGetGlobal, 4, genAddLit(ac, fil)));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 5, genAddLit(ac, testacn)));
		genAddInstr(ac, BCINS_ABC(OpGetCall, 3, 2, 1));
		genAddInstr(ac, BCINS_ABC(OpReturn, 1, 1, 0));
		popValue(th);
		popValue(th);
		popValue(th);
			} break;

	default:
		break;
	}
	popValue(th);
	popValue(th);
	popValue(th);
	popValue(th);
	return ac->method;
}

/* Method to compile and run an Acorn resource.
   Pass it a string containing the program source and a symbol for the baseurl.
   It returns the value returned by running the program's compiled method. */
int acn_new(Value th) {
	// Retrieve pgmsrc and baseurl from parameters
	Value pgmsrc, baseurl;
	if (getTop(th)<2 || !isStr(pgmsrc = getLocal(th,1)))
	{
		pushValue(th, aNull);
		return 1;
	}
	if (getTop(th)<3 || !isSym(baseurl = getLocal(th,2)))
		baseurl = aNull;

	// Parse and Generate
	CompInfo* comp = (CompInfo*) pushCompiler(th, pgmsrc, baseurl);
	parseProgram(comp);
	Value aststr = pushSerialized(th, comp->ast);
	vm_log("Resulting AST is: %s", toStr(aststr));
	popValue(th);
	popValue(th);

	pushValue(th, aNull);
	return 1;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

