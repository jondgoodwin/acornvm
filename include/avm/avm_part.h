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

/** Information about an part. (uses MemCommonInfoT) */
typedef struct PartInfo {
	MemCommonInfoT;
	Value items;	//< An array of the part's parts
	Value props;	//< A table of the part's properties
	Value methods;	//< A table of the part's methods
	Value mixins;	//< An array of mixin Types
} PartInfo;

/** Flags in flags1 */
#define PartType 0x80	//!< Flag to indicate whether Part is a Type

/** Point to part information, by recasting a Value pointer */
#define part_info(val) (assert_exp(isEnc(val,PartEnc), (PartInfo*) val))

/** Retrieves item array (creating if null) */
#define part_items(val) ((((PartInfo*) val)->items!=aNull)? (((PartInfo*) val)->items) : \
	(((PartInfo*) val)->items = newArr(th, 0)))

/** Retrieves properties table (creating if null) */
#define part_props(val) ((((PartInfo*) val)->props!=aNull)? (((PartInfo*) val)->props) : \
	(((PartInfo*) val)->props = newTbl(th, 0)))

/** Retrieves methods table (creating if null) */
#define part_methods(val) ((((PartInfo*) val)->methods!=aNull)? (((PartInfo*) val)->methods) : \
	(((PartInfo*) val)->methods = newTbl(th, 0)))

/** Add a method to a part (easy C macro) */
#define addMethod(th, part, methsym, meth, methnm) \
	partAddMethod(th, part, aSym(th, methsym), aCMethod(th, meth, methnm, __FILE__));

/** Retrieves mixins array (creating if null) */
#define part_mixins(val) ((((PartInfo*) val)->mixins!=aNull)? (((PartInfo*) val)->mixins) : \
	(((PartInfo*) val)->mixins = newArr(th, 0)))

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif