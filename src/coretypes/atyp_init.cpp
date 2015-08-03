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
int atyp_int_plus(Value th) {
	if (!isInt(stkGet(th, 1)))
		stkSet(th, 1, anInt(0));
	stkPush(th, anInt(toAint(stkGet(th,0)) + toAint(stkGet(th,1))));
	return 1;
}

/** Subtract two integers */
int atyp_int_minus(Value th) {
	if (!isInt(stkGet(th, 1)))
		stkSet(th, 1, anInt(0));
	stkPush(th, anInt(toAint(stkGet(th,0)) - toAint(stkGet(th,1))));
	return 1;
}

/** Multiply two integers */
int atyp_int_mult(Value th) {
	if (!isInt(stkGet(th, 1)))
		stkSet(th, 1, anInt(0));
	stkPush(th, anInt(toAint(stkGet(th,0)) * toAint(stkGet(th,1))));
	return 1;
}

/** Initialize the Integer type */
Value atyp_int_init(Value th) {
	Value typ = newType(th, "Integer");
	addCMethod(th, typ, "+", atyp_int_plus, "Integer::+");
	addCMethod(th, typ, "-", atyp_int_minus, "Integer::-");
	addCMethod(th, typ, "*", atyp_int_mult, "Integer::*");
	return typ;
}

/** Create a new List */
int atyp_list_new(Value th) {
	stkPush(th, newArr(th, 0));
	return 1;
}

/** Add to a List */
int atyp_list_add(Value th) {
	arrAdd(th, stkGet(th,0), stkGet(th,1));
	return 0;
}

/** Get next item from a List */
int atyp_list_next(Value th) {
	Value arr = stkGet(th,0);
	AintIdx sz = arr_size(arr);
	AintIdx pos = (stkGet(th,1)==aNull)? 0 : toAint(stkGet(th,1));
	stkPush(th, pos>=sz? aNull : arrGet(th, arr, pos));
	stkSet(th, 1, pos>=sz? aNull : anInt((Aint)pos+1));
	return 2;
}

/** Initialize the List type */
Value atyp_list_init(Value th) {
	Value typ = newType(th, "List");
	addCMethod(th, typ, "new", atyp_list_new, "List::new");
	addCMethod(th, typ, "+=", atyp_list_add, "List::+=");
	addCMethod(th, typ, "next", atyp_list_next, "List::next");
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
	def[ArrEnc] = atyp_list_init(th);
	def[AllEnc] = newType(th, "All");
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

