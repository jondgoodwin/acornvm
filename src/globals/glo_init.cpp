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

Value typ_file_init(Value th);
Value typ_url_init(Value th);

void env_stream_init(Value th);

/** Add two integers */
int typ_int_plus(Value th) {
	if (!isInt(stkGet(th, 1)))
		stkSet(th, 1, anInt(0));
	stkPush(th, anInt(toAint(stkGet(th,0)) + toAint(stkGet(th,1))));
	return 1;
}

/** Subtract two integers */
int typ_int_minus(Value th) {
	if (!isInt(stkGet(th, 1)))
		stkSet(th, 1, anInt(0));
	stkPush(th, anInt(toAint(stkGet(th,0)) - toAint(stkGet(th,1))));
	return 1;
}

/** Multiply two integers */
int typ_int_mult(Value th) {
	if (!isInt(stkGet(th, 1)))
		stkSet(th, 1, anInt(0));
	stkPush(th, anInt(toAint(stkGet(th,0)) * toAint(stkGet(th,1))));
	return 1;
}

/** Initialize the Integer type */
Value typ_int_init(Value th) {
	Value typ = newType(th, "Integer");
	addCMethod(th, typ, "+", typ_int_plus, "Integer.+");
	addCMethod(th, typ, "-", typ_int_minus, "Integer.-");
	addCMethod(th, typ, "*", typ_int_mult, "Integer.*");
	return typ;
}

/** Create a new List */
int typ_list_new(Value th) {
	stkPush(th, newArr(th, 0));
	return 1;
}

/** Add to a List */
int typ_list_add(Value th) {
	arrAdd(th, stkGet(th,0), stkGet(th,1));
	return 0;
}

/** Get next item from a List */
int typ_list_next(Value th) {
	Value arr = stkGet(th,0);
	AintIdx sz = arr_size(arr);
	AintIdx pos = (stkGet(th,1)==aNull)? 0 : toAint(stkGet(th,1));
	stkPush(th, pos>=sz? aNull : arrGet(th, arr, pos));
	stkSet(th, 1, pos>=sz? aNull : anInt((Aint)pos+1));
	return 2;
}

/** Initialize the List type */
Value typ_list_init(Value th) {
	Value typ = newType(th, "List");
	addCMethod(th, typ, "New", typ_list_new, "List.New");
	addCMethod(th, typ, "<<", typ_list_add, "List.<<");
	addCMethod(th, typ, "next", typ_list_next, "List.next");
	return typ;
}

/** Call the method, passing its parameters */
int typ_meth_get(Value th) {
	funcCall(th, stkSize(th)-1, BCVARRET);
	return stkSize(th);
}

/** Initialize the Type type, used to create other types */
Value typ_meth_init(Value th) {
	Value typ = newType(th, "Method");
	addCMethod(th, typ, "()", typ_meth_get, "Method.()");
	return typ;
}

/** Lookup a value from type's named property */
int typ_type_get(Value th) {
	stkPush(th, stkSize(th)>=2? tblGet(th, part_props(stkGet(th,0)), stkGet(th,1)) : aNull);
	return 1;
}

/** Initialize the Type type, used to create other types */
Value typ_type_init(Value th) {
	Value typ = newType(th, "Type");
	setType(th, typ, typ); // Needed because default encoding not in place until this finishes
	addCMethod(th, typ, "()", typ_type_get, "Type.()");
	return typ;
}

/** Initialize all core types */
void glo_init(Value th) {
	Value *def = &vm(th)->defEncTypes[0];

	def[TypeEnc] = typ_type_init(th); // This MUST be first in the list.
	def[FuncEnc] = typ_meth_init(th);
	def[IntEnc] = typ_int_init(th);
	def[ArrEnc] = typ_list_init(th);
	def[AllEnc] = newType(th, "All");

	typ_file_init(th);

	env_stream_init(th);
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

