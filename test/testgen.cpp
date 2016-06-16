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
	// Run with HardMemTest to test garbage collector

	// Bytecode-function and Thread call stack tests
	Value testbfn = genTestPgm(th, 0);
	stkPush(th, testbfn);
	funcCall(th, 0, 1); // Call with no parameters, will return puffed null
	t(stkPop(th)==aNull, "b-function return success: stkPop(th)==aNull");
	t(stkSize(th)==0, "stkSize(th)==0");

	// Call function that tests loader
	stkPush(th, testbfn);
	stkPush(th, anInt(4));
	funcCall(th, 1, 4); // Call with 1 parameter, will return 4
	t(stkPop(th)==aFloat(3.14f), "b-function return success: stkPop(th)==3.14");
	t(stkPop(th)==aFloat(3.14f), "stkPop(th)==3.14");
	t(stkPop(th)==aTrue, "stkPop(th)==true");
	t(toAint(stkPop(th))==4, "stkPop(th)==4");
	t(stkSize(th)==0, "stkSize(th)==0");

	// Call function for 1 fixed and 2 varargs
	stkPush(th, genTestPgm(th,1));
	stkPush(th, anInt(1));
	stkPush(th, anInt(2));
	stkPush(th, anInt(3));
	funcCall(th, 3, 3); // Call with 3 parameters, will return 3
	t(stkPop(th)==anInt(3), "stkPop(th)==3");
	t(stkPop(th)==anInt(2), "stkPop(th)==2");
	t(stkPop(th)==anInt(1), "stkPop(th)==1");
	t(stkSize(th)==0, "stkSize(th)==0");

	// Call fibonacci calculator with (conditional) jumps and calls
	stkPush(th, genTestPgm(th,2));
	stkPush(th, anInt(4));
	funcCall(th, 1, 1);
	t(stkPop(th)==anInt(5), "stkPop(th)==5");
	t(stkSize(th)==0, "stkSize(th)==0");

	// Call recursive factorial program with tailcall
	stkPush(th, genTestPgm(th,3));
	stkPush(th, anInt(4));
	funcCall(th, 1, 1);
	t(stkPop(th)==anInt(24), "stkPop(th)==24");
	t(stkSize(th)==0, "stkSize(th)==0");

	// Call List build and for loop program (summing its numbers)
	stkPush(th, genTestPgm(th,4));
	stkPush(th, anInt(4));
	funcCall(th, 1, 1);
	t(stkPop(th)==anInt(20), "stkPop(th)==20");
	t(stkSize(th)==0, "stkSize(th)==0");

	// File, Url etc
	stkPush(th, genTestPgm(th,5));
	funcCall(th, 0, 0);

	vm_close(th);
	printf("All %ld Gen tests completed. %ld failed.\n", tests, fails);
}
