/** Provides access to the external Acorn C-API.
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * Copyright (C) 2017  Jonathan Goodwin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef avm_h
#define avm_h

#define AVM_VERSION_MAJOR	"0"		//!< Major version number
#define AVM_VERSION_MINOR	"2"		//!< Minor version number
#define AVM_VERSION_NUM		0		//!< Version number
#define AVM_VERSION_RELEASE	"7"		//!< Version release

#define AVM_VERSION	"AcornVM " AVM_VERSION_MAJOR "." AVM_VERSION_MINOR		//!< Full version string
#define AVM_RELEASE	AVM_VERSION "." AVM_VERSION_RELEASE		//!< Full version+release string
#define AVM_COPYRIGHT	AVM_RELEASE "  Copyright (C) 2017 Jonathan Goodwin"		//!< Name + version + copyright string

#include "avm/avm_memory.h"
#include "avm/avm_api.h"

#endif
