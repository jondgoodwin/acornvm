/** Implements parts: hybrid collections of items, properties, and methods.
 *
 * Parts are comprenhensive collections, whose type is customizable:
 * - Ordered items in an Array
 * - Indexed properties in a Table
 * - Methods in a Table
 * - Types in a Mixin Array
 *
 * Types are a special sort of Part. A flag ensures a Type never uses its own methods
 * (they are intended for instances created by the Type).
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_part_h
#define avm_part_h

#include "avm/avm_memory.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

// ***********
// Part info declaration and access macros
// ***********

/** Information about a part. (uses MemCommonInfoT) */
typedef struct PartInfo {
	MemCommonInfoT;
	Value items;	//< An array of the part's parts
	Value props;	//< A table of the part's properties
	Value methods;	//< A table of the part's methods
	Value mixins;	//< An array of mixin Types
} PartInfo;

/** Mark all Part values for garbage collection 
 * Increments how much allocated memory the Part uses. */
#define partMark(th, p) \
	{mem_markobj(th, (p)->type); \
	mem_markobj(th, (p)->items); \
	mem_markobj(th, (p)->props); \
	mem_markobj(th, (p)->methods); \
	mem_markobj(th, (p)->mixins); \
	vm(th)->gcmemtrav += sizeof(PartInfo);}

/** Free all of an part's allocated memory */
#define partFree(th, p) \
	{mem_free(th, (p));}

/** Flags in flags1 */
#define PartType 0x80	//!< Flag to indicate whether Part is a Type

/** Point to part information, by recasting a Value pointer */
#define part_info(val) (assert_exp(isEnc(val,PartEnc), (PartInfo*) val))

/** Place a new table or Array collection into a part's Value member */
Value partSetColl(Value th, Value part, Value* mbr, Value coll);

/** Retrieves item array (creating if null) */
#define part_items(val) ((((PartInfo*) val)->items!=aNull)? (((PartInfo*) val)->items) : \
	partSetColl(th, val, &((PartInfo*) val)->items, newArr(th, 0)))

/** Retrieves properties table (creating if null) */
#define part_props(val) ((((PartInfo*) val)->props!=aNull)? (((PartInfo*) val)->props) : \
	partSetColl(th, val, &((PartInfo*) val)->props, newTbl(th, 0)))

/** Retrieves methods table (creating if null) */
#define part_methods(val) ((((PartInfo*) val)->methods!=aNull)? (((PartInfo*) val)->methods) : \
	partSetColl(th, val, &((PartInfo*) val)->methods, newTbl(th, 0)))

/** Retrieves mixins array (creating if null) */
#define part_mixins(val) ((((PartInfo*) val)->mixins!=aNull)? (((PartInfo*) val)->mixins) : \
	partSetColl(th, val, &((PartInfo*) val)->mixins, newArr(th, 0)))

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif