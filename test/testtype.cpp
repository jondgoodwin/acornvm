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
static void t(int test, const char *text) {
	tests++;
	if (!test) {
		printf("'%s' test failed!\n", text);
		fails++;
	}
}
static void tstrcmp(const char *str1, const char *str2, const char *text) {
	tests++;
	if (0!=strcmp(str1, str2)) {
		printf(text, str1, str2);
		fails++;
	}
}

void restest(Value th, const char* url, const char *baseurl, const char *normurl) {
	// Create the resource
	pushSym(th, "new");
	pushGloVar(th, "Resource");
	pushSym(th, url);
	if (strlen(baseurl)) pushSym(th, baseurl); else pushValue(th, aNull);
	methodCall(th, 3, 1);
	Value resource = getLocal(th, getTop(th)-1);

	// Test that resource generates the correct url */
	pushSym(th, "url");
	pushValue(th, resource);
	methodCall(th, 1, 1);
	const char *madeurl = toStr(popValue(th));
	tstrcmp(normurl, madeurl, "Resource failure: Expected '%s', but made '%s'\n");

	popValue(th); // resource

}

void testResource(Value th) {
	// No baseurl. Make the most of url
	restest(th, "ftp:afile.jpg", "", "ftp:afile.jpg");
	restest(th, "/coolbeans.gif", "", "file:///coolbeans.gif");
	restest(th, "www.funkyworld.com", "", "http://www.funkyworld.com");
	restest(th, "/c:/something.acn", "", "file:///c:/something.acn");
	restest(th, "animals.edu/giraffe.acn", "", "http://animals.edu/giraffe.acn");
	restest(th, "funkyworld.com/image.jpg", "", "http://funkyworld.com/image.jpg");
	// If url is given scheme or authority, ignnore baseurl
	restest(th, "http://abc.def", "http://domain.com/funkypoo", "http://abc.def");
	restest(th, "//aworld.org", "file://another.org", "http://aworld.org");
	restest(th, "//aworld.org/flea.ttp", "file://another.org/x.y", "http://aworld.org/flea.ttp");
	// Relative urls
	restest(th, "clue.acn", "file:///c:/user/jond", "file:///c:/user/clue.acn");
	restest(th, "afile.acn", "newworld.com", "http://newworld.com/afile.acn");
	restest(th, "/punkymoo.acn", "http://domain.com/funkypoo.acn", "http://domain.com/punkymoo.acn");
	restest(th, "afile.acn", "newworld.com/folder/", "http://newworld.com/folder/afile.acn");
	restest(th, "afile.acn", "newworld.com/x.acn?query#anchor", "http://newworld.com/afile.acn");
	restest(th, "base.acn?query#fragment", "//domain.com/funkypoo.acn", "http://domain.com/base.acn?query");
	// Relative urls that use . and ..
	restest(th, "../monster.acn", "//domain.com/funkypoo/palder", "http://domain.com/monster.acn");
	restest(th, "../../monster.acn", "//domain.com/funkypoo/mealymouth", "http://domain.com/monster.acn");
}

void testType(void) {
	Value th = newVM();

	testResource(th);

	vm_close(th);
	printf("All %ld Type tests completed. %ld failed.\n", tests, fails);
}
