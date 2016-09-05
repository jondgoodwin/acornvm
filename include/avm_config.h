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
#define AVM_SYMTBLMINSIZE	128

// Garbage Collection tuning
/** How many new objects will trigger start of a GC cycle */
#define GCNEWTRIGGER 200
/** How many objects converted from new to old will trigger a full collection */
#define GCOLDTRIGGER 1000
/** How much work to perform (at most) per GC step */
#define GCMAXSTEPCOST 500
/** Unit cost for marking an object's values */
#define GCMARKCOST 2
/** Unit cost for re-coloring a live object during a GC sweep */
#define GCSWEEPLIVECOST 1
/** Unit cost for freeing a dead object during a GC sweep */
#define GCSWEEPDEADCOST 12

/** Minimum expected room on data stack for c-methods */
#define STACK_MINSIZE 20
/** Extra just-in-case room on stack, to provide safety in case a method goes too far */
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
