/** Range type methods and properties
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

/** Create a new range */
int range_new(Value th) {
	Value from = getTop(th)>1? getLocal(th,1) : aNull;
	Value step;
	if (getTop(th)>3)
		step = getLocal(th, 3);
	else if (isInt(from))
		step = anInt(1);
	else if (isFloat(from))
		step = aFloat(1.0);
	else
		step = aNull;

	Value rng = pushArray(th, vmlit(TypeRangem), 3);
	arrSet(th, rng, 0, from);
	arrSet(th, rng, 1, getTop(th)>2? getLocal(th, 2) : from);
	arrSet(th, rng, 2, step);
	return 1;
}

/** Get the 'from' value out of the range */
int range_from_get(Value th) {
	pushValue(th, arrGet(th, getLocal(th, 0), 0));
	return 1;
}

/** Set the 'from' value out of the range */
int range_from_set(Value th) {
	arrSet(th, getLocal(th, 0), 0, getLocal(th, 1));
	setTop(th, 1);
	return 1;
}

/** Get the 'to' value out of the range */
int range_to_get(Value th) {
	pushValue(th, arrGet(th, getLocal(th, 0), 1));
	return 1;
}

/** Set the 'to' value out of the range */
int range_to_set(Value th) {
	arrSet(th, getLocal(th, 0), 1, getLocal(th, 1));
	setTop(th, 1);
	return 1;
}

/** Get the 'step' value out of the range */
int range_step_get(Value th) {
	pushValue(th, arrGet(th, getLocal(th, 0), 2));
	return 1;
}

/** Set the 'step' value out of the range */
int range_step_set(Value th) {
	arrSet(th, getLocal(th, 0), 2, getLocal(th, 1));
	setTop(th, 1);
	return 1;
}

/** Closure iterator that retrieves Range's next value */
int range_each_get(Value th) {
	Value current = pushCloVar(th, 2); popValue(th);
	Value to      = pushCloVar(th, 3); popValue(th);
	Value step    = pushCloVar(th, 4); popValue(th);
	if (isInt(current)) {
		Aint curi = toAint(current);
		Aint toi = toAint(to);
		Aint stepi = toAint(step);
		if (stepi>=0 && curi>toi || stepi<0 && curi<toi)
			return 0;
		pushValue(th, anInt(curi + stepi));
		popCloVar(th, 2);
		pushValue(th, aTrue);
		pushValue(th, current);
	}
	else if (isFloat(current)) {
		Afloat curf = toAfloat(current);
		Afloat tof = toAfloat(to);
		Afloat stepf = toAfloat(step);
		if (stepf>=0.0f && curf>tof || stepf<0.0f && curf<tof)
			return 0;
		pushValue(th, aFloat(curf + stepf));
		popCloVar(th, 2);
		pushValue(th, aTrue);
		pushValue(th, current);
	}
	else {
		pushValue(th, vmlit(SymRocket));
		pushValue(th, current);
		pushValue(th, to);
		getCall(th, 2, 1);
		Value test = popValue(th);
		if (test==aNull || test==anInt(1))
			return 0;
		pushValue(th, aTrue);
		pushValue(th, current);
		pushSym(th, "Incr");
		pushValue(th, current);
		pushValue(th, step);
		getCall(th, 2, 1);
		popCloVar(th, 2);
	}
	return 2;
}

/** Reset the current iterated value */
int range_each_set(Value th) {
	if (getTop(th)>1) {
		pushLocal(th, 1);
		popCloVar(th, 0);
	}
	return 0;
}

/** Return a get/set closure that iterates over the range */
int range_each(Value th) {
	Value self = pushLocal(th, 0);
	pushCMethod(th, range_each_get);
	pushCMethod(th, range_each_set);
	pushValue(th, arrGet(th, self, 0));
	pushValue(th, arrGet(th, self, 1));
	pushValue(th, arrGet(th, self, 2));
	pushClosure(th, 5);
	return 1;
}

/** Compare to see whether value is between (inclusively) 'from' and 'to' */
int range_match(Value th) {
	Value self = getLocal(th, 0);
	Value val = getTop(th)>1? getLocal(th, 1) : aNull;
	Value from = arrGet(th, self, 0);
	Value to   = arrGet(th, self, 1);
	Value step = arrGet(th, self, 2);
	bool ismatch = false;
	if (isInt(from)) {
		if (isInt(val)) {
			Aint vali = toAint(val);
			Aint fromi = toAint(from);
			Aint toi = toAint(to);
			Aint stepi = toAint(step);
			ismatch = stepi>=0 && vali>=fromi && vali<=toi || vali<=fromi && vali>=toi;
		}
	}
	else if (isFloat(from)) {
		if (isFloat(val)) {
			Afloat valf = toAfloat(val);
			Afloat fromf = toAfloat(from);
			Afloat tof = toAfloat(to);
			Afloat stepf = toAfloat(step);
			ismatch = stepf>=0.0f && valf>=fromf && valf<=tof || valf<=fromf && valf>=tof;
		}
	}
	else {
		pushValue(th, vmlit(SymRocket));
		pushValue(th, from);
		pushValue(th, val);
		getCall(th, 2, 1);
		Value comp1 = popValue(th);
		pushValue(th, vmlit(SymRocket));
		pushValue(th, val);
		pushValue(th, to);
		getCall(th, 2, 1);
		Value comp2 = popValue(th);
		Value greater = anInt(1);
		ismatch = comp1!=aNull && comp1!=greater && comp2!=aNull && comp2!=greater;
	}
	pushValue(th, ismatch? aTrue : aFalse);
	return 1;
}

/** Initialize the Range type */
void core_range_init(Value th) {
	vmlit(TypeRangec) = pushType(th, vmlit(TypeObject), 4);
		pushSym(th, "Range");
		popProperty(th, 0, "_name");
		vmlit(TypeRangem) = pushMixin(th, vmlit(TypeObject), aNull, 16);
			pushSym(th, "*Range");
			popProperty(th, 1, "_name");
			pushCMethod(th, range_from_get);
			pushCMethod(th, range_from_set);
			pushClosure(th, 2);
			popProperty(th, 1, "from");
			pushCMethod(th, range_to_get);
			pushCMethod(th, range_to_set);
			pushClosure(th, 2);
			popProperty(th, 1, "to");
			pushCMethod(th, range_step_get);
			pushCMethod(th, range_step_set);
			pushClosure(th, 2);
			popProperty(th, 1, "step");
			pushCMethod(th, range_each);
			popProperty(th, 1, "Each");
			pushCMethod(th, range_match);
			popProperty(th, 1, "~~");
		popProperty(th, 0, "traits");

		pushCMethod(th, range_new);
		popProperty(th, 0, "New");
	popGloVar(th, "Range");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
