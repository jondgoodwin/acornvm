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
	if (!isInt(getLocal(th, 1)))
		setLocal(th, 1, anInt(0));
	pushValue(th, anInt(toAint(getLocal(th,0)) + toAint(getLocal(th,1))));
	return 1;
}

/** Subtract two integers */
int typ_int_minus(Value th) {
	if (!isInt(getLocal(th, 1)))
		setLocal(th, 1, anInt(0));
	pushValue(th, anInt(toAint(getLocal(th,0)) - toAint(getLocal(th,1))));
	return 1;
}

/** Multiply two integers */
int typ_int_mult(Value th) {
	if (!isInt(getLocal(th, 1)))
		setLocal(th, 1, anInt(0));
	pushValue(th, anInt(toAint(getLocal(th,0)) * toAint(getLocal(th,1))));
	return 1;
}

/** Initialize the Integer type */
Value typ_int_init(Value th) {
	Value typ = pushType(th);
	addCMethod(th, typ, "+", typ_int_plus, "Integer.+");
	addCMethod(th, typ, "-", typ_int_minus, "Integer.-");
	addCMethod(th, typ, "*", typ_int_mult, "Integer.*");
	popGlobal(th, "Integer");
	return typ;
}

/** Create a new List */
int typ_list_new(Value th) {
	pushValue(th, newArr(th, 0));
	return 1;
}

/** Add to a List */
int typ_list_add(Value th) {
	arrAdd(th, getLocal(th,0), getLocal(th,1));
	return 0;
}

/** Get next item from a List */
int typ_list_next(Value th) {
	Value arr = getLocal(th,0);
	AintIdx sz = arr_size(arr);
	AintIdx pos = (getLocal(th,1)==aNull)? 0 : toAint(getLocal(th,1));
	pushValue(th, pos>=sz? aNull : arrGet(th, arr, pos));
	setLocal(th, 1, pos>=sz? aNull : anInt((Aint)pos+1));
	return 2;
}

/** Initialize the List type */
Value typ_list_init(Value th) {
	Value typ = pushType(th);
	addCMethod(th, typ, "New", typ_list_new, "List.New");
	addCMethod(th, typ, "<<", typ_list_add, "List.<<");
	addCMethod(th, typ, "next", typ_list_next, "List.next");
	popGlobal(th, "List");
	return typ;
}

/** Call the method, passing its parameters */
int typ_meth_get(Value th) {
	funcCall(th, getTop(th)-1, BCVARRET);
	return getTop(th);
}

/** Initialize the Type type, used to create other types */
Value typ_meth_init(Value th) {
	Value typ = pushType(th);
	addCMethod(th, typ, "()", typ_meth_get, "Method.()");
	popGlobal(th, "Method");
	return typ;
}

/** Lookup a value from type's named property */
int typ_type_get(Value th) {
	pushValue(th, getTop(th)>=2? tblGet(th, part_props(getLocal(th,0)), getLocal(th,1)) : aNull);
	return 1;
}

/** Initialize the Type type, used to create other types */
Value typ_type_init(Value th) {
	Value typ = pushType(th);
	setType(th, typ, typ); // Needed because default encoding not in place until this finishes
	addCMethod(th, typ, "()", typ_type_get, "Type.()");
	popGlobal(th, "Type");
	return typ;
}

/** Initialize all core types */
void glo_init(Value th) {
	Value *def = &vm(th)->defEncTypes[0];

	def[TypeEnc] = typ_type_init(th); // This MUST be first in the list.
	def[FuncEnc] = typ_meth_init(th);
	def[IntEnc] = typ_int_init(th);
	def[ArrEnc] = typ_list_init(th);
	def[AllEnc] = pushType(th);
	popGlobal(th, "All");

	typ_file_init(th);

	env_stream_init(th);
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

