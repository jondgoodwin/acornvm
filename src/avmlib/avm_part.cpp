/** Implements parts and types: hybrid collections of items, properties, and methods.
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

/* Return a new Part. */
Value newPart(Value th, Value type) {
	PartInfo *part;
	mem_gccheck(th);	// Incremental GC before memory allocation events

	// Create an part object
	part = (PartInfo *) mem_new(th, PartEnc, sizeof(PartInfo), NULL, 0);
	part->flags1 = 0;
	part->type = type;
	part->items = aNull;
	part->props = aNull;
	part->methods = aNull;
	part->mixins = aNull;
	return (Value) part;
}

/* Return a new Type. */
Value newType(Value th, const char* typnm) {
	PartInfo *part;

	// Create and initialize Type object (which is a flagged Part)
	part = (PartInfo *) newPart(th, vm(th)->defEncTypes[TypeEnc]);
	part->flags1 = PartType;

	gloSet(th, aSym(th, typnm), part);	// Register Type as a global variable
	return (Value) part;
}

/* Return 1 if the value is an Part, otherwise 0 */
int isPart(Value val) {
	return isEnc(val, PartEnc);
}

/* Return 1 if the value is a Type, otherwise 0 */
int isType(Value val) {
	return isEnc(val, PartEnc) && part_info(val)->flags1 & PartType;
}

/* Places a new table or Array collection into a part's Value member */
Value partSetColl(Value th, Value part, Value* mbr, Value coll) {
	*mbr = coll;
	mem_markChk(th, part, coll);
	return coll;
}

/* Get the Items array (use array API functions to manipulate). 
 * This allocates the array, if it does not exist yet. */
Value partGetItems(Value th, Value part) {
	assert(isPart(part));
	return part_items(part);
}

/* Add an item to the Part's array */
void partAddItem(Value th, Value part, Value item) {
	assert(isPart(part));
	arrAdd(th, part_items(part), item);
}

/* Get the Properties table (use table API functions to manipulate). 
 * This allocates the table, if it does not exist yet. */
Value partGetProps(Value th, Value part) {
	assert(isPart(part));
	return part_props(part);
}

/* Add a Property to the Part's properties */
void partAddProp(Value th, Value part, Value key, Value val) {
	assert(isPart(part));
	tblSet(th, part_props(part), key, val);
}

/** Add a Property to the Part's properties */
void partAddPropc(Value th, Value part, const char* key, Value val) {
	assert(isPart(part));
	tblSetc(th, part_props(part), key, val);
}

/* Get the Methods table (use table API functions to manipulate). 
 * This allocates the table, if it does not exist yet. */
Value partGetMethods(Value th, Value part) {
	assert(isPart(part));
	return part_methods(part);
}

/* Add a Method to the Part */
void partAddMethod(Value th, Value part, Value methnm, Value meth) {
	assert(isPart(part));
	tblSet(th, part_methods(part), methnm, meth);
}

/** Add a C method to a part */
void partAddMethodc(Value th, Value part, const char* methsym, Value meth) {
	Value methval, methsymval;
	assert(isPart(part));
	// Use stack to ensure GC does not collect either value
	methval=pushValue(th, meth);
	methsymval=pushValue(th, aSym(th, methsym));
	tblSet(th, part_methods(part), methsymval, methval);
	setTop(th, -2);
}

/* Get the Mixins array (use array API functions to manipulate). 
 * This allocates the array, if it does not exist yet. */
Value partGetMixins(Value th, Value part) {
	assert(isPart(part));
	return part_mixins(part);
}

/* Add a type to the Part's mixins */
void partAddType(Value th, Value part, Value type) {
	assert(isPart(part) && isType(type));
	arrAdd(th, part_mixins(part), type);
}

/* Copy a type's methods to the Part */
void partCopyMethods(Value th, Value part, Value type) {
	assert(isPart(part) && isPart(type));
	Value key = tblNext(part_methods(type), aNull);
	while (key != aNull) {
		tblSet(th, part_methods(part), key, tblGet(th, part_methods(type), key));
		key = tblNext(part_methods(type), key);
	}
	arrAdd(th, part_mixins(part), type);
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

