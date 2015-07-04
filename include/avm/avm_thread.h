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
	Value *stk_base;					//< function index in the stack
	Value *top;							//< highest indexable positive stack value 
	struct CallInfo *previous, *next;	//< call stack chain links
	short nresults;						//< expected number of results from this function
	char callstatus;

	// Acorn code only
	Value *base;						//< Start of function's parameters and local vars 
//	Instruction *ip;					//< Pointer to current instruction
} CallInfo;

/** Information about a Thread */
typedef struct ThreadInfo {
	MemCommonInfo;

	// VM and global namespace
	VmInfo *vm;			//< Virtual machine that thread is part of
	Value global;		//< thread's global namespace

	// Data stack
	// Note that "size" is the data stack's allocated size
	Value *stack;		//< Points to the lowest value on the stack
	Value *stk_last;	//< Points to EXTRA slots below the highest stack value
	Value *stk_top;		//< Points to the next available value on the stack

	// Call stack
	CallInfo *curfn;	//< Call info for current function
	CallInfo entryfn;	//< Call info for C-function that started this thread
} ThreadInfo;

/** Turn the thread value into a pointer */
#define th(th) ((ThreadInfo*) th)

/** Point to thread's vm info */
#define vm(th) (((ThreadInfo*)th)->vm)

void stkRealloc(Value th, int newsize);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif