/** Defines Acorn VM configurable properties - change as needed for optimal operation
 * Included by avmlib.h, as only appropriate when compiling the VM source code.
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_config_h
#define avm_config_h

/** Initial allocated slots for global variables */
#define GLOBAL_NEWSIZE    256

/** Smallest size for "arrays", like a block of code */
#define MINSIZEARRAY	4

/** Symbol table minimum size - starting size, in number of entries */
#define AVM_SYMTBLMINSIZE	4096

// Garbage Collection tuning
/** Controls how long the collector waits before starting a new cycle. 
 * Larger values make the collector less aggressive. 
 * Values smaller than 100 mean the collector will not wait to start a new cycle. 
 * A value of 200 means that the collector waits for the total memory in use to double 
 * before starting a new cycle. */
#define AVM_GCPAUSE	200  /* 200% */
/** When generational garbage collection should do a major vs. minor collection cycle */
#define AVM_GCMAJOR	200  /* 200% */
/** Controls the relative speed of the collector relative to memory allocation. 
 * Larger values make the collector more aggressive but also increase the size of each incremental step. 
 * Values smaller than 100 make the collector too slow and can result 
 * in the collector never finishing a cycle. 
 * The default is 200, which means that the collector runs at "twice" the speed of memory allocation. */
#define AVM_GCMUL	200 /* GC runs 'twice the speed' of memory allocation */

/** Minimum expected room on data stack c-functions */
#define STACK_MINSIZE 20
/** Extra just-in-case room on stack, to provide safety in case a function goes too far */
#define STACK_EXTRA 5
/** Initial size of a stack for a new thread */
#define STACK_NEWSIZE 256
/** Maximum size of stack */
#define STACK_MAXSIZE 16384
/** Maximum size of stack under error recovery */
#define STACK_ERRORSIZE (STACK_MAXSIZE+200)

/** 2^AVM_STRHASHLIMIT is roughly max number of bytes used to compute string hash */
#define AVM_STRHASHLIMIT	5

/** Only define this if you are testing garbage collection. It forces full garbage collect before asking for more memory */
// #define AVM_GCHARDMEMTEST
#endif
