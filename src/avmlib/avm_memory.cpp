/** Memory allocation and garbage collection
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

/** Garbage-collection savvy memory malloc, free and realloc function
 * - If nsize==0, it frees the memory block (if non-NULL)
 * - If ptr==NULL, it allocates a new uninitialized memory block
 * - Otherwise it changes the size of the memory block (and may move its location)
 * It returns the location of the new block or NULL (if freed). */
void *mem_gcrealloc(Value th, void *block, Auint osize, Auint nsize) {
	Value newblock;

	// Check consistency of block and osize (both must be null or specified)
	Auint realosize = (block) ? osize : 0;
	assert((realosize == 0) == (block == NULL));

	// Allocate/free/resize the memory block
	newblock = (Value) mem_frealloc(block, nsize);

#ifdef MEMORYLOG
	if (nsize==0)
		vmLog("Freeing %p", block);
	else
		vmLog("Allocating %p from %p for %d", newblock, block, nsize);
#endif

	// If alloc or resize failed, compact memory and try again
	if (newblock == NULL && nsize > 0) {
		// realloc cannot fail when shrinking a block
		mem_gcfull(th, 1);  // try to free some memory...
		newblock = (Value) mem_frealloc(block, nsize);  // try again
		if (newblock == NULL)
			logSevere("Out of memory trying allocate or grow a memory block.");
	}

	// Make sure it worked, adjust GC debt and return address of new block
	assert((nsize == 0) == (newblock == NULL));
	vm(th)->gcdebt += nsize - realosize;
	return newblock;
}

void* mem_gcreallocv(Value th, void *block, Auint osize, Auint nsize, Auint esize) {
	// Ensure we are not asking for more memory than available in address space
	// If we do not do this, calculating the needed memory will overflow
	if (nsize+1 > ~((Auint)0)/esize)
		logSevere("Out of memory trying to ask for more memory than address space has.");
	return mem_gcrealloc(th, block, osize*esize, nsize*esize);
}

/** General-purpose memory malloc, free and realloc function.
 * - If size==0, it frees the memory block (if non-NULL)
 * - If block==NULL, it allocates a new uninitialized memory block
 * - Otherwise it changes the size of the memory block (and may move its location)
 * It returns the location of the new block or NULL (if freed). */
void *mem_frealloc(void *block, Auint size) {
	if (size == 0) {
		free(block);
		return NULL;
	}
	else {
		return realloc(block, size);
	}
}

/** Create a new variable-sized object (with given encoding and size) and add to front of *list. */
/**
 * \param th the current thread
 * \param enc the encoding of the new object
 * \param sz number of bytes to allocate
 * \param list forward-link chain to push allocated object onto
 * \param offset how many bytes to allocate before the object itself (used only by states).
 */
MemInfo *mem_new(Value th, int enc, Auint sz, MemInfo **list, int offset) {
#if defined(AVM_GCHARDMEMTEST)
	if (vm(th)->gcrunning)
		mem_gcfull(th, 1);  /* force a full GC to see if any unattached objects die */
#endif

	MemInfo *o = (MemInfo*) (offset + (char *) mem_gcrealloc(th, NULL, 0, sz));
	if (list == NULL)
		list = &vm(th)->objlist;  // standard list for collectable objects
	o->marked = vm(th)->currentwhite & WHITEBITS;
	o->enctyp = enc;
	o->next = *list;
	*list = o;
	return o;
}

/* double size of vector array, up to limits */
void *mem_growaux_(Value th, void *block, AuintIdx *size, AuintIdx size_elems,
                     AuintIdx limit) {
	void *newblock;
	AuintIdx newsize;
	if (*size >= limit/2) {  /* cannot double it? */
		if (*size >= limit)  /* cannot grow even a little? */
			logSevere("Out of memory trying to grow a vector array.");
		newsize = limit;  /* still have at least one free place */
	}
	else {
		newsize = (*size)*2;
		if (newsize < MINSIZEARRAY)
			newsize = MINSIZEARRAY;  /* minimum size */
	}
	newblock = mem_gcreallocv(th, block, *size, newsize, size_elems);
	*size = newsize;  /* update only when everything else is OK */
	return newblock;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
