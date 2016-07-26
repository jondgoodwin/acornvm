/** Implements arrays: variable-sized, ordered collections of Values (see avm_array.h)
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

/* Return a new Array, allocating len slots for Values. */
Value newArr(Value th, Value *dest, Value type, AuintIdx len) {
	ArrInfo *val;
	mem_gccheck(th);	// Incremental GC before memory allocation events

	// Create an array object
	MemInfo **linkp = NULL;
	val = (ArrInfo *) mem_new(th, ArrEnc, sizeof(ArrInfo), linkp, 0);
	val->avail = len;
	val->size = 0;
	val->arr = NULL;
	if (len>0)
		mem_reallocvector(th, val->arr, 0, len, Value);
	val->flags1 = 0;	// Initialize Flags1 flags
	val->type = type;
	return *dest = (Value) val;
}

/* Return a new Array, allocating len slots for Values. */
Value newClosure(Value th, Value *dest, Value type, AuintIdx len) {
	ArrInfo *val;
	mem_gccheck(th);	// Incremental GC before memory allocation events

	// Create an array object
	MemInfo **linkp = NULL;
	val = (ArrInfo *) mem_new(th, ArrEnc, sizeof(ArrInfo), linkp, 0);
	val->avail = len;
	val->size = 0;
	val->arr = NULL;
	if (len>0)
		mem_reallocvector(th, val->arr, 0, len, Value);
	val->flags1 = TypeClo;	// Initialize Flags1 flags
	val->type = type;
	return *dest = (Value) val;
}

/* Return 1 if the value is an Array, otherwise 0 */
int isArr(Value val) {
	return isEnc(val, ArrEnc);
}

/* Return 1 if the value is an Array, otherwise 0 */
int isClosure(Value val) {
	return isEnc(val, ArrEnc) && arr_info(val)->flags1&TypeClo;
}

/* Ensure array has room for len Values, allocating memory as needed.
 * Allocated space will not shrink. Changes nothing about array's contents. */
void arrMakeRoom(Value th, Value arr, AuintIdx len) {
	ArrInfo *a = arr_info(arr);

	if (len>a->avail) {
		mem_gccheck(th);	// Incremental GC before memory allocation events
		mem_reallocvector(th, a->arr, a->avail, len, Value);
		a->avail = len;
	}
}

/* Force allocated and used array to a specified size, truncating 
 * or expanding as needed. Growth space is initialized to aNull. */
void arrForceSize(Value th, Value val, AuintIdx len) {
	ArrInfo *arr = arr_info(val);
	AuintIdx i;
	assert(isArr(val));

	// Expand or contract allocation, as needed
	if (len != arr->avail) {
		mem_gccheck(th);	// Incremental GC before memory allocation events
		mem_reallocvector(th, arr->arr, 0, len, Value);
		arr->avail = len;
	}

	// Fill growth area with nulls
	for (i=arr->size; i<len; i++)
		arr->arr[i] = aNull;
	arr->size = len;
}

/* Retrieve the value in array at specified position. */
Value arrGet(Value th, Value arr, AuintIdx pos) {
	ArrInfo *a = arr_info(arr);
	assert(isArr(arr));
	return (pos > a->size)? aNull : a->arr[pos];
}

/* Put val into the array starting at pos.
 * This can expand the size of the array.*/
void arrSet(Value th, Value arr, AuintIdx pos, Value val) {
	ArrInfo *a = arr_info(arr);
	AuintIdx i;
	assert(isArr(arr));

	// Grow, if needed
	if (pos+1>a->avail)
		arrMakeRoom(th, arr, pos+1);
	// Fill with nulls if pos starts after end of array
	if (pos > a->size)
		for (i=a->size; i<pos; i++)
			a->arr[i]=aNull;
	// Perform copy
	a->arr[pos]=val;
	mem_markChk(th, arr, val);
	// If final fill is past array size, reset size higher
	if (pos+1 > a->size)
		a->size = pos+1;
}

/* Append val to the end of the array (increasing array's size). */
void arrAdd(Value th, Value arr, Value val) {
	ArrInfo *a = arr_info(arr);
	AuintIdx sz = arr_size(arr);
	assert(isArr(arr));

	// Grow, if needed
	if (sz+1>a->avail)
		arrMakeRoom(th, arr, sz+1);

	// Append value
	a->arr[sz]=val;
	mem_markChk(th, arr, val);
	a->size++;
}

