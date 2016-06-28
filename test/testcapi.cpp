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
	t(getTop(th)==1, "getTop(th)==1");
	t(getLocal(th, 0)==aTrue, "getLocal(th, 0)==aTrue");

	// Test return of a value
	pushValue(th, aFalse);
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
	weight
};

void testCapi(void) {
	Value th = newVM();
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
	AuintIdx i = getTop(th);
	needMoreLocal(th, 40);
	t(getTop(th)==i, "getTop(th)==0");
	pushValue(th, aTrue);
	pushLocal(th, i);
	t(getTop(th)==i+2, "getTop(th)==2");
	t(getLocal(th,i)==aTrue, "getLocal(th,0)==aTrue");
	t(getLocal(th,i+1)==aTrue, "getLocal(th,1)==aTrue");
	setLocal(th, i+1, aFalse);
	t(getLocal(th,i+1)==aFalse, "getLocal(th,1)==aFalse");
	pushValue(th, aSym(th, "self"));
	t(isSym(getLocal(th,i+2)), "isSym(getLocal(th,2))");
	insertLocal(th, i);
	t(getTop(th)==i+3, "getTop(th)==3");
	t(getLocal(th,i+1)==aTrue, "getLocal(th,1)==aTrue");
	t(isSym(getLocal(th,i)), "isSym(getLocal(th,0))");
	deleteLocal(th, i+1);
	t(getTop(th)==i+2, "getTop(th)==2");
	t(getLocal(th,i+1)==aFalse, "getLocal(th,1)==aFalse");
	t(isFalse(getFromTop(th, 0)), "isFalse(getFromTop(th, 0))");
	t(isFalse(popValue(th)), "isFalse(popValue(th))");
	pushValue(th, aNull);
	insertLocal(th, i);
	popLocal(th, i);
	t(isSym(getLocal(th, i)), "isSym(getLocal(th, 0))");
	t(getTop(th)==i+1, "getTop(th)==1");
	setTop(th, 4);
	t(getTop(th)==i+4, "getTop(th)==4");
	t(isNull(popValue(th)), "isNull(popValue(th))");
	setTop(th, i);
	t(getTop(th)==0, "getTop(th)==0");

	// Symbol API tests
	t(!isSym(aNull), "!isSym(aNull)");
	t(!isSym(aTrue), "!isSym(aTrue)");
	pushValue(th, aSym(th, "true")); // true1
	pushValue(th, aSyml(th, "true", 4)); // true2
	pushValue(th, aSym(th, "false")); // false1
	t(isSame(getLocal(th, true1), getLocal(th, true2)), "aSym('true')==aSyml('true',4)");
	t(isSym(getLocal(th, true1)), "isSym(true1)");
	t(!isSame(getLocal(th, true2), false), "aSym('true')!=aSym(false')");
	t(getSize(getLocal(th, true1))==4, "getSize('true')==4");
	t(strEq(getLocal(th, false1),"false"), "strEq(aSym('false'),'false')");
	t(strcmp(toStr(getLocal(th, true2)),"true")==0, "toStr('true')=='true'");

	// String API tests
	t(!isStr(aNull), "!isSym(aNull)");
	t(!isStr(aTrue), "!isSym(aTrue)");
	pushValue(th, newStr(th, "Happiness is hard-won")); // string1
	pushValue(th, newStrl(th, "Happiness is hard-won", 21)); // string2
	pushValue(th, newStr(th, "True happiness requires work")); // string3
	t(!isSame(getLocal(th, string1),getLocal(th, string2)), "aStr('Happiness is hard-won')!=aStrl('Happiness is hard-won',21)");
	t(isStr(getLocal(th, string1)), "isStr(aStr('Happiness is hard-won'))");
	t(getSize(getLocal(th, string1))==21, "getSize('Happiness is hard-won')==21");
	t(strEq(getLocal(th, string1),"Happiness is hard-won"), "strEq(aStr('Happiness is hard-won'),'Happiness is hard-won')");
	t(strcmp(toStr(getLocal(th, string3)),"True happiness requires work")==0, "toStr('True happiness requires work')=='True happiness requires work'");
	strSub(th, getLocal(th, string2), 4, getSize(getLocal(th, string1))-4, NULL, 0); // Truncates size to 4
	t(getSize(getLocal(th, string2))==4, "getSize(strResize(string2, 4))==4");
	t(strEq(getLocal(th, string2),"Happ"), "strEq(strResize(string2, 4))=='Happ'");
	strSub(th, getLocal(th, string2), 4, 0, "y Birthday", 10); // Append
	t(strEq(getLocal(th, string2),"Happy Birthday"), "string2=='Happy Birthday'");
	strSub(th, getLocal(th, string2), 6, 0, "Pucking ", 8); // Insert
	t(strEq(getLocal(th, string2),"Happy Pucking Birthday"), "string2=='Happy Pucking Birthday'");
	strSub(th, getLocal(th, string2), 6, 2, "Fri", 3); // Replace & grow
	t(strEq(getLocal(th, string2), "Happy Fricking Birthday"), "string2=='Happy Fricking Birthday'");
	strSub(th, getLocal(th, string2), 6, 9, NULL, 0); // Delete
	t(strEq(getLocal(th, string2), "Happy Birthday"), "string2=='Happy Birthday'");

	// Array API tests
	pushValue(th, newArr(th, 10)); // array1
	t(!isArr(getLocal(th, string1)), "!isArr('a string')");
	t(isArr(getLocal(th, array1)), "isArr(array1)");
	t(getSize(getLocal(th, array1))==0, "getSize(array1)==0");
	arrRpt(th, getLocal(th, array1), 4, 2, aTrue); // Test Set: early elements will be aNull
	t(arrGet(th, getLocal(th, array1), 0)==aNull, "arrGet(th, array1, 0)==aNull");
	t(arrGet(th, getLocal(th, array1), 5)==aTrue, "arrGet(th, array1, 5)==aTrue");
	t(getSize(getLocal(th, array1))==6, "getSize(array1)==6");
	arrDel(th, getLocal(th, array1), 0, 4); // Test Del 
	arrDel(th, getLocal(th, array1), 1, 20); // should clip n
	t(arrGet(th, getLocal(th, array1), 0)==aTrue, "arrGet(th, array1, 0)==aTrue");
	t(getSize(getLocal(th, array1))==1, "getSize(array1)==1");
	arrDel(th, getLocal(th, array1), 1, 20); // Nop
	t(getSize(getLocal(th, array1))==1, "getSize(array1)==1");
	arrIns(th, getLocal(th, array1), 0, 2, aFalse);
	t(getSize(getLocal(th, array1))==3, "getSize(array1)==3");
	t(arrGet(th, getLocal(th, array1), 0)==aFalse, "arrGet(th, array1, 0)==aFalse");
	t(arrGet(th, getLocal(th, array1), 1)==aFalse, "arrGet(th, array1, 1)==aFalse");
	t(arrGet(th, getLocal(th, array1), 2)==aTrue, "arrGet(th, array1, 2)==aTrue");
	arrSub(th, getLocal(th, array1), 2, 0, getLocal(th, array1), 2, 1); // Insert from self
	t(getSize(getLocal(th, array1))==4, "getSize(array1)==4");
	t(arrGet(th, getLocal(th, array1), 3)==aTrue, "arrGet(th, array1, 3)==aTrue");
	pushValue(th, newArr(th,4)); // array2
	arrRpt(th, getLocal(th, array2), 4, 5, getLocal(th, string1));
	t(getSize(getLocal(th, array2))==9, "getSize(array2)==9");
	arrSub(th, getLocal(th, array1), 1, 2, getLocal(th, array2), 2, 4);
	t(getSize(getLocal(th, array1))==6, "getSize(array1)==6");
	t(getSize(arrGet(th, getLocal(th, array1), 4))==21, "getSize(arrGet(th, array1, 5))==21");

	// Table API tests
	pushValue(th, newTbl(th, 0)); // tbl1
	pushValue(th, aSym(th, "name")); // name
	pushValue(th, aSym(th, "George")); // george
	pushValue(th, aSym(th, "Peter")); // peter
	pushValue(th, aSym(th, "weight")); // weight
	t(!isTbl(getLocal(th, string1)), "!isTbl('a string')");
	t(isTbl(getLocal(th, tbl1)), "isTbl(hash1)");
	t(getSize(getLocal(th, tbl1))==0, "getSize(tbl1)==0");
	tblSet(th, getLocal(th, tbl1), getLocal(th, name),  getLocal(th, george)); // This will trigger table index growth
	t(getSize(getLocal(th, tbl1))==1, "getSize(tbl1)==1");
	t(tblGet(th, getLocal(th, tbl1), getLocal(th, name))==getLocal(th, george), "tblGet(th, tbl1, aSym(th, 'name'))==aSym(th, 'George')");
	tblSet(th, getLocal(th, tbl1), getLocal(th, name), getLocal(th, peter));
	t(getSize(getLocal(th, tbl1))==1, "getSize(tbl1)==1");
	t(tblGet(th, getLocal(th, tbl1), getLocal(th, name))==getLocal(th, peter), "tblGet(th, tbl1, aSym(th, 'name'))==aSym(th, 'Peter')");
	Value iter=aNull;
	iter = tblNext(getLocal(th, tbl1), iter); // Find first entry
	t(iter==getLocal(th, name), "iter==aSym(th, 'name')");
	iter = tblNext(getLocal(th, tbl1), iter); // End of entries
	t(iter==aNull, "iter==aNull");
	t(tblGet(th, getLocal(th, tbl1), getLocal(th, weight))==aNull, "tblGet(th, tbl1, aSym(th, 'weight'))==aNull"); // not found
	t(tblNext(getLocal(th, tbl1), getLocal(th, weight))==aNull, "tblNext(tbl1, aSym(th, 'weight'))==aNull"); // not found
	tblSet(th, getLocal(th, tbl1), aTrue, aFalse); // Bool as key
	t(aFalse == tblGet(th, getLocal(th, tbl1), aTrue), "aFalse == tblGet(th, tbl1, aTrue)");
	tblSet(th, getLocal(th, tbl1), anInt(23), anInt(24)); // Integer as key
	t(isInt(tblGet(th, getLocal(th, tbl1), anInt(23))), "isInt(tblGet(th, tbl1, anInt(23)))");
	tblSet(th, getLocal(th, tbl1), aFloat(258.f), aFloat(-0.f)); // Float as key
	t(isFloat(tblGet(th, getLocal(th, tbl1), aFloat(258.f))), "isFloat(tblGet(th, tbl1, aFloat(258.f)))");
	tblSet(th, getLocal(th, tbl1), getLocal(th, array1), getLocal(th, string3)); // Array as key
	arrSet(th, getLocal(th, array1), 6, aTrue); // Modify array, should still work as table key
	t(isStr(tblGet(th, getLocal(th, tbl1), getLocal(th, array1))), "isStr(tblGet(th, tbl1, array1))");
	t(getSize(getLocal(th, tbl1))==5, "getSize(tbl1)==5"); // table has doubled 4 times: 0->1->2->4->8
	tblSet(th, getLocal(th, tbl1), getLocal(th, name), aNull); // Delete 'name' entry
	t(tblGet(th, getLocal(th, tbl1), getLocal(th, name))==aNull, "tblGet(th, tbl1, aSym(th, 'name'))==aNull"); // not found
	t(getSize(getLocal(th, tbl1))==4, "getSize(tbl1)==4");

	// Thread tests - global namespace
	gloSetc(th, "$v", getLocal(th, array1));
	t(isArr(gloGetc(th, "$v")), "isArr(gloGetc(th, '$v'))");
	t(gloGetc(th, "$p")==aNull, "gloGetc(th, '$p')==aNull"); // unknown variable

	// C-function and Thread call stack tests
	i = getTop(th);
	Value testcfn = aCFunc(th, test_cfunc, "test_cfunc", __FILE__);
	pushValue(th, testcfn);
	pushValue(th, aTrue); // Pass parameter
	funcCall(th, 1, 1);
	t(popValue(th)==aFalse, "c-function return success: popValue(th)==aFalse");
	t(getTop(th)==i, "getTop(th)==0");

	// Type API tests - makes use of built-in types, which use the API to create types and its methods
	pushValue(th, aSym(th, "+"));
	pushValue(th, anInt(50));
	pushValue(th, anInt(40));
	funcCall(th, 2, 1);
	t(popValue(th)==anInt(90), "popValue(th)==anInt(90)"); // Yay - first successful O-O request!
	t(isType(gloGetc(th, "Integer")), "isType(gloGetc(th, 'Integer'))");
	t(getType(th, gloGetc(th, "Integer"))==gloGetc(th, "Type"), "isType(getType(th, gloGetc(th, 'Integer'))==gloGetc(th, 'Type'))");
	vm_close(th);
	printf("All %ld C-API tests completed. %ld failed.\n", tests, fails);
}