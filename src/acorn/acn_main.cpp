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

	// Create an compiler context (this block of code can be gc-locked as atomic)
	comp = (CompInfo *) mem_new(th, CompEnc, sizeof(CompInfo));
	*dest = (Value) comp;
	comp->th = th;
	comp->lex = NULL;
	comp->ast = (Value) NULL;
	comp->method = NULL;
	comp->prevcomp = aNull;

	if (isStr(src)) {
		newLex(th, (Value *) &comp->lex, src, url);
		mem_markChk(th, comp, comp->lex);

		// Prime the pump by getting the first token
		lexGetNextToken(comp->lex);
	} else {
		comp->lex = ((CompInfo*)src)->lex;
		mem_markChk(th, comp, comp->lex);
		comp->prevcomp = src;
	}
	newArr(th, &comp->ast, aNull, 2);
	mem_markChk(th, comp, comp->ast);
	newBMethod(th, (Value *)&comp->method);
	mem_markChk(th, comp, comp->method);

	comp->nextreg = 0;
	comp->whileBegIp = -1;

	return *dest;
}

//! Get index for a standard symbol identified using its VM literal name
#define ss(lit) (toAint(tblGet(th, vm(th)->stdidx, vmlit(lit))))

/** Generate bytecode test programs */
Value genTestPgm(Value th, int pgm) {
	int saveip = 0;
	Value src = pushString(th, aNull, "");
	CompInfo* comp = (CompInfo*) pushCompiler(th, src, aNull);
	pushValue(th, comp->method);
	Value self = pushSym(th, "self");
	switch (pgm) {

	// Test the Load instructions
	case 0: {
		Value glosym = pushSym(th, "$g");
		genAddParm(comp, self);
		genAddInstr(comp, BCINS_ABC(OpLoadReg, 1, genLocalVar(comp, self), 0)); // load self into 1
		genAddInstr(comp, BCINS_ABC(OpLoadPrim, 2, 2, 0)); // Load primitive true
		genAddInstr(comp, BCINS_ABx(OpLoadLit, 3, genAddLit(comp, aFloat(3.14f))));  // Load literal
		genAddInstr(comp, BCINS_ABx(OpSetGlobal, 3, genAddLit(comp, glosym)));
		genAddInstr(comp, BCINS_ABx(OpGetGlobal, 4, genAddLit(comp, glosym)));
		genAddInstr(comp, BCINS_ABC(OpReturn, 1, 4, 0));
		popValue(th);
			} break;

	// Test variable parameters
	case 1:
		genAddParm(comp, self);
		genVarParms(comp);
		genAddInstr(comp, BCINS_ABC(OpLoadReg, 1, genLocalVar(comp, self), 0)); // load self into 1
		genAddInstr(comp, BCINS_ABC(OpLoadVararg, 2, BCVARRET, 0)); // Load var args
		genAddInstr(comp, BCINS_ABC(OpReturn, 1, BCVARRET, 0));
		break;

	// Test call and jump instructions with a fibonacci calculation
	case 2:
		genAddParm(comp, self);
		genMaxStack(comp, 6);
		genAddInstr(comp, BCINS_AJ(OpJTrue, 0, 2));
		genAddInstr(comp, BCINS_ABx(OpLoadLit, 1, genAddLit(comp, anInt(5))));
		genAddInstr(comp, BCINS_AJ(OpJump, 0, 1));
		genAddInstr(comp, BCINS_ABx(OpLoadLit, 1, genAddLit(comp, anInt(1))));
		genAddInstr(comp, BCINS_AJ(OpJTrue, 0, 1));
		genAddInstr(comp, BCINS_ABx(OpLoadLit, 0, genAddLit(comp, anInt(10))));
		genAddInstr(comp, BCINS_ABC(OpLoadReg, 2, 1, 0));
		genAddInstr(comp, BCINS_AJ(OpJLe, 0, 10));
		genAddInstr(comp, BCINS_ABC(OpLoadReg, 3, 2, 0));
		genAddInstr(comp, BCINS_ABC(OpLoadStd, 4, 1, ss(SymPlus)));
		genAddInstr(comp, BCINS_ABC(OpLoadReg, 6, 2, 0));
		genAddInstr(comp, BCINS_ABC(OpGetCall, 4, 2, 1));
		genAddInstr(comp, BCINS_ABC(OpLoadRegs, 1, 3, 2));
		genAddInstr(comp, BCINS_ABC(OpLoadStd, 3, 0, ss(SymMinus)));
		genAddInstr(comp, BCINS_ABx(OpLoadLit, 5, genAddLit(comp, anInt(1))));
		genAddInstr(comp, BCINS_ABC(OpGetCall, 3, 2, 1));
		genAddInstr(comp, BCINS_ABC(OpLoadReg, 0, 3, 0));
		genAddInstr(comp, BCINS_AJ(OpJump, 0, -11));
		genAddInstr(comp, BCINS_ABC(OpReturn, 1, 1, 0));
		break;

	// Test tailcall with a factorial method
	case 3: {
		Value a = pushSym(th, "a");
		Value fact = pushSym(th, "fact");
		genAddParm(comp, self);
		genAddParm(comp, a);
		genMaxStack(comp, 6);
		genAddInstr(comp, BCINS_AJ(OpJGt, 0, 1));
		genAddInstr(comp, BCINS_ABC(OpReturn, 1, 1, 0));
		genAddInstr(comp, BCINS_ABx(OpLoadLit, 2, genAddLit(comp, fact)));
		genAddInstr(comp, BCINS_ABC(OpLoadStd, 3, 0, ss(SymMinus)));
		genAddInstr(comp, BCINS_ABx(OpLoadLit, 5, genAddLit(comp, anInt(1))));
		genAddInstr(comp, BCINS_ABC(OpGetCall, 3, 2, 1));
		genAddInstr(comp, BCINS_ABC(OpLoadStd, 4, 0, ss(SymMult)));
		genAddInstr(comp, BCINS_AJ(OpJFalse, 1, 2));
		genAddInstr(comp, BCINS_ABC(OpLoadReg, 6, 1, 0));
		genAddInstr(comp, BCINS_AJ(OpJump, 0, 1));
		genAddInstr(comp, BCINS_ABx(OpLoadLit, 6, genAddLit(comp, anInt(1))));
		genAddInstr(comp, BCINS_ABC(OpGetCall, 4, 2, 1));
		genAddInstr(comp, BCINS_ABC(OpTailCall, 2, 2, BCVARRET));

		tblSet(th, vmlit(TypeIntm), fact, comp->method);

		popValue(th);
		popValue(th);
			} break;

	// Test repetitive preps and calls, doing a build and for loop on a list, summing its integers
	case 4: {
		Value list = pushSym(th, "List");
		genAddParm(comp, self);
		genMaxStack(comp, 9);
		genAddInstr(comp, BCINS_ABC(OpLoadStd, 3, 0, ss(SymNew)));
		genAddInstr(comp, BCINS_ABx(OpGetGlobal, 4, genAddLit(comp, list)));
		genAddInstr(comp, BCINS_ABC(OpGetCall, 3, 1, 1));
		genAddInstr(comp, BCINS_ABC(OpRptPrep, 2, 3, ss(SymAppend)));
		genAddInstr(comp, BCINS_ABx(OpLoadLit, 4, genAddLit(comp, anInt(5))));
		genAddInstr(comp, BCINS_ABC(OpRptCall, 2, 2, 0));
		genAddInstr(comp, BCINS_ABx(OpLoadLit, 4, genAddLit(comp, anInt(7))));
		genAddInstr(comp, BCINS_ABC(OpRptCall, 2, 2, 0));
		genAddInstr(comp, BCINS_ABx(OpLoadLit, 4, genAddLit(comp, anInt(8))));
		genAddInstr(comp, BCINS_ABC(OpRptCall, 2, 2, 0));
		genAddInstr(comp, BCINS_ABx(OpLoadLit, 1, genAddLit(comp, anInt(0))));
		genAddInstr(comp, BCINS_ABC(OpForPrep, 2, 3, ss(SymNext)));
		genAddInstr(comp, BCINS_ABC(OpRptCall, 2, 2, 2));
		saveip = comp->method->size; // Save loc of jump
		genAddInstr(comp, BCINS_AJ(OpJNull, 4, BCNO_JMP)); // Will calculate to 5
		genAddInstr(comp, BCINS_ABC(OpLoadStd, 6, 1, ss(SymPlus)));
		genAddInstr(comp, BCINS_ABC(OpLoadReg, 8, 5, 0));
		genAddInstr(comp, BCINS_ABC(OpGetCall, 6, 2, 1));
		genAddInstr(comp, BCINS_ABC(OpLoadReg, 1, 6, 0));
		genAddInstr(comp, BCINS_AJ(OpJump, 0, -7));
		genSetJumpList(comp, saveip, comp->method->size); // Correct the jump to go to here
		genAddInstr(comp, BCINS_ABC(OpReturn, 1, 1, 0));
		popValue(th);
			} break;

	// Test File and URL stuff
	case 5: {
		Value res = pushSym(th, "Resource");
		Value load = pushSym(th, "Load");
		Value testacn = pushString(th, vmlit(TypeTextm), "file://./test.acn");
		genAddParm(comp, self);
		genAddInstr(comp, BCINS_ABx(OpLoadLit, 3, genAddLit(comp, load)));
		genAddInstr(comp, BCINS_ABx(OpLoadLit, 4, genAddLit(comp, vmlit(SymNew))));
		genAddInstr(comp, BCINS_ABx(OpGetGlobal, 5, genAddLit(comp, res)));
		genAddInstr(comp, BCINS_ABx(OpLoadLit, 6, genAddLit(comp, testacn)));
		genAddInstr(comp, BCINS_ABC(OpGetCall, 4, 2, 1));
		genAddInstr(comp, BCINS_ABC(OpGetCall, 3, 1, 1));
		genAddInstr(comp, BCINS_ABC(OpReturn, 1, 1, 0));
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
	return comp->method;
}

/* Method to compile an Acorn method.
   Pass it a string containing the program source and a symbol or null for the baseurl.
   It returns the compiled byte-code method. */
int acn_newmethod(Value th) {
	// Retrieve pgmsrc and baseurl from parameters
	Value pgmsrc, baseurl;
	if (getTop(th)<2 || !(isStr(pgmsrc = getLocal(th,1)) || (isPtr(pgmsrc) && isEnc(pgmsrc, CompEnc))))
	{
		pushValue(th, aNull);
		return 1;
	}
	if (getTop(th)<3 || !isSym(baseurl = getLocal(th,2)))
		baseurl = aNull;

	// Create compiler context, then parse source to AST
	CompInfo* comp = (CompInfo*) pushCompiler(th, pgmsrc, baseurl);
	parseProgram(comp);
#ifdef COMPILERLOG
	Value aststr = pushSerialized(th, comp->ast);
	vmLog("Resulting AST is: %s", toStr(aststr));
	popValue(th);
#endif

	// Generate method instructions from AST
	genBMethod(comp);
#ifdef COMPILERLOG
	Value bmethod = pushSerialized(th, comp->method);
	vmLog("Resulting bytecode is: %s", toStr(bmethod));
	popValue(th);
#endif

	// Return generated method
	pushValue(th, comp->method);
	return 1;
}

/* Method to compile and run an Acorn program.
   Pass it a string containing the program source and a symbol or null for the baseurl.
   Any additional parameters are passed to the compiled method when run. */
int acn_newprogram(Value th) {
	// Validate pgmsrc and baseurl parameters
	if (getTop(th)<2 || !isStr(getLocal(th,1)))
	{
		pushValue(th, aNull);
		return 1;
	}
	if (getTop(th)<3)
		pushValue(th, aNull);
	int i = getTop(th);

	// Compile program as a method
	pushValue(th, vmlit(SymNew));
	pushGloVar(th, "Method");
	pushLocal(th, 1);
	pushLocal(th, 2);
	getCall(th, 3, 1);

	// Call the compiled method, with self=null. 
	// Use as parms, the ones passed to us.
	// Return a single value
	setLocal(th, 1, getFromTop(th, 0));
	setLocal(th, 2, getFromTop(th, 0)); 
	popValue(th);
	getCall(th, getTop(th)-2, 1);

#ifdef COMPILERLOG
	Value ret = pushSerialized(th, getLocal(th, 1));
	vmLog("Value returned from running compiled program: %s", toStr(ret));
	popValue(th);
#endif
	return 1;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

