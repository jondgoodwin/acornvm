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
	struct CallInfo *previous, *next;	//< call stack chain links

	// Data stack pointers
	Value *funcbase;					//< Points to function value on stack
	Value *begin;						//< Points to function's parameters and local vars 
	Value *end;							//< Points to highest allocated area for function

	short nresults;						//< expected number of results from this function
	char callstatus;

	// Bytecode only
	Instruction *ip;					//< Pointer to current instruction
} CallInfo;

/*
 * CallInfo callstatus flags 
 */
#define CIST_BYTECODE	(1<<0)	/* call is running a Bytecode function */
#define CIST_HOOKED	(1<<1)	/* call is running a debug hook */
#define CIST_REENTRY	(1<<2)	/* call is running on same invocation of
                                   execute of previous call */
#define CIST_YIELDED	(1<<3)	/* call reentered after suspension */
#define CIST_YPCALL	(1<<4)	/* call is a yieldable protected call */
#define CIST_STAT	(1<<5)	/* call has an error status (pcall) */
#define CIST_TAIL	(1<<6)	/* call was tail called */
#define CIST_HOOKYIELD	(1<<7)	/* last hook called yielded */


/** Information about a Thread */
typedef struct ThreadInfo {
	MemCommonInfo;

	// VM and global namespace
	VmInfo *vm;			//< Virtual machine that thread is part of
	Value global;		//< thread's global namespace

	// Data stack pointers
	// Note: "size" is the data stack's allocated size
	Value *stack;		//< Points to the lowest value on the stack
	Value *stk_top;		//< Points to the next available value on the stack
	Value *stk_last;	//< Points to EXTRA slots below the highest stack value

	// Call stack
	CallInfo *curfn;	//< Call info for current function
	CallInfo entryfn;	//< Call info for C-function that started this thread
} ThreadInfo;

/** Turn the thread value into a pointer */
#define th(th) ((ThreadInfo*) th)

/** Point to thread's vm info */
#define vm(th) (((ThreadInfo*)th)->vm)

/** Internal function to re-allocate stack's size */
void stkRealloc(Value th, int newsize);

/** Restore call and data stack after call, copying return values down 
 * nreturned is how many values the called function actually returned */
void thrReturn(Value th, int nreturned);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif