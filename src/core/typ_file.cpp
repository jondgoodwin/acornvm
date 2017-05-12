/** File type methods and properties
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
 */

#include "avmlib.h"
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/** Get contents for passed filename path string */
int file_get(Value th) {
	int nparms = getTop(th);
	// Get string value of filename path
	Value fnval;
	if (nparms<2 || (!isStr(fnval = getLocal(th,1)) && !isSym(fnval))) {
		pushValue(th, aNull);
		return 1;
	}
	const char *fn = toStr(fnval);
	if (0==strncmp(fn, "file://", 7))
		fn+=7;

	// Open the file - return null on failure
	FILE *file;
	if (!(file = fopen(fn, "rb"))) {
		// Do callback, passing null for stream and an error diagnostic
		if (nparms>2 && canCall(getLocal(th,2))) {
			pushLocal(th, 2);
			pushValue(th, aNull);
			pushValue(th, aNull);
			pushString(th, aNull, "File open fails.");
			getCall(th, 3, 0);
		}
		return 0;
	}

	// Determine the file length (so we can accurately allocate buffer)
	fseek(file, 0, SEEK_END);
	size_t size=ftell(file);
	fseek(file, 0, SEEK_SET);

	// Load the data into an allocated Text string and close file
	Value strbuf = pushStringl(th, vmlit(TypeTextm), NULL, size);
	fread(str_cstr(strbuf), 1, size, file);
	str_size(strbuf) = size;
	fclose(file);

	// Call the success method, passing it the stream
	if (nparms>2 && canCall(getLocal(th,2))) {
		pushLocal(th, 2);
		pushValue(th, aNull);
		pushValue(th, strbuf);
		getCall(th, 2, 0);
	}

	return 1;
}

/** Initialize the File type */
void core_file_init(Value th) {
	Value typ = pushType(th, vmlit(TypeObject), 4);
		pushSym(th, "File");
		popProperty(th, 0, "_name");
		pushCMethod(th, file_get);
		popProperty(th, 0, "Get");
	popGloVar(th, "File");

	// Register this type as Resource's 'file' scheme
	pushGloVar(th, "Resource");
		pushProperty(th, getTop(th) - 1, "schemes");
			pushValue(th, typ);
			popTblSet(th, getTop(th) - 2, "file");
		popValue(th);
	popValue(th);
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

