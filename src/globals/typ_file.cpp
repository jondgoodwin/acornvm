/** Implements the File type
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

/** '()': Get contents for passed filename path string */
int typ_file_get(Value th) {
	// Get string value of filename path
	Value fnval;
	if (getTop(th)<2 || (!isStr(fnval = getLocal(th,1)) && !isSym(fnval))) {
		pushValue(th, aNull);
		return 1;
	}
	const char *fn = toStr(fnval);
	if (0==strncmp(fn, "file://", 7))
		fn+=7;

	// Open the file - return null on failure
	FILE *file;
	if (!(file = fopen(fn, "rb"))) {
		vmLog("Cannot open 'file://' resource: %s", fn);
		pushValue(th, aNull);
		return 1;
	}

	// Determine the file length
	size_t size;
	fseek(file, 0, SEEK_END);
	size=ftell(file);
	fseek(file, 0, SEEK_SET);

	// Create the string buffer (which will be returned)
	Value strbuf = pushStringl(th, aNull, NULL, size);

	// Load the data into an allocated buffer
	fread(str_cstr(strbuf), 1, size, file);
	str_size(strbuf) = size;

	// Close the file
	fclose(file);
	return 1;
}

/** Initialize the File type */
void typ_file_init(Value th) {
	Value typ = pushType(th, vmlit(TypeType), 1);
		pushSym(th, "File");
		popProperty(th, 0, "_name");
		pushCMethod(th, typ_file_get);
		popProperty(th, 0, "()");
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

