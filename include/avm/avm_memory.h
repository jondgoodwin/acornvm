/** Manages memory and garbage collection
 * @file
 *
 * The mem_ macros and functions provide memory allocation, resizing,
 * and garbage collection needed by all variable sized values.
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_memory_h
#define avm_memory_h

#include "avm/avm_value.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/** The type for a hash value */
typedef unsigned int AHash;

/** The common header fields for any variable-sized Value. */
#define MemCommonInfo \
	struct MemInfo *next;  /** Pointer to next memory block in chain */ \
	char enctyp;    /** Encoding type (see EncType) */ \
	char marked     /** Garbage collection flags */

/** The header structure for any variable-sized Value (see MemCommonInfo) */
typedef struct MemInfo {
  MemCommonInfo;
} MemInfo;

enum EncType {
	SymEnc,
	StrEnc,
	ArrEnc,
	HashEnc,
	ThrEnc
};

// Bit arithmetic macros
#define resetbits(x,m)		((x) &= ((char) ~(m))) //!< Turn off the m bits in x
#define setbits(x,m)		((x) |= (m)) //!< Turn on the m bits in x
#define testbits(x,m)		((x) & (m)) //!< Return true if any m bits are on in x
#define bitmask(b)		(1<<(b)) //!< Return bit flag for a bit position b
#define bit2mask(b1,b2)		(bitmask(b1) | bitmask(b2)) //!< Combine two bit flags
#define l_setbit(x,b)		setbits(x, bitmask(b)) //!< Turn on the bth bit in x
#define resetbit(x,b)		resetbits(x, bitmask(b)) //!< Turn off the bth bit in x
#define testbit(x,b)		testbits(x, bitmask(b)) //!< Return true if bth bit is on in x

/* Layout for bit use in `marked' field: */
#define WHITE0BIT	0  //!< object is white (type 0)
#define WHITE1BIT	1  //!< object is white (type 1)
#define BLACKBIT	2  //!< object is black
#define FINALIZEDBIT	3  //!< object has been separated for finalization
#define SEPARATED	4  //!< object is in 'finobj' list or in 'tobefnz'
#define FIXEDBIT	5  //!< object is fixed (should not be collected) 
#define OLDBIT		6  //!< object is old (only in generational mode)
// bit 7 is currently used by tests (checkmemory)
#define WHITEBITS	bit2mask(WHITE0BIT, WHITE1BIT)

#define otherwhite()	(mem_currentwhite ^ WHITEBITS)
#define isdeadm(ow,m)	(!(((m) ^ WHITEBITS) & (ow)))
#define isdead(v)	isdeadm(otherwhite(), (v)->marked)

#define resetoldbit(o)	resetbit((o)->marked, OLDBIT)

void mem_keepalive(Value Thread, MemInfo* blk);

/** Is value a pointer to the encoding data structure? */
AVM_API int isEnc(Value v, int enc);

/** Initialize memory and garbage collection for VM */
void mem_init(struct VmInfo* vm);

/** Create a new variable-sized object (with given encoding and size) and add to front of *list. */
/**
 * \param enc the encoding of the new object
 * \param sz number of bytes to allocate
 * \param list forward-link chain to push allocated object onto
 * \param offset how many bytes to allocate before the object itself (used only by states).
 */
MemInfo *mem_new(VmInfo *vm, int enc, Auint sz, MemInfo **list, int offset);

/** Garbage-collection savvy memory malloc, free and realloc function
 * - If nsize==0, it frees the memory block (if non-NULL)
 * - If ptr==NULL, it allocates a new uninitialized memory block
 * - Otherwise it changes the size of the memory block (and may move its location)
 * It returns the location of the new block or NULL (if freed). */
Value mem_gcrealloc(struct VmInfo *vm, void *block, Auint osize, Auint nsize);

/** Allocate or resize array memory */
#define mem_reallocvector(vm,v,oldn,n,t) \
   ((v)=(t *) mem_gcreallocv(vm, v, oldn, n, sizeof(t)))

/** Garbage-collection savvy vector memory malloc, free and realloc function
 * - esize is the size of each entry in the vector
 * - If nsize==0, it frees the memory block (if non-NULL)
 * - If ptr==NULL, it allocates a new uninitialized memory block
 * - Otherwise it changes the size of the memory block (and may move its location)
 * It returns the location of the new block or NULL (if freed). */
void* mem_gcreallocv(VmInfo* vm, void *block, Auint osize, Auint nsize, Auint esize);

/** General-purpose memory malloc, free and realloc function.
 * - If size==0, it frees the memory block (if non-NULL)
 * - If block==NULL, it allocates a new uninitialized memory block
 * - Otherwise it changes the size of the memory block (and may move its location)
 * It returns the location of the new block or NULL (if freed). */
void *mem_frealloc(void *block, Auint size);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif