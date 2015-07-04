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

/** Symbol table minimum size - starting size, in number of entries */
#define AVM_SYMTBLMINSIZE	4096

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

/** Percent for garbage collection pause */
#define AVM_GCPAUSE	200
/** Percent for garbage collection major sweep */
#define AVM_GCMAJOR	200
/** Percentage GC runs of memory allocation (200=twice the speed) */
#define AVM_GCMUL	200

/** Only define this if you are testing garbage collection. It forces full garbage collect before asking for more memory */
// #define AVM_GCHARDMEMTEST
#endif