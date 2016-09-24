/** Float type methods and properties
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
 */

#include "avmlib.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <float.h>
#include <stdlib.h>

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

#define returnNullIfVar2NotFloat() \
	if (getTop(th)<2) {pushValue(th, aNull); return 1;} \
	if (isInt(getLocal(th, 1))) {setLocal(th, 1, aFloat((Afloat) toAint(getLocal(th, 1))));} \
	if (!isFloat(getLocal(th, 1))) {pushValue(th, aNull); return 1;} \
	Afloat self = toAfloat(getLocal(th,0)); \
	Afloat var2 = toAfloat(getLocal(th,1));
	
/** Nan? */
int float_isnan(Value th) {
	Afloat self = toAfloat(getLocal(th, 0));
	pushValue(th, (self!=self)? aTrue : aFalse);
	return 1;
}

/** Return true if self is an Float */
int float_isfloat(Value th) {
	pushValue(th, isFloat(getLocal(th,0))? aTrue : aNull);
	return 1;
}

/** sign */
int float_sign(Value th) {
	Afloat self = toAfloat(getLocal(th, 0));
	pushValue(th, anInt((self>0.) - (self<0.)));
	return 1;
}

/** Negate */
int float_neg(Value th) {
	pushValue(th, aFloat(-toAfloat(getLocal(th,0))));
	return 1;
}

/** Next number (increment by 1) */
int float_next(Value th) {
	pushValue(th, aFloat(1.0f+toAfloat(getLocal(th,0))));
	return 1;
}

/** 'Abs'olute value */
int float_abs(Value th) {
	pushValue(th, aFloat(fabs(toAfloat(getLocal(th,0)))));
	return 1;
}

/** Add two floats */
int float_plus(Value th) {
	returnNullIfVar2NotFloat();
	pushValue(th, aFloat(self + var2));
	return 1;
}

/** Subtract two floats */
int float_minus(Value th) {
	returnNullIfVar2NotFloat();
	pushValue(th, aFloat(self - var2));
	return 1;
}

/** Multiply two floats */
int float_mult(Value th) {
	returnNullIfVar2NotFloat();
	pushValue(th, aFloat(self * var2));
	return 1;
}

/** Divide two floats */
int float_div(Value th) {
	returnNullIfVar2NotFloat();
	pushValue(th, aFloat(self / var2));
	return 1;
}

/** The remainder after dividing two floats */
int float_remainder(Value th) {
	returnNullIfVar2NotFloat();
	pushValue(th, aFloat(fmod(self, var2)));
	return 1;
}

/** Power of two floats */
int float_power(Value th) {
	returnNullIfVar2NotFloat();
	pushValue(th, aFloat(pow(self, var2)));
	return 1;
}

/** Power of two floats */
int float_sqrt(Value th) {
	pushValue(th, aFloat(sqrt(toAfloat(getLocal(th, 0)))));
	return 1;
}

/** Log-based 10 function */
int float_log(Value th) {
	pushValue(th, aFloat(log10(toAfloat(getLocal(th,0)))));
	return 1;
}

/** Logarithm based e function */
int float_ln(Value th) {
	pushValue(th, aFloat(log(toAfloat(getLocal(th,0)))));
	return 1;
}

/** e^x function */
int float_exp(Value th) {
	pushValue(th, aFloat(exp(toAfloat(getLocal(th,0)))));
	return 1;
}

bool float_almostequal(Afloat a, Afloat b) {
	Afloat diff;
	if ((diff = fabs(a - b))<0.0000001)
		return true;
    a = fabs(a);
    b = fabs(b);
    Afloat largest = (b > a) ? b : a;
	return (diff <= largest * FLT_EPSILON);
}

/** Compare two floats */
int float_compare(Value th) {
	returnNullIfVar2NotFloat();
	pushValue(th, anInt(float_almostequal(self,var2)? 0 : self<var2? -1 : 1));
	return 1;
}

/** Maximum of two floats */
int float_max(Value th) {
	returnNullIfVar2NotFloat();
	pushValue(th, aFloat(self>var2? self : var2));
	return 1;
}

/** Minimum of two floats */
int float_min(Value th) {
	returnNullIfVar2NotFloat();
	pushValue(th, aFloat(self<var2? self : var2));
	return 1;
}

/** 'Rad'ian conversion from degrees */
int float_rad(Value th) {
	pushValue(th, aFloat((Afloat)M_PI*toAfloat(getLocal(th,0))/180.0f));
	return 1;
}

/** Sine function */
int float_sin(Value th) {
	pushValue(th, aFloat(sin(toAfloat(getLocal(th,0)))));
	return 1;
}

/** Cosine function */
int float_cos(Value th) {
	pushValue(th, aFloat(cos(toAfloat(getLocal(th,0)))));
	return 1;
}

/** Tangent function */
int float_tan(Value th) {
	pushValue(th, aFloat(tan(toAfloat(getLocal(th,0)))));
	return 1;
}

/** Arc-Sine function */
int float_asin(Value th) {
	pushValue(th, aFloat(asin(toAfloat(getLocal(th,0)))));
	return 1;
}

/** Arc-Cosine function */
int float_acos(Value th) {
	pushValue(th, aFloat(acos(toAfloat(getLocal(th,0)))));
	return 1;
}

/** Arc-Tangent function */
int float_atan(Value th) {
	if (getTop(th)<2)
		pushValue(th, aFloat(atan(toAfloat(getLocal(th,0)))));
	else
		pushValue(th, aFloat(atan2(toAfloat(getLocal(th,0)), toAfloat(getLocal(th,1)))));
	return 1;
}


/** round up function */
int float_ceil(Value th) {
	pushValue(th, aFloat(ceil(toAfloat(getLocal(th,0)))));
	return 1;
}

/** round down function */
int float_floor(Value th) {
	pushValue(th, aFloat(floor(toAfloat(getLocal(th,0)))));
	return 1;
}

/* Coerce symbol, string or float to float */
int float_new(Value th) {
	pushValue(th, aNull); // Default value if we fail to coerce
	if (getTop(th)<2)
		return 1;
	Value from = getLocal(th, 1);
	if (isFloat(from))
		pushValue(th, from);
	else if (isInt(from))
		pushValue(th, aFloat((Afloat) toAint(from)));
	else if (isSym(from) || isStr(from)) {
		const char *p = toStr(from);
		while (*p && (*p==' ' || *p=='\t'))
			p++;
		if (*p>='0' && *p<='9')
			pushValue(th, aFloat((Afloat)atof(p)));
	}
	return 1;
}

uint32_t int_pcgrng(Value th);
	
/** Return a random number from 0 to 1 */
int float_rand(Value th) {
	pushValue(th, aFloat((Afloat) ldexp((double) int_pcgrng(th), -32)));
	return 1;
}

/** Initialize the Integer type */
void core_float_init(Value th) {
	vmlit(TypeFloc) = pushType(th, vmlit(TypeType), 8);
		pushSym(th, "Float");
		popProperty(th, 0, "_name");
		vmlit(TypeFlom) = pushMixin(th, vmlit(TypeType), aNull, 32);
			pushSym(th, "*Float");
			popProperty(th, 1, "_name");

			pushCMethod(th, float_isfloat);
			popProperty(th, 1, "Integer?");
			pushCMethod(th, float_isnan);
			popProperty(th, 1, "Nan?");
			pushCMethod(th, float_sign);
			popProperty(th, 1, "Sign");

			pushCMethod(th, float_neg);
			popProperty(th, 1, "-@");
			pushCMethod(th, float_next);
			popProperty(th, 1, "Next");
			pushCMethod(th, float_abs);
			popProperty(th, 1, "Abs");
			pushCMethod(th, float_plus);
			popProperty(th, 1, "+");
			pushCMethod(th, float_minus);
			popProperty(th, 1, "-");
			pushCMethod(th, float_mult);
			popProperty(th, 1, "*");
			pushCMethod(th, float_div);
			popProperty(th, 1, "/");
			pushCMethod(th, float_remainder);
			popProperty(th, 1, "%");
			pushCMethod(th, float_power);
			popProperty(th, 1, "**");
			pushCMethod(th, float_sqrt);
			popProperty(th, 1, "Sqrt");

			pushCMethod(th, float_compare);
			popProperty(th, 1, "<=>");
			pushCMethod(th, float_max);
			popProperty(th, 1, "Max");
			pushCMethod(th, float_min);
			popProperty(th, 1, "Min");

			pushCMethod(th, float_rad);
			popProperty(th, 1, "Rad");
			pushCMethod(th, float_sin);
			popProperty(th, 1, "Sin");
			pushCMethod(th, float_cos);
			popProperty(th, 1, "Cos");
			pushCMethod(th, float_tan);
			popProperty(th, 1, "Tan");
			pushCMethod(th, float_asin);
			popProperty(th, 1, "Asin");
			pushCMethod(th, float_acos);
			popProperty(th, 1, "Acos");
			pushCMethod(th, float_atan);
			popProperty(th, 1, "Atan");

			pushCMethod(th, float_log);
			popProperty(th, 1, "Log");
			pushCMethod(th, float_ln);
			popProperty(th, 1, "Ln");
			pushCMethod(th, float_exp);
			popProperty(th, 1, "Exp");

			pushCMethod(th, float_ceil);
			popProperty(th, 1, "Ceil");
			pushCMethod(th, float_floor);
			popProperty(th, 1, "Floor");
		popProperty(th, 0, "_newtype");

		pushCMethod(th, float_new);
		popProperty(th, 0, "New");
		pushCMethod(th, float_rand);
		popProperty(th, 0, "Random");
		pushValue(th, aFloat((Afloat) M_PI));
		popProperty(th, 0, "pi");
		pushValue(th, aFloat((Afloat) M_E));
		popProperty(th, 0, "e");
	popGloVar(th, "Float");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
