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
	t(getSize(true1)==4, "getSize('true')==4");
	t(strEq(false1,"false"), "strEq(aSym('false'),'false')");
	t(strcmp(toStr(true2),"true")==0, "toStr('true')=='true'");

	// String API tests
	t(!isStr(aNull), "!isSym(aNull)");
	t(!isStr(aTrue), "!isSym(aTrue)");
	Value string1 = newStr(th, "Happiness is hard-won");
	Value string2 = newStrl(th, "Happiness is hard-won", 21);
	Value string3 = newStr(th, "True happiness requires work");
	t(!isSame(string1,string2), "aStr('Happiness is hard-won')!=aStrl('Happiness is hard-won',21)");
	t(isStr(string1), "isStr(aStr('Happiness is hard-won'))");
	t(getSize(string1)==21, "getSize('Happiness is hard-won')==21");
	t(strEq(string1,"Happiness is hard-won"), "strEq(aStr('Happiness is hard-won'),'Happiness is hard-won')");
	t(strcmp(toStr(string3),"True happiness requires work")==0, "toStr('True happiness requires work')=='True happiness requires work'");
	strSub(th, string2, 4, getSize(string1)-4, NULL, 0); // Truncates size to 4
	t(getSize(string2)==4, "getSize(strResize(string2, 4))==4");
	t(strEq(string2,"Happ"), "strEq(strResize(string2, 4))=='Happ'");
	strSub(th, string2, 4, 0, "y Birthday", 10); // Append
	t(strEq(string2,"Happy Birthday"), "string2=='Happy Birthday'");
	strSub(th, string2, 6, 0, "Pucking ", 8); // Insert
	t(strEq(string2,"Happy Pucking Birthday"), "string2=='Happy Pucking Birthday'");
	strSub(th, string2, 6, 2, "Fri", 3); // Replace & grow
	t(strEq(string2, "Happy Fricking Birthday"), "string2=='Happy Fricking Birthday'");
	strSub(th, string2, 6, 9, NULL, 0); // Delete
	t(strEq(string2, "Happy Birthday"), "string2=='Happy Birthday'");

	// Array API tests
	Value array1 = newArr(th, 10);
	t(getTuple(array1)==aFalse, "getTuple(array1)==aFalse");
	setTuple(array1, aTrue);
	t(getTuple(array1)==aTrue, "getTuple(array1)==aTrue");
	t(!isArr(string1), "!isArr('a string')");
	t(isArr(array1), "isArr(array1)");
	t(getSize(array1)==0, "getSize(array1)==0");
	arrSet(th, array1, 4, 2, aTrue); // Test Set: early elements will be aNull
	t(arrGet(th, array1, 0)==aNull, "arrGet(th, array1, 0)==aNull");
	t(arrGet(th, array1, 5)==aTrue, "arrGet(th, array1, 5)==aTrue");
	t(getSize(array1)==6, "getSize(array1)==6");
	arrDel(th, array1, 0, 4); // Test Del 
	arrDel(th, array1, 1, 20); // should clip n
	t(arrGet(th, array1, 0)==aTrue, "arrGet(th, array1, 0)==aTrue");
	t(getSize(array1)==1, "getSize(array1)==1");
	arrDel(th, array1, 1, 20); // Nop
	t(getSize(array1)==1, "getSize(array1)==1");
	arrIns(th, array1, 0, 2, aFalse);
	t(getSize(array1)==3, "getSize(array1)==3");
	t(arrGet(th, array1, 0)==aFalse, "arrGet(th, array1, 0)==aFalse");
	t(arrGet(th, array1, 1)==aFalse, "arrGet(th, array1, 1)==aFalse");
	t(arrGet(th, array1, 2)==aTrue, "arrGet(th, array1, 2)==aTrue");
	arrSub(th, array1, 2, 0, array1, 2, 1); // Insert from self
	t(getSize(array1)==4, "getSize(array1)==4");
	t(arrGet(th, array1, 3)==aTrue, "arrGet(th, array1, 3)==aTrue");
	Value array2 = newArr(th,4);
	arrSet(th, array2, 4, 5, string1);
	t(getSize(array2)==9, "getSize(array2)==9");
	arrSub(th, array1, 1, 2, array2, 2, 4);
	t(getSize(array1)==6, "getSize(array1)==6");
	t(getSize(arrGet(th, array1, 4))==21, "getSize(arrGet(th, array1, 5))==21");

	// Hash API tests
	Value tbl1 = newTbl(th, 0);
	t(!isTbl(string1), "!isTbl('a string')");
	t(isTbl(tbl1), "isTbl(hash1)");
	t(getSize(tbl1)==0, "getSize(tbl1)==0");
	tblSet(th, tbl1, aSym(th, "name"),  aSym(th,"George")); // This will trigger table index growth
	t(getSize(tbl1)==1, "getSize(tbl1)==1");
	t(tblGet(th, tbl1, aSym(th, "name"))==aSym(th, "George"), "tblGet(th, tbl1, aSym(th, 'name'))==aSym(th, 'George')");
	Value name = newStr(th, "name");
	tblSet(th, tbl1, name, aSym(th, "Peter")); // Should replace George with Peter, despite being a string
	t(getSize(tbl1)==1, "getSize(tbl1)==1");
	t(tblGet(th, tbl1, aSym(th, "name"))==aSym(th, "Peter"), "tblGet(th, tbl1, aSym(th, 'name'))==aSym(th, 'Peter')");
	Value iter=aNull;
	iter = tblNext(tbl1, iter); // Find first entry
	t(iter==aSym(th, "name"), "iter==aSym(th, 'name')");
	iter = tblNext(tbl1, iter); // End of entries
	t(iter==aNull, "iter==aNull");
	t(tblGet(th, tbl1, aSym(th, "weight"))==aNull, "tblGet(th, tbl1, aSym(th, 'weight'))==aNull"); // not found
	t(tblNext(tbl1, aSym(th, "weight"))==aNull, "tblNext(tbl1, aSym(th, 'weight'))==aNull"); // not found
	tblSet(th, tbl1, aTrue, aFalse); // Bool as key
	t(aFalse == tblGet(th, tbl1, aTrue), "aFalse == tblGet(th, tbl1, aTrue)");
	tblSet(th, tbl1, anInt(23), anInt(24)); // Integer as key
	t(isInt(tblGet(th, tbl1, anInt(23))), "isInt(tblGet(th, tbl1, anInt(23)))");
	tblSet(th, tbl1, aFloat(258.f), aFloat(-0.f)); // Float as key
	t(isFloat(tblGet(th, tbl1, aFloat(258.f))), "isFloat(tblGet(th, tbl1, aFloat(258.f)))");
	tblSet(th, tbl1, array1, string3); // Array as key
	arrSet(th, array1, 6, 0, aTrue); // Modify array, should still work as table key
	t(isStr(tblGet(th, tbl1, array1)), "isStr(tblGet(th, tbl1, array1))");
	t(getSize(tbl1)==5, "getSize(tbl1)==5"); // table has doubled 4 times: 0->1->2->4->8
	tblSet(th, tbl1, aSym(th, "name"), aNull); // Delete 'name' entry
	t(tblGet(th, tbl1, aSym(th, "name"))==aNull, "tblGet(th, tbl1, aSym(th, 'name'))==aNull"); // not found
	t(getSize(tbl1)==4, "getSize(tbl1)==4"); 
	
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
