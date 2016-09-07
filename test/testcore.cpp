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

/** Initialize $test */
void core_test_init(Value th) {
	pushType(th, aNull, 4);
		pushCMethod(th, test_equal);
		popProperty(th, 0, "Equal");
	popGloVar(th, "$test");
	return;
}

void testCore(void) {
	Value th = newVM();
	core_test_init(th);

	pushSym(th, "()");
	pushGloVar(th, "Resource");
	pushString(th, aNull, "file://./testcore.acn");
	getCall(th, 2, 0);

	vmClose(th);
	printf("All %ld Core tests completed. %ld failed.\n", tests, fails);
}
