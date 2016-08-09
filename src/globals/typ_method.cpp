/** Defines the properties for the Method type.
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
int typ_method_get(Value th) {
	pushValue(th, aNull);
	insertLocal(th, 1); // Insert null as self
	getCall(th, getTop(th)-1, BCVARRET); // Pass all parameters
	return getTop(th); // return all return values
}

/** 'arity': Return the number of fixed parameters the method expects */
int typ_method_arity(Value th) {
	pushValue(th, anInt(methodNParms(getLocal(th, 0))));
	return 1;
}

/** 'varargs?': Return true if method accepts a variable number of parameters. */
int typ_method_varargs(Value th) {
	pushValue(th, methodFlags(getLocal(th, 0)) & METHOD_FLG_VARPARM? aTrue : aFalse);
	return 1;
}

/** Initialize the Method type and its properties in Global */
void typ_method_init(Value th) {
	vmlit(TypeMethc) = pushType(th, vmlit(TypeType), 2);
		vmlit(TypeMethm) = pushMixin(th, vmlit(TypeType), aNull, 3);
			pushCMethod(th, typ_method_get);
			popProperty(th, 1, "()");
			pushCMethod(th, typ_method_arity);
			popProperty(th, 1, "arity");
			pushCMethod(th, typ_method_varargs);
			popProperty(th, 1, "varargs?");
		popProperty(th, 0, "newtype");
		pushCMethod(th, acn_newmethod);
		popProperty(th, 0, "new");
	popGloVar(th, "Method");

	Value AcornPgm = pushType(th, vmlit(TypeType), 2);
		pushCMethod(th, acn_newprogram);
		popProperty(th, 0, "new");
	popGloVar(th, "AcornProgram");

	// Register this type as Resource's 'acn' extension
	pushGloVar(th, "Resource");
		pushProperty(th, getTop(th) - 1, "extensions");
			pushValue(th, AcornPgm);
			popTblSet(th, getTop(th) - 2, "acn");
		popValue(th);
	popValue(th);
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif