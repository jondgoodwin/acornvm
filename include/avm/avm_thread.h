/** Implements threads, which manage execution, the stack state, and global namespace
 *
 * The thread's global namespace holds all its global variables (the environment
 * variables and types). For the main thread, it is simply a table, with 
 * variable names as symbols, paired with their values. Other threads can
 * 'grow' the namespace into an Array of tables. Changes are restricted to the 
 * first table, but variables can be found in any of the Array's tables.
 * (see avm_global.cpp)
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_thread_h
#define avm_thread_h

#include "avm/avm_memory.h"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/** A single entry on the thread's call stack */
typedef struct CallInfo {
	struct CallInfo *previous;			//!< call stack chain back link
	struct CallInfo *next;				//!< call stack chain next link

	// Data stack pointers
	Value *methodbase;					//!< Points to method value, just below varargs
	Value *retTo;						//!< Where to place return values
	Value *begin;						//!< Points to method's parameters and local vars 
	Value *end;							//!< Points to highest allocated area for method

	// Bytecode only
	Instruction *ip;					//!< Pointer to current instruction

	short nresults;						//!< expected number of results from this method
	// char callstatus;					//!< flags indicating call status for this frame
} CallInfo;

/** Information about a Thread */
typedef struct ThreadInfo {
	MemCommonInfoGray;	//!< Common header for markable objects

	// VM and global namespace
	VmInfo *vm;			//!< Virtual machine that thread is part of
	Value global;		//!< thread's global namespace

	// Data stack pointers
	// Note: "size" is the data stack's allocated size
	Value *stack;		//!< Points to the lowest value on the stack
	Value *stk_top;		//!< Points to the next available value on the stack
	Value *stk_last;	//!< Points to EXTRA slots below the highest stack value

	// Call stack
	CallInfo *curmethod;	//!< Call info for current method
	CallInfo entrymethod;	//!< Call info for C-method that started this thread
} ThreadInfo;

/** Mark all in-use thread values for garbage collection 
 * Increments how much allocated memory the thread uses. */
#define thrMark(th, t) \
	{for (Value *stkp = (t)->stk_top - 1; stkp >= (t)->stack; stkp--) \
		mem_markobj(th, *stkp); \
	mem_markobj(th, (t)->global); \
	vm(th)->gcmemtrav += sizeof(ThreadInfo) + sizeof(Value) * (t)->size;}

/** Free all of an array's allocated memory */
#define thrFree(th, t) \
	{thrFreeStacks(t); \
	mem_free(th, (t));}

/** Turn the thread value into a pointer */
#define th(th) ((ThreadInfo*) th)

/** Point to thread's vm info */
#define vm(th) (((ThreadInfo*)th)->vm)

/** Internal function to re-allocate stack's size */
void stkRealloc(Value th, int newsize);

/** Initialize a thread */
void thrInit(ThreadInfo* thr, VmInfo* vm, AuintIdx stksz);

/** Free everything allocated for thread */
void thrFreeStacks(Value th);

/** Internal routine to allocate and append a new CallInfo structure to end of call stack */
CallInfo *thrGrowCI(Value th);

/** Retrieve a value from global namespace */
Value gloGet(Value th, Value var);
/** Add or change a global variable */
void gloSet(Value th, Value var, Value val);

/** Create a new Thread with a starter stack. */
Value newThread(Value th, Value *dest, AuintIdx stksz);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif