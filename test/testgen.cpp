/* Perform TDD / Regression byte-code generation tests
 *
 * This source file is not part of avm - Acorn Virtual Machine.
*/

#define AVM_LIBRARY_STATIC
#include <avm.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
using namespace avm;
#endif

extern "C" Value genTestPgm(Value th, int pgm);

static long tests = 0;
static long fails = 0;
static void t(int test, const char *text) {
	tests++;
	if (!test) {
		printf("'%s' test failed!\n", text);
		fails++;
	}
}

void testGen(void) {
	Value th = newVM();
	// Run with HardMemTest to test garbage collecto

	// Bytecode-function and Thread call stack tests
	Value testbfn = genTestPgm(th, 0);
	pushValue(th, testbfn);
	funcCall(th, 0, 1); // Call with no parameters, will return puffed null
	t(popValue(th)==aNull, "b-function return success: popValue(th)==aNull");
	t(getTop(th)==0, "getTop(th)==0");

	// Call function that tests loade
	pushValue(th, testbfn);
	pushValue(th, anInt(4));
	funcCall(th, 1, 4); // Call with 1 parameter, will return 4
	t(popValue(th)==aFloat(3.14f), "b-function return success: popValue(th)==3.14");
	t(popValue(th)==aFloat(3.14f), "popValue(th)==3.14");
	t(popValue(th)==aTrue, "popValue(th)==true");
	t(toAint(popValue(th))==4, "popValue(th)==4");
	t(getTop(th)==0, "getTop(th)==0");

	// Call function for 1 fixed and 2 varargs
	pushValue(th, genTestPgm(th,1));
	pushValue(th, anInt(1));
	pushValue(th, anInt(2));
	pushValue(th, anInt(3));
	funcCall(th, 3, 3); // Call with 3 parameters, will return 3
	t(popValue(th)==anInt(3), "popValue(th)==3");
	t(popValue(th)==anInt(2), "popValue(th)==2");
	t(popValue(th)==anInt(1), "popValue(th)==1");
	t(getTop(th)==0, "getTop(th)==0");

	// Call fibonacci calculator with (conditional) jumps and calls
	pushValue(th, genTestPgm(th,2));
	pushValue(th, anInt(4));
	funcCall(th, 1, 1);
	t(popValue(th)==anInt(5), "popValue(th)==5");
	t(getTop(th)==0, "getTop(th)==0");

	// Call recursive factorial program with tailcall
	pushValue(th, genTestPgm(th,3));
	pushValue(th, anInt(4));
	funcCall(th, 1, 1);
	t(popValue(th)==anInt(24), "popValue(th)==24");
	t(getTop(th)==0, "getTop(th)==0");

	// Call List build and for loop program (summing its numbers)
	pushValue(th, genTestPgm(th,4));
	pushValue(th, anInt(4));
	funcCall(th, 1, 1);
	t(popValue(th)==anInt(20), "popValue(th)==20");
	t(getTop(th)==0, "getTop(th)==0");

	// File, Url etc
	pushValue(th, genTestPgm(th,5));
	funcCall(th, 0, 0);

	vm_close(th);
	printf("All %ld Gen tests completed. %ld failed.\n", tests, fails);
}
