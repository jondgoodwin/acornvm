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
void typ_int_init(Value th) {
	vmlit(TypeIntc) = pushType(th, vmlit(TypeType), 2);
		vmlit(TypeIntm) = pushMixin(th, vmlit(TypeType), aNull, 3);
			pushCMethod(th, typ_int_plus);
			popMember(th, 1, "+");
			pushCMethod(th, typ_int_minus);
			popMember(th, 1, "-");
			pushCMethod(th, typ_int_mult);
			popMember(th, 1, "*");
		popMember(th, 0, "newtype");
	popGlobal(th, "Integer");
	return;
}

/** Create a new List */
int typ_list_new(Value th) {
	pushList(th, 0);
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
void typ_list_init(Value th) {
	vmlit(TypeListc) = pushType(th, vmlit(TypeType), 2);
		vmlit(TypeListm) = pushMixin(th, vmlit(TypeType), aNull, 2);
			pushCMethod(th, typ_list_add);
			popMember(th, 1, "<<");
			pushCMethod(th, typ_list_next);
			popMember(th, 1, "next");
		popMember(th, 0, "newtype");
		pushCMethod(th, typ_list_new);
		popMember(th, 0, "new");
	popGlobal(th, "List");
	return;
}

/** Call the method, passing its parameters */
int typ_meth_get(Value th) {
	methodCall(th, getTop(th)-1, BCVARRET);
	return getTop(th);
}

/** Initialize the Method type, used to create other types */
void typ_meth_init(Value th) {
	vmlit(TypeMethc) = pushType(th, vmlit(TypeType), 1);
		vmlit(TypeMethm) = pushMixin(th, vmlit(TypeType), aNull, 1);
			pushCMethod(th, typ_meth_get);
			popMember(th, 1, "()");
		popMember(th, 0, "newtype");
	popGlobal(th, "Method");
	return;
}

/** Lookup a value from type's named property */
int typ_type_get(Value th) {
	pushValue(th, getTop(th)>=2? getProperty(th, getLocal(th,0), getLocal(th,1)) : aNull);
	return 1;
}

/** Initialize the Type type, used to create other types */
void typ_type_init(Value th) {
	vmlit(TypeType) = pushType(th, aNull, 1);
		pushCMethod(th, typ_type_get);
		popMember(th, 0, "()");
	popGlobal(th, "Type");
	return;
}

/** Initialize all core types */
void glo_init(Value th) {

	typ_type_init(th); // Type must be first, so other types can use this as their type
	typ_meth_init(th);
	typ_int_init(th);
	typ_list_init(th);

	vmlit(TypeAll) = pushType(th, aNull, 0);
	popGlobal(th, "All");

	typ_file_init(th);

	env_stream_init(th);
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

