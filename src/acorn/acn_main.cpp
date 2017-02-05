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
	comp->forcelocal = false;

	return *dest;
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

// Found in typ_resource.cpp
AuintIdx resource_resolve(Value th, Value meth, Value *resource);

/* Try to resolve all static Resources (externs) in 'self's method and its extern methods.
	Will start the loading of any static resources not already loading.
	null is returned if link is successful, otherwise it returns number of unresolved Resources */
int acn_linker(Value th) {
	BMethodInfo* meth = (BMethodInfo*) getLocal(th, 0);

	// Return null when there are no unresolved externs
	if (meth->nbrexterns == 0)
		return 0;

	AuintIdx counter = 0;
	Value *externp = meth->lits;
	for (Auint i=0; i<meth->nbrexterns; i++) {
		counter += resource_resolve(th, meth, externp);
		externp++;
	}

	// Return null if all externs resolved.
	if (counter==0) {
		meth->nbrexterns = 0; // Mark that no more static Resources externs are to be found
		return 0;
	}
	else {
		pushValue(th, anInt(counter)); // Return count of unresolved static resources
		return 1;
	}

	return 1;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
