/** Symbol type (immutable utf-8 strings) methods and properties
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

/** Create a new symbol, converting first parm to Text if necessary. */
int symbol_new(Value th) {
	// No parms returns null
	if (getTop(th)<2)
		return 0;

	// If it is already Symbol, just return it
	if (isSym(getLocal(th, 1))) {
		pushLocal(th, 1);
		return 1;
	}

	// If not Text, convert first to a Text string
	Value textval = getLocal(th,1);
	if (!isStr(textval)) {
		pushSym(th, "Text");
		pushValue(th, textval);
		getCall(th, 1, 1);
		if (!isStr(textval = getFromTop(th, 0)))
			return 0;
	}

	// Convert text to symbol
	pushSym(th, toStr(textval));
	return 1;
}

/** Return 0 if symbol exactly equal to parm, else return null */
int symbol_rocket(Value th) {
	if (getTop(th)>1 && getLocal(th, 0)==getLocal(th, 1)) {
		pushValue(th, anInt(0));
		return 1;
	}
	return 0;
}

/** Convert symbol to a Text value containing the same characters */
int symbol_text(Value th) {
	pushString(th, vmlit(TypeTextm), toStr(getLocal(th,0)));
	return 1;
}

/** Initialize the Symbol type */
void core_symbol_init(Value th) {
	vmlit(TypeSymc) = pushType(th, vmlit(TypeObject), 4);
		pushSym(th, "Symbol");
		popProperty(th, 0, "_name");
		vmlit(TypeSymm) = pushMixin(th, vmlit(TypeObject), aNull, 32);
			pushSym(th, "*Symbol");
			popProperty(th, 1, "_name");
			pushCMethod(th, symbol_rocket);
			popProperty(th, 1, "<=>");
			pushCMethod(th, symbol_text);
			popProperty(th, 1, "Text");
		popProperty(th, 0, "traits");
		pushCMethod(th, symbol_new);
		popProperty(th, 0, "New");
	popGloVar(th, "Symbol");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
