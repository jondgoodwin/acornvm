/** Implements the File type
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
 */

#include "avmlib.h"
#include <stdio.h>

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/** Get contents for passed filename string or Url */
int typ_file_get(Value th) {
	Value fnval = getLocal(th,0);

	// Assume fnval is a string for now
	char *fn = isStr(fnval)? str_cstr(fnval) : NULL;

	// Open the file - return null on failure
	FILE *file;
	if (fn==NULL || !(file = fopen(fn, "rb"))) {
		pushValue(th, aNull);
		return 1;
	}

	// Determine the file length
	long int size;
	fseek(file, 0, SEEK_END);
	size=ftell(file);
	fseek(file, 0, SEEK_SET);

	// Create the string buffer (which will be returned)
	Value strbuf;
	pushValue(th, strbuf = newStrl(th, "", size));

	// Load the data into an allocated buffer
	fread(str_cstr(strbuf), 1, size, file);

	// Close the file
	fclose(file);
	return 1;
}

/** Initialize the File type */
Value typ_file_init(Value th) {
	Value typ = pushType(th);
	addCPropfn(th, typ, "get", typ_file_get, "File::get");
	popGlobal(th, "File");
	return typ;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

