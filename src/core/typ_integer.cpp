/** Integer type methods and properties
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
 */

#include "avmlib.h"
#include <stdlib.h>

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

#define returnNullIfVar2NotInt() \
	if (getTop(th)<2) {pushValue(th, aNull); return 1;} \
	if (isFloat(getLocal(th, 1))) {setLocal(th, 1, anInt((Aint) toAfloat(getLocal(th, 1))));} \
	if (!isInt(getLocal(th, 1))) {pushValue(th, aNull); return 1;} \
	int self = toAint(getLocal(th,0)); \
	int var2 = toAint(getLocal(th,1));
	
/** Negate (2's complement) */
int int_neg(Value th) {
	pushValue(th, anInt(-toAint(getLocal(th,0))));
	return 1;
}

/** Next number (increment by 1) */
int int_next(Value th) {
	pushValue(th, anInt(1+toAint(getLocal(th,0))));
	return 1;
}

/** sign */
int int_sign(Value th) {
	Aint self = toAint(getLocal(th, 0));
	pushValue(th, anInt((self>0) - (self<0)));
	return 1;
}

/** 'Abs'olute value */
int int_abs(Value th) {
	pushValue(th, anInt(abs(toAint(getLocal(th,0)))));
	return 1;
}

/** Add two integers */
int int_plus(Value th) {
	returnNullIfVar2NotInt();
	pushValue(th, anInt(self + var2));
	return 1;
}

/** Subtract two integers */
int int_minus(Value th) {
	returnNullIfVar2NotInt();
	pushValue(th, anInt(self - var2));
	return 1;
}

/** Multiply two integers */
int int_mult(Value th) {
	returnNullIfVar2NotInt();
	pushValue(th, anInt(self * var2));
	return 1;
}

/** Divide two integers */
int int_div(Value th) {
	returnNullIfVar2NotInt();
	if (var2 == 0)
		pushValue(th, aNull);
	else
		pushValue(th, anInt(self / var2));
	return 1;
}

/** The remainder after dividing two integers */
int int_remainder(Value th) {
	returnNullIfVar2NotInt();
	if (var2 == 0)
		pushValue(th, aNull);
	else
		pushValue(th, anInt(self % var2));
	return 1;
}

/** Power of two integers */
int int_power(Value th) {
	returnNullIfVar2NotInt();
	int result = 1;
    while (var2)
    {
        if (var2 & 1)
            result *= self;
        var2 >>= 1;
        self *= self;
    }
	pushValue(th, anInt(result));
	return 1;
}

/** Compare two integers */
int int_compare(Value th) {
	returnNullIfVar2NotInt();
	pushValue(th, anInt(self==var2? 0 : self<var2? -1 : 1));
	return 1;
}

/** Maximum of two integers */
int int_max(Value th) {
	returnNullIfVar2NotInt();
	pushValue(th, anInt(self>var2? self : var2));
	return 1;
}

/** Minimum of two integers */
int int_min(Value th) {
	returnNullIfVar2NotInt();
	pushValue(th, anInt(self<var2? self : var2));
	return 1;
}

/** Boolean 'Not' - bit reversal */
int int_not(Value th) {
	pushValue(th, anInt(~toAint(getLocal(th,0))));
	return 1;
}

/** Bitwise or of two integers */
int int_or(Value th) {
	returnNullIfVar2NotInt();
	pushValue(th, anInt(self | var2));
	return 1;
}

/** Bitwise and of two integers */
int int_and(Value th) {
	returnNullIfVar2NotInt();
	pushValue(th, anInt(self & var2));
	return 1;
}

/** Bitwise xor of two integers */
int int_xor(Value th) {
	returnNullIfVar2NotInt();
	pushValue(th, anInt(self ^ var2));
	return 1;
}

/** Bitwise left shift of two integers */
int int_shl(Value th) {
	returnNullIfVar2NotInt();
	pushValue(th, anInt(self << var2));
	return 1;
}

/** Bitwise right shift of two integers */
int int_shr(Value th) {
	returnNullIfVar2NotInt();
	pushValue(th, anInt(self >> var2));
	return 1;
}