/* Propagate n copies of val into the array starting at pos.
 * This can expand the size of the array.*/
void arrRpt(Value th, Value arr, AuintIdx pos, AuintIdx n, Value val) {
	ArrInfo *a = arr_info(arr);
	AuintIdx i;
	assert(isArr(arr));

	// Prevent unlikely overflow
	if (pos+n<n)
		return;
	// Grow, if needed
	if (pos+n>a->avail)
		arrMakeRoom(th, arr, pos+n);
	// Fill with nulls if pos starts after end of array
	if (pos > a->size)
		for (i=a->size; i<pos; i++)
			a->arr[i]=aNull;
	// Perform repeat copy
	for (i=pos; i<pos+n; i++)
		a->arr[i]=val;
	mem_markChk(th, arr, val); // only need to check once
	// If final fill is past array size, reset size higher
	if (pos+n > a->size)
		a->size = pos+n;
}

/* Delete n values out of the array starting at pos. 
 * All values after these are preserved, essentially shrinking the array. */
void arrDel(Value th, Value arr, AuintIdx pos, AuintIdx n) {
	ArrInfo *a = arr_info(arr);
	assert(isArr(arr));

	// Nothing to delete (or overflow)
	if (pos>=a->size || pos+n<n)
		return;

	// Copy high end down over deleted portion
	if (pos+n < a->size)
		memmove(&a->arr[pos], &a->arr[pos+n], (a->size-pos-n)*sizeof(Value));
	else
		n = a->size - pos;  // Clip n to end of array, if too large
	a->size -= n; // Adjust size accordingly
}

/* Insert n copies of val into the array starting at pos, expanding the array's size. */
void arrIns(Value th, Value arr, AuintIdx pos, AuintIdx n, Value val) {
	ArrInfo *a = arr_info(arr);
	assert(isArr(arr));

	// Prevent unlikely overflow
	if (a->size+n<n)
		return;

	// Ensure array is large enough
	if (n+a->size > a->avail)
		arrMakeRoom(th, arr, n+a->size);

	// Move values up to make room for insertions
	if (pos<a->size)
		memmove(&a->arr[pos+n], &a->arr[pos], (a->size-pos)*sizeof(Value));
	a->size += n;

	// Do any needed null fill plus the repeat copy
	arrRpt(th, arr, pos, n, val);
}

/* Copy n2 values from arr2 starting at pos2 into array, replacing the n values in first array starting at pos.
 * This can increase or decrease the size of the array. arr and arr2 may be the same array. */
void arrSub(Value th, Value arr, AuintIdx pos, AuintIdx n, Value arr2, AuintIdx pos2, AuintIdx n2) {
	ArrInfo *a = arr_info(arr);
	AuintIdx i;
	assert(isArr(arr));

	// Prevent unlikely overflow
	if (a->size-n+n2 < n2)
		return;

	// Ensure array is large enough
	if (a->size-n+n2 > a->avail)
		arrMakeRoom(th, arr, a->size-n+n2);

	// Adjust position of upper values to make precise space for copy
	if (n!=n2 && pos<a->size)
		memmove(&a->arr[pos+n2], &a->arr[pos+n], (a->size-pos-n)*sizeof(Value));

	// Fill with nulls if pos starts after end of array
	if (pos > a->size)
		for (i=a->size; i<pos; i++)
			a->arr[i]=aNull;

	// Perform copy
	if (arr2 && isPtr(arr2))
		memmove(&a->arr[pos], &arr_info(arr2)->arr[pos2], n2*sizeof(Value));
	for (AintIdx i=n2-1; i>=0; i--) {
		mem_markChk(th, arr, a->arr[pos+i]);
	}

	a->size += n2-n;
}

/* Serialize an array's contents to indented text */
void arrSerialize(Value th, Value str, int indent, Value arr) {
	ArrInfo *a = arr_info(arr);
	AuintIdx sz = arr_size(arr);
	int ind;

	strAppend(th, str, "+List", 5);
	for (AuintIdx i=0; i<sz; i++) {
		strAppend(th, str, "\n", 1);
		ind = indent+1;
		while (ind--)
			strAppend(th, str, "\t", 1);
		serialize(th, str, indent+1, a->arr[i]);
	}
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

