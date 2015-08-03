/** Bytecode generator for Acorn compiler
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef acn_main_h
#define acn_main_h

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

// ***********
// Non-API C-Function functions
// ***********

/** A temporary function for generating byte-code test programs */
AVM_API Value genTestPgm(Value th, int pgm);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif