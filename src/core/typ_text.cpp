/** Text type (utf-8 strings) methods and properties
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

/** Create a new Text string, converting first parm to Text if necessary. */
int text_new(Value th) {
	// No parms returns an empty string
	if (getTop(th)<2) {
		pushString(th, vmlit(TypeTextm), "");
		return 1;
	}

	// If it is already Text, just return it
	if (isStr(getLocal(th, 1))) {
		pushLocal(th, 1);
		return 1;
	}

	// Return value converted to a Text string
	pushSym(th, "Text");
	pushLocal(th, 1);
	getCall(th, 1, 1);
	if (isNull(getFromTop(th, 0)))
		pushString(th, vmlit(TypeTextm), "");
	return 1;
}

int text_clone(Value th) {
	Value self = getLocal(th,0);
	pushStringl(th, str_info(self)->type, toStr(self), str_size(self));
	return 1;
}

/** Append to end of text string */
int text_append(Value th) {
	Value self = getLocal(th,0);
	Value parm;
	if (getTop(th)>1 && isStr(parm=getLocal(th,1))) {
		strAppend(th, self, toStr(parm), str_size(parm));
	}
	setTop(th, 1);
	return 1;
}

/** Prepend to start of text string */
int text_preppend(Value th) {
	Value self = getLocal(th,0);
	Value parm;
	if (getTop(th)>1 && isStr(parm=getLocal(th,1))) {
		strSub(th, self, 0, 0, toStr(parm), str_size(parm));
	}
	setTop(th, 1);
	return 1;
}

/** Return true if string is empty (has no characters) */
int text_isempty(Value th) {
	pushValue(th, str_size(getLocal(th, 0))==0? aTrue : aFalse);
	return 1;
}

/** Return number of unicode code point in UTF-8 string */
int text_getsize(Value th) {
	pushValue(th, anInt(getSize(getLocal(th, 0))));
	return 1;
}

/** Set size of list */
int text_setsize(Value th) {
	if (getTop(th)<2 || !isInt(getLocal(th, 1)))
		return 0;
	arrSetSize(th, getLocal(th, 0), toAint(getLocal(th, 1)));
	return 0;
}

/** Force size of list */
int text_forcesize(Value th) {
	if (getTop(th)<2 || !isInt(getLocal(th, 1)))
		return 0;
	arrForceSize(th, getLocal(th, 0), toAint(getLocal(th, 1)));
	return 0;
}

/** Get array element at specified integer position */
int text_get(Value th) {
	if (getTop(th)<2 || !isInt(getLocal(th, 1)))
		return 0;
	AintIdx idx = toAint(getLocal(th,1));
	if (idx<0)
		idx += arr_size(getLocal(th,0));
	pushValue(th, arrGet(th, getLocal(th,0), idx));
	return 1;
}

/** Set array element at specified integer position */
int text_set(Value th) {
	if (getTop(th)<3 || !isInt(getLocal(th, 2)))
		return 0;
	AintIdx idx = toAint(getLocal(th,2));
	if (idx<0)
		idx += arr_size(getLocal(th,0));
	arrSet(th, getLocal(th,0), idx, getLocal(th, 1));
	return 0;
}

/** Copy specified elements from second list into self, replacing element's at specified positions */
int text_sub(Value th) {
	if (getTop(th)<6 || !isInt(getLocal(th,1)) || !isInt(getLocal(th,2))
		|| !isArr(getLocal(th, 3)) || !isInt(getLocal(th, 4)) || !isInt(getLocal(th,5)))
		return 0;
	Value arr = getLocal(th, 0);
	AintIdx size = arr_size(arr);
	AintIdx pos = toAint(getLocal(th,1));
	if (pos<0) pos += size;
	AintIdx len = toAint(getLocal(th,2));
	Value arr2 = getLocal(th, 3);
	AintIdx size2 = arr_size(arr2);
	AintIdx pos2 = toAint(getLocal(th,4));
	if (pos2<0) pos2 += size2;
	AintIdx len2 = toAint(getLocal(th,6));

	arrSub(th, arr, pos, len, arr2, pos2, len2);
	return 1;
}

/** Initialize the Text type */
void core_text_init(Value th) {
	vmlit(TypeTextc) = pushType(th, vmlit(TypeType), 4);
		pushSym(th, "Text");
		popProperty(th, 0, "_name");
		vmlit(TypeTextm) = pushMixin(th, vmlit(TypeType), aNull, 32);
			pushSym(th, "*Text");
			popProperty(th, 1, "_name");
			pushCMethod(th, text_clone);
			popProperty(th, 1, "Clone");
			pushCMethod(th, text_append);
			popProperty(th, 1, "<<");
			pushCMethod(th, text_preppend);
			popProperty(th, 1, ">>");
			pushCMethod(th, text_isempty);
			popProperty(th, 1, "Empty?");
			pushCMethod(th, text_getsize);
			pushCMethod(th, text_setsize);
			pushClosure(th, 2);
			popProperty(th, 1, "size");
			pushCMethod(th, text_get);
			pushCMethod(th, text_set);
			pushClosure(th, 2);
			popProperty(th, 1, "()");
			pushCMethod(th, text_sub);
			popProperty(th, 1, "Sub");
		popProperty(th, 0, "_newtype");
		pushCMethod(th, text_new);
		popProperty(th, 0, "New");
	popGloVar(th, "Text");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
