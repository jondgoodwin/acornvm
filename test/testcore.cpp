/* Perform TDD / Regression tests against the Acorn Virtual Machine library.
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

static long tests = 0;
static long fails = 0;

int test_equal(Value th) {
	tests++;
	if (getTop(th)<4) {
		printf("Insufficient parameters for $test.Equal\n");
		fails++;
		return 0;
	}
	Value val1 = getLocal(th, 1);
	Value val2 = getLocal(th, 2);
	Value msg = getLocal(th, 3);
	if (val1 != val2) {
		printf("'%s' test failed!\n", toStr(msg));
		fails++;
	}
	return 0;
}

int test_true(Value th) {
	tests++;
	if (getTop(th)<3) {
		printf("Insufficient parameters for $test.True\n");
		fails++;
		return 0;
	}
	Value val = getLocal(th, 1);
	Value msg = getLocal(th, 2);
	if (isFalse(val)) {
		printf("'%s' test failed!\n", toStr(msg));
		fails++;
	}
	return 0;
}

int test_serialize(Value th) {
	pushSerialized(th, getLocal(th, 1));
	puts(toStr(getFromTop(th, 0)));
	popValue(th);
	return 0;
}

/** Initialize $test */
void core_test_init(Value th) {
	pushType(th, aNull, 4);
		pushCMethod(th, test_equal);
		popProperty(th, 0, "Equal");
		pushCMethod(th, test_true);
		popProperty(th, 0, "True");
		pushCMethod(th, test_serialize);
		popProperty(th, 0, "Serialize");
	popGloVar(th, "$test");
	return;
}

void testLang(void) {
	fails = tests = 0;
	Value th = newVM();
	core_test_init(th);

	pushSym(th, "Load");
	pushSym(th, "New");
	pushGloVar(th, "Resource");
	pushString(th, aNull, "file://./testlang.acn");
	getCall(th, 2, 1);
	getCall(th, 1, 0);

	vmClose(th);
	printf("All %ld Language tests completed. %ld failed.\n", tests, fails);
}

void testCore(void) {
	fails = tests = 0;
	Value th = newVM();
	core_test_init(th);

	pushSym(th, "Load");
	pushSym(th, "New");
	pushGloVar(th, "Resource");
	pushString(th, aNull, "file://./testcore.acn");
	getCall(th, 2, 1);
	getCall(th, 1, 0);

	vmClose(th);
	printf("All %ld Core tests completed. %ld failed.\n", tests, fails);
}
