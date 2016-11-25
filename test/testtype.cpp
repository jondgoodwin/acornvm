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
	pushSym(th, "New");
	pushGloVar(th, "Resource");
	pushSym(th, url);
	if (strlen(baseurl)) pushSym(th, baseurl); else pushValue(th, aNull);
	getCall(th, 3, 1);
	Value resource = getLocal(th, getTop(th)-1);

	// Test that resource generates the correct url */
	pushSym(th, "url");
	pushValue(th, resource);
	getCall(th, 1, 1);
	const char *madeurl = toStr(popValue(th));
	tstrcmp(normurl, madeurl, "Resource failure: Expected '%s', but made '%s'\n");

	popValue(th); // resource

}

void testResource(Value th) {
	// No baseurl. Treat url as absolute url (and fill in default scheme, resource name & extension
	restest(th, "ftp://fileman.com/afile.jpg", "", "ftp://fileman.com/afile.jpg");
	restest(th, "/coolbeans.gif", "", "file:///coolbeans.gif");
	restest(th, "c:/something.acn", "", "file:///c:/something.acn");
	restest(th, "./world.acn", "", "file:///./world.acn");
	restest(th, "./scene", "", "file:///./scene.acn");
	restest(th, "animals.edu/giraffe.acn", "", "http://animals.edu/giraffe.acn");
	restest(th, "www.funkyworld.com", "", "http://www.funkyworld.com/world.acn");
	restest(th, "http://www.funkyworld.com", "", "http://www.funkyworld.com/world.acn");
	restest(th, "funkyworld.com/image.jpg", "", "http://funkyworld.com/image.jpg");
	// If url has scheme, ignore baseurl
	restest(th, "http://abc.def/world.acn", "http://domain.com/funkypoo", "http://abc.def/world.acn");
	restest(th, "http://abc.def", "http://domain.com/funkypoo", "http://abc.def/world.acn");
	// Relative urls
	restest(th, "clue.acn", "file:///c:/user/jond.acn", "file:///c:/user/clue.acn");
	restest(th, "afile.acn", "http://newworld.com/world.acn", "http://newworld.com/afile.acn");
	restest(th, "aworld.org/flea.ttp", "file://another.org/x.y", "file://another.org/aworld.org/flea.ttp");
	restest(th, "/punkymoo", "http://domain.com/funkypoo.acn", "http://domain.com/punkymoo.acn");
	restest(th, "/punkymoo/", "http://domain.com/funkypoo.acn", "http://domain.com/punkymoo/");
	restest(th, "afile.acn", "http://newworld.com/folder/", "http://newworld.com/folder/afile.acn");
	restest(th, "afile.acn", "http://newworld.com/x.acn?query#anchor", "http://newworld.com/afile.acn");
	restest(th, "base.acn?query#fragment", "http://domain.com/funkypoo.acn", "http://domain.com/base.acn?query");
	// Relative urls that use . and ..  and /
	restest(th, "/monster.acn", "http://domain.com/funkypoo/palder", "http://domain.com/monster.acn");
	restest(th, "./monster.acn", "http://domain.com/funkypoo/palder", "http://domain.com/funkypoo/monster.acn");
	restest(th, "../monster.acn", "http://domain.com/funkypoo/palder", "http://domain.com/monster.acn");
	restest(th, "../../monster.acn", "http://domain.com/funkypoo/x/mealymouth", "http://domain.com/monster.acn");
}

void testType(void) {
	Value th = newVM();

	testResource(th);

	vmClose(th);
	printf("All %ld Type tests completed. %ld failed.\n", tests, fails);
}
