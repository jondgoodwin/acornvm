/** Resource type methods and properties
 * The resource instance is an array holding the resource's url information.
 * The resource type and instance mixin have properties for manipulating
 * actual resources found at those addresses. These work through the resource's
 * scheme type and extension type, as determined by the url.
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
 */

#include "avmlib.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/** Index of the values contained in the Resource array */
enum resourceValues {
	ResNormUrl,			//!< (symbol) The normalized url (without the anchor)
	ResFragment,		//!< (symbol) The url's anchor
	ResSchemeType,		//!< (type) The scheme type
	ResExtType,			//!< (type) The extension type
	nResVals			//!< number of values in the Resource array
};

/** Lexical state when scanning a Url's characters */
enum UrlState {
	UScheme,	//!< Scheme state
	UAuth,		//!< Authority state
	UPath,		//!< Path state
	UQuery,		//!< Query state
	UFrag		//!< Anchor state
};

/** Build a new resource (an array) by creating a normalized url 
  from url and baseurl (for resolving a relative address).
  It then populates the resource array with extracts from the normalized url */
void newResource(Value th, const char *url, Value baseurl, Value *resarray) {
	AintIdx stktop = getTop(th);
	const char *urlscanp, *urlbeg;
	const char *basescanp, *basebeg;
	const char *basefilep = NULL;
	int basenslash = 0;
	int isrelative = 0;
	const char *schemep;
	size_t schemel = 0;
	const char *authp;
	size_t authl = 0;
	const char *extp;
	size_t extl;

	// **** Scan url to identify key pieces
	urlbeg = urlscanp = authp = url;
	int urlstate = UScheme;
	const char *lastdotp = NULL;
	const char *queryp = NULL;
	const char *fragp = NULL;
	while (*urlscanp) {
		switch (*urlscanp) {
		case ':':
			if (urlstate == UScheme) {
				urlstate = UAuth;
				schemep = urlbeg; // url has it
				schemel = urlscanp-urlbeg;
				authp = urlscanp;
			}
			break;
		case '/':
			// Ignore optional '//' at beginning of authority
			if (urlscanp[1] == '/' && urlscanp==&urlbeg[schemel? schemel+1 : schemel]) {
				urlstate = UAuth;
				authp = urlscanp + 2;
				urlscanp++;
			}
			else {
				if (urlstate <= UAuth) {
					urlstate = UPath;
					authl = urlscanp - authp;
				}
				if (urlstate == UPath)
					lastdotp = NULL; // Reset as we only want last dot on resource name
			}
			break;
		case '.':
			if (urlstate == UScheme)
				urlstate = UAuth; // Clearly no longer doing a scheme
			if (urlstate <= UPath) {
				lastdotp = urlscanp; // Remember last '.' in path for extension
			}
			break;
		case '?':
			if (urlstate < UQuery) {
				urlstate = UQuery;
				queryp = urlscanp;
			}
			break;
		case '#':
			if (urlstate < UFrag) {
				urlstate = UFrag;
				fragp = urlscanp + 1;
			}
			break;
		}
		urlscanp++;
	}
	// Decide if authority specified in url
	if (baseurl!=aNull && schemel==0 && 0!=strncmp(urlbeg, "//", 2))
		authl = 0; // If we have baseurl but no scheme&authority, treat as relative url
	else if (authl == 0 && urlstate <= UAuth)
		authl = urlscanp - authp; // Otherwise capture authority when no "/" found

	// If url has no scheme and authority, it is a relative url. 
	// We must scan baseurl for the info we need to resolve the relative url.
	if (schemel == 0 && authl == 0 && baseurl!=aNull) {
		basescanp = authp = basebeg = (isSym(baseurl) || isStr(baseurl))? toStr(baseurl) : "";
		isrelative = 1;
		urlstate = UScheme;
		while (*basescanp) {
			switch (*basescanp) {
			case ':':
				if (urlstate == UScheme) {
					urlstate = UAuth;
					schemep = basebeg; // baseurl has it
					schemel = basescanp-basebeg;
					authp = basescanp;
				}
				break;
			case '/':
				// Ignore optional '//' at beginning of authority
				if (basescanp[1] == '/' && basescanp==&basebeg[schemel? schemel+1 : schemel]) {
					urlstate = UAuth;
					authp = basescanp + 2;
					basescanp++;
				}
				else {
					if (urlstate <= UAuth) {
						urlstate = UPath;
						authl = basescanp - authp;
					}
					if (urlstate == UPath) {
						basenslash++;
						basefilep = basescanp+1;
					}
				}
				break;
			case '.':
				if (urlstate == UScheme)
					urlstate = UAuth; // Clearly no longer doing a scheme
				break;
			}
			basescanp++;
		}
		// Capture authority if no '/' found
		if (authl == 0 && urlstate <= UAuth)
			authl = basescanp - authp;
	}

	// **** Store the scheme type, mapped from schemes
	if (schemel==0) {
		schemep = authl? "http" : "file"; // default scheme
		schemel = strlen(schemep);
	}
	pushProperty(th, 0, "schemes"); // Get table from Resource (self)
	pushSyml(th, schemep, schemel);
	resarray[ResSchemeType] = tblGet(th, getFromTop(th, 1), getFromTop(th, 0));
	setTop(th, stktop); // reset stack

	// *** Store the extension type, mapped from extensions
	if (lastdotp) {
		extp = lastdotp+1;
		extl = (queryp? queryp : fragp? fragp : urlscanp) - extp;
	}
	else {
		extp = "acn";
		extl = strlen(extp);
	}
	pushProperty(th, 0, "extensions"); // Get table from Resource (self)
	pushSyml(th, extp, extl);
	resarray[ResExtType] = tblGet(th, getFromTop(th, 1), getFromTop(th, 0));
	setTop(th, stktop); // reset stack

	// **** Build the fragment symbol
	if (fragp)
		newSym(th, &resarray[ResFragment], fragp, urlscanp-fragp);
	else
		resarray[ResFragment] = aNull;

	// **** Construct the normalized url and store
	size_t maxsz = strlen(urlbeg) + (isrelative? strlen(basebeg):0) + 20;
	char *newstr = (char*) mem_gcrealloc(th, NULL, 0, maxsz);
	// Insert the scheme if we did not have it
	if (schemep!=(isrelative? basebeg : urlbeg)) {
		strcpy(newstr, schemep);
		strcat(newstr, "://");
	}
	else
		newstr[0]='\0';
	// Copy over the relevant part of the relative path from baseurl
	if (isrelative) {
		if (strncmp(basebeg, "//", 2)==0) basebeg+=2;
		if (basefilep) {
			size_t newlen = strlen(newstr) + basefilep-basebeg;
			strncpy(&newstr[strlen(newstr)], basebeg, basefilep-basebeg);
			newstr[newlen] = '\0';
		}
		else {
			strcat(newstr, basebeg);
			strcat(newstr, "/");
		}
		if (*urlbeg=='/') urlbeg++;
		// Back up when relative path uses '..'
		char *backscanp = &newstr[strlen(newstr)-1]; // pointing to last slash
		while (0==strncmp(urlbeg, "./", 2) || 0==strncmp(urlbeg, "../", 3)) {
			if (*urlbeg=='.') {
				if (basenslash>1) {
					while (*(backscanp-1)!='/')
						backscanp--;
					*backscanp='\0';
					basenslash--;
				}
				urlbeg++;
			}
			urlbeg+=2;
		}
	}
	else
		if (strncmp(urlbeg, "//", 2)==0) urlbeg += 2;
	// Copy over the url's file path and query, minus the fragment
	if (fragp==NULL)
		strcat(newstr, urlbeg);
	else {
		size_t strl = strlen(newstr) + fragp - urlbeg-1;
		strncpy(newstr+strlen(newstr), urlbeg, fragp-urlbeg-1);
		newstr[strl] = '\0';
	}
	newSym(th, &resarray[ResNormUrl], newstr, strlen(newstr));
	mem_gcrealloc(th, newstr, maxsz, 0);
}

/** Deserialize the stream pushed on stack and remember its value */
void deserialize(Value th, Value *resarray, Value cache) {
	int streamvidx = getTop(th)-1;

	// Convert stream into usable content, using extension's type
	pushValue(th, vmlit(SymNew));
	pushValue(th, resarray[ResExtType]);
	pushLocal(th, streamvidx);
	pushValue(th, resarray[ResNormUrl]);
	pushValue(th, resarray[ResFragment]);
	getCall(th, 4, 1);

	// Save resource's value in cache
	if (getFromTop(th, 0)!=aNull)
		tblSet(th, cache, resarray[ResNormUrl], getFromTop(th, 0));
}

#define MINIZ_NO_STDIO
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#include "miniz.c"
/** Use the normalized url, scheme type and extension type to get the resource.
  It will be a Text value found at the top of the stack. */
void getResource(Value th, Value *resarray) {
	// If it was retrieved already and is found in the cache, return its known value
	pushGloVar(th, "Resource");
	Value cache = pushProperty(th, getTop(th)-1, "cache");
	Value resval = tblGet(th, cache, resarray[ResNormUrl]);
	popValue(th);
	popValue(th);
	if (resval!=aNull) {
		pushValue(th, resval);
		return;
	}

	// Retrieve and push a byte stream for the named resource using scheme's type
	int streamvidx = getTop(th);
	vmLog("Retrieving resource: %s", toStr(resarray[ResNormUrl]));
	pushValue(th, vmlit(SymParas));
	pushValue(th, resarray[ResSchemeType]);
	pushValue(th, resarray[ResNormUrl]);
	getCall(th, 2, 1);

	// If it is a zip archive, unpack it. Otherwise treat it as a single stream
	Value streamv = getFromTop(th,0);
	char *stream = (char*) toStr(streamv);
	if (strncmp(stream, "PK\3\4", 4)==0) {
		// To unpack all files in the Zip-compatible archive, first open it
		mz_zip_archive zip_archive;
		memset(&zip_archive, 0, sizeof(zip_archive));
		mz_bool status = mz_zip_reader_init_mem(&zip_archive, stream, str_size(streamv), 0);
		if (status) {
			// Extract each file in the archive.
			int nbrfiles = (int)mz_zip_reader_get_num_files(&zip_archive);
			for (int i = 0; i < nbrfiles; i++) {
				// Get file's metadata: name, comment, sizes, directory flag
				mz_zip_archive_file_stat file_stat;
				if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
					vmLog("Corrupted metadata for zip archive file");
					continue;
				}

				// Extract and decompress a non-directory file's stream
				if (!mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
					// Create a resource instance (an array), populate it to get correct url
					Value extresarray = pushArray(th, vmlit(TypeResm), nResVals);
					arrSet(th, extresarray, nResVals-1, aNull); // Fill it with nulls
					newResource(th, file_stat.m_filename, resarray[ResNormUrl], arr_info(extresarray)->arr);

					vmLog("Extracting %s from resource zip", toStr(arr_info(extresarray)->arr[ResNormUrl]));
					AuintIdx streamsz = (AuintIdx) file_stat.m_uncomp_size;
					Value nstreamv = pushStringl(th, aNull, NULL, streamsz);
					mz_zip_reader_extract_to_mem(&zip_archive, i, (void*)toStr(nstreamv), streamsz, 0);
					str_size(nstreamv) = streamsz;
					deserialize(th, arr_info(extresarray)->arr, cache);
					popValue(th); // deserialized stream
					popValue(th); // stream
					popValue(th); // Resource array
				}
			}

			// Close the archive, freeing any resources it was using
			mz_zip_reader_end(&zip_archive);
			popValue(th); // zip resource's stream

			// Get deserialized value of the requested member of archive (if it was included!)
			pushValue(th, tblGet(th, cache, resarray[ResNormUrl]));
		}
		else
			vmLog("Resource looks like a PK zip archive, but won't open correctly");
	}
	else {
		deserialize(th, resarray, cache); // will pop resource stream
	}
}

