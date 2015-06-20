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

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

	// #include "avm_xxx.h"

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif