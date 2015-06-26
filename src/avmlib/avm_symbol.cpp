/** Symbol value manipulation
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

/** Symbol table structure. 
 * There is only one symbol table used to index all symbols. Its array will double in size
 * when empty space gets tight. The symbol table is simply a collection of symbol Values,
 * indexed by the symbol's hash value. Since different symbols can hash to the same index, 
 * one may need to follow the Value forward-link chain to find the desired symbol.
 */
struct SymTable {
  SymInfo **symArray; /**< Array of symbol pointers */
  Auint nbrAvail; /**< Number of allocated table entries */
  Auint nbrUsed;  /**< Number of table entries in use */
};

/** Memory size of the symbol's header and 0-terminated c-string value */
#define sym_memsize(strlen) (sizeof(struct SymInfo) + sizeof(char)*(1+strlen))

/** modulo operation for hashing (size is always a power of 2) */
#define hash_binmod(s,size) \
	(assert_exp((size&(size-1))==0, (AHash) ((s) & ((size)-1)) ))

/** Resize the symbol table */
void sym_resize_tbl(VmInfo *vm, Auint newsize) {
	SymTable* sym_tbl = vm->sym_table;
	Auint i;

	// If we need to grow, allocate more cleared space for array
	if (newsize > sym_tbl->nbrAvail) {
		mem_reallocvector(vm, sym_tbl->symArray, sym_tbl->nbrAvail, newsize, SymInfo *);
		for (i = sym_tbl->nbrAvail; i < newsize; i++) 
			sym_tbl->symArray[i] = NULL;
	}

	// Move all symbols to re-hashed positions in array
	for (i=0; i<sym_tbl->nbrAvail; i++) {
		SymInfo *p = sym_tbl->symArray[i];
		sym_tbl->symArray[i] = NULL;
		while (p) {  // for each node in the list
			SymInfo *next = (SymInfo*) p->next;  // save next
			AHash h = hash_binmod(p->hash, newsize);  // new position
			p->next = (MemInfo*) sym_tbl->symArray[h];  // chain it
			sym_tbl->symArray[h] = (SymInfo*) p;
			resetoldbit(p);  // see MOVE OLD rule
			p = next;
		}
	}

	// Shrink array
	if (newsize < sym_tbl->nbrAvail) {
		// shrinking slice must be empty
		assert(sym_tbl->symArray[newsize] == NULL && sym_tbl->symArray[sym_tbl->nbrAvail - 1] == NULL);
		mem_reallocvector(vm, sym_tbl->symArray, sym_tbl->nbrAvail, newsize, SymInfo *);
	}
	sym_tbl->nbrAvail = newsize;
}

/** Initialize the symbol table that hash indexes all symbols */
void sym_init(VmInfo* vm) {
	struct SymTable* sym_tbl = vm->sym_table = (struct SymTable*) mem_frealloc(NULL, sizeof(SymTable));
	sym_tbl->nbrAvail = 0;
	sym_tbl->nbrUsed = 0;
	sym_tbl->symArray = NULL;
	sym_resize_tbl(vm, AVM_SYMTBLMINSIZE);
}

/* If symbol exists in symbol table, reuse it. Otherwise, add it. Return symbol value. */
Value aSyml(Value th, const char *str, Auint32 len) {
	SymInfo *sym;
	SymTable* sym_tbl = th(th)->vm->sym_table;
	unsigned int hash = hash_bytes(str, len, th(th)->vm->hashseed);

	// Look for symbol in symbol table. Return it, if found.
	for (sym = sym_tbl->symArray[hash_binmod(hash, sym_tbl->nbrAvail)]; sym != NULL; sym = (SymInfo*) sym->next) {
		if (hash == sym->hash &&
				len == sym->size &&
				(memcmp(str, sym_cstr(sym), len) == 0)) {
			mem_keepalive(th, (MemInfo*) sym); // Keep it alive, if it had been marked for deletion
			return (Value) sym;
		}
	}

	// Not found. Double symbol table size if needed to hold another entry
	if (sym_tbl->nbrUsed >= sym_tbl->nbrAvail)
		sym_resize_tbl(th(th)->vm, sym_tbl->nbrAvail*2);

	// Create a symbol object, adding to symbol table at hash entry
	MemInfo **linkp = (MemInfo**) &sym_tbl->symArray[hash_binmod(hash, sym_tbl->nbrAvail)];
	sym = (SymInfo *) mem_new(vm(th), SymEnc, sym_memsize(len), linkp, 0);
	sym->size = len;
	sym->hash = hash;
	memcpy(sym_cstr(sym), str, len);
	(sym_cstr(sym))[len] = '\0';
	sym_tbl->nbrUsed++;
	return (Value) sym;
}

/* Calculate length of c-string, then use aSyml(). */
Value aSym(Value th, const char *str) {
	return aSyml(th,str,strlen(str));
}

/* Iterate to next symbol after key in symbol table (or first if key is NULL). Return Null if no more. 
 * This can be used to sequentially iterate through the symbol table.
 * Results may be inaccurate if the symbol table is changed during iteration.
 */
Value sym_next(Value th, Value key) {
	SymTable *sym_tbl = th(th)->vm->sym_table;
	SymInfo *sym;

	// If table empty, return null
	if (sym_tbl->nbrUsed == 0)
		return aNull;

	// If key is null, return first symbol in table
	if (key==aNull) {
		SymInfo **symtblp = sym_tbl->symArray;
		while ((sym=*symtblp++)==NULL);
		return (Value) sym;
	}

	// If key is not a symbol, return null
	if (!isSym(key))
		return aNull;

	// Look for the symbol in table, then return next one
	AHash hash = ((SymInfo*)key)->hash;
	Auint len = ((SymInfo*)key)->size;
	Auint i = hash_binmod(hash, sym_tbl->nbrAvail);
	for (sym = sym_tbl->symArray[i]; sym != NULL; sym = (SymInfo*) sym->next) {
		if (hash == sym->hash &&
				len == sym->size &&
				(memcmp(sym_cstr(key), sym_cstr(sym), len) == 0)) {
			// If the next one is populated, return it
			if ((sym = (SymInfo*) sym->next))
				return (Value) sym;
			// Look for next non-null entry in symbol array
			for (i++; i<sym_tbl->nbrAvail; i++) {
				if ((sym=sym_tbl->symArray[i]))
					return (Value) sym;
			}
			return aNull; // No next symbol, return null
		}
	}
	return aNull;
}


#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

