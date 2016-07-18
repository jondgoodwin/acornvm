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
	methodCall(th, getTop(th)-1, BCVARRET); // Pass all parameters
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
			popMember(th, 1, "()");
			pushCMethod(th, typ_method_arity);
			popMember(th, 1, "arity");
			pushCMethod(th, typ_method_varargs);
			popMember(th, 1, "varargs?");
		popMember(th, 0, "newtype");
		pushCMethod(th, acn_new);
		popMember(th, 0, "new");
	popGloVar(th, "Method");

	// Register this type as Resource's 'acn' extension
	pushGloVar(th, "Resource");
		pushMember(th, getTop(th) - 1, "_extensions");
			pushValue(th, vmlit(TypeMethc));
			popMember(th, getTop(th) - 2, "acn");
		popValue(th);
	popValue(th);
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif