/** Implements hashed tables: variable-sized, indexed collections of Values
 *
 * Like an Array, a Table holds an arbitray number of Values.
 * However, Table values are not ordered, but are indexed using a key.
 * The key may be any type of value, but the results vary:
 * - Symbols are the preferred key, as they are expressive, yet hash and compare quickly.
 *   The symbol's hash is calculated at creation based on its character contents.
 * - String keys are always converted to symbols, to ensure the same text
 *   in a string and symbol match. This is a big performance hit over using symbols.
 *   Only use strings as keys when one needs to dynamically generate the key each time.
 * - Null is never really a key. It always returns Null as its value.
 * - Integers, floats, and true/false are valid keys whose hash is based on their contents.
 *   (0.f, -0.f, -INF, +INF, and Nan are all valid, separate float keys).
 * - All other values are valid keys, but the hash is based on the 'object', not its contents.
 *   So, the object can change its contents and still have the same hash index.
 *
 * Any type of value can be stored in a Table, other than Null. 
 * Storing Null using an already-existing key effectively deletes the entry.
 *
 * The Table's key-value pair is essentially a property. 
 * The Table structure is critical for storing methods & global variables.
 *
 * Tables are mutable; their content and size can be changed as needed.
 * Increases in size often requires allocating a larger block and copying the contents.
 * To minimize this, size it properly at creation or resize it using large increments.
 * The allocated size of a Table's index may increase or decrease as elements are added and deleted.
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_table_h
#define avm_table_h

#include "avm/avm_memory.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

// ***********
// Table info declaration and access macros
// ***********

/** Structure of a table index node The index has an array of many such nodes. */
typedef struct Node {
	Value val;			//!< Node's value
	Value key;			//!< Node's key
	struct Node *next;  //!< Link to next node with same hash
} Node;

/** Information about an table information block. (uses MemCommonInfoT) 
 * Note that flags2 is used to indicate the log2 of size of 'nodes' buffer */
typedef struct TblInfo {
	MemCommonInfoT;				//!< Common header for typed value
	struct Node *nodes;			//!< Pointer to allocated table index
	struct Node *lastfree;		//!< any free node in index is before this position
} TblInfo;

/** flags2 holds log2 of available number of nodes in 'node' buffer */
#define lAvailNodes flags2

/** Mark all in-use table values for garbage collection 
 * Increments how much allocated memory the table uses. */
#define tblMark(th, t) \
	{mem_markobj(th, (t)->type); \
	for (Node *n = &(t)->nodes[(1<<(t)->lAvailNodes)-1]; n >= (t)->nodes; n--) \
		if (n->key != aNull) { \
			assert(n->val != aNull); \
			mem_markobj(th, n->key); \
			mem_markobj(th, n->val); \
		} vm(th)->gcmemtrav += sizeof(TblInfo) + sizeof(Node) * (1<<(t)->lAvailNodes);}

/** Free all of an array's allocated memory */
#define tblFree(th, t) \
	{if ((t)->lAvailNodes>0) \
		mem_freearray(th, (t)->nodes, 1<<(t)->lAvailNodes); \
	mem_free(th, (t));}

/** Point to table information, by recasting a Value pointer */
#define tbl_info(val) (assert_exp(isEnc(val,TblEnc), (TblInfo*) val))

/** Return the number of Values stored in the table */
#define tbl_size(val) (tbl_info(val)->size)

// ***********
// Non-API Table functions
// ***********

AuintIdx tblCalcStrHash(const char *str, Auint len, AuintIdx seed);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif