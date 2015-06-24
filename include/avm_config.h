/** Defines Acorn VM configurable properties - change as needed for optimal operation
 * Included by avmlib.h, as only appropriate when compiling the VM source code.
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avm_config_h
#define avm_config_h

/** Symbol table minimum size - starting size, in number of entries */
#define AVM_SYMTBLMINSIZE	4096

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