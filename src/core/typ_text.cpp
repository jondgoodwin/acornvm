/** Text type (utf-8 strings) methods and properties
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
 */

#include "avmlib.h"
#include <string.h>

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

	// If it is already Text, create a clone of it
	Value parm = getLocal(th,1);
	if (isStr(parm)) {
		pushStringl(th, vmlit(TypeTextm), toStr(parm), str_size(parm));
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

/** Create a copy of a text value */
int text_clone(Value th) {
	Value self = getLocal(th,0);
	pushStringl(th, str_info(self)->type, toStr(self), str_size(self));
	return 1;
}

/** Create a new string that concatenates two strings */
int text_add(Value th) {
	if (getTop(th)<2 || !isStr(getLocal(th,1)))
		return 0;
	Value self = getLocal(th,0);
	Value parm = getLocal(th,1);
	Value newstr = pushStringl(th, vmlit(TypeTextm), NULL, str_size(self)+str_size(parm));
	strAppend(th, newstr, toStr(self), str_size(self));
	strAppend(th, newstr, toStr(parm), str_size(parm));
	return 1;
}

/** Create a new string that duplicates self n times */
int text_multiply(Value th) {
	if (getTop(th)<2 || !isInt(getLocal(th,1)))
		return 0;
	Value self = getLocal(th,0);
	Aint n = toAint(getLocal(th,1));
	if (n<0) n=0;
	Value newstr = pushStringl(th, vmlit(TypeTextm), NULL, n * str_size(self));
	while (n--)
		strAppend(th, newstr, toStr(self), str_size(self));
	return 1;
}

/** Append to end of text string */
int text_append(Value th) {
	if (getTop(th)>1) {
		Value self = getLocal(th,0);
		Value parm = getLocal(th,1);
		if (!isStr(parm)) {
			pushSym(th, "Text");
			pushValue(th, parm);
			getCall(th, 1, 1);
			parm = getFromTop(th, 0);
		}
		if (isStr(parm))
			strAppend(th, self, toStr(parm), str_size(parm));
	}
	setTop(th, 1);
	return 1;
}

/** Prepend to start of text string */
int text_prepend(Value th) {
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

/** Return 0 if equal, 1 if self is greater, -1 if self is less, null if not comparable */
int text_compare(Value th) {
	if (getTop(th)<2 || !isStr(getLocal(th,1)))
		return 0;
	pushValue(th, anInt(strcmp(toStr(getLocal(th, 0)), toStr(getLocal(th, 1)))));
	return 1;
}

/** Calculates the number of UTF-8 bytes used to encode the Unicode code point pointed at by textp */
#define utf8_charsize(textp) \
	((*textp&0x80) == 0x00? (*textp? 1 : 0) : \
	(*textp&0xE0) == 0xC0? 2 : (*textp&0xF0) == 0xE0? 3 : 4)

/** Scan entire utf8 string to determine how many unicode code points it has */
#define utf8_length(lenvar, textp) { \
	lenvar = 0; \
	const char* strp = textp; \
		while (*strp) { \
			strp += utf8_charsize(strp); \
			lenvar++; \
		} \
	}

/** Extract current utf-8 character */
Auchar utf8_getchar(char *textp) {
	int nbytes;
	Auchar chr;

	// Get info from first UTF-8 byte
	if ((*textp&0xF0) == 0xF0) {nbytes=4; chr = *textp&0x07;}
	else if ((*textp&0xE0) == 0xE0) {nbytes=3; chr = *textp&0x0F;}
	else if ((*textp&0xC0) == 0xC0) {nbytes=2; chr = *textp&0x1F;}
	else if ((*textp&0x80) == 0x00) {nbytes=1; chr = *textp&0x7F;}
	else {nbytes=1; chr = 0;} // error

	// Obtain remaining bytes
	while (--nbytes) {
		textp++;
		if ((*textp&0xC0)==0x80)
			chr = (chr<<6) + (*textp&0x3F);
	}
	return chr;
}

/** Return number of unicode code points in UTF-8 string */
int text_getsize(Value th) {
	const char *textp = toStr(getLocal(th, 0));
	Auint nchars;
	utf8_length(nchars, textp);
	pushValue(th, anInt(nchars));
	return 1;
}

/** Truncate size of a Text string to no more than specified number of utf-8 characters */
int text_setsize(Value th) {
	if (getTop(th)<2 || !isInt(getLocal(th, 1)))
		return 0;
	const char *textp = toStr(getLocal(th, 0));
	Auint size = toAint(getLocal(th,1));
	while (*textp && size--) {
		textp += utf8_charsize(textp);
	}
	*((char*)textp) = '\0';
	return 0;
}

/** Return character position within 'self' of passed text, starting from passed position or null if not found */
int text_find(Value th) {
	if (getTop(th)<2 || !isStr(getLocal(th, 1)))
		return 0;
	const char *self = toStr(getLocal(th, 0));
	const char *fstr = toStr(getLocal(th, 1));

	// Adjust self to point to indexed character
	Aint index = (getTop(th)>2 && isInt(getLocal(th,2)))? toAint(getLocal(th,2)) : 0;
	Aint i = index;
	while (i--)
		self+=utf8_charsize(self);

	// Look for a match
	const char *found = strstr(self, fstr);
	if (found==NULL)
		return 0;

	// Count up to get indexed position of first matching char
	while (self!=found) {
		self+=utf8_charsize(self);
		index++;
	}
	pushValue(th, anInt(index));
	return 1;
}

/** Get one or a sequence of character(s) from a text string */
int text_get(Value th) {
	if (getTop(th)<2 || !isInt(getLocal(th,1)))
		return 0;
	const char *textp = toStr(getLocal(th, 0));
	Aint length;
	utf8_length(length, textp);

	// Calculate position of from character
	Aint from = toAint(getLocal(th,1));
	if (from > length)
		return 0;
	if (from < 0)
		from += length + 1;
	while (textp && from--)
		textp += utf8_charsize(textp);

	// Calculate byte size of requested excerpt
	Aint excerptsz;
	if (getTop(th)>2 && isInt(getLocal(th,2))) {
		Aint to = toAint(getLocal(th,2));
		if (to < 0)
			to += length + 1;
		to++; // Point to next character
		if (to > length)
			to = length;
		// Scan forward to find that after-character
		const char *totextp = textp;
		to -= toAint(getLocal(th,1));
		while (totextp && to--)
			totextp += utf8_charsize(totextp);
		excerptsz = totextp - textp;
	}
	else
		excerptsz = utf8_charsize(textp);

	pushStringl(th, vmlit(TypeTextm), textp, excerptsz);
	return 1;
}

/** Replace a sequence of character(s) with another text string */
int text_set(Value th) {
	if (getTop(th)<3 || !isStr(getLocal(th,1)) || !isInt(getLocal(th,2)))
		return 0;
	const char *textp = toStr(getLocal(th, 0));
	Aint length;
	utf8_length(length, textp);

	// Calculate position of from character
	Aint from = toAint(getLocal(th,2));
	if (from > length)
		return 0;
	if (from < 0)
		from += length + 1;
	while (textp && from--)
		textp += utf8_charsize(textp);

	// Calculate byte size of requested excerpt
	Aint excerptsz;
	if (getTop(th)>3 && isInt(getLocal(th,3))) {
		Aint to = toAint(getLocal(th,3));
		if (to < 0)
			to += length + 1;
		to++; // Point to next character
		if (to > length)
			to = length;
		// Scan forward to find that after-character
		const char *totextp = textp;
		to -= toAint(getLocal(th,2));
		while (totextp && to--)
			totextp += utf8_charsize(totextp);
		excerptsz = totextp - textp;
	}
	else
		excerptsz = utf8_charsize(textp);

	strSub(th, getLocal(th,0), textp-toStr(getLocal(th,0)), excerptsz, toStr(getLocal(th,1)), str_size(getLocal(th,1)));
	setTop(th, 1);
	return 1;
}

/** Replace a sequence of character(s) with another text string */
int text_remove(Value th) {
	if (getTop(th)<3 || !isInt(getLocal(th,1)))
		return 0;
	const char *textp = toStr(getLocal(th, 0));
	Aint length;
	utf8_length(length, textp);

	// Calculate position of from character
	Aint from = toAint(getLocal(th,1));
	if (from > length)
		return 0;
	if (from < 0)
		from += length + 1;
	while (textp && from--)
		textp += utf8_charsize(textp);

	// Calculate byte size of requested excerpt
	Aint excerptsz;
	if (getTop(th)>2 && isInt(getLocal(th,2))) {
		Aint to = toAint(getLocal(th,2));
		if (to < 0)
			to += length + 1;
		to++; // Point to next character
		if (to > length)
			to = length;
		// Scan forward to find that after-character
		const char *totextp = textp;
		to -= toAint(getLocal(th,1));
		while (totextp && to--)
			totextp += utf8_charsize(totextp);
		excerptsz = totextp - textp;
	}
	else
		excerptsz = utf8_charsize(textp);

	strSub(th, getLocal(th,0), textp-toStr(getLocal(th,0)), excerptsz, NULL, 0);
	setTop(th, 1);
	return 1;
}

/** Insert a sequence of character(s) into 'self' */
int text_insert(Value th) {
	if (getTop(th)<2 || !isInt(getLocal(th,1)) || !isStr(getLocal(th,2)))
		return 0;
	const char *textp = toStr(getLocal(th, 0));
	Aint length;
	utf8_length(length, textp);

	// Calculate position of from character
	Aint from = toAint(getLocal(th,1));
	if (from > length)
		return 0;
	if (from < 0)
		from += length + 1;
	while (textp && from--)
		textp += utf8_charsize(textp);

	strSub(th, getLocal(th,0), textp-toStr(getLocal(th,0)), 0, toStr(getLocal(th,2)), str_size(getLocal(th,2)));
	setTop(th, 1);
	return 1;
}

/** Closure iterator that retrieves Text's next character as new string */
int text_each_get(Value th) {
	Value self    = pushCloVar(th, 2); popValue(th);
	AuintIdx curpos = toAint(pushCloVar(th, 3)); popValue(th);
	AuintIdx charidx = toAint(pushCloVar(th, 4)); popValue(th);
	if (curpos>=str_size(self))
		return 0;
	const char *textp = toStr(self)+curpos;
	pushValue(th, anInt(curpos + utf8_charsize(textp)));
	popCloVar(th, 3);
	pushValue(th, anInt(charidx + 1));
	popCloVar(th, 4);
	pushValue(th, anInt(charidx));
	pushStringl(th, vmlit(TypeTextm), textp, utf8_charsize(textp));
	return 2;
}

/** Return a get/set closure that iterates over the Text value */
int text_each(Value th) {
	Value self = pushLocal(th, 0);
	pushCMethod(th, text_each_get);
	pushValue(th, aNull);
	pushValue(th, self);
	pushValue(th, anInt(0));
	pushValue(th, anInt(0));
	pushClosure(th, 5);
	return 1;
}

/** Initialize the Text type */
void core_text_init(Value th) {
	vmlit(TypeTextc) = pushType(th, vmlit(TypeObject), 4);
		pushSym(th, "Text");
		popProperty(th, 0, "_name");
		vmlit(TypeTextm) = pushMixin(th, vmlit(TypeObject), aNull, 32);
			pushSym(th, "*Text");
			popProperty(th, 1, "_name");
			pushCMethod(th, text_clone);
			popProperty(th, 1, "Clone");
			pushCMethod(th, text_add);
			popProperty(th, 1, "+");
			pushCMethod(th, text_multiply);
			popProperty(th, 1, "*");
			pushCMethod(th, text_append);
			popProperty(th, 1, "<<");
			pushCMethod(th, text_prepend);
			popProperty(th, 1, ">>");
			pushCMethod(th, text_isempty);
			popProperty(th, 1, "empty?");
			pushCMethod(th, text_compare);
			popProperty(th, 1, "<=>");
			pushCMethod(th, text_getsize);
			pushCMethod(th, text_setsize);
			pushClosure(th, 2);
			popProperty(th, 1, "size");
			pushCMethod(th, text_find);
			popProperty(th, 1, "Find");
			pushCMethod(th, text_get);
			pushCMethod(th, text_set);
			pushClosure(th, 2);
			popProperty(th, 1, "[]");
			pushCMethod(th, text_remove);
			popProperty(th, 1, "Remove");
			pushCMethod(th, text_insert);
			popProperty(th, 1, "Insert");
			pushCMethod(th, text_set);
			popProperty(th, 1, "Replace");
			pushCMethod(th, text_each);
			popProperty(th, 1, "Each");
		popProperty(th, 0, "traits");
		pushCMethod(th, text_new);
		popProperty(th, 0, "New");
	popGloVar(th, "Text");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
