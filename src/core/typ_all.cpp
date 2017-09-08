/** All type methods and properties
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

/** <=> */
int all_compare(Value th) {
	if (getTop(th)>1 && getLocal(th,0)==getLocal(th,1)) {
		pushValue(th, anInt(0));
		return 1;
	}
	return 0;
}

/** === Exact match of values */
int all_same(Value th) {
	pushValue(th, getTop(th)>1 && getLocal(th,0)==getLocal(th,1)? aTrue : aFalse);
	return 1;
}

#define all_rocket \
	if (getTop(th)<2) \
		return 0; \
	pushValue(th, vmlit(SymRocket)); \
	pushValue(th, getLocal(th,0)); \
	pushValue(th, getLocal(th,1)); \
	getCall(th, 2, 1); \
	Value ret = popValue(th);

/** ~~, == equal using <=> */
int all_equal(Value th) {
	all_rocket;
	pushValue(th, ret == anInt(0)? aTrue : aFalse);
	return 1;
}

/** < */
int all_lesser(Value th) {
	all_rocket;
	pushValue(th, ret == anInt(-1)? aTrue : aFalse);
	return 1;
}

/** > */
int all_greater(Value th) {
	all_rocket;
	pushValue(th, ret == anInt(1)? aTrue : aFalse);
	return 1;
}

/** <= */
int all_lesseq(Value th) {
	all_rocket;
	pushValue(th, ret == anInt(-1) || ret == anInt(0)? aTrue : aFalse);
	return 1;
}

/** >= */
int all_greateq(Value th) {
	all_rocket;
	pushValue(th, ret == anInt(1) || ret == anInt(0)? aTrue : aFalse);
	return 1;
}

/** executable? */
int all_isexec(Value th) {
	pushValue(th, canCall(getLocal(th,0))? aTrue : aFalse);
	return 1;
}

/** type */
int all_type(Value th) {
	pushValue(th, getType(th, getLocal(th,0)));
	return 1;
}

/** property */
int all_property(Value th) {
	if (getTop(th)>1) {
		pushValue(th, getProperty(th, getLocal(th, 0), getLocal(th, 1)));
		return 1;
	}
	return 0;
}

/** .Mixin(mixin) */
int all_mixin(Value th) {
	if (getTop(th)>1)
		addMixin(th, getLocal(th, 0), getLocal(th, 1));
	setTop(th, 1);
	return 1;
}

/** Initialize the All type */
void core_all_init(Value th) {
	vmlit(TypeAll) = pushMixin(th, vmlit(TypeObject), aNull, 32);
		pushSym(th, "All");
		popProperty(th, 0, "_name");
		pushCMethod(th, all_compare);
		popProperty(th, 0, "<=>");
		pushCMethod(th, all_equal);
		popProperty(th, 0, "~~");
		pushCMethod(th, all_equal);
		popProperty(th, 0, "==");
		pushCMethod(th, all_same);
		popProperty(th, 0, "===");
		pushCMethod(th, all_lesser);
		popProperty(th, 0, "<");
		pushCMethod(th, all_lesseq);
		popProperty(th, 0, "<=");
		pushCMethod(th, all_greater);
		popProperty(th, 0, ">");
		pushCMethod(th, all_greateq);
		popProperty(th, 0, ">=");
		pushCMethod(th, all_isexec);
		popProperty(th, 0, "callable?");
		pushCMethod(th, all_property);
		popProperty(th, 0, "property");
		pushCMethod(th, all_type);
		popProperty(th, 0, "type");
		pushCMethod(th, all_mixin);
		popProperty(th, 0, "Mixin");
	popGloVar(th, "All");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