/** Return text value with one Unicode character whose code corresponds to the integer */
int int_char(Value th) {
	Aint code = toAint(getLocal(th,0));
	char result[6];
	char *p = &result[0];

	if (code<0x80)
		*p++ = code;
	else if (code<0x800) {
		*p++ = 0xC0 | (code >> 6);
		*p++ = 0x80 | (code & 0x3f);
	}
	else if (code<0x10000) {
		*p++ = 0xE0 | (code >> 12);
		*p++ = 0x80 | ((code >> 6) & 0x3F);
		*p++ = 0x80 | (code & 0x3f);
	}
	else if (code<0x110000) {
		*p++ = 0xF0 | (code >> 18);
		*p++ = 0x80 | ((code >> 12) & 0x3F);
		*p++ = 0x80 | ((code >> 6) & 0x3F);
		*p++ = 0x80 | (code & 0x3f);
	}
	*p = '\0';

	pushString(th, aNull, result);
	return 1;
}

/** Biggest integer */
int int_biggest(Value th) {
	pushValue(th, anInt(((Aint)-1)>>2));
	return 1;
}

/* Coerce symbol, string or float to integer */
int int_new(Value th) {
	pushValue(th, aNull); // Default value if we fail to coerce
	if (getTop(th)<2)
		return 1;
	Value from = getLocal(th, 1);
	if (isInt(from))
		pushValue(th, from);
	else if (isFloat(from))
		pushValue(th, anInt((Aint) toAfloat(from)));
	else if (isSym(from) || isStr(from)) {
		const char *p = toStr(from);
		while (*p && (*p==' ' || *p=='\t'))
			p++;
		if (*p>='0' && *p<='9')
			pushValue(th, anInt(atoi(p)));
	}
	return 1;
}

/** Initialize the Integer type */
void core_int_init(Value th) {
	vmlit(TypeIntc) = pushType(th, vmlit(TypeType), 4);
		pushSym(th, "Integer");
		popProperty(th, 0, "_name");
		vmlit(TypeIntm) = pushMixin(th, vmlit(TypeType), aNull, 30);
			pushSym(th, "*Integer");
			popProperty(th, 1, "_name");

			pushCMethod(th, int_neg);
			popProperty(th, 1, "-@");
			pushCMethod(th, int_next);
			popProperty(th, 1, "Next");
			pushCMethod(th, int_abs);
			popProperty(th, 1, "Abs");
			pushCMethod(th, int_sign);
			popProperty(th, 1, "Sign");
			pushCMethod(th, int_plus);
			popProperty(th, 1, "+");
			pushCMethod(th, int_minus);
			popProperty(th, 1, "-");
			pushCMethod(th, int_mult);
			popProperty(th, 1, "*");
			pushCMethod(th, int_div);
			popProperty(th, 1, "/");
			pushCMethod(th, int_remainder);
			popProperty(th, 1, "%");
			pushCMethod(th, int_power);
			popProperty(th, 1, "**");

			pushCMethod(th, int_compare);
			popProperty(th, 1, "<=>");
			pushCMethod(th, int_max);
			popProperty(th, 1, "Max");
			pushCMethod(th, int_min);
			popProperty(th, 1, "Min");

			pushCMethod(th, int_not);
			popProperty(th, 1, "Not");
			pushCMethod(th, int_or);
			popProperty(th, 1, "Or");
			pushCMethod(th, int_and);
			popProperty(th, 1, "And");
			pushCMethod(th, int_xor);
			popProperty(th, 1, "Xor");
			pushCMethod(th, int_shl);
			popProperty(th, 1, "Shl");
			pushCMethod(th, int_shr);
			popProperty(th, 1, "Shr");

			pushCMethod(th, int_char);
			popProperty(th, 1, "Char");
		popProperty(th, 0, "_newtype");

		pushCMethod(th, int_biggest);
		popProperty(th, 0, "biggest");
		pushCMethod(th, int_new);
		popProperty(th, 0, "New");
	popGloVar(th, "Integer");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
