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

/** Compile and run an Acorn resource */
int acn_new(Value th) {
	pushGlobal(th, "Acorn");
	pushValue(th, newPart(th, getFromTop(th, 0)));
	return 1;
}

/** Initialize all core types */
void acn_init(Value th) {
	// Initialize program compile state
	Acorn* ac = &vm(th)->acornProgram;
	ac->prev = ac->next = NULL;
	ac->th = th;

	// Create Acorn type, properties and methods
	// Value typ = pushType(th);
	// partAddProp(th, typ, pushSym(th, "run"), acn_new);
	// popGlobal(th, "Acorn");
}

/** Generate bytecode test programs */
Value genTestPgm(Value th, int pgm) {
	int saveip = 0;
	Acorn* ac = &vm(th)->acornProgram;
	genNew(ac, 0, aNull, aNull);
	pushValue(th, ac->func);
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
		genAddInstr(ac, BCINS_ABC(OpLoadStd, 4, 1, SymPlus));
		genAddInstr(ac, BCINS_ABC(OpLoadReg, 6, 2, 0));
		genAddInstr(ac, BCINS_ABC(OpCall, 4, 2, 1));
		genAddInstr(ac, BCINS_ABC(OpLoadRegs, 1, 3, 2));
		genAddInstr(ac, BCINS_ABC(OpLoadStd, 3, 0, SymMinus));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 5, genAddLit(ac, anInt(1))));
		genAddInstr(ac, BCINS_ABC(OpCall, 3, 2, 1));
		genAddInstr(ac, BCINS_ABC(OpLoadReg, 0, 3, 0));
		genAddInstr(ac, BCINS_AJ(OpJump, 0, -11));
		genAddInstr(ac, BCINS_ABC(OpReturn, 1, 1, 0));
		break;

	// Test tailcall with a factorial method
	case 3: {
		Value a = pushSym(th, "a");
		Value fact = pushSym(th, "fact");
		Value integer = pushSym(th, "Integer");
		genAddParm(ac, self);
		genAddParm(ac, a);
		genMaxStack(ac, 6);
		genAddInstr(ac, BCINS_AJ(OpJGt, 0, 1));
		genAddInstr(ac, BCINS_ABC(OpReturn, 1, 1, 0));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 2, genAddLit(ac, fact)));
		genAddInstr(ac, BCINS_ABC(OpLoadStd, 3, 0, SymMinus));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 5, genAddLit(ac, anInt(1))));
		genAddInstr(ac, BCINS_ABC(OpCall, 3, 2, 1));
		genAddInstr(ac, BCINS_ABC(OpLoadStd, 4, 0, SymMult));
		genAddInstr(ac, BCINS_AJ(OpJFalse, 1, 2));
		genAddInstr(ac, BCINS_ABC(OpLoadReg, 6, 1, 0));
		genAddInstr(ac, BCINS_AJ(OpJump, 0, 1));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 6, genAddLit(ac, anInt(1))));
		genAddInstr(ac, BCINS_ABC(OpCall, 4, 2, 1));
		genAddInstr(ac, BCINS_ABC(OpTailCall, 2, 2, BCVARRET));

		partAddMethod(th, gloGet(th, integer), fact, ac->func);
		popValue(th);
		popValue(th);
		popValue(th);
			} break;

	// Test repetitive preps and calls, doing a build and for loop on a list, summing its integers
	case 4: {
		Value list = pushSym(th, "List");
		genAddParm(ac, self);
		genMaxStack(ac, 9);
		genAddInstr(ac, BCINS_ABC(OpLoadStd, 3, 0, SymNew));
		genAddInstr(ac, BCINS_ABx(OpGetGlobal, 4, genAddLit(ac, list)));
		genAddInstr(ac, BCINS_ABC(OpCall, 3, 1, 1));
		genAddInstr(ac, BCINS_ABC(OpRptPrep, 2, 3, SymAppend));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 4, genAddLit(ac, anInt(5))));
		genAddInstr(ac, BCINS_ABC(OpRptCall, 2, 2, 0));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 4, genAddLit(ac, anInt(7))));
		genAddInstr(ac, BCINS_ABC(OpRptCall, 2, 2, 0));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 4, genAddLit(ac, anInt(8))));
		genAddInstr(ac, BCINS_ABC(OpRptCall, 2, 2, 0));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 1, genAddLit(ac, anInt(0))));
		genAddInstr(ac, BCINS_ABC(OpForPrep, 2, 3, SymNext));
		genAddInstr(ac, BCINS_ABC(OpRptCall, 2, 2, 2));
		saveip = ac->ip; // Save loc of jump
		genAddInstr(ac, BCINS_AJ(OpJNull, 4, BCNO_JMP)); // Will calculate to 5
		genAddInstr(ac, BCINS_ABC(OpLoadStd, 6, 1, SymPlus));
		genAddInstr(ac, BCINS_ABC(OpLoadReg, 8, 5, 0));
		genAddInstr(ac, BCINS_ABC(OpCall, 6, 2, 1));
		genAddInstr(ac, BCINS_ABC(OpLoadReg, 1, 6, 0));
		genAddInstr(ac, BCINS_AJ(OpJump, 0, -7));
		genSetJumpList(ac, saveip, ac->ip); // Correct the jump to go to here
		genAddInstr(ac, BCINS_ABC(OpReturn, 1, 1, 0));
		popValue(th);
			} break;

	// Test File and URL stuff
	case 5: {
		Value str = pushSym(th, "$stream");
		Value fil = pushSym(th, "File");
		Value get = pushSym(th, "get");
		Value testacn = pushValue(th, newStr(th, "test.acn"));
		genAddParm(ac, self);
		genAddInstr(ac, BCINS_ABC(OpLoadStd, 1, 2, SymAppend));
		genAddInstr(ac, BCINS_ABx(OpGetGlobal, 2, genAddLit(ac, str)));
		genAddInstr(ac, BCINS_ABC(OpLoadStd, 3, 4, SymParGet));
		genAddInstr(ac, BCINS_ABC(OpLoadStd, 4, 5, SymParGet));
		genAddInstr(ac, BCINS_ABx(OpGetGlobal, 5, genAddLit(ac, fil)));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 6, genAddLit(ac, get)));
		genAddInstr(ac, BCINS_ABC(OpCall, 4, 2, 1));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 5, genAddLit(ac, testacn)));
		genAddInstr(ac, BCINS_ABC(OpCall, 3, 2, 1));
		genAddInstr(ac, BCINS_ABC(OpCall, 1, 2, 1));
		genAddInstr(ac, BCINS_ABC(OpReturn, 1, 1, 0));
		popValue(th);
		popValue(th);
		popValue(th);
		popValue(th);
			} break;

	default:
		break;
	}
	popValue(th);
	popValue(th);
	return ac->func;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

