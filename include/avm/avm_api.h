/** Defines API accessible functions, beyond those in avm_value.h
 *
 * This defines all API functions that can be used by any C/C++ program that embeds
 * and runs the Acorn Virtual Machine, or by any C/C++ library that extends
 * the global environment, built-in types, methods or functions.
 *
 * The C-code implementation of these API function is scattered across
 * various .cpp files, as indicated.
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_api_h
#define avm_api_h

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

// Implemented in avm_value.cpp
/** Set the type used by a value (if encoding allows it to change) */
AVM_API void setType(Value th, Value val, Value type);
/** Return the value's type (works for all values) */
AVM_API Value getType(Value th, Value val);
/** Find method in self or its type. Return aNull if not found */
AVM_API Value findMethod(Value th, Value self, Value methsym);
/** Return the size of a symbol, array, hash, or other collection. Any other value type returns 0 */
AVM_API Auint getSize(Value val);

// Implemented in avm_string.cpp and avm_symbol.cpp
/** Return symbol for a c-string. */
AVM_API Value aSym(Value th, const char *str);
/** Return symbol for a byte-sequence. */
AVM_API Value aSyml(Value th, const char *str, AuintIdx len);
/** Return string value for a c-string. */
AVM_API Value newStr(Value th, const char *str);
/** Return string value for a byte-sequence. str may be NULL (to reserve space for empty string). */
AVM_API Value newStrl(Value th, const char *str, AuintIdx len);
/** Return 1 if the value is a Symbol, otherwise 0 */
AVM_API int isSym(Value sym);
/** Return 1 if the value is a String, otherwise 0 */
AVM_API int isStr(Value str);
/** Return a read-only pointer into a C-string encoded by a symbol or string Value. 
 * It is guaranteed to have a 0-terminating character just after its full length. 
 * Any other value type returns NULL.
 */
AVM_API const char* toStr(Value sym);
/** Return 1 if the symbol or string value's characters match the zero-terminated c-string, otherwise 0. */
AVM_API int strEq(Value val, const char* str);
/** Iterate to next symbol after key in symbol table (or first if key is NULL). Return Null if no more. 
 * This can be used to sequentially iterate through the symbol table.
 * Results may be inaccurate if the symbol table is changed during iteration.
 */
AVM_API Value sym_next(Value th, Value key);
/** Ensure string has room for len Values, allocating memory as needed.
 * Allocated space will not shrink. Changes nothing about string's contents. */
AVM_API void strMakeRoom(Value th, Value val, AuintIdx len);
/**	Replace part of a string with the c-string contents starting at pos.
 *	If sz==0, it becomes an insert. If str==NULL or len==0, it becomes a deletion.
 *	The Acorn string will be resized automatically to accommodate excess characters.
 *	The operation will not be performed if resizing is not possible. */
AVM_API void strSub(Value th, Value val, AuintIdx pos, AuintIdx sz, const char *str, AuintIdx len);

// Implemented in avm_array.cpp
/** Return new array with allocated space for len Values. */
AVM_API Value newArr(Value th, AuintIdx len);
/** Return 1 if the value is an Array, otherwise 0 */
AVM_API int isArr(Value sym);
/** Ensure array has room for len Values, allocating memory as needed.
 * Allocated space will not shrink. Changes nothing about array's contents. */
AVM_API void arrMakeRoom(Value th, Value val, AuintIdx len);
/** Force allocated and used array to a specified size, truncating 
 * or expanding as needed. Growth space is initialized to aNull. */
AVM_API void arrForceSize(Value th, Value val, AuintIdx len);
/** Retrieve the value in array at specified position. */
AVM_API Value arrGet(Value th, Value arr, AuintIdx pos);
/** Put val into the array starting at pos.
 * This can expand the size of the array.*/
AVM_API void arrSet(Value th, Value arr, AuintIdx pos, Value val);
/** Append val to the end of the array (increasing array's size). */
AVM_API void arrAdd(Value th, Value arr, Value val);
/** Propagate n copies of val into the array starting at pos.
 * This can expand the size of the array.*/
AVM_API void arrRpt(Value th, Value arr, AuintIdx pos, AuintIdx n, Value val);
/** Delete n values out of the array starting at pos. 
 * All values after these are preserved, essentially shrinking the array. */
AVM_API void arrDel(Value th, Value arr, AuintIdx pos, AuintIdx n);
/** Insert n copies of val into the array starting at pos, expanding the array's size. */
AVM_API void arrIns(Value th, Value arr, AuintIdx pos, AuintIdx n, Value val);
/** Copy n2 values from arr2 starting at pos2 into array, replacing the n values in first array starting at pos.
 * This can increase or decrease the size of the array. arr and arr2 may be the same array. */
AVM_API void arrSub(Value th, Value arr, AuintIdx pos, AuintIdx n, Value arr2, AuintIdx pos2, AuintIdx n2);

// Implemented in avm_table.cpp
/** Create and initialize a new empty hash value */
AVM_API Value newTbl(Value th, AuintIdx len);
/** Return 1 if the value is a Hash, otherwise 0 */
AVM_API int isTbl(Value val);
/** Resize a table for more/fewer elements (cannot be less than used size) */
AVM_API void tblResize(Value th, Value tbl, AuintIdx newsize);
/** Return the value paired with 'key', or 'null' if not found */
AVM_API Value tblGet(Value th, Value tbl, Value key);
/** Inserts, alters or deletes the table's 'key' entry with value. 
 * - Deletes 'key' when value is null.
 * - Inserts 'key' if key is not already there
 * - Otherwise, it changes 'key' value */
AVM_API void tblSet(Value th, Value tbl, Value key, Value val);
/** Inserts, alters or deletes the table's 'key' entry with value. 
 * - Deletes 'key' when value is null.
 * - Inserts 'key' if key is not already there
 * - Otherwise, it changes 'key' value 
 * This C API version is safer than tblSet from accidental garbage 
 * collection when key and val are both new values. */
AVM_API void tblSetc(Value th, Value tbl, const char* key, Value val);
/** Get the next sequential key/value pair in table after 'key'.
 * To sequentially traverse the table, start with 'key' of 'null'.
 * Each time called, the next key/value pair is returned.
 * After the last key, null is returned.
 * Warning: Accurate traversal requires the table remains unchanged.
*/
AVM_API Value tblNext(Value tbl, Value key);

// Implemented in avm_part.cpp
/** Return a new Part. */
AVM_API Value newPart(Value th, Value type);
/** Return 1 if the value is an Part, otherwise 0 */
AVM_API int isPart(Value val);
/** Return a new Type. */
AVM_API Value newType(Value th, const char* typnm);
/** Return 1 if the value is an Type, otherwise 0 */
AVM_API int isType(Value val);
/** Get the Items array (use array API functions to manipulate). 
 * This allocates the array, if it does not exist yet. */
AVM_API Value partGetItems(Value th, Value part);
/** Add an item to the Part's array */
AVM_API void partAddItem(Value th, Value part, Value item);
/** Get the Properties table (use table API functions to manipulate). 
 * This allocates the table, if it does not exist yet. */
AVM_API Value partGetProps(Value th, Value part);
/** Add a Property to the Part's properties */
AVM_API void partAddProp(Value th, Value part, Value key, Value val);
/** Add a Property to the Part's properties */
AVM_API void partAddPropc(Value th, Value part, const char* key, Value val);
/** Get the Methods table (use table API functions to manipulate). 
 * This allocates the table, if it does not exist yet. */
AVM_API Value partGetMethods(Value th, Value part);
/** Add a Property to the Part's properties */
AVM_API void partAddMethod(Value th, Value part, Value methnm, Value meth);
/** Add a Method to the Part */
AVM_API void partAddMethodc(Value th, Value part, const char* methnm, Value meth);
/** Get the Mixins array (use array API functions to manipulate). 
 * This allocates the array, if it does not exist yet. */
