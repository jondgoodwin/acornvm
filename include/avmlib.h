/** Use this in Acorn library C++ source files - gets all #includes
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef avmlib_h
#define avmlib_h

#include "avm_config.h"

/* By default, use AVM is loaded as a dynamic library */
#ifndef AVM_LIBRARY_STATIC
#define AVM_LIBRARY
#endif

#include "avm.h" // Includes avm_value.h and avm_api.h

#include "avm/avm_symbol.h" // Includes avm/avm_memory.h
#include "avm/avm_hash.h"
#include "avm/avm_vm.h"
#include "avm/avm_thread.h"

#endif