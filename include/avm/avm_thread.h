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

	Value method;						//!< The method being executed in this frame

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

	// Data stack pointers
	// Note: "size" is the data stack's allocated size
	Value *stack;		//!< Points to the lowest value on the stack
	Value *stk_top;		//!< Points to the next available value on the stack
	Value *stk_last;	//!< Points to EXTRA slots below the highest stack value

	// Call stack
	Value yieldTo;			//!< Thread to yield back to
	CallInfo *curmethod;	//!< Call info for current method
	CallInfo entrymethod;	//!< Call info for C-method that started this thread
} ThreadInfo;

/* flags1 flags */
// 0x80 reserved for Locked
#define ThreadYielder 0x40	//!< Flags1 bit, if thread is a yielder
#define ThreadThread  0x20	//!< Flags1 bit, if thread is a thread
#define ThreadDone    0x10	//!< Flags1 bit, if thread has finished

/** Mark all in-use thread values for garbage collection 
 * Increments how much allocated memory the thread uses. */
#define thrMark(th, t) \
	{if ((t)->stack) { \
	for (Value *stkp = (t)->stk_top - 1; stkp >= (t)->stack; stkp--) \
		mem_markobj(th, *stkp); \
	} mem_markobj(th, (t)->yieldTo);}

/** Free all of an array's allocated memory */
#define thrFree(th, t) \
	{assert(th!=t && "Never sweep thread we are using"); \
	thrFreeStacks(t); \
	mem_free(th, (t));}

/** Turn the thread value into a pointer */
#define th(th) ((ThreadInfo*) th)

/** Point to thread's vm info */
#define vm(th) (((ThreadInfo*)th)->vm)

/** Is thread a yielder? */
#define isYielder(th) (isThread(th) && (((ThreadInfo*)th)->flags1) & ThreadYielder)

/** Is thread finished running */
#define thrIsDone(th) (isThread(th) && (((ThreadInfo*)th)->flags1) & ThreadDone)

/** Internal function to re-allocate stack's size */
void stkRealloc(Value th, int newsize);

/** Initialize a thread */
void thrInit(ThreadInfo* thr, VmInfo* vm, Value method, AuintIdx stksz, char flags);

/** Free everything allocated for thread */
void thrFreeStacks(Value th);

/** Internal routine to allocate and append a new CallInfo structure to end of call stack */
CallInfo *thrGrowCI(Value th);

/** Retrieve a value from global namespace */
Value gloGet(Value th, Value var);
/** Add or change a global variable */
void gloSet(Value th, Value var, Value val);

/** Create a new Thread with a starter stack. */
Value newThread(Value th, Value *dest, Value method, AuintIdx stksz, char flags);
/** Push and return a new CompInfo value, compiler state for an Acorn method */
Value pushCompiler(Value th, Value src, Value url);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif