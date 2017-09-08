/** Provides access to the external Acorn C-API.
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * Copyright (C) 2017  Jonathan Goodwin
 *
 * Permission is hereby granted, free of charge, to any
 * person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the
 * Software without restriction, including without
 * limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software
 * is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice
 * shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 * ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
 * SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE. 
 */

#ifndef avm_h
#define avm_h

#define AVM_VERSION_MAJOR	"0"		//!< Major version number
#define AVM_VERSION_MINOR	"2"		//!< Minor version number
#define AVM_VERSION_NUM		0		//!< Version number
#define AVM_VERSION_RELEASE	"10"		//!< Version release

#define AVM_VERSION	"AcornVM " AVM_VERSION_MAJOR "." AVM_VERSION_MINOR		//!< Full version string
#define AVM_RELEASE	AVM_VERSION "." AVM_VERSION_RELEASE		//!< Full version+release string
#define AVM_COPYRIGHT	AVM_RELEASE "  Copyright (C) 2017 Jonathan Goodwin"		//!< Name + version + copyright string

#include "avm/avm_memory.h"
#include "avm/avm_api.h"

#endif