/** Create a new Resource using a url string and baseurl context */
int resource_new(Value th) {
	// Get url and baseurl parameters
	Value urlval, baseurl;
	if (getTop(th)<2 || (!isStr(urlval = getLocal(th,1)) && !isSym(urlval))) {
		pushValue(th, aNull);
		return 1;
	}
	if (getTop(th)<3 || (!isStr(baseurl = getLocal(th,2)) && !isSym(baseurl))) 
		baseurl = aNull;

	// Create the resource instance (an array), populate it, then return the pushed instance
	Value resarray = pushArray(th, vmlit(TypeResm), nResVals);
	arrSet(th, resarray, nResVals-1, aNull); // Fill it with nulls
	newResource(th, toStr(urlval), baseurl, arr_info(resarray)->arr);
	return 1;
}

/** Load and decode a resource using a url string and baseurl context, returning its value */
int resource_get(Value th) {
	// Get url and baseurl parameters
	Value urlval, baseurl;
	if (getTop(th)<2 || (!isStr(urlval = getLocal(th,1)) && !isSym(urlval))) {
		pushValue(th, aNull);
		return 1;
	}
	if (getTop(th)<3 || (!isStr(baseurl = getLocal(th,2)) && !isSym(baseurl))) 
		baseurl = aNull;

	// Create the resource array, populate it, then get the resource
	Value resarray = pushArray(th, vmlit(TypeResm), nResVals);
	arrSet(th, resarray, nResVals-1, aNull); // Fill it with nulls
	newResource(th, toStr(urlval), baseurl, arr_info(resarray)->arr);
	getResource(th, arr_info(resarray)->arr);
	return 1;
}

/** Load and decode a resource instance, returning its value */
int resource_inst_get(Value th) {
	getResource(th, arr_info(getLocal(th, 0))->arr);
	return 1;
}

/** Return the symbol containing the resource's normalized url */
int resource_inst_url(Value th) {
	Value *resarray = arr_info(getLocal(th, 0))->arr;
	pushValue(th, resarray[ResNormUrl]);
	return 1;
}

/** Return the symbol containing the resource's fragment */
int resource_inst_frag(Value th) {
	Value *resarray = arr_info(getLocal(th, 0))->arr;
	pushValue(th, resarray[ResFragment]);
	return 1;
}

/** Initialize the Resource type */
void core_resource_init(Value th) {
	vmlit(TypeResc) = pushType(th, vmlit(TypeType), 8);
		pushSym(th, "Resource");
		popProperty(th, 0, "_name");
		vmlit(TypeResm) = pushMixin(th, vmlit(TypeType), aNull, 5);
			pushSym(th, "*Resource");
			popProperty(th, 1, "_name");
			pushCMethod(th, resource_inst_get);
			popProperty(th, 1, "()");
			pushCMethod(th, resource_inst_frag);
			popProperty(th, 1, "fragment");
			pushCMethod(th, resource_inst_url);
			popProperty(th, 1, "url");
		popProperty(th, 0, "_newtype");
		pushCMethod(th, resource_new);
		popProperty(th, 0, "New");
		pushCMethod(th, resource_get);
		popProperty(th, 0, "()");
		pushTbl(th, vmlit(TypeIndexm), 10);
		popProperty(th, 0, "schemes");
		pushTbl(th, vmlit(TypeIndexm), 10);
		popProperty(th, 0, "extensions");
		pushTbl(th, vmlit(TypeIndexm), 10);
		popProperty(th, 0, "cache");
	popGloVar(th, "Resource");
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

