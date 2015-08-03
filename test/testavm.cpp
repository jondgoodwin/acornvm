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

void testCapi(void);
void testGen(void);

void testAll(void) {
	testCapi();
	testGen();
}

int main(int argc, char **argv) {

	printf("Testing %d-bit %s\n", AVM_ARCH, AVM_RELEASE);
	testAll();
	// testGen();

	// Arbitrary pause so we see results in Visual Studio
	getchar();
	return 0;
}
