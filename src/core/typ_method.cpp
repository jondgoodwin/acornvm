/** Method type methods and properties
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

/* 'Method.new', the Acorn compiler is defined in acn_main.cpp */

/** '()': Run the method with self as null, passing all parameters */
int method_get(Value th) {
	pushValue(th, aNull);
	insertLocal(th, 1); // Insert null as self
	getCall(th, getTop(th)-1, BCVARRET); // Pass all parameters
	return getTop(th); // return all return values
}

/** 'arity': Return the number of fixed parameters the method expects */
int method_arity(Value th) {
	pushValue(th, anInt(methodNParms(getLocal(th, 0))));
	return 1;
}

/** 'varargs?': Return true if method accepts a variable number of parameters. */
int method_varargs(Value th) {
	pushValue(th, methodFlags(getLocal(th, 0)) & METHOD_FLG_VARPARM? aTrue : aFalse);
	return 1;
}

/** Initialize the Method type and its properties in Global */
void core_method_init(Value th) {
	vmlit(TypeMethc) = pushType(th, vmlit(TypeObject), 4);
		pushSym(th, "Nethod");
		popProperty(th, 0, "_name");
		vmlit(TypeMethm) = pushMixin(th, vmlit(TypeObject), aNull, 8);
			pushSym(th, "*Method");
			popProperty(th, 1, "_name");
			pushCMethod(th, method_get);
			popProperty(th, 1, "[]");
			pushCMethod(th, acn_linker);
			popProperty(th, 1, "Link");
			pushCMethod(th, method_arity);
			popProperty(th, 1, "arity");
			pushCMethod(th, method_varargs);
			popProperty(th, 1, "varargs?");
		popProperty(th, 0, "traits");
		pushCMethod(th, acn_newmethod);
		popProperty(th, 0, "New");
	popGloVar(th, "Method");

	// Register this type as Resource's 'acn' extension
	pushGloVar(th, "Resource");
		pushProperty(th, getTop(th) - 1, "extensions");
			pushValue(th, vmlit(TypeMethc));
			popTblSet(th, getTop(th) - 2, "acn");
		popValue(th);
	popValue(th);
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif