/** Implements strings, mutable byte-sequences. (see avm_string.h)
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

/* Return a string value containing the byte sequence. */
Value newStrl(Value th, const char *str, AuintIdx len) {
	StrInfo *val;

	// Create a string object
	MemInfo **linkp = NULL;
	val = (StrInfo *) mem_new(vm(th), StrEnc, sizeof(StrInfo), linkp, 0);
	val->avail = len;
	val->str = (char*) mem_gcrealloc(vm(th), NULL, 0, len+1); // an extra byte for 0-terminator
	val->flags1 = 0;

	// Copy string's contents over, if provided.
	if (str) {
		memcpy(val->str, str, len);
		val->size = len;
	}
	// If not provided, create null string
	else {
		val->str[0] = '\0';
		val->size = 0;
	}
	val->str[len] = '\0'; // put guaranteed 0-terminator, just in case
	val->type = vm(th)->defEncTypes[StrEnc]; // Assume default type
	return (Value) val;
}

/* Calculate length of c-string, then use aStrl(). */
Value newStr(Value th, const char *str) {
	return newStrl(th,str,strlen(str));
}

/* Return 1 if the value is a String, otherwise 0 */
int isStr(Value str) {
	return isEnc(str, StrEnc);
}

/* Ensure string has room for len Values, allocating memory as needed.
 * Allocated space will not shrink. Changes nothing about string's contents. */
void strMakeRoom(Value th, Value val, AuintIdx len) {
	StrInfo *str = str_info(val);

	/* Expand available space, if needed */
	if (len > str->avail) {
		str->str = (char*) mem_gcrealloc(vm(th), str->str, str->avail+1, len+1);
		str->avail = len;
		str->str[len] = '\0';
	}
}

/*	Replace part of a string with the c-string contents starting at pos.
 *	If sz==0, it becomes an insert. If str==NULL or len==0, it becomes a deletion.
 *	The Acorn string will be resized automatically to accommodate excess characters.
 *	The operation will not be performed if resizing is not possible. */
void strSub(Value th, Value val, AuintIdx pos, AuintIdx sz, const char *repstr, AuintIdx replen) {
	StrInfo* str = str_info(val);

	/* Protect from bad parms */
	if (repstr==NULL) replen = 0; /* If no string, replacement len is zero */
	if (pos > str->size) pos = str->size; /* clamp to end of old string */
	if (pos + sz > str->size) sz = str->size - pos; /* clamp to end */

	AuintIdx len = str->size - sz + replen; /* Calculate new length */

	/* Return if nothing to do or new length overflows past 32-bits */
	if ((sz==0 && replen==0) || len < replen)
		return;

	/* Expand available space, if needed */
	if (len > str->avail)
		strMakeRoom(th, val, len);

	/* Move part of string above replacement zone to its new position */
	if (str->size > pos+sz)
		memmove(&str->str[pos+replen], &str->str[pos+sz], str->size-pos-sz);

	/* Move replacement string in where it belongs */
	if (replen)
		memcpy(&str->str[pos], repstr, replen);

	/* Place 0-terminator as needed, and reset hash to uncalculated */
	str->size = len;
	str->str[len] = '\0';
}

} // extern "C"
} // namespace avm


