/** Manages memory and garbage collection
 * @file
 *
 * The mem_ macros and functions provide memory allocation, resizing,
 * and garbage collection needed by all variable sized values.
 *
 * Warning, Warning, Will Robinson!!
 * Allocated new objects are free radicals, with a political valence so unstable 
 * that they are easily led astray by the Dread Pirate Roberts (no, not Ross: the sneaky Garbage Collector).
 * To protect any new objects you create, store them immediately in a safe container such as the stack or global.
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef Aintory_h
#define Aintory_h

/** C99 include file for 32-bit integer definitions */
#include <stdint.h>

#include "avm/avm_value.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/** An unsigned index into an object, whether integer or hash 
 * We save some object space, at the not unreasonable cost that
 * indexing into an object is capped at a few billion. */
typedef uint32_t AuintIdx;

/** A signed index into an object, whether integer or hash */
typedef int32_t AintIdx;

/** The type for a byte */
typedef unsigned char AByte;

/** The common header fields for any variable-sized pointer Value. 
 * We want to conserve space given the way C compilers automatically align data.
 * The following structure should be economical for both 32-bit and 64-bit
 * machines (resolving to 12 bytes for 32-bit and 16 for 64-bits). 
 * However, sometimes there will be some wasted space as some data structures
 * will not use the flags1-2 fields. Most will use the size field, but not all.
 *
 * Because the size field is forced 32-bit, it means no individual object
 * can address higher than 4 billion, which should rarely (if ever) be an impediment.
*/
#define MemCommonInfo \
	struct MemInfo *next;  /**< Pointer to next memory block in chain */ \
	AByte enctyp;    /**< Encoding type (see EncType) */ \
	AByte marked;    /**< Garbage collection flags */ \
	AByte flags1;	/**< Encoding-specific flags */ \
	AByte flags2;	/**< Encoding-specific flags */ \
	AuintIdx size	/**< Encoding-specific sizing info */

/** The common header fields for any Value containing other Values (will be marked Gray) */
#define MemCommonInfoGray \
	MemCommonInfo; \
	MemInfoGray* graylink;

/** The common header fields for any Typed variable-sized pointer Value (see MemCommonInfo). */
#define MemCommonInfoT \
	MemCommonInfoGray; \
	Value type		/**< Specifies data structure's Type (for methods) */

/** The header structure for any variable-sized Value (see MemCommonInfo) */
typedef struct MemInfo {
	MemCommonInfo;	//!< Common header
} MemInfo;

/** The generic structure for any Value containing other Values (will be marked Gray) */
typedef struct MemInfoGray {
	MemCommonInfoGray;	//!< Common header for value-containing objects
} MemInfoGray;

/** The generic structure for all typed variable-sized Value */
typedef struct MemInfoT {
	MemCommonInfoT;		//!< Common header for typed Values
} MemInfoT;

/** Various types of Value encodings, some stored in pointer Values and
 * all used as an index into the VM's default encodings-to-types map */
enum EncType {
	/* default typed that allocate memory */
	SymEnc,		//!< Symbol
	ThrEnc,		//!< Thread
	VmEnc,		//!< Virtual Machine
	MethEnc,	//!< C or Bytecode method

	/* Internal for Acorn compiler */
	LexEnc,		//!< Lexer context
	CompEnc,	//!< Compiler context for a method

	/* after this, all encodings use typed info header */
	StrEnc,		//!< String (bytes)
	ArrEnc,		//!< Array
	TblEnc,		//!< Table
	PartEnc,	//!< Part

	/* Immediate value encoding types (not Value Pointers) */
	NullEnc,	//!< Null
	BoolEnc,	//!< Boolean: true or false
	IntEnc,		//!< Integer
	FloatEnc,	//!< Floating point

	NbrEnc  /**< The number of encodings */
};
#define TypedEnc StrEnc /**< Below this, encodings are self-typed */
#define NbrPEnc NullEnc /**< Number of pointer encodings */

/** Is value a pointer to the encoding data structure? */
#define isEnc(val, enc) (isPtr(val) && ((MemInfo*) val)->enctyp==enc)

#define ValLocked 0x80	//!< On if value is locked

/* ************************************
   Garbage Collection
   ********************************* */

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
#define WHITEBITS	bit2mask(WHITE0BIT, WHITE1BIT)	//!< Both white colors together

#define iswhite(x)      testbits((x)->marked, WHITEBITS) //!< Return true if object is white
#define isblack(x)      testbit((x)->marked, BLACKBIT) //!< Return true if object is black
#define isfinalized(x)	testbit((x)->marked, FINALIZEDBIT) //!< Return true if object's type has a finalizer

#define resetoldbit(o)	resetbit((o)->marked, OLDBIT) //!< Reset the old bit to zero

/** Initialize memory and garbage collection for VM */
void mem_init(struct VmInfo* vm);

/** Create a new variable-sized object (with given encoding and size) and add to front of *list. */
/**
 * \param th the current thread
 * \param enc the encoding of the new object
 * \param sz number of bytes to allocate
 * \param list forward-link chain to push allocated object onto
 * \param offset how many bytes to allocate before the object itself (used only by states).
 */
MemInfo *mem_new(Value th, int enc, Auint sz, MemInfo **list, int offset);

/** Garbage-collection savvy memory malloc, free and realloc function
 * - If nsize==0, it frees the memory block (if non-NULL)
 * - If ptr==NULL, it allocates a new uninitialized memory block
 * - Otherwise it changes the size of the memory block (and may move its location)
 * It returns the location of the new block or NULL (if freed). */
Value mem_gcrealloc(Value th, void *block, Auint osize, Auint nsize);

/** Allocate or resize array memory */
#define mem_reallocvector(th,v,oldn,n,t) \
   ((v)=(t *) mem_gcreallocv(th, v, oldn, n, sizeof(t)))

/** Garbage-collection savvy vector memory malloc, free and realloc function
 * - esize is the size of each entry in the vector
 * - If nsize==0, it frees the memory block (if non-NULL)
 * - If ptr==NULL, it allocates a new uninitialized memory block
 * - Otherwise it changes the size of the memory block (and may move its location)
 * It returns the location of the new block or NULL (if freed). */
void* mem_gcreallocv(Value th, void *block, Auint osize, Auint nsize, Auint esize);

/** Grow allocated area by at least one more */
#define mem_growvector(th,area,nelems,size,t,limit) \
          if ((nelems)+1 > (size)) \
            ((area)=(t*) (mem_growaux_(th,area,&(size),sizeof(t),limit)))

/** Double size of vector area, up to limits */
void *mem_growaux_(Value th, void *block, AuintIdx *size, AuintIdx size_elems,
                     AuintIdx limit);

/** General-purpose memory malloc, free and realloc function.
 * - If size==0, it frees the memory block (if non-NULL)
 * - If block==NULL, it allocates a new uninitialized memory block
 * - Otherwise it changes the size of the memory block (and may move its location)
 * It returns the location of the new block or NULL (if freed). */
void *mem_frealloc(void *block, Auint size);

/** Free allocated memory block based on a structure */
#define mem_free(th, b)		mem_gcrealloc(th, (b), sizeof(*(b)), 0)

/** Free allocated memory block, given its old size */
#define mem_freemem(th, b, s)	mem_gcrealloc(th, (b), (s), 0)

/** Free allocated memory block based on array structure */
#define mem_freearray(th, b, n)   mem_gcreallocv(th, (b), n, 0, sizeof((b)[0]))


/** Confirm it is a white object, then mark it black/gray */
#define mem_markobj(th, obj) \
	if (isPtr(obj) && iswhite((MemInfo*)obj)) \
		mem_markobjraw(th, (MemInfo*)obj);

/** Fix a value's color mark when placing it within another value.
 * During incremental GC marking, if we add a white object to an object
 * already marked black, then add the white object to the gray list. */
#define mem_markChk(th, parent, val) \
	if (isPtr(val) && iswhite((MemInfo*)val) && isblack((MemInfo*)parent) /* && !isdead(th, (MemInfo*)parent)*/) \
		mem_markobjraw(th, (MemInfo*)val);

/** Mark a current white object to black or gray (use mem_markobj if unsure whether obj is white).
 * Black is chosen for simple objects without embedded Values, updating gcmemtrav.
 * Gray is chosen for collections with embedded Values, and added to a gray chain for later marking
 * by mem_marktopgray. */
void mem_markobjraw(Value th, MemInfo *mem);

/** Keep value (symbol) alive, if dead but not yet collected */
void mem_keepalive(Value Thread, MemInfo* blk);

/** Before allocating more memory, do a GC step if done with pause */
#define mem_gccheck(th) \
	{if (vm(th)->gcdebt >= 0) \
	mem_gcstep(th);}

/** Free all allocated objects, ahead of VM shut-down */
void mem_freeAll(Value th);

/** Perform a step's worth of garbage collection. */
void mem_gcstep(Value th);

/** Perform a full garbage collection cycle.
 * It will be automatically called in emergency mode if memory fills up.
 * If emergency, do not call finalizers, which could change stack positions
 */
void mem_gcfull(Value th, int isemergency);


#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif