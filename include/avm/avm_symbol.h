/** Implements symbols, immutable byte-sequences.
 *
 * A symbol is similar to a string, as it is a collection of characters. However, symbols are
 * immutable and indexed in a symbol table so that any two symbols with identical characters
 * will be represented only once. This speeds up comparing whether two symbols are the same, 
 * making them excellent for hash table keys.
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_symbol_h
#define avm_symbol_h

#include "avm/avm_memory.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

// ***********
// Symbol info declaration and access macros
// ***********

/** Information about a symbol memory block. (uses MemCommonInfo) */
typedef struct SymInfo {
	MemCommonInfo; //!< Common header
	AuintIdx hash; //!< Calculated hash
	// The symbol characters follow here: char sym[len+1];
} SymInfo;

/** Memory size of the symbol's header and 0-terminated c-string value */
#define sym_memsize(strlen) (sizeof(struct SymInfo) + sizeof(char)*(1+strlen))

/** Free the memory allocated for the symbol */
#define symFree(th, s) \
		vm(th)->sym_table.nbrUsed--; \
		mem_freemem(th, s, sym_memsize((s)->size));

/** Point to symbol information, by recasting a Value pointer */
#define sym_info(val) (assert_exp(isEnc(val,SymEnc), (SymInfo*) val))

/** Point to the symbol's 0-terminated c-string value */
#define sym_cstr(val) ((char*) (sym_info(val)+1))

/** Return the length of the symbol's string (without 0-terminator) */
#define sym_size(val) (sym_info(val)->size)

/** Symbol table structure. 
 * There is only one symbol table used to index all symbols. Its array will double in size
 * when empty space gets tight. The symbol table is simply a collection of symbol Values,
 * indexed by the symbol's hash value. Since different symbols can hash to the same index, 
 * one may need to follow the Value forward-link chain to find the desired symbol. */
struct SymTable {
	SymInfo **symArray; /**< Array of symbol pointers */
	Auint nbrAvail; /**< Number of allocated table entries */
	Auint nbrUsed;  /**< Number of table entries in use */
};

/** Memory size of symbol table - used by garbage collector */
#define sym_tblsz(th) \
	(sizeof(SymTable) + vm(th)->sym_table.nbrAvail * sizeof(SymInfo*))

/** After deleting unused symbols, shrink symbol table by half, if using less than half of it */
#define sym_tblshrinkcheck(th) \
	{Auint hs = vm(th)->sym_table.nbrAvail >> 1; \
	if (vm(th)->sym_table.nbrUsed < hs) \
		sym_resize_tbl(th, hs);}


// ***********
// Non-API Symbol functions
// ***********

/** Initialize vm's symbol table */
void sym_init(Value th);
/** Free the symbol table */
void sym_free(Value th);
/** Resize the symbol table */
void sym_resize_tbl(Value th, Auint newsize);

/** If symbol exists in symbol table, reuse it. Otherwise, add it. 
   Anchor (store) symbol value in dest and return it. */
Value newSym(Value th, Value *dest, const char *str, AuintIdx len);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif