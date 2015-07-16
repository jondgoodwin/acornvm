/** Implements c-functions and c-methods.
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
 */

#include "avmlib.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/* Build a new c-function value, pointing to a function written in C */
Value aCFunc(Value th, AcFuncp func, const char* name, const char* src) {
	// Put on stack to keep safe from GC
	stkPush(th, newStr(th, name));
	stkPush(th, newStr(th, src));

	mem_gccheck(th);	// Incremental GC before memory allocation events
	CFuncInfo *fn = (CFuncInfo*) mem_new(th, FuncEnc, sizeof(CFuncInfo), NULL, 0);
	funcFlags(fn) = FUNC_FLG_C;
	fn->funcp = func;
	fn->name = stkGet(th, stkFromTop(th,1));
	fn->source = stkGet(th, stkFromTop(th,0));
	stkSetSize(th, -2); // Pop values off stack

	return fn;
}

/* Build a new c-method value, pointing to a function written in C */
Value aCMethod(Value th, AcFuncp func, const char* name, const char* src) {
	// Put on stack to keep safe from GC
	stkPush(th, newStr(th, name));
	stkPush(th, newStr(th, src));

	mem_gccheck(th);	// Incremental GC before memory allocation events
	CFuncInfo *fn = (CFuncInfo*) mem_new(th, FuncEnc, sizeof(CFuncInfo), NULL, 0);
	funcFlags(fn) = FUNC_FLG_C | FUNC_FLG_METHOD;
	fn->funcp = func;
	fn->name = stkGet(th, stkFromTop(th,1));
	fn->source = stkGet(th, stkFromTop(th,0));
	stkSetSize(th, -2); // Pop values off stack

	return fn;
}

/* Build a new function value, written in bytecode. Negative nparms allows variable number of parameters. */
Value aBFunc(Value th, int nparms, Value name, Value src) {
	mem_gccheck(th);	// Incremental GC before memory allocation events
	BFuncInfo *fn = (BFuncInfo*) mem_new(th, FuncEnc, sizeof(BFuncInfo), NULL, 0);
	funcFlags(fn) = (nparms<0)? FUNC_FLG_VARPARM : 0;
	funcNParms(fn) = (nparms<0)? -nparms : nparms;
	fn->name = name;
	fn->source = src;
	fn->code = NULL;
	fn->maxstacksize = 20;
	return fn;
}

/* Build a new method value, written in bytecode. Negative nparms allows variable number of parameters. */
Value aBMethod(Value th, int nparms, Value name, Value src) {
	mem_gccheck(th);	// Incremental GC before memory allocation events
	BFuncInfo *fn = (BFuncInfo*) mem_new(th, FuncEnc, sizeof(BFuncInfo), NULL, 0);
	funcFlags(fn) = FUNC_FLG_METHOD | ((nparms<0)? FUNC_FLG_VARPARM : 0);
	funcNParms(fn) = (nparms<0)? -nparms : nparms;
	fn->name = name;
	fn->source = src;
	fn->code = NULL;
	fn->maxstacksize = 20;
	return fn;
}

/* Execute byte-code program pointed at by thread's current call frame */
void bfnRun(Value th) {

	// Fake code, for testing purposes
	stkSetSize(th, 2);
	thrReturn(th, 1); // Return last parameter
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif


