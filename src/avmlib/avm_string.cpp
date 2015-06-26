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
Value aStrl(Value th, const char *str, Auint32 len) {
	StrInfo *val;

	// Create a string object, adding to symbol table at hash entry
	MemInfo **linkp = NULL;
	val = (StrInfo *) mem_new(vm(th), StrEnc, sizeof(StrInfo), linkp, 0);
	val->avail = len;
	val->str = (char*) mem_gcrealloc(vm(th), NULL, 0, len+1); // an extra byte for 0-terminator

	// Flags1 holds StrHashed flag - default is to not hash string unless needed
	// by avm_hash structure. Then it uses str_calchash() to hash the string.
	// Any time string is changed, hash flag should be cleared.
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
	val->type = aNull; // type is unspecified
	return (Value) val;
}

/* Calculate length of c-string, then use aStrl(). */
Value aStr(Value th, const char *str) {
	return aStrl(th,str,strlen(str));
}

/* Retrieve string's hash, calculating it if not done already */
AHash str_hash(Value th, Value val) {
	StrInfo *str = str_info(val);
	/* Has hash been calculated? */
	if (!(str->flags1 & StrHashed)) {
		/* Calculate hash and indicate it has been calculated */
		str->hash = hash_bytes(str->str, str->size, th(th)->vm->hashseed);
		str->flags1 |= StrHashed;
	}
	return str->hash;
}

/* Resize string to a specified length, truncating as needed.
 * Allocated space will not shrink, but may expand if required.
 */
void strResize(Value th, Value val, Auint32 len) {
	StrInfo *str = str_info(val);

	/* If new length is between string size and buffer size, then change nothing */
	if (len >= str->size && len <= str->avail)
		return;

	/* Truncate, if string must be smaller. */
	if (len < str->size) {
		str->size = len;
	}

	/* Expand available space, if needed */
	else {
		assert(len > str->avail);
		str->str = (char*) mem_gcrealloc(vm(th), str->str, str->avail+1, len+1);
		str->avail = len;
	}

	/* Place 0-terminator as needed, and reset hash to uncalculated */
	str->str[len] = '\0';
	str->flags1 = 0;
}

#include <stdio.h>
/*	Replace part of a string with the c-string contents starting at pos.
 *	If sz==0, it becomes an insert. If str==NULL or len==0, it becomes a deletion.
 *	The Acorn string will be resized automatically to accommodate excess characters.
 *	The operation will not be performed if resizing is not possible. */
void strSub(Value th, Value val, Auint32 pos, Auint32 sz, const char *repstr, Auint32 replen) {
	StrInfo* str = str_info(val);

	/* Protect from bad parms */
	if (repstr==NULL) replen = 0; /* If no string, replacement len is zero */
	if (pos > str->size) pos = str->size; /* clamp to end of old string */
	if (pos + sz > str->size) sz = str->size - pos; /* clamp to end */

	Auint32 len = str->size - sz + replen; /* Calculate new length */

	/* Return if nothing to do or new length overflows past 32-bits */
	if ((sz==0 && replen==0) || len < replen)
		return;

	/* Expand available space, if needed */
	if (str->size < len)
		strResize(th, val, len);

	/* Move part of string above replacement zone to its new position */
	if (str->size > pos+sz)
		memmove(&str->str[pos+replen], &str->str[pos+sz], str->size-pos-sz);

	/* Move replacement string in where it belongs */
	if (replen)
		memcpy(&str->str[pos], repstr, replen);

	/* Place 0-terminator as needed, and reset hash to uncalculated */
	str->size = len;
	str->str[len] = '\0';
	str->flags1 = 0;
}

} // extern "C"
} // namespace avm


