/** Initializer for all the core types
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

/** Add two integers */
int atyp_int_add(Value th) {
	if (!isInt(stkGet(th, 1)))
		stkSet(th, 1, anInt(0));
	stkPush(th, anInt(toAint(stkGet(th,0)) + toAint(stkGet(th,1))));
	return 1;
}

/** Initialize the Integer type */
Value atyp_int_init(Value th) {
	Value typ = newType(th, "Integer");
	addCMethod(th, typ, "+", atyp_int_add, "Integer::+");
	return typ;
}

/** Initialize the Type type, used to create other types */
Value atyp_type_init(Value th) {
	Value typ = newType(th, "Type");
	setType(th, typ, typ); // Needed because default encoding not in place until this finishes
	return typ;
}

/** Initialize all core types */
void atyp_init(Value th) {
	Value *def = &vm(th)->defEncTypes[0];

	def[TypeEnc] = atyp_type_init(th); // This MUST be first in the list.
	def[IntEnc] = atyp_int_init(th);
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

