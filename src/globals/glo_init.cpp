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

void typ_resource_init(Value th); // Must define before Method, File etc.
void typ_method_init(Value th);

void typ_file_init(Value th);

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
			popProperty(th, 1, "+");
			pushCMethod(th, typ_int_minus);
			popProperty(th, 1, "-");
			pushCMethod(th, typ_int_mult);
			popProperty(th, 1, "*");
		popProperty(th, 0, "newtype");
	popGloVar(th, "Integer");
	return;
}

/** Create a new List. Parameters fill the List */
int typ_list_new(Value th) {
	int arrsz = getTop(th)-1;
	Value arr = pushArray(th, vmlit(TypeListm), arrsz);
	int idx = 0;
	while (arrsz--) {
		arrSet(th, arr, idx, getLocal(th, idx+1));
		idx++;
	}
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
			popProperty(th, 1, "<<");
			pushCMethod(th, typ_list_next);
			popProperty(th, 1, "next");
		popProperty(th, 0, "newtype");
		pushCMethod(th, typ_list_new);
		popProperty(th, 0, "new");
	popGloVar(th, "List");
	return;
}

/** Create a new Index */
int typ_index_new(Value th) {
	Value type = pushTbl(th, getLocal(th, 0), 4);
	return 1;
}

/** Initialize the Index type, used to create other types */
void typ_index_init(Value th) {
	vmlit(TypeType) = pushType(th, aNull, 1);
		pushCMethod(th, typ_index_new);
		popProperty(th, 0, "new");
	popGloVar(th, "Index");
	return;
}

/** Create a new Type */
int typ_type_new(Value th) {
	Value type = pushType(th, getLocal(th, 0), 4);
	return 1;
}

/** Lookup a value from type's named property */
int typ_type_get(Value th) {
	pushValue(th, getTop(th)>=2? getProperty(th, getLocal(th,0), getLocal(th,1)) : aNull);
	return 1;
}

/** Initialize the Type type, used to create other types */
void typ_type_init(Value th) {
	vmlit(TypeType) = pushType(th, aNull, 1);
		pushCMethod(th, typ_type_new);
		popProperty(th, 0, "new");
		pushCMethod(th, typ_type_get);
		popProperty(th, 0, "()");
	popGloVar(th, "Type");
	return;
}

/** Initialize all core types */
void glo_init(Value th) {

	typ_type_init(th); // Type must be first, so other types can use this as their type
	typ_int_init(th);
	typ_list_init(th);
	typ_index_init(th);
	vmlit(TypeAll) = pushType(th, aNull, 0);
	popGloVar(th, "All");

	// Load resource before the types it uses
	typ_resource_init(th);
	typ_method_init(th);

	typ_file_init(th);
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

