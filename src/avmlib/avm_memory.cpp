/* Memory allocation and garbage collection
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

/** Keep value alive, if dead but not yet collected */
void mem_keepalive(Value Thread, MemInfo* blk) {
	//if (isdead(sym))  // symbol is dead (but was not collected yet)?
		//changewhite(sym);  // resurrect it
}

/** Initialize the global state for objects, mostly for garbage collection */
void mem_init(struct VmInfo* vm) {
	vm->gcdebt = 0;
	vm->gcpause = AVM_GCPAUSE;
	vm->gcmajorinc = AVM_GCMAJOR;
	vm->gcstepmul = AVM_GCMUL;

	//vm->gcmode = KGC_GEN;
	//vm->gcstate = GCSpause;
	vm->currentwhite = bit2mask(WHITE0BIT, FIXEDBIT);

	vm->objlist = vm->rootobj = NULL;
}

void mem_gcfull(int flag) {
}

/** Garbage-collection savvy memory malloc, free and realloc function
 * - If nsize==0, it frees the memory block (if non-NULL)
 * - If ptr==NULL, it allocates a new uninitialized memory block
 * - Otherwise it changes the size of the memory block (and may move its location)
 * It returns the location of the new block or NULL (if freed). */
void *mem_gcrealloc(struct VmInfo* vm, void *block, Auint osize, Auint nsize) {
	Value newblock;

	// Check consistency of block and osize (both must be null or specified)
	Auint realosize = (block) ? osize : 0;
	assert((realosize == 0) == (block == NULL));

#if defined(AVM_GCHARDMEMTEST)
	if (nsize > realosize)
		mem_gcfull(1);  /* force a GC whenever possible */
#endif

	// Allocate/free/resize the memory block
	newblock = (Value) mem_frealloc(block, nsize);

	// If alloc or resize failed, compact memory and try again
	if (newblock == NULL && nsize > 0) {
		// realloc cannot fail when shrinking a block
		mem_gcfull(1);  // try to free some memory...
		newblock = (Value) mem_frealloc(block, nsize);  // try again
		if (newblock == NULL)
			vm_outofmemory();
	}

	// Make sure it worked, adjust GC debt and return address of new block
	assert((nsize == 0) == (newblock == NULL));
	vm->gcdebt += nsize - realosize;
	return newblock;
}

void* mem_gcreallocv(VmInfo* vm, void *block, Auint osize, Auint nsize, Auint esize) {
	// Ensure we are not asking for more memory than available in address space
	// If we do not do this, calculating the needed memory will overflow
	if (nsize+1 > ~((Auint)0)/esize)
		vm_outofmemory();
	return mem_gcrealloc(vm, block, osize*esize, nsize*esize);
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
	else
		return realloc(block, size);
}

/** Create a new variable-sized object (with given encoding and size) and add to front of *list. */
/**
 * \param enc the encoding of the new object
 * \param sz number of bytes to allocate
 * \param list forward-link chain to push allocated object onto
 * \param offset how many bytes to allocate before the object itself (used only by states).
 */
MemInfo *mem_new(VmInfo* vm, int enc, Auint sz, MemInfo **list, int offset) {
	MemInfo *o = (MemInfo*) (offset + (char *) mem_gcrealloc(vm, NULL, 0, sz));
	if (list == NULL)
		list = &vm->objlist;  // standard list for collectable objects
	o->marked = vm->currentwhite & WHITEBITS;
	o->enctyp = enc;
	o->next = *list;
	*list = o;
	return o;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