AVM_API Value partGetMixins(Value th, Value part);
/** Add a type to the Part's mixins */
AVM_API void partAddType(Value th, Value part, Value type);
/** Copy a type's methods to the Part */
AVM_API void partCopyMethods(Value th, Value part, Value type);
/** Macro to add a c-method to a type */
#define addCMethod(th,part,methsym,meth,methnm) \
	partAddMethodc(th, part, methsym, aCMethod(th, meth, methnm, __FILE__));

// Implemented in avm_func.cpp
/** Build a new c-function value, pointing to a function written in C */
AVM_API Value aCFunc(Value th, AcFuncp func, const char* name, const char* src);
/** Build a new c-method value, pointing to a function written in C */
AVM_API Value aCMethod(Value th, AcFuncp func, const char* name, const char* src);
/** Call a method or function value placed on stack (with nparms above it). 
 * Indicate how many return values to expect to find on stack. */
AVM_API void funcCall(Value th, int nparms, int nexpected);

// Implemented in avm_thread.cpp
/** Return a new Thread with a starter namespace and stack. */
AVM_API Value newThread(Value th, Value ns, AuintIdx stksz);
/** Return 1 if a Thread, else return 0 */
AVM_API int isThread(Value th);

// Implemented in avm_global.cpp
/** Create a new global namespace (typically for main thread) */
AVM_API Value newGlobal(Value th, AuintIdx size);
/** Creates a new global namespace, extending thread's current global to an Array */
AVM_API Value growGlobal(Value th, AuintIdx size);
/** Retrieve a value from global namespace */
AVM_API Value gloGet(Value th, Value var);
/** Retrieve a value from global namespace */
#define gloGetc(th, var) (gloGet(th, aSym(th, var)))
/** Add or change a global variable */
AVM_API void gloSet(Value th, Value var, Value val);
/** Add or change a global variable */
AVM_API void gloSetc(Value th, const char* var, Value val);

// Implemented in avm_stack.cpp
/** Retrieve the stack value at the index. Be sure 0<= idx < top.
 * Good for getting method's parameters: 0=self, 1=parm 1, etc. */
AVM_API Value stkGet(Value th, AintIdx idx);
/** Put the value on the stack at the designated position. Be sure 0<= idx < top. */
AVM_API void stkSet(Value th, AintIdx idx, Value val);
/** Copy the stack value at fromidx into toidx */
AVM_API void stkCopy(Value th, AintIdx toidx, AintIdx fromidx);
/** Remove the value at index (shifting down all values above it to top) */
AVM_API void stkRemove(Value th, AintIdx idx);
/** Insert the value at index (shifting up all values above it) */
AVM_API void stkInsert(Value th, AintIdx idx, Value val);
/** Push a value on the stack's top */
AVM_API void stkPush(Value th, Value val);
/** Push a copy of a stack's value at index onto the stack's top */
AVM_API void  stkPushCopy(Value th, AintIdx idx);
/** Pop a value off the top of the stack */
AVM_API Value stkPop(Value th);
/** Pops the top value and writes it at idx. Often used to set return value */
AVM_API void stkPopTo(Value th, AintIdx idx);
/** Obtain value "from top" index , where 0=top, 1 is next */
AVM_API Value stkFromTop(Value th, AintIdx fromtop);
/** Return number of values on the current function's stack */
AVM_API AuintIdx stkSize(Value th);
/** When index is positive, this indicates how many Values are on the function's stack.
 *	This can shrink the stack or grow it (padding with 'null's).
 *	A negative index removes that number of values off the top. */
AVM_API void stkSetSize(Value th, AintIdx idx);
/** Ensure stack has room for 'size' values. Returns 0 on failure. 
 * This may grow the stack, but never shrinks it. */
AVM_API int stkNeeds(Value th, AuintIdx size);

// Implemented in avm_vm.cpp
/** Start a new Virtual Machine. Return the main thread */
AVM_API Value newVM(void);
/** Close down the virtual machine, freeing all allocated memory */
AVM_API void vm_close(Value th);

/** Start garbage collection */
AVM_API void mem_gcstart(Value th);
/** Stop garbage collection */
AVM_API void mem_gcstop(Value th);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif
