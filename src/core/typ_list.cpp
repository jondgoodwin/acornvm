/** List type methods and properties
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

/** Create a new List. Parameters fill the List */
int list_new(Value th) {
	Value traits = pushProperty(th, 0, "traits"); popValue(th);
	if (getTop(th)==1)
		pushArray(th, traits, 4);
	else if (getTop(th)==2 && isInt(getLocal(th, 1))) {
		pushArray(th, traits, toAint(getLocal(th, 1)));
	}
	else {
		int arrsz = getTop(th)-1;
		Value arr = pushArray(th, traits, arrsz);
		int idx = 0;
		while (arrsz--) {
			arrSet(th, arr, idx, getLocal(th, idx+1));
			idx++;
		}
	}
	return 1;
}

/** Return true if list has no elements */
int list_isempty(Value th) {
	pushValue(th, arr_size(getLocal(th, 0))==0? aTrue : aFalse);
	return 1;
}

/** Append to a List */
int list_append(Value th) {
	Value arr = getLocal(th,0);
	AuintIdx idx = 1;
	while (idx < getTop(th))
		arrAdd(th, arr, getLocal(th,idx++));
	setTop(th, 1);
	return 1;
}

/** Add to the beginning of a List */
int list_prepend(Value th) {
	Value arr = getLocal(th,0);
	AuintIdx idx = 1;
	while (idx < getTop(th))
		arrIns(th, arr, 0, 1, getLocal(th, idx++));
	setTop(th, 1);
	return 1;
}

/** Insert array element(s) at specified integer position */
int list_insert(Value th) {
	if (getTop(th)<2 || !isInt(getLocal(th, 1)))
		return 0;
	AuintIdx pos = toAint(getLocal(th,1));
	Value arr = getLocal(th,0);
	for (AuintIdx idx = 2; idx < getTop(th); idx++)
		arrIns(th, arr, pos+idx-2, 1, getLocal(th, idx));
	setTop(th, 1);
	return 1;
}

/** Remove and return a value from the end of the List */
int list_pop(Value th) {
	Value arr = getLocal(th,0);
	AintIdx size;
	if ((size = arr_size(arr))==0)
		return 0;
	pushValue(th, arrGet(th, arr, --size));
	arrSetSize(th, arr, size);
	return 1;
}

/** Remove and return a value from the start of the List */
int list_shift(Value th) {
	Value arr = getLocal(th,0);
	if (arr_size(arr)==0)
		return 0;
	pushValue(th, arrGet(th, arr, 0));
	arrDel(th, arr, 0, 1);
	return 1;
}

/** Get array element at specified integer position */
int list_get(Value th) {
	if (getTop(th)<2 || !isInt(getLocal(th, 1)))
		return 0;
	AintIdx idx = toAint(getLocal(th,1));
	if (idx<0)
		idx += arr_size(getLocal(th,0));
	pushValue(th, arrGet(th, getLocal(th,0), idx));
	return 1;
}

/** Set array element at specified integer position */
int list_set(Value th) {
	if (getTop(th)<3 || !isInt(getLocal(th, 2)))
		return 0;
	AintIdx idx = toAint(getLocal(th,2));
	if (idx<0)
		idx += arr_size(getLocal(th,0));
	arrSet(th, getLocal(th,0), idx, getLocal(th, 1));
	return 0;
}

/** Remove array element(s) from first specified position to second position */
int list_remove(Value th) {
	if (getTop(th)<2 || !isInt(getLocal(th, 1)))
		return 0;
	Value arr = getLocal(th, 0);
	AintIdx size = arr_size(arr);
	AintIdx pos = toAint(getLocal(th,1));
	if (pos<0) pos += size;
	AintIdx len = getTop(th)>2 && isInt(getLocal(th,2))? toAint(getLocal(th,2)) : 1;

	arrDel(th, arr, pos, len);
	setTop(th, 1);
	return 1;
}

/** Remove the specified value from list, wherever found */
int list_removeValue(Value th) {
	if (getTop(th)<2)
		return 0;
	// Scan list from end to start, looking for value
	Value arr = getLocal(th, 0);
	Value val = getLocal(th, 1);
	AintIdx size = arr_size(arr);
	for (AintIdx i=size-1; i>0; i--) {
		// Remove list's entry if exactly the same as val
		if (arrGet(th, arr, i)==val)
			arrDel(th, arr, i, 1);
	}
	setTop(th, 1);
	return 1;
}

/** Find the specified value in list */
int list_find(Value th) {
	if (getTop(th)<2)
		return 0;
	// Scan list from start to end, looking for value
	Value arr = getLocal(th, 0);
	Value val = getLocal(th, 1);
	AintIdx size = arr_size(arr);
	for (AintIdx i=0; i<size; i++) {
		// Return index for value if found
		if (arrGet(th, arr, i)==val) {
			pushValue(th, anInt(i));
			return 1;
		}
	}
	return 0;
}

/** Create and return new list containing a shallow copy of the elements from original list.
	One can specify the position (default 0) and length (to end of list) of the elements to copy from the original list. */
int list_clone(Value th) {
	Value arr = getLocal(th, 0);
	AintIdx size = arr_size(arr);
	AintIdx pos = getTop(th)>1 && isInt(getLocal(th,1))? toAint(getLocal(th,1)) : 0;
	if (pos<0) pos += size;
	AintIdx len = getTop(th)>2 && isInt(getLocal(th,2))? toAint(getLocal(th,2)) : size-pos+1;

	Value arr2 = pushArray(th, vmlit(TypeListm), toAint(len));
	arrSub(th, arr2, 0, len, arr, pos, len);
	return 1;
}

/** Copy specified elements from second list into self, replacing element's at specified positions */
int list_sub(Value th) {
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

/** Append second list's contents to end of self */
int list_merge(Value th) {
	if (getTop(th)<2 || !isArr(getLocal(th, 1)))
		return 0;
	Value arr = getLocal(th, 0);
	AintIdx size = arr_size(arr);
	Value arr2 = getLocal(th, 1);
	AintIdx size2 = arr_size(arr2);

	arrSub(th, arr, size, 0, arr2, 0, size2);
	return 1;
}

/** Fill elements of list to a specified value. The first value is the fill value.
	The starting position for the fill is the second parameter.
	Its default is 0. If negative, it is relative to the size of the list.
	The last parameter specifies the number of elements to fill. Unspecified, it fills to the end of the list. */
int list_fill(Value th) {
	Value arr = getLocal(th, 0);
	AintIdx size = arr_size(arr);
	Value fillvalue = getTop(th)>1? getLocal(th,1) : aNull;
	AintIdx pos = getTop(th)>2 && isInt(getLocal(th,2))? toAint(getLocal(th,2)) : 0;
	if (pos<0) pos += size;
	AintIdx len = getTop(th)>3 && isInt(getLocal(th,3))? toAint(getLocal(th,3)) : size-pos+1;
	arrRpt(th, arr, pos, len, fillvalue);
	return 1;
}

/** Return number of elements in list */
int list_getsize(Value th) {
	pushValue(th, anInt(getSize(getLocal(th, 0))));
	return 1;
}

/** Set size of list */
int list_setsize(Value th) {
	if (getTop(th)<2 || !isInt(getLocal(th, 1)))
		return 0;
	arrSetSize(th, getLocal(th, 0), toAint(getLocal(th, 1)));
	return 0;
}

/** Force size of list */
int list_forcesize(Value th) {
	if (getTop(th)<2 || !isInt(getLocal(th, 1)))
		return 0;
	arrForceSize(th, getLocal(th, 0), toAint(getLocal(th, 1)));
	return 0;
}

/* Reverse the order of elements */
int list_reverse(Value th) {
	Value arr = getLocal(th, 0);
	AuintIdx size = arr_size(arr);
	if (size>0) {
		AuintIdx limit = (size-1)>>1;
		for (AuintIdx i = size-1; i > limit; i--) {
			AuintIdx j = size-i-1;
			pushValue(th, arrGet(th, arr, i));
			arrSet(th, arr, i, arrGet(th, arr, j));
			arrSet(th, arr, j, getFromTop(th, 0));
			popValue(th);
		}
	}
	setTop(th,1);
	return 1;
}

/* Sort the list into a random order */
unsigned int int_boundrand(Value th, unsigned int bound);
int list_randomize(Value th) {
	Value arr = getLocal(th, 0);
	AuintIdx size = arr_size(arr);
	if (size>0) {
		for (AuintIdx i = size-1; i >= 1; i--) {
			AuintIdx j = int_boundrand(th, i);
			pushValue(th, arrGet(th, arr, i));
			arrSet(th, arr, i, arrGet(th, arr, j));
			arrSet(th, arr, j, getFromTop(th, 0));
			popValue(th);
		}
	}
	return 0;
}

/* Sort the list using passed method or '<=>' */
int list_sort(Value th) {
	ArrInfo* arr = arr_info(getLocal(th,0));
	Value compop = getTop(th)>1? getLocal(th,0) : vmlit(SymRocket);

	for (AuintIdx i = 1; i<arr_size(arr); i++) {
		Value newval = pushValue(th, arrGet(th, arr, i));
		// Binary search to determine where to insert
		AuintIdx low = 0;
		AuintIdx high = i-1;
		AuintIdx j;
		while (1) {
			// Compare midway between high and low
			j = low + ((high-low)>>1);
			pushValue(th, compop);
			pushValue(th, newval);
			pushValue(th, arrGet(th, arr, j));
			getCall(th, 2, 1);
			Value comp = popValue(th);

			if (comp==anInt(0)) {
				j++; // equal inserts right after
				break;
			}
			// newval < j's compare value
			if (comp==anInt(-1)) {
				if (j==low)
					break;
				high = j-1;
			}
			// newval > j's compare value (or uncomparable)
			else if (j++==high)
				break;
			else
				low = j;
		}
		if (j<i) {
			arrSub(th, arr, j+1, i-j, arr, j, i-j); // move up
			arrSet(th, arr, j, newval); // insert
		}
		popValue(th);
	}
	setTop(th, 1);
	return 1;
}

/** Closure iterator that retrieves List's next value */
int list_each_get(Value th) {
	Value self    = pushCloVar(th, 2); popValue(th);
	AuintIdx current = toAint(pushCloVar(th, 3)); popValue(th);
	if (current>=arr_size(self))
		return 0;
	pushValue(th, anInt(current + 1));
	popCloVar(th, 3);
	pushValue(th, anInt(current));
	pushValue(th, arrGet(th, self, current));
	return 2;
}

/** Return a get/set closure that iterates over the list */
int list_each(Value th) {
	Value self = pushLocal(th, 0);
	pushCMethod(th, list_each_get);
	pushValue(th, aNull);
	pushValue(th, self);
	pushValue(th, anInt(0));
	pushClosure(th, 4);
	return 1;
}

/** Initialize the List type */
void core_list_init(Value th) {
	vmlit(TypeListc) = pushType(th, vmlit(TypeObject), 4);
		pushSym(th, "List");
		popProperty(th, 0, "_name");
		vmlit(TypeListm) = pushMixin(th, vmlit(TypeObject), aNull, 32);
			pushSym(th, "*List");
			popProperty(th, 1, "_name");
			pushCMethod(th, list_isempty);
			popProperty(th, 1, "empty?");
			pushCMethod(th, list_get);
			pushCMethod(th, list_set);
			pushClosure(th, 2);
			popProperty(th, 1, "[]");
			pushCMethod(th, list_remove);
			popProperty(th, 1, "Remove");
			pushCMethod(th, list_removeValue);
			popProperty(th, 1, "RemoveValue");
			pushCMethod(th, list_find);
			popProperty(th, 1, "Find");
			pushCMethod(th, list_clone);
			popProperty(th, 1, "Clone");
			pushCMethod(th, list_sub);
			popProperty(th, 1, "Sub");
			pushCMethod(th, list_fill);
			popProperty(th, 1, "Fill");
			pushCMethod(th, list_getsize);
			pushCMethod(th, list_setsize);
			pushClosure(th, 2);
			popProperty(th, 1, "size");
			pushCMethod(th, list_forcesize);
			popProperty(th, 1, "Resize");
			pushCMethod(th, list_append);
			popProperty(th, 1, "<<");
			pushCMethod(th, list_prepend);
			popProperty(th, 1, ">>");
			pushCMethod(th, list_merge);
			popProperty(th, 1, "Merge");
			pushCMethod(th, list_insert);
			popProperty(th, 1, "Insert");
			pushCMethod(th, list_pop);
			popProperty(th, 1, "Pop");
			pushCMethod(th, list_shift);
			popProperty(th, 1, "Shift");
			pushCMethod(th, list_randomize);
			popProperty(th, 1, "Randomize");
			pushCMethod(th, list_reverse);
			popProperty(th, 1, "Reverse");
			pushCMethod(th, list_sort);
			popProperty(th, 1, "Sort");
			pushCMethod(th, list_each);
			popProperty(th, 1, "Each");
		popProperty(th, 0, "traits");
		pushCMethod(th, list_new);
		popProperty(th, 0, "New");
	popGloVar(th, "List");
	return;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
