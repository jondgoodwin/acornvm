# avm - Acorn Virtual Machine

This lightweight library supports the execution of Acorn programs within a dedicated virtual machine.
Acorn is a dynamically typed, object-oriented language designed to make it easier to define
and animate interactive, web-based 3-D worlds.

Acorn Virtual Machine library features:

- Compilation of Acorn programs into byte-code.
- Execution of compiled byte-code (or C) methods or functions
- A object-oriented library of core data types
- Automated memory management and garbage collection

It is designed to be embedded for use within another program, such as the Web3D browser or server.

## Documentation

The Acorn world-building language has a comprehensive [guide and reference document][acorn].
This includes a guide to the C-API available for implementing high-performance types.

[Doxygen][] can be used to generate API documentation from the source code.

This [website][web3d] offers additional information about the Web3D vision,
and Acorn's place in it.

## Building (Linux)

To build the library:

	make

1. By default, a local 'obj' directory is created, and all 'src' compiled into it.
2. A local 'lib' directory is created, and 'w3d_data.a' is created.
3. The test program is created in 'test' from its compiled source and library.

To run the test program:

	test/test_data
	
## Building (Windows)

A Visual C++ 2010 solutions file can be created using the project files. 
The generated object, library, and executable files are created relative to the location of the 
solutions file.

## License

Copyright (C) 2015  Jonathan Goodwin

 This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

[acorn]: http://web3d.jondgoodwin.com/acorn
[web3d]: http://web3d.jondgoodwin.com
[doxygen]: http://doxygen.org
