# acornvm - Acorn Virtual Machine

This lightweight library supports the execution of Acorn programs within a dedicated virtual machine.
Acorn is a dynamically typed, object-oriented language designed to make it easier to define
and animate interactive, web-based 3-D worlds.

Acorn Virtual Machine library features:

- Compilation of Acorn programs into byte-code.
- Execution of compiled byte-code (or C) methods or functions
- A object-oriented library for core types
- Automated memory management and garbage collection

It is designed to be embedded for use within another program, such as a Web3D browser or server.

## Documentation

The Acorn language has a comprehensive [reference document][acorn].

[Doxygen][] can be used to generate [API documentation][doc] from the source code.
The acorn_vm.conf Doxygen configuration file may be used to accomplish this.

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

The Acorn Virtual Machine (acornvm) is distributed under the terms of the MIT license. 
See LICENSE and COPYRIGHT for details.

[acorn]: http://web3d.jondgoodwin.com/acorn
[doc]: http://web3d.jondgoodwin.com/acorndoc
[web3d]: http://web3d.jondgoodwin.com
[doxygen]: http://doxygen.org
