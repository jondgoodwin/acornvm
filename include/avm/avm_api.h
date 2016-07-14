/** Defines API accessible functions, beyond those in avm_value.h
 *
 * This defines all API functions that can be used by any C/C++ program that embeds
 * and runs the Acorn Virtual Machine, or by any C/C++ library that extends
 * the global environment, built-in types, or methods.
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
AVM_API Value getProperty(Value th, Value self, Value methsym);
/** Return the size of a symbol, array, hash, or other collection. Any other value type returns 0 */
AVM_API Auint getSize(Value val);

// Implemented in avm_string.cpp and avm_symbol.cpp
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
AVM_API int isEqStr(Value val, const char* str);
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
/** Return 1 if the value is an Array, otherwise 0 */
AVM_API int isArr(Value sym);
/** Return 1 if the value is an Closure, otherwise 0 */
AVM_API int isClosure(Value val);
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
/** Return 1 if the value is a Hash Table, otherwise 0 */
AVM_API int isTbl(Value val);
/** Return 1 if the value is a Type, otherwise 0 */
AVM_API int isType(Value val);
/* Return 1 if the value is a Prototype, otherwise 0 */
AVM_API int isPrototype(Value val);
/** Resize a table for more/fewer elements (cannot be less than used size) */
AVM_API void tblResize(Value th, Value tbl, AuintIdx newsize);
/** Return 1 if table has an entry at key, 0 if not */
AVM_API int tblHas(Value th, Value tbl, Value key);
/** Return the value paired with 'key', or 'null' if not found */
AVM_API Value tblGet(Value th, Value tbl, Value key);
/** Inserts or alters the table's 'key' entry with value. 
 * - Inserts 'key' if key is not already there
 * - Otherwise, it changes 'key' value */
AVM_API void tblSet(Value th, Value tbl, Value key, Value val);
/** Delete a key from hash table, if found. */
AVM_API void tblRemove(Value th, Value tbl, Value key);
/** Get the next sequential key/value pair in table after 'key'.
 * To sequentially traverse the table, start with 'key' of 'null'.
 * Each time called, the next key/value pair is returned.
 * After the last key, null is returned.
 * Warning: Accurate traversal requires the table remains unchanged.
*/
AVM_API Value tblNext(Value tbl, Value key);
/** For types, add mixin to top of list of types found at inheritype (and type)*/
AVM_API void addMixin(Value th, Value type, Value mixin);

// Implemented in avm_method.cpp
/** Return 1 if callable: a method or closure */
AVM_API int isCallable(Value val);
/** Call a method whose property symbol is placed on stack (with nparms above it). 
 * nexpected specifies how many return values to expect to find on stack.*/
AVM_API void methodCall(Value th, int nparms, int nexpected);
/** Call a 'set' method whose property symbol is placed on stack (with nparms above it). 
 * nexpected specifies how many return values to expect to find on stack.*/
AVM_API void methodSetCall(Value th, int nparms, int nexpected);

// Implemented in avm_thread.cpp
/** Return 1 if a Thread, else return 0 */
AVM_API int isThread(Value th);

// Implemented in avm_global.cpp
/** Push and return the symbolically-named global variable's value */
AVM_API Value pushGloVar(Value th, const char *var);
/** Alter the symbolically-named global variable to have the value popped off the local stack */
AVM_API void popGloVar(Value th, const char *var);
/** Push the value of the current process thread's global variable table. */
AVM_API Value pushGlobal(Value th);

// Implemented in avm_stack.cpp
/** Retrieve the stack value at the index. Be sure 0<= idx < top.
 * Good for getting method's parameters: 0=self, 1=parm 1, etc. */
AVM_API Value getLocal(Value th, AintIdx idx);
/** Put the value on the stack at the designated position. Be sure 0<= idx < top. */
AVM_API void setLocal(Value th, AintIdx idx, Value val);
/** Copy the stack value at fromidx into toidx */
AVM_API void copyLocal(Value th, AintIdx toidx, AintIdx fromidx);
/** Remove the value at index (shifting down all values above it to top) */
AVM_API void deleteLocal(Value th, AintIdx idx);
/** Insert the popped value into index (shifting up all values above it) */
AVM_API void insertLocal(Value th, AintIdx idx);
/** Push a value on the stack's top */
AVM_API Value pushValue(Value th, Value val);
/** Push and return the corresponding Symbol value for a 0-terminated c-string */
AVM_API Value pushSym(Value th, const char *str);
/** Push and return the corresponding Symbol value for a byte sequence of specified length */
AVM_API Value pushSyml(Value th, const char *str, AuintIdx len);
/** Push and return the value for a method written in C */
AVM_API Value pushCMethod(Value th, AcMethodp func);
/** Push and return a new String value */
AVM_API Value pushString(Value th, Value type, const char *str);
/** Push and return a new String value of size with a copy of str bytes */
AVM_API Value pushStringl(Value th, Value type, const char *str, AuintIdx size);
/** Push and return a new typed CData value of size */
AVM_API Value pushCData(Value th, Value type, AuintIdx size);
/** Push and return a new Array value */
AVM_API Value pushArray(Value th, Value type, AuintIdx size);
/** Push and return a new Closure value.
   Size is get and set methods plus closure variables, all pushed on stack */
AVM_API Value pushClosure(Value th, AintIdx size);
/** Push a closure variable. */
AVM_API Value pushCloVar(Value th, AuintIdx idx);
/** Pop a value into a closure variable. */
AVM_API void popCloVar(Value th, AuintIdx idx);
/** Push and return a new Table value */
AVM_API Value pushTbl(Value th, Value type, AuintIdx size);
/** Push and return a new Type value */
AVM_API Value pushType(Value th, Value type, AuintIdx size);
/* Push and return a new Mixin value */
AVM_API Value pushMixin(Value th, Value type, Value inheritype, AuintIdx size);
/** Push and return a new Stack value */
AVM_API Value pushThread(Value th);
/** Push and return the VM's value */
AVM_API Value pushVM(Value th);
/*( Push and return the value of the named member of the table found at the stack's specified index */
AVM_API Value pushMember(Value th, AintIdx tblidx, const char *mbrnm, Value val);
/** Put the local stack's top value into the named member of the table found at the stack's specified index */
AVM_API void popMember(Value th, AintIdx tblidx, const char *mbrnm);
/** Push a copy of a stack's value at index onto the stack's top */
AVM_API Value pushLocal(Value th, AintIdx idx);
/** Pop a value off the top of the stack */
AVM_API Value popValue(Value th);
/** Pops the top value and writes it at idx. Often used to set return value */
AVM_API void popLocal(Value th, AintIdx idx);
/** Obtain value "from top" index , where 0=top, 1 is next */
AVM_API Value getFromTop(Value th, AintIdx fromtop);
/** Return number of values on the current method's stack */
AVM_API AuintIdx getTop(Value th);
/** When index is positive, this indicates how many Values are on the method's stack.
 *	This can shrink the stack or grow it (padding with 'null's).
 *	A negative index removes that number of values off the top. */
AVM_API void setTop(Value th, AintIdx idx);
/** Ensure stack has room for 'needed' values above top. Returns 0 on failure. 
 * This may grow the stack, but never shrinks it. */
AVM_API int needMoreLocal(Value th, AuintIdx needed);

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
