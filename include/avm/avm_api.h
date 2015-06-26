/** Defines API accessible functions, beyond those in avm_value.h
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

/** Return symbol for a c-string. */
AVM_API Value aSym(Value th, const char *str);
/** Return symbol for a byte-sequence. */
AVM_API Value aSyml(Value th, const char *str, Auint32 len);
/** Return string value for a c-string. */
AVM_API Value aStr(Value th, const char *str);
/** Return string value for a byte-sequence. str may be NULL (to reserve space for empty string). */
AVM_API Value aStrl(Value th, const char *str, Auint32 len);
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
/** Return the byte size of a symbol or string. Any other value type returns 0 */
AVM_API Auint strSz(Value val);
/** Iterate to next symbol after key in symbol table (or first if key is NULL). Return Null if no more. 
 * This can be used to sequentially iterate through the symbol table.
 * Results may be inaccurate if the symbol table is changed during iteration.
 */
AVM_API Value sym_next(Value th, Value key);
/** Resize string to a specified length, truncating as needed.
 * Allocated space will not shrink, but may expand if required.
 */
AVM_API void strResize(Value th, Value val, Auint32 len);
/**	Replace part of a string with the c-string contents starting at pos.
 *	If sz==0, it becomes an insert. If str==NULL or len==0, it becomes a deletion.
 *	The Acorn string will be resized automatically to accommodate excess characters.
 *	The operation will not be performed if resizing is not possible. */
AVM_API void strSub(Value th, Value val, Auint32 pos, Auint32 sz, const char *str, Auint32 len);
	
/** Start a new Virtual Machine. Return the main thread */
AVM_API Value newVM(void);

	/* TO DO:
	<h3>Type checking</h3>
	<p>In many cases, use getGlosym("type") to retrieve the value for a specific type.</p>
	<ul>
	<li><b>int ofType(Value, Value)</b> -
	Return 1 if the first value's type is what is specified by second value, otherwise 0.</li>
	<li><b>int hasType(Value, Value)</b> -
	Return 1 if the first value uses the type specified by the second value (mixin or parents), otherwise 0.</li>
	<li><b>int hasMethod(Value, Value)</b> -
	Return 1 if the first value implements the method specified by the second value's symbol, otherwise 0.</li>
	<li><b>Value whatType(Value)</b> -
	Return the values Type value.</li>
	</ul>
	
	<h3>Cast a Value to C datatype</h3>
	<p>None of these do any type-checking. They assume the value is of the correct type.</p>
	<ul>
	<li><b>char* toStr(Value)</b> -
	Return a read-only pointer into a C-string encoded by a symbol or byte-oriented Value.</li>
	</ul>
	
	<h3>Value creation</h3>
	<p>In addition to the pre-defined value aNull, aFalse, and aTrue, 
	these functions create values:</p>
	<ul>
	<li><b>Value aSym(char*)</b> -
	Return the corresponding Acorn Symbol value.</li>
	<li><b>Value newProp(Value, Value)</b> -
	Return a new Property value whose key and value are specified.</li>
	<li><b>Value newPropC(char*, char*)</b> -
	Return a new Property value whose key and value are specified.</li>
	<li><b>Value newRangeC(aInt, aInt)</b> -
	Return a new Range value whose from and to are specified.</li>
	<li><b>Value newTuple(Value, Value, ...)</b> -
	Return a new Tuple value comprised of all parameter values.</li>
	<li><b>Value aNewP(Value, Value)</b> -
	Return a new value where first value is the Type and second is the Tupled parameters.</li>
	</ul>

	<h3>Collections</h3>
	<ul>
	<li><b>Value aNew(char*)</b> -
	Return a new (empty) value where string is the global Type.</li>
	<li><b>Value newCStr(char*, char*)</b> -
	Return a new value, where first string is the type and second is its bytes value.</li>
	<li><b>int append(Value, Value)</b> -
	Equivalent to "+=". Appends the second value to the first value's collection. 
	Return the second value if appended, otherwise null.</li>
	<li><b>int isCollection(Value)</b> -
	Return 1 if the value is a collection. Otherwise, return 0.</li>
	<li><b>int isEmpty(Value)</b> -
	Return 1 if the value is empty or not a collection. Otherwise, return 0.</li>
	<li><b>Aint sizeOf(Value)</b> -
	Equivalent to '.size'. Return the size of the collection. Otherwise, return 0.</li>
	<li><b>Value getVal(Value, Value, ...)</b> -
	Equivalent to '()'. Get from a collection (first value), passing subsequent values as parameters.
	Return what that gives you (or null).</li>
	<li><b>Value setVal(Value, Value, Value)</b> -
	Equivalent to '()='. Place the third value into the second value's index into the first value's collection.
	Return third value, or null if not placed.</li>
	<li><b>Value forEach(Value, Afunc)</b> -
	Retrieve each value from the first value's collection, and pass as a parameter to the callable C-function.</li>
	<li><b>Value anIter(Value)</b> -
	Get an iterator into the value's collection, or else null.</li>
	<li><b>Value getNext(Value)</b> -
	Get the collection value pointed at in the value's iterator.
	The iterator will be altered to the next element of the collection, or null if there is not one.</li>
	</ul>
	
	
	<h3>Functions, Methods, and Types</h3>
	<p>Generally, use Global APIs (below) to get and put Types.</p>
	<ul>
	<li><b>Value newTypeC(char*)</b> -
	Return a new Type and save it in global with symbol name of string.</li>
	<li><b>int isType(Value)</b> -
	Return 1 if the first value is a Type value, otherwise 0.</li>
	<li><b>Value newCFunc(Afunc, int)</b> -
	Return a callable anonymous c-function value with a certain number of parameters.</li>
	<li><b>int isFunc(Value)</b> -
	Return 1 if the value is an Acorn function, 2 if a C-function, otherwise 0.</li>
	<li><b>Value newCMethodC(char*, Afunc, int)</b> -
	Return a named callable method value with a certain number of parameters (including one for self).</li>
	<li><b>Value newCMethodC(Value, Afunc, int)</b> -
	Return a named callable method value with a certain number of parameters (including one for self).</li>
	<li><b>int isMethod(Value)</b> -
	Return 1 if the value is an Acorn method, 2 if a C-method, otherwise 0.</li>
	<li><b>Value doFunc(Value, Value)</b> -
	Equivalent to '()'. Call function (first value), passing second Tuple as parameters.
	Return what that gives you (or null).</li>
	<li><b>Value doMethodC(char*, Value, Value, Value)</b> -
	Invoke the string's method symbol on second value (self), passing third Tuple value as parameters.
	Return what that gives you (or null).</li>
	<li><b>Value doMethod(Value, Value, Value)</b> -
	Invoke the first value's method symbol on second value (self), passing third Tuple value as parameters.
	Return what that gives you (or null).</li>
	</ul>

	<h3>Global</h3>
	<p>Useful for environmental variables and most types.</p>
	<ul>
	<li><b>Value getGlobal(Value)</b> -
	Get the global value, using the value as a key.</li>
	<li><b>Value getGloSym(char*)</b> -
	Get the global value, using the string as a symbol key.</li>
	<li><b>Value setGlobal(Value, Value)</b> -
	Set the global value whose key is the first value with the second value.</li>
	<li><b>Value setGlosym(char*, Value)</b> -
	Set the global value whose symbol key is the string with the value.</li>
	</ul>
	*/

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif
