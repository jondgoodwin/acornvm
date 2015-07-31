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
	stkPush(th, newPart(th, gloGet(th, aSym(th, "Acorn"))));
	return 1;
}

/** Initialize all core types */
void acn_init(Value th) {
	// Initialize program compile state
	Acorn* ac = &vm(th)->acornProgram;
	ac->prev = ac->next = NULL;
	ac->th = th;

	// Create Acorn type, properties and methods
	// Value typ = newType(th, "Acorn");
	// partAddProp(th, typ, aSym(th, "run"), acn_new);
}

Value genTestPgm(Value th, int pgm) {
	Acorn* ac = &vm(th)->acornProgram;
	genNew(ac, 0, aNull, aNull);
	switch (pgm) {
	case 0:
		genAddParm(ac, aSym(th, "self"));
		genAddInstr(ac, BCINS_ABC(OpLoadReg, 1, genLocalVar(ac, aSym(th, "self")), 0)); // load self into 1
		genAddInstr(ac, BCINS_ABC(OpLoadPrim, 2, 2, 0)); // Load primitive true
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 3, genAddLit(ac, aFloat(3.14f))));  // Load literal
		genAddInstr(ac, BCINS_ABx(OpSetGlobal, 3, genAddLit(ac, aSym(th, "$g"))));
		genAddInstr(ac, BCINS_ABx(OpGetGlobal, 4, genAddLit(ac, aSym(th, "$g"))));
		genAddInstr(ac, BCINS_ABC(OpReturn, 1, 4, 0));
		break;
	case 1:
		genAddParm(ac, aSym(th, "self"));
		genVarParms(ac);
		genAddInstr(ac, BCINS_ABC(OpLoadReg, 1, genLocalVar(ac, aSym(th, "self")), 0)); // load self into 1
		genAddInstr(ac, BCINS_ABC(OpLoadVararg, 2, BCVARRET, 0)); // Load var args
		genAddInstr(ac, BCINS_ABC(OpReturn, 1, BCVARRET, 0));
		break;
	case 2:
		genAddParm(ac, aSym(th, "self"));
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
	case 3:
		genAddParm(ac, aSym(th, "self"));
		genAddParm(ac, aSym(th, "a"));
		genMaxStack(ac, 6);
		genAddInstr(ac, BCINS_AJ(OpJGt, 0, 1));
		genAddInstr(ac, BCINS_ABC(OpReturn, 1, 1, 0));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 2, genAddLit(ac, aSym(th, "fact"))));
		genAddInstr(ac, BCINS_ABC(OpLoadStd, 3, 0, SymMinus));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 5, genAddLit(ac, anInt(1))));
		genAddInstr(ac, BCINS_ABC(OpCall, 3, 2, 1));
		genAddInstr(ac, BCINS_ABC(OpLoadStd, 4, 0, SymMult));
		genAddInstr(ac, BCINS_AJ(OpJFalse, 1, 2));
		genAddInstr(ac, BCINS_ABC(OpLoadReg, 6, 1, 0));
		genAddInstr(ac, BCINS_AJ(OpJump, 0, 1));
		genAddInstr(ac, BCINS_ABx(OpLoadLit, 6, genAddLit(ac, anInt(1))));
		genAddInstr(ac, BCINS_ABC(OpCall, 4, 2, 1));
		genAddInstr(ac, BCINS_ABC(OpTailcall, 2, 2, BCVARRET));

		partAddMethod(th, gloGet(th, aSym(th,"Integer")), aSym(th, "fact"), ac->func);

		break;
	default:
		break;
	}
	return ac->func;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

