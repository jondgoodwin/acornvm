/** Implements hashed tables: variable-sized, indexed collections of Values (see avm_table.h)
 *
 * The table's look-up index grows and shrinks automatically based on how full it is.
 * Changes to index size are always doubling or halving, so that size is a power of 2.
 * This restriction ensures that hash calculation is very easy and fast.
 *
 * Inspired by Lua, the implementation uses a mix of chained scatter table with Brent's variation.
 * A main invariant of these tables is that, if an element is not
 * in its main position (i.e. the `original' position that its hash gives
 * to it), then the colliding element is in its own main position.
 * Hence even when the load factor reaches 100%, performance remains good.
 *
 * See avm_table.h for usage notes about keys.
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

/** An empty Nodes structure, when a table's index has no entries (size of 0) */
const struct Node emptyNode = {
	aNull,
	aNull,
	NULL
};

/** Returns next highest integer value of log2(x) */
unsigned char ceillog2(AuintIdx x) {
	static const unsigned char log_2[256] = {
		0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
		6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
		8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
		8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
		8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
	};
	int log = 0;
	x--;
	while (x >= 256) { log += 8; x >>= 8; }
	return log + log_2[x];
}

/** Calculate the hash for a sequence of bytes (used by symbols). */
AuintIdx tblCalcStrHash(const char *str, Auint len, AuintIdx seed) {
	AuintIdx hash = seed ^ (AuintIdx) len;
	Auint l1;
	Auint step = (len >> AVM_STRHASHLIMIT) + 1;
	for (l1 = len; l1 >= step; l1 -= step)
		hash = hash ^ ((hash<<5) + (hash>>2) + str[l1 - 1]);
	return hash;
}

/** Fast power-of-two calculation from hash to entry in table's index.
 *  Essentially uses just the proper bottom bits. Used for symbols, integers & bool */
#define hash2NodeMod2(hash, size) \
	(((AuintIdx) (hash)) & ((size)-1))

/** Slower division-based mod calculation from hash to entry in table's index.
 *  Used where bottom bits are often less random (often 0's), like pointers and float.
 *  It divides by power of 2 - 1, which should jumble things up a bit more */
#define hash2NodeDiv(hash, size) \
	(((Auint)(hash)) % ((size-1)|1))

/** Calculate the preferred index Node by hashing the key's value */
Node *tblKey2Node(Value tbl, Value key) {

	// Ensure parms comply
	assert(isTbl(tbl) && key!=aNull);

	TblInfo *t = tbl_info(tbl);
	AuintIdx size = 1 << t->lAvailNodes;
	assert((size&(size-1))==0); // Must be power of 2

	switch ((Auint)key & ValMask) {
	// Floats use entire number as hash (including bottom bits).
	case ValFloat:
		return &t->nodes[hash2NodeDiv(key, size)];

	// For Integer and Bool, the value is the hash (after removing bottom encoding bits)
	case ValInt:
	case ValCons:
		return &t->nodes[hash2NodeMod2((Auint)key >> ValShift, size)];

	// Handle ValPtr allocated values
	default:
		// Symbols use the pre-calculated hash
		if (isSym(key))
			return &t->nodes[hash2NodeMod2(sym_info(key)->hash, size)];
		// Other pointers, the value is the hash
		return &t->nodes[hash2NodeDiv(key, size)];
	}
}

/** Find the node containing the key, if it exists, or return NULL */
Node *tblFind(Value tbl, Value key) {
	assert(isTbl(tbl) && key!=aNull);

	// Look for the 'key' in the linked chain
	Node *n = tblKey2Node(tbl, key);
	do {
		if (n->key == key)
			return n;  // Found it
		n = n->next;
	} while (n);
	return NULL;
}

/* Return a pointer to the value in the table at key, or NULL if not found. */
Value *tblGetp(Value tbl, Value key) {
	if (!isTbl(tbl) || key==aNull)
		return NULL;

	// Look for the 'key' in the linked chain
	Node *n = tblKey2Node(tbl, key);
	do {
		if (n->key == key)
			return &n->val;  // Found it
		n = n->next;
	} while (n);
	return NULL;
}

/* Get the next sequential key/value pair in table after 'key'.
 * To sequentially traverse the table, start with 'key' of 'null'.
 * Each time called, the next key/value pair is returned.
 * After the last key, null is returned.
 * Warning: Accurate traversal requires the table remains unchanged.
*/
Value tblNext(Value tbl, Value key) {
	assert(isTbl(tbl));
	// Find node for old key, or start of nodes (backtracked)
	Node *n = (key==aNull)? (tbl_info(tbl)->nodes-1) : tblFind(tbl, key);
	Node *last = &tbl_info(tbl)->nodes[1 << tbl_info(tbl)->lAvailNodes];
	if (n) {
		for (n++; n < last; n++)  
			if (n->key != aNull)
				return n->key;
	}
	return aNull;
}

/** Return last table node with a 'null' key, else return NULL. */
Node *tblLastFreeNode(TblInfo* t) {
	// Start at table's lastfree pointer (initialized to just after nodes)
	while (t->lastfree > t->nodes) {
		// Decrement back to check
		t->lastfree--;
		// If free, take it
		if (t->lastfree->key == aNull)
			return t->lastfree;
	}
	return NULL;  // could not find a free place
}

/** Insert a *new* key into a hash table, growing table if necessary.
 * Do not use this function if key is already in the table. */
void tblAdd(Value th, Value tbl, Value key, Value val) {
	Node *mp; // main position node for new key/value pair
	assert(isTbl(tbl) && key!=aNull);

	// Handle when calculated position is not available
	mp = tblKey2Node(tbl, key);
	if (mp->key != aNull || tbl_info(tbl)->nodes == &emptyNode) {
		Node *othern;
		Node *n = tblLastFreeNode(tbl_info(tbl));  /* get a free place */
		if (n == NULL) {  // cannot find a free place?
			tblResize(th, tbl, (1<<tbl_info(tbl)->lAvailNodes) + 1);  // grow table
			return tblAdd(th, tbl, key, val);  // insert key into grown table
		}
		assert(tbl_info(tbl)->nodes != &emptyNode);

		// Examine the other key/value colliding with our preferred position
		othern = tblKey2Node(tbl, mp->key); // Its preferred position
		if (othern != mp) {  // is colliding node out of its main position?
			// yes; move colliding node into free position 
			while (othern->next != mp) othern = othern->next;  // find previous
			othern->next = n;  // redo the chain with `n' in place of `mp'
			*n = *mp;  // copy colliding node into free pos. (mp->next also goes)
			mp->next = NULL;  // now `mp' is free
		}
		else {  // colliding node is in its own main position
			// new node will go into free position
			n->next = mp->next;  // chain new position
			mp->next = n;
			mp = n;
		}
	}

	// Set the key/value pair
	mp->key = key;
	mp->val = val;
	tbl_info(tbl)->size++;
}

/* Delete a key from hash table, if found. 
 * Deletion is fortunately rare, as it is a bit slower and involved because
 * we have to clean up any post-chained nodes by reinserting them into the table.
 * If we don't do this, the empty nodes accumulate unused.
 * This could shrink the table, if we want to ... */
void tblRemove(Value th, Value tbl, Value key) {
	assert(isTbl(tbl));
	Value val;

	// Null is never a key
	if (key==aNull)
		return;

	// Find the 'key' in the linked chain
	Node *prevp = NULL; // previous node that chains to key's node
	Node *n = tblKey2Node(tbl, key);
	do {
		if (n->key == key)
			break;
		prevp = n;
		n = n->next;
	} while (n);

	// Return if key does not exist to delete
	if (n==NULL)
		return;

	// Zero out the node (and break chain to it)
	if (prevp)
		prevp->next = NULL;
	n->key = aNull;
	n->val = aNull;
	prevp = n->next; // Save any follow-on link
	n->next = NULL;
	tbl_info(tbl)->size--;

	// If you want to shrink the table, do it here and then return,
	// as the rest of Remove code is unneeded since Resize replaces index.
	//
	// if (ceillog2(tbl_info(tbl)->size) < tbl_info(tbl)->lAvailNodes) {
	//	tblResize(th, tbl, tbl_info(tbl)->size);
	//	return;
	//	}

	// Indicate node is deleted and reset pointer to show it is available
	if (tbl_info(tbl)->lastfree < n)
		tbl_info(tbl)->lastfree = n+1;

	// Cycle through rest of node chain, reinserting node's into table
	while (prevp) {
		// Save node's contents
		n = prevp;
		key = n->key;
		val = n->val;
		prevp = n->next;

		// Zero out the node
		n->key = aNull;
		n->val = aNull;
		n->next = NULL;
		if (tbl_info(tbl)->lastfree < n)
			tbl_info(tbl)->lastfree = n+1; // Reset to show node is available

		// Add the node back (possibly into the same spot, alas)
		tbl_info(tbl)->size--; // Add will re-increment
		tblAdd(th, tbl, key, val);
	}
}

/** Allocate and initialize table's index containing 'size' nodes.
 * The table's node pointer will point to the newly allocated node vector,
 * which is initialized with 'null' values for all keys and values.
 * Sizes are increased to next power of two. A size of zero means nothing is allocated. */
void tblAllocnodes(Value th, TblInfo *t, AuintIdx size) {
	unsigned char logsize;
	// If empty, just point to 'emptynode'
	if (size == 0) {  
		t->nodes = (Node*) &emptyNode;
		logsize = 0;
	}
	else {
		AuintIdx i;
		// Convert size to the next highest power of two
		logsize = ceillog2(size);
		size = 1 << logsize;  // 2^logsize

		// Allocate new nodes buffer and initialize with empty nodes
		t->nodes = (Node*) mem_gcreallocv(th, NULL, 0, size, sizeof(Node));
		for (i=0; i<size; i++) {
			memcpy((char *) &t->nodes[i], (char*) &emptyNode, sizeof(Node));
		}
	}
	t->lAvailNodes = logsize;
	t->lastfree = &t->nodes[size];  // all positions are free 
}

/* Resize a table for more/fewer elements (cannot be smaller than used size)
 * This allocates a new node array, inserts key/value pairs
 * from the old node array, then frees the old node array.
*/
void tblResize(Value th, Value tbl, AuintIdx newsize) {
	Node *nodep;
	TblInfo *t = tbl_info(tbl);
	assert(isTbl(tbl));
	mem_gccheck(th);	// Incremental GC before memory allocation events

	// Preserve pointer to old index, then allocate a new one
	AuintIdx oldsize = 1 << t->lAvailNodes; // 2^
	if (newsize < oldsize)
		newsize = oldsize;
	Node *oldnodes = t->nodes;  // save old index
	tblAllocnodes(th, t, newsize);
	t->size = 0; // Will recount entries as we re-add

	// re-insert elements from old index
	for (nodep = &oldnodes[oldsize - 1]; nodep >= oldnodes; nodep--) {
		if (nodep->key != aNull) {
			tblAdd(th, tbl, nodep->key, nodep->val);
		}
	}

	// free old index
	if (oldnodes != &emptyNode)
		mem_gcrealloc(th, oldnodes, oldsize*sizeof(Node), 0); 
}

/* Create and initialize a new hashed Table */
Value newTbl(Value th, Value *dest, Value type, AuintIdx size) {
	TblInfo *t = (TblInfo*) mem_new(th, TblEnc, sizeof(TblInfo));
	t->flags1 = 0;
	t->type = type;
	t->inheritype = aNull;
	t->size = 0;

	tblAllocnodes(th, t, size);
	return *dest = (Value) t;
}

/* Create and initialize a new Type (a table where members are properties) */
Value newType(Value th, Value *dest, Value type, AuintIdx size) {
	TblInfo *t = (TblInfo*) mem_new(th, TblEnc, sizeof(TblInfo));
	t->flags1 = TypeTbl | ProtoType;
	t->type = t->inheritype = type;
	t->size = 0;

	tblAllocnodes(th, t, size);
	return *dest = (Value) t;
}

/* Create and initialize a new Mixin type */
Value newMixin(Value th, Value *dest, Value type, Value inheritype, AuintIdx size) {
	TblInfo *t = (TblInfo*) mem_new(th, TblEnc, sizeof(TblInfo));
	t->flags1 = TypeTbl;
	t->type = type;
	t->inheritype = inheritype;
	t->size = 0;

	tblAllocnodes(th, t, size);
	return *dest = (Value) t;
}

/* Return 1 if the value is a Table, otherwise 0 */
int isTbl(Value val) {
	return isEnc(val, TblEnc);
}

/* Return 1 if the value is a Type table, otherwise 0 */
int isType(Value val) {
	return isEnc(val, TblEnc) && tbl_info(val)->flags1 & TypeTbl;
}

/* Return 1 if the value is a Type table, otherwise 0 */
int isPrototype(Value val) {
	return isEnc(val, TblEnc) && ((tbl_info(val)->flags1 & (TypeTbl | ProtoType))==(TypeTbl|ProtoType));
}

/* Return 1 if table has an entry at key, 0 if not */
int tblHas(Value th, Value tbl, Value key) {
	assert(isTbl(tbl));

	// Null is never a key
	if (key==aNull)
		return 0;

	// Find key, and return what we find (or not)
	Node *n = tblFind(tbl, key);
	return (n)? 1 : 0;
}

/* Return the value paired with 'key', or 'null' if not found */
Value tblGet(Value th, Value tbl, Value key) {
	assert(isTbl(tbl));

	// Null is never a key
	if (key==aNull)
		return aNull;

	// Find key, and return what we find (or not)
	Node *n = tblFind(tbl, key);
	return (n)? n->val : aNull;
}

/* Inserts or alters the table's 'key' entry with value. 
 * - Inserts 'key' if key is not already there
 * - Otherwise, it changes 'key' value */
void tblSet(Value th, Value tbl, Value key, Value val) {
	assert(isTbl(tbl));

	// Null is never a key
	if (key==aNull)
		return;

	// Look for key. If found, replace value. Otherwise, insert key/value pair
	Node *n = tblFind(tbl, key);
	if (n) {
		n->val = val;
		mem_markChk(th, tbl, val);
	}
	else {
		tblAdd(th, tbl, key, val);
		mem_markChk(th, tbl, key);
		mem_markChk(th, tbl, val);
	}
}

/* Add mixin to top of list of types found at inheritype */
void addMixin(Value th, Value type, Value mixin) {
	// Can only add mixins to prototypes and mixins
	if (!isType(type))
		return;
	TblInfo *typp = tbl_info(type);

	// If field is empty, give it the mixin as a value
	if (typp->inheritype == aNull)
		typp->inheritype = mixin;

	// If field is a list already, add to top
	else if (isArr(typp->inheritype))
		arrIns(th, typp->inheritype, 0, 1, mixin);

	// Field is a type; turn into an array
	else {
		Value arr = pushArray(th, aNull, 2);
		arrSet(th, arr, 0, mixin);
		arrSet(th, arr, 1, typp->inheritype);
		typp->inheritype = popValue(th);
	}

	// If this is a prototype, sync change to type as well
	if (typp->flags1 && ProtoType)
		typp->type = typp->inheritype;

	// Allow mixin to initialize itself or alter the type it joins
	pushSym(th, "New");
	pushValue(th, mixin);
	pushValue(th, type);
	getCall(th, 2, 0);
}

/* Serialize an table's contents to indented text */
void tblSerialize(Value th, Value str, int indent, Value tbl) {
	Node *n = tbl_info(tbl)->nodes;
	Node *last = &tbl_info(tbl)->nodes[1 << tbl_info(tbl)->lAvailNodes];
	int ind;

	const char *typ = isType(tbl)? "Type" : "Index";
	Value typenm = getProperty(th, getType(th, tbl), vmlit(SymName));
	if (isSym(typenm))
		typ = toStr(typenm);

	strAppend(th, str, "+", 1);
	strAppend(th, str, typ, strlen(typ));
	// strAppend(th, str, "\n", 1);
	// ind = indent+1;
	// while (ind--)
	// 	strAppend(th, str, "\t", 1);
	// strAppend(th, str, "TYPE: ", 6);
	// serialize(th, str, indent+1, getType(th, tbl));
	for (; n < last; n++) {
		if (n->key != aNull) {
			strAppend(th, str, "\n", 1);
			ind = indent+1;
			while (ind--)
				strAppend(th, str, "\t", 1);
			serialize(th, str, indent+1, n->key);
			strAppend(th, str, ": ", 2);
			serialize(th, str, indent+1, n->val);
		}
	}
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
