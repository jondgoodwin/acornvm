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
Value newStr(Value th, Value *dest, Value type, const char *str, AuintIdx len) {
	StrInfo *val;
	mem_gccheck(th);	// Incremental GC before memory allocation events

	// Create a string object
	unsigned int extrahdr = 0; // No extra header for strings
	MemInfo **linkp = NULL;
	val = (StrInfo *) mem_new(th, StrEnc, sizeof(StrInfo)+extrahdr, linkp, 0);
	val->flags1 = 0;
	val->flags2 = 0;
	val->type = type;

	val->avail = len;
	val->str = (char*) mem_gcrealloc(th, NULL, 0, len+1); // an extra byte for 0-terminator
	// Copy string's contents over, if provided.
	if (str) {
		memcpy(val->str, str, len);
		val->size = len;
	}
	// If not provided, create null string
	else {
		val->str[0] = '\0'; // just in case
		val->size = 0;
	}
	val->str[len] = '\0'; // put guaranteed 0-terminator, just in case
	return *dest = (Value) val;
}

/* Return a string value containing C-data for C-methods.
   Its type may have a _finalizer, called just before the GC frees the C-Data value. */
Value newCData(Value th, Value *dest, Value type, AuintIdx len, unsigned int extrahdr) {
	StrInfo *val;
	mem_gccheck(th);	// Incremental GC before memory allocation events

	// Create a string object
	MemInfo **linkp = NULL;
	// we only have four bits to represent size of extrahdr (in multiples of four)
	extrahdr = extrahdr>=60? 60 : (extrahdr&3)? (extrahdr&StrExtraHdrMask)+4 : extrahdr;
	val = (StrInfo *) mem_new(th, StrEnc, sizeof(StrInfo) + extrahdr, linkp, 0);
	val->flags1 = StrCDataFlg | extrahdr;
	val->flags2 = 0;
	val->type = type;

	val->avail = val->size = len;
	if (len>0) {
		val->str = (char*) mem_gcrealloc(th, NULL, 0, len+1); // an extra byte for 0-terminator
		val->str[0] = val->str[len] = '\0'; // put in guaranteed 0-terminators, just in case
	}
	else
		val->str = NULL;
	return *dest = (Value) val;
}

/* Return a string value containing room for identically structured numbers. */
Value newNumbers(Value th, Value *dest, Value type, AuintIdx nStructs, unsigned int nVals, unsigned int valSz, unsigned int extrahdr) {
	StrInfo *val;
	mem_gccheck(th);	// Incremental GC before memory allocation events

	// Create a string object metadata header (StrInfo)
	MemInfo **linkp = NULL;
	// we only have four bits to represent size of extrahdr (in multiples of four)
	extrahdr = extrahdr>=60? 60 : (extrahdr&3)? (extrahdr&StrExtraHdrMask)+4 : extrahdr;
	val = (StrInfo *) mem_new(th, StrEnc, sizeof(StrInfo) + extrahdr, linkp, 0);
	val->type = type;
	val->flags1 = extrahdr;

	// Store information about number organization, hardcoding into the flags
	if (valSz>4)		{ valSz=8; val->flags2=0x30; } // StrValByteSzMask 0x30
	else if (valSz>2)	{ valSz=4; val->flags2=0x20; }
	else if (valSz>1)	{ valSz=2; val->flags2=0x10; }
	else				{ valSz=1; val->flags2=0x00; }
	if (nVals>16) nVals=16;
	else if (nVals==0) nVals=1;
	val->flags2 |= (nVals-1); // StrStructSzMask 0x0F
	AuintIdx len = nStructs * nVals * valSz;

	// Allocate block for number values
	val->avail = len;
	val->size = (nStructs==1)? len : 0; // Assume it is full if only one structure, but empty if a collection
	val->str = (char*) mem_gcrealloc(th, NULL, 0, len+1); // an extra byte for 0-terminator
	val->str[0] = val->str[len] = '\0'; // put guaranteed 0-terminators, just in case
	return *dest = (Value) val;
}

/* Return 1 if the value is a String, otherwise 0 */
int isStr(Value str) {
	return isEnc(str, StrEnc);
}

/* Return a pointer to the small header extension allocated by a CData or Numbers creation */
const void *toHeader(Value str) {
	assert(isStr(str));
	return (void*) (str_info(str)+1);
}

/* Return size of every number in an Numbers block */
AuintIdx getValSz(Value str) {
	if (!isStr(str))
		return 1;
	switch ((((StrInfo*)str)->flags2 & StrValByteSzMask)>>4) {
	case 0: return 1;
	case 1: return 2;
	case 2: return 4;
	case 3: return 8;
	}
	return 1;
}

/* Return number of values in a Numbers block structure */
AuintIdx getNVals(Value str) {
	return isStr(str)? (((StrInfo*)str)->flags2 & StrStructSzMask)+1 : 1;
}

/* Return number of structures in a Numbers block */
AuintIdx getNStructs(Value str) {
	return isStr(str)? ((StrInfo*)str)->size/getNVals(str)/getValSz(str) : 0;
}

/* Ensure string has room for len Values, allocating memory as needed.
 * Allocated space will not shrink. Changes nothing about string's contents. */
void strMakeRoom(Value th, Value val, AuintIdx len) {
	StrInfo *str = str_info(val);

	/* Expand available space, if needed */
	if (len > str->avail) {
		mem_gccheck(th);	// Incremental GC before memory allocation events
		str->str = (char*) mem_gcrealloc(th, str->str, str->avail+1, len+1);
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

	/* Adjust size and place 0-terminator */
	str->size = len;
	str->str[len] = '\0';
}

/*	Append characters to the end of a string. */
void strAppend(Value th, Value val, const char *addstr, AuintIdx addstrlen) {
	StrInfo* str = str_info(val);
	AuintIdx newlen = str->size + addstrlen;

	/* Return if nothing to add, or resulting string is greater than possible */
	if (addstr==NULL || addstrlen==0 || newlen<str->size) 
		return;

	/* Expand available space, if needed */
	if (newlen > str->avail) {
		AuintIdx newavail = str->avail+str->avail; // try doubling
		if (newavail < str->avail || newavail<newlen+1)
			newavail = newlen+1;	// Ask for what we need
		strMakeRoom(th, val, newavail);
	}

	// Append addstr
	memcpy(&str->str[str->size], addstr, addstrlen);

	/* Adjust size and place 0-terminator */
	str->size = newlen;
	str->str[newlen] = '\0';
}

/* Return a read-only pointer into a C-string encoded by a symbol or string-oriented Value. 
 * It is guaranteed to have a 0-terminating character just after its full length. 
 * Anything other value type returns NULL.
 */
const char* toStr(Value val) {
	if (isSym(val))
		return (const char*) sym_cstr(val);
	if (isStr(val))
		return str_cstr(val);
	return 0;
}

/* Return 1 if the symbol or string value's characters match the zero-terminated c-string, otherwise 0. */
int isEqStr(Value val, const char* str) {
	if (isSym(val))
		return sym_size(val)==strlen(str) && !strcmp(sym_cstr(val), str);
	if (isStr(val))
		return str_size(val)==strlen(str) && !strcmp(str_cstr(val), str);
	return 0;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

