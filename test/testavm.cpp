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

long tests = 0;
long fails = 0;
void t(int test, const char *text) {
	tests++;
	if (!test) {
		printf("'%s' test failed!\n", text);
		fails++;
	}
}

void testRecent(Value th) {
	// Integer value primitive tests
	t(isInt(anInt(-1000)), "isInt(anInt(-1000))");
	t(!isSame(anInt(-8), anInt(2)), "!isSame(anInt(-8), anInt(2))");
	t(isSame(anInt(0), anInt(0)), "isSame(anInt(0), anInt(0))");
	t(-1001 == toAint(anInt(-1001)), "-1001 == toAint(anInt(-1001))");

	// Float value primitive tests
	t(!isFloat(anInt(1654)), "!isFloat(anInt(1654))");
	t(isFloat(aFloat(102.03f)), "isFloat(aFloat(102.03))");
	t(!isSame(aFloat(-8.), aFloat(0.25)), "!isSame(aFloat(-8.), aFloat(0.25))");
	t(isSame(aFloat(20.2f), aFloat(10.1f*2.0f)), "isSame(aFloat(20.2), aFloat(10.1*2.0))");
	t(-1001. == toAfloat(aFloat(-1001.f)), "-1001. == toAfloat(aFloat(-1001.))");

	// Null, false and true tests
	t(isNull(aNull), "isNull(aNull)");
	t(!isNull(anInt(-10)), "!isNull(anInt(-10))");
	t(isFalse(aNull), "isFalse(aNull)");
	t(isFalse(aFalse), "isFalse(aFalse)");
	t(!isFalse(aTrue), "!isFalse(aTrue)");
	t(!isFalse(anInt(405)), "!isFalse(anInt(405))");
	t(isBool(aTrue), "isBool(aTrue)");
	t(isBool(aFalse), "isBool(aFalse)");
	t(!isBool(aNull), "!isBool(aNull)");

	// Symbol API tests
	t(!isSym(aNull), "!isSym(aNull)");
	t(!isSym(aTrue), "!isSym(aTrue)");
	Value true1 = aSym(th, "true");
	Value true2 = aSyml(th, "true", 4);
	Value false1 = aSym(th, "false");
	t(isSame(true1, true2), "aSym('true')==aSyml('true',4)");
	t(isSym(true1), "isSym(true1)");
	t(!isSame(true2, false), "aSym('true')!=aSym(false')");
	t(strSz(true1)==4, "strSz('true')==4");
	t(strEq(false1,"false"), "strEq(aSym('false'),'false')");
	t(strcmp(toStr(true2),"true")==0, "toStr('true')=='true'");
}

void testAll(Value th) {
	testRecent(th);
}

int main(int argc, char **argv) {
	Value th;

	printf("Testing %d-bit %s\n", AVM_ARCH, AVM_RELEASE);
	th = newVM();
	testAll(th);
	printf("All %ld tests completed. %ld failed.\n", tests, fails);

	// Arbitrary pause ...
	getchar();
	return 0;
}
