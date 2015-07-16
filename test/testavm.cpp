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

int test_cfunc(Value th) {
	t(stkSize(th)==1, "stkSize(th)==1");
	t(stkGet(th, 0)==aTrue, "stkGet(th, 0)==aTrue");

	// Test return of a value
	stkPush(th, aFalse);
	return 1;
}

enum stkit {
	true1,
	true2,
	false1,
	string1,
	string2,
	string3,
	array1,
	array2,
	tbl1,
	name,
	george,
	peter,
	weight,
	groglb,
	newth
};

void testRecent(Value th) {
	// Run with HardMemTest to test garbage collector

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

	// Thread tests - Data stack
	AuintIdx i = stkSize(th);
	stkNeeds(th, 40);
	t(stkSize(th)==i, "stkSize(th)==0");
	stkPush(th, aTrue);
	stkPushCopy(th, i);
	t(stkSize(th)==i+2, "stkSize(th)==2");
	t(stkGet(th,i)==aTrue, "stkGet(th,0)==aTrue");
	t(stkGet(th,i+1)==aTrue, "stkGet(th,1)==aTrue");
	stkSet(th, i+1, aFalse);
	t(stkGet(th,i+1)==aFalse, "stkGet(th,1)==aFalse");
	stkInsert(th, i, aSym(th, "self"));
	t(stkSize(th)==i+3, "stkSize(th)==3");
	t(stkGet(th,i+1)==aTrue, "stkGet(th,1)==aTrue");
	t(isSym(stkGet(th,i)), "isSym(stkGet(th,0))");
	stkRemove(th, i+1);
	t(stkSize(th)==i+2, "stkSize(th)==2");
	t(stkGet(th,i+1)==aFalse, "stkGet(th,1)==aFalse");
	t(isFalse(stkGet(th, stkFromTop(th, 0))), "isFalse(stkGet(th, stkFromTop(th, 0)))");
	t(isFalse(stkPop(th)), "isFalse(stkPop(th))");
	stkInsert(th, 0, aNull);
	stkPopTo(th, 0);
	t(isSym(stkGet(th, 0)), "isSym(stkGet(th, 0))");
	t(stkSize(th)==i+1, "stkSize(th)==1");
	stkSetSize(th, 4);
	t(stkSize(th)==i+4, "stkSize(th)==4");
	t(isNull(stkPop(th)), "isNull(stkPop(th))");
	stkSetSize(th, i);
	t(stkSize(th)==0, "stkSize(th)==0");

	// Symbol API tests
	t(!isSym(aNull), "!isSym(aNull)");
	t(!isSym(aTrue), "!isSym(aTrue)");
	stkPush(th, aSym(th, "true")); // true1
	stkPush(th, aSyml(th, "true", 4)); // true2
	stkPush(th, aSym(th, "false")); // false1
	t(isSame(stkGet(th, true1), stkGet(th, true2)), "aSym('true')==aSyml('true',4)");
	t(isSym(stkGet(th, true1)), "isSym(true1)");
	t(!isSame(stkGet(th, true2), false), "aSym('true')!=aSym(false')");
	t(getSize(stkGet(th, true1))==4, "getSize('true')==4");
	t(strEq(stkGet(th, false1),"false"), "strEq(aSym('false'),'false')");
	t(strcmp(toStr(stkGet(th, true2)),"true")==0, "toStr('true')=='true'");

	// String API tests
	t(!isStr(aNull), "!isSym(aNull)");
	t(!isStr(aTrue), "!isSym(aTrue)");
	stkPush(th, newStr(th, "Happiness is hard-won")); // string1
	stkPush(th, newStrl(th, "Happiness is hard-won", 21)); // string2
	stkPush(th, newStr(th, "True happiness requires work")); // string3
	t(!isSame(stkGet(th, string1),stkGet(th, string2)), "aStr('Happiness is hard-won')!=aStrl('Happiness is hard-won',21)");
	t(isStr(stkGet(th, string1)), "isStr(aStr('Happiness is hard-won'))");
	t(getSize(stkGet(th, string1))==21, "getSize('Happiness is hard-won')==21");
	t(strEq(stkGet(th, string1),"Happiness is hard-won"), "strEq(aStr('Happiness is hard-won'),'Happiness is hard-won')");
	t(strcmp(toStr(stkGet(th, string3)),"True happiness requires work")==0, "toStr('True happiness requires work')=='True happiness requires work'");
	strSub(th, stkGet(th, string2), 4, getSize(stkGet(th, string1))-4, NULL, 0); // Truncates size to 4
	t(getSize(stkGet(th, string2))==4, "getSize(strResize(string2, 4))==4");
	t(strEq(stkGet(th, string2),"Happ"), "strEq(strResize(string2, 4))=='Happ'");
	strSub(th, stkGet(th, string2), 4, 0, "y Birthday", 10); // Append
	t(strEq(stkGet(th, string2),"Happy Birthday"), "string2=='Happy Birthday'");
	strSub(th, stkGet(th, string2), 6, 0, "Pucking ", 8); // Insert
	t(strEq(stkGet(th, string2),"Happy Pucking Birthday"), "string2=='Happy Pucking Birthday'");
	strSub(th, stkGet(th, string2), 6, 2, "Fri", 3); // Replace & grow
	t(strEq(stkGet(th, string2), "Happy Fricking Birthday"), "string2=='Happy Fricking Birthday'");
	strSub(th, stkGet(th, string2), 6, 9, NULL, 0); // Delete
	t(strEq(stkGet(th, string2), "Happy Birthday"), "string2=='Happy Birthday'");

	// Array API tests
	stkPush(th, newArr(th, 10)); // array1
	t(!isArr(stkGet(th, string1)), "!isArr('a string')");
	t(isArr(stkGet(th, array1)), "isArr(array1)");
	t(getSize(stkGet(th, array1))==0, "getSize(array1)==0");
	arrRpt(th, stkGet(th, array1), 4, 2, aTrue); // Test Set: early elements will be aNull
	t(arrGet(th, stkGet(th, array1), 0)==aNull, "arrGet(th, array1, 0)==aNull");
	t(arrGet(th, stkGet(th, array1), 5)==aTrue, "arrGet(th, array1, 5)==aTrue");
	t(getSize(stkGet(th, array1))==6, "getSize(array1)==6");
	arrDel(th, stkGet(th, array1), 0, 4); // Test Del 
	arrDel(th, stkGet(th, array1), 1, 20); // should clip n
	t(arrGet(th, stkGet(th, array1), 0)==aTrue, "arrGet(th, array1, 0)==aTrue");
	t(getSize(stkGet(th, array1))==1, "getSize(array1)==1");
	arrDel(th, stkGet(th, array1), 1, 20); // Nop
	t(getSize(stkGet(th, array1))==1, "getSize(array1)==1");
	arrIns(th, stkGet(th, array1), 0, 2, aFalse);
	t(getSize(stkGet(th, array1))==3, "getSize(array1)==3");
	t(arrGet(th, stkGet(th, array1), 0)==aFalse, "arrGet(th, array1, 0)==aFalse");
	t(arrGet(th, stkGet(th, array1), 1)==aFalse, "arrGet(th, array1, 1)==aFalse");
	t(arrGet(th, stkGet(th, array1), 2)==aTrue, "arrGet(th, array1, 2)==aTrue");
	arrSub(th, stkGet(th, array1), 2, 0, stkGet(th, array1), 2, 1); // Insert from self
	t(getSize(stkGet(th, array1))==4, "getSize(array1)==4");
	t(arrGet(th, stkGet(th, array1), 3)==aTrue, "arrGet(th, array1, 3)==aTrue");
	stkPush(th, newArr(th,4)); // array2
	arrRpt(th, stkGet(th, array2), 4, 5, stkGet(th, string1));
	t(getSize(stkGet(th, array2))==9, "getSize(array2)==9");
	arrSub(th, stkGet(th, array1), 1, 2, stkGet(th, array2), 2, 4);
	t(getSize(stkGet(th, array1))==6, "getSize(array1)==6");
	t(getSize(arrGet(th, stkGet(th, array1), 4))==21, "getSize(arrGet(th, array1, 5))==21");

	// Table API tests
	stkPush(th, newTbl(th, 0)); // tbl1
	stkPush(th, aSym(th, "name")); // name
	stkPush(th, aSym(th, "George")); // george
	stkPush(th, aSym(th, "Peter")); // peter
	stkPush(th, aSym(th, "weight")); // weight
	t(!isTbl(stkGet(th, string1)), "!isTbl('a string')");
	t(isTbl(stkGet(th, tbl1)), "isTbl(hash1)");
	t(getSize(stkGet(th, tbl1))==0, "getSize(tbl1)==0");
	tblSet(th, stkGet(th, tbl1), stkGet(th, name),  stkGet(th, george)); // This will trigger table index growth
	t(getSize(stkGet(th, tbl1))==1, "getSize(tbl1)==1");
	t(tblGet(th, stkGet(th, tbl1), stkGet(th, name))==stkGet(th, george), "tblGet(th, tbl1, aSym(th, 'name'))==aSym(th, 'George')");
	tblSet(th, stkGet(th, tbl1), stkGet(th, name), stkGet(th, peter));
	t(getSize(stkGet(th, tbl1))==1, "getSize(tbl1)==1");
	t(tblGet(th, stkGet(th, tbl1), stkGet(th, name))==stkGet(th, peter), "tblGet(th, tbl1, aSym(th, 'name'))==aSym(th, 'Peter')");
	Value iter=aNull;
	iter = tblNext(stkGet(th, tbl1), iter); // Find first entry
	t(iter==stkGet(th, name), "iter==aSym(th, 'name')");
	iter = tblNext(stkGet(th, tbl1), iter); // End of entries
	t(iter==aNull, "iter==aNull");
	t(tblGet(th, stkGet(th, tbl1), stkGet(th, weight))==aNull, "tblGet(th, tbl1, aSym(th, 'weight'))==aNull"); // not found
	t(tblNext(stkGet(th, tbl1), stkGet(th, weight))==aNull, "tblNext(tbl1, aSym(th, 'weight'))==aNull"); // not found
	tblSet(th, stkGet(th, tbl1), aTrue, aFalse); // Bool as key
	t(aFalse == tblGet(th, stkGet(th, tbl1), aTrue), "aFalse == tblGet(th, tbl1, aTrue)");
	tblSet(th, stkGet(th, tbl1), anInt(23), anInt(24)); // Integer as key
	t(isInt(tblGet(th, stkGet(th, tbl1), anInt(23))), "isInt(tblGet(th, tbl1, anInt(23)))");
	tblSet(th, stkGet(th, tbl1), aFloat(258.f), aFloat(-0.f)); // Float as key
	t(isFloat(tblGet(th, stkGet(th, tbl1), aFloat(258.f))), "isFloat(tblGet(th, tbl1, aFloat(258.f)))");
	tblSet(th, stkGet(th, tbl1), stkGet(th, array1), stkGet(th, string3)); // Array as key
	arrSet(th, stkGet(th, array1), 6, aTrue); // Modify array, should still work as table key
	t(isStr(tblGet(th, stkGet(th, tbl1), stkGet(th, array1))), "isStr(tblGet(th, tbl1, array1))");
	t(getSize(stkGet(th, tbl1))==5, "getSize(tbl1)==5"); // table has doubled 4 times: 0->1->2->4->8
	tblSet(th, stkGet(th, tbl1), stkGet(th, name), aNull); // Delete 'name' entry
	t(tblGet(th, stkGet(th, tbl1), stkGet(th, name))==aNull, "tblGet(th, tbl1, aSym(th, 'name'))==aNull"); // not found
	t(getSize(stkGet(th, tbl1))==4, "getSize(tbl1)==4");

	// Thread tests - global namespace
	gloSetc(th, "$v", stkGet(th, array1));
	t(isArr(gloGetc(th, "$v")), "isArr(gloGetc(th, '$v'))");
	t(gloGetc(th, "$p")==aNull, "gloGetc(th, '$p')==aNull"); // unknown variable
	stkPush(th, growGlobal(th, 512));
	stkPush(th, newThread(th, stkGet(th, groglb), 20)); // newth
	t(isArr(gloGetc(stkGet(th, newth), "$v")), "isArr(gloGetc(newth, '$v'))"); // Does it shine through?
	gloSetc(stkGet(th, newth), "$v", stkGet(th, string1));
	t(isStr(gloGetc(stkGet(th, newth), "$v")), "isStr(gloGetc(newth, '$v'))"); // Can we change it?
	t(isArr(gloGetc(th, "$v")), "isArr(gloGetc(th, '$v'))"); // Still same in original thread?
	gloSetc(stkGet(th, newth), "$p", aTrue);
	t(gloGetc(stkGet(th, newth), "$p")==aTrue, "gloGetc(newth, '$p')==aTrue"); // Create in new thread
	t(gloGetc(th, "$p")==aNull, "gloGetc(th, '$p')==aNull"); // But not visible in original

	// C-function and Thread call stack tests
	i = stkSize(th);
	Value testcfn = aCFunc(th, test_cfunc, "test_cfunc", __FILE__);
	stkPush(th, testcfn);
	stkPush(th, aTrue); // Pass parameter
	thrCall(th, 1, 1);
	t(stkPop(th)==aFalse, "c-function return success: stkPop(th)==aFalse");
	t(stkSize(th)==i, "stkSize(th)==0");

	// Bytecode-function and Thread call stack tests
	Value testbfn = aBFunc(th, 2, aNull, aNull);
	stkPush(th, testbfn);
	thrCall(th, 0, 1); // Call with no parameters, will return puffed null
	t(stkPop(th)==aNull, "b-function return success: stkPop(th)==aNull");
	t(stkSize(th)==i, "stkSize(th)==0");
	stkPush(th, testbfn);
	stkPush(th, aTrue);
	stkPush(th, stkGet(th, array1));
	thrCall(th, 2, 1); // Call with parameters, will return array
	t(isArr(stkPop(th)), "b-function return success: isArr(stkPop(th)");
	t(stkSize(th)==i, "stkSize(th)==0");
	Value testbfnv = aBFunc(th, -2, aNull, aNull);
	stkPush(th, testbfnv);
	stkPush(th, aTrue);
	stkPush(th, stkGet(th, true1));
	thrCall(th, 2, 1); // Call with parameters, will return symbol
	t(isSym(stkPop(th)), "b-function return success: isSym(stkPop(th)");
	t(stkSize(th)==i, "stkSize(th)==0");

	// Type API tests - makes use of built-in types, which use the API to create types and its methods
	stkPush(th, anInt(50));
	stkPush(th, aSym(th, "+"));
	stkPush(th, anInt(40));
	thrCallMethod(th, 2, 1);
	t(stkPop(th)==anInt(90), "stkPop(th)==anInt(90)"); // Yay - first successful O-O request!
	t(isType(gloGetc(th, "Integer")), "isType(gloGetc(th, 'Integer'))");
	t(getType(th, gloGetc(th, "Integer"))==gloGetc(th, "Type"), "isType(getType(th, gloGetc(th, 'Integer'))==gloGetc(th, 'Type'))");
	vm_close(th);
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
