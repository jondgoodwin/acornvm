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

#define MINIZ_NO_STDIO
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#include "miniz.c"

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/** Index of the values contained in the Resource array */
enum resourceValues {
	ResUrl,				//!< (symbol) The absolute url (without the anchor)
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

/** Build a new resource (an array) by creating an absolute, complete url 
  from url and baseurl (for resolving a relative address).
  It then populates the resource array with extracts from the constructed url */
void newResource(Value th, const char *url, Value baseurl, Value *resarray) {
	AintIdx stktop = getTop(th);
	const char *urlscanp, *urlbeg;
	const char *basescanp, *basebeg;
	const char *basefilep = NULL;
	bool isrelative = false;
	bool nopath = false;
	const char *schemep;
	size_t schemel = 0;
	const char *authp;
	size_t authl = 0;
	const char *extp;
	size_t extl;

	// **** Scan url to identify key pieces
	urlbeg = urlscanp = authp = url;
	const char *lastdotp = NULL;
	const char *queryp = NULL;
	const char *fragp = NULL;
	int urlstate = UScheme;
	if (*urlscanp == '/' || *urlscanp == '.' || urlscanp[1] == ':')
		urlstate = UPath;
	while (*urlscanp) {
		switch (*urlscanp) {
		case ':':
			if (urlstate == UScheme) {
				urlstate = UAuth;
				// Capture url's scheme segment
				schemep = urlbeg;
				schemel = urlscanp-urlbeg;
				// Skip past '//' which should follow
				if (urlscanp[1] == '/' && urlscanp[2] == '/')
					urlscanp+=2;
				authp = urlscanp+1;
			}
			break;
		case '/':
			if (urlstate <= UAuth) {
				urlstate = UPath;
				authl = urlscanp - authp;
			}
			if (urlstate == UPath)
				lastdotp = NULL; // Reset as we only want last dot on resource name
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

	// If url has no scheme and baseurl was provided, it is a relative url. 
	if (schemel == 0 && baseurl!=aNull) {
		isrelative = true;
		// We must scan baseurl for the info we need to resolve the relative url.
		basescanp = authp = basebeg = (isSym(baseurl) || isStr(baseurl))? toStr(baseurl) : "";
		urlstate = UScheme;
		while (*basescanp) {
			switch (*basescanp) {
			case ':':
				if (urlstate == UScheme) {
					urlstate = UAuth;
					// capture scheme segment of baseurl
					schemep = basebeg;
					schemel = basescanp-basebeg;
					// Skip past '//' which should follow
					if (basescanp[1] == '/' && basescanp[2] == '/')
						basescanp+=2;
					authp = basescanp+1;
				}
				break;
			case '/':
				if (urlstate <= UAuth) {
					urlstate = UPath;
					authl = basescanp - authp;
				}
				if (urlstate == UPath)
					basefilep = basescanp+1;
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
	else {
		// If no '/' found, consider the entire url after scheme to be the authority
		if (authl == 0 && urlstate <= UAuth) {
			nopath = true;
			authl = urlscanp - authp;
		}
	}
	// **** Store the scheme type, mapped from schemes
	int resourcetype = getTop(th);
	pushGloVar(th, "Resource");
	if (schemel==0) {
		schemep = authl? "http" : "file"; // default scheme
		schemel = strlen(schemep);
	}
	pushProperty(th, resourcetype, "schemes"); // Get table from Resource
	pushSyml(th, schemep, schemel);
	resarray[ResSchemeType] = tblGet(th, getFromTop(th, 1), getFromTop(th, 0));

	// *** Store the extension type, mapped from extensions
	if (lastdotp) {
		extp = lastdotp+1;
		extl = (queryp? queryp : fragp? fragp : urlscanp) - extp;
	}
	else {
		extp = "acn";
		extl = strlen(extp);
	}
	pushProperty(th, resourcetype, "extensions"); // Get table from Resource
	pushSyml(th, extp, extl);
	resarray[ResExtType] = tblGet(th, getFromTop(th, 1), getFromTop(th, 0));
	setTop(th, stktop); // reset stack

	// **** Build the fragment symbol
	if (fragp)
		newSym(th, &resarray[ResFragment], fragp, urlscanp-fragp);
	else
		resarray[ResFragment] = aNull;

	// **** Allocate string then construct the absolute url
	size_t maxsz = strlen(urlbeg) + (isrelative? strlen(basebeg):0) + 20; // 20 > adding "http:///world.acn" defaults
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
		// Point to first and last slash (or null) in baseurl's path
		const char *basefirstslashp = authp; // after authority
		while (*basefirstslashp && *basefirstslashp!='/')
			basefirstslashp++;
		// last slash is before resource filename
		const char *baselastslashp = basefilep? basefilep-1 : &basebeg[strlen(basebeg)];

		// if relative url starts with '/', do not include any of the path
		if (*urlbeg=='/') {
			urlbeg++; // avoid dupe '/'
			baselastslashp = basefirstslashp;
		} else {
			// Move last slash back as much as we can, asked for by relative url '..'s
			while (baselastslashp!=basefirstslashp) {
				if (*urlbeg=='.' && urlbeg[1]=='/')
					urlbeg+=2; // ignore
				else if (*urlbeg=='.' && urlbeg[1]=='.' && urlbeg[2]=='/') {
					while (*--baselastslashp && *baselastslashp!='/');
					urlbeg+=3;
				} else
					break;
			}
		}

		// We can now copy from basebeg[basebeg] to baselastslashp
		size_t newlen = strlen(newstr) + baselastslashp-basebeg+1;
		strncpy(&newstr[strlen(newstr)], basebeg, baselastslashp-basebeg+1);
		if (*baselastslashp==0)
			strcat(newstr, "/");
		else
			newstr[newlen]= '\0';
	}
	else {
		if (strncmp(urlbeg, "//", 2)==0) urlbeg += 2;
		if (authl==0 && *urlbeg!='/')
			strcat(newstr, "/");
	}
	// Copy over the url's file path without query and fragment
	if (fragp==NULL && queryp==NULL)
		strcat(newstr, urlbeg);
	else if (queryp) {
		size_t strl = strlen(newstr) + queryp - urlbeg;
		strncpy(newstr+strlen(newstr), urlbeg, queryp-urlbeg);
		newstr[strl] = '\0';
	}
	else {
		size_t strl = strlen(newstr) + fragp - urlbeg-1;
		strncpy(newstr+strlen(newstr), urlbeg, fragp-urlbeg-1);
		newstr[strl] = '\0';
	}
	// Add in /world.acn or .acn defaults as needed
	if (nopath) strcat(newstr,"/world.acn");
	if (lastdotp==0 && newstr[strlen(newstr)-1]!='/') strcat(newstr,".acn");
	// Copy over query without fragment
	if (queryp) {
		if (fragp==NULL)
			strcat(newstr, queryp);
		else {
			size_t strl = strlen(newstr) + fragp - queryp-1;
			strncpy(newstr+strlen(newstr), queryp, fragp-urlbeg-1);
			newstr[strl] = '\0';
		}
	}
	newSym(th, &resarray[ResUrl], newstr, strlen(newstr));
	mem_gcrealloc(th, newstr, maxsz, 0);
}

bool resource_equal(Value res1, Value res2) {
	if (!isArr(res1) || !isArr(res2))
		return false;
	return (arr_info(res1)->arr)[ResUrl] == (arr_info(res2)->arr)[ResUrl];
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

/** Set the resolved value for a resource in Resource.values. It also:
	- removes its loader from Resources.loaders.
	- Tries to resolve the value for any loaders whose status is "resolve"
	  (i.e., any method that might have been waiting for this resource's value to be determined) */
void resource_setvalue(Value th, Value *resarray, Value val) {
	int resourceidx = getTop(th);
	pushGloVar(th, "Resource");

	// Store value in Resource.values
	Value resvalues = pushProperty(th, resourceidx, "values");
	tblSet(th, resvalues, resarray[ResUrl], val);
	popValue(th);

	// Get rid of resource's loader in Resource.loaders (we are done)
	Value resloaders = pushProperty(th, resourceidx, "loaders");
	tblRemove(th, resloaders, resarray[ResUrl]);
	popValue(th);

	// Check if any other resource in Resource.loaders is waiting on this value to resolve
	// TBD...

	popValue(th);  // Resource
}

/** Deserialize the stream pushed on stack and remember its value */
void resource_deserialize(Value th, Value *resarray) {
	int streamvidx = getTop(th)-1;

	// Convert stream into usable content, using extension's type
	int64_t start = vmStartTimer();
	pushValue(th, vmlit(SymNew));
	pushValue(th, resarray[ResExtType]);
	pushLocal(th, streamvidx);
	pushValue(th, resarray[ResUrl]);
	pushValue(th, resarray[ResFragment]);
	getCall(th, 4, 1);
	vmLog("Deserialization took %f seconds", vmEndTimer(start));

	// Finalize: Save resource's value in values cache
	resource_setvalue(th, resarray, getFromTop(th, 0));
}

/** Load and decode a resource instance, returning its value */
int resource_inst_load(Value th) {
	Value *resarray = arr_info(getLocal(th, 0))->arr;
	int resourceidx = getTop(th);
	pushGloVar(th, "Resource");
	Value resvalues = pushProperty(th, resourceidx, "values");

	// If already loaded, resource's value can be found in the values cache
	if (tblHas(th, resvalues, resarray[ResUrl])) {
		pushValue(th, tblGet(th, resvalues, resarray[ResUrl]));
		return 1;
	}

	// Return if we are already in process of loading this resource
	Value resloaders = pushProperty(th, resourceidx, "loaders");
	if (tblGet(th, resloaders, resarray[ResUrl]) != aNull) {
		pushValue(th, aNull);
		return 1;
	}

	// Create & populate the resource's loader, then store it in Resource.loaders
	int loaderidx = getTop(th);
	pushType(th, aNull, 4);
	pushValue(th, getLocal(th, 0));
	popProperty(th, loaderidx, "resource"); // Save the resource (self)
	tblSet(th, resloaders, resarray[ResUrl], getFromTop(th, 0));
	popValue(th);

	// Retrieve and push a byte stream for the named resource using scheme's type
	int streamvidx = getTop(th);
	vmLog("Loading resource: %s", toStr(resarray[ResUrl]));
	pushValue(th, vmlit(SymParas));
	pushValue(th, resarray[ResSchemeType]);
	pushValue(th, resarray[ResUrl]);
	getCall(th, 2, 1);

	// If it is a zip archive, unpack it. Otherwise treat it as a single stream
	Value streamv = getFromTop(th,0);
	char *stream = (char*) toStr(streamv);
	if (stream && strncmp(stream, "PK\3\4", 4)!=0) {
		resource_deserialize(th, resarray);
	}
	else {
		// To unpack all files in the Zip-compatible archive, first open it
		mz_zip_archive zip_archive;
		memset(&zip_archive, 0, sizeof(zip_archive));
		mz_bool status = mz_zip_reader_init_mem(&zip_archive, stream, str_size(streamv), MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
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
					newResource(th, file_stat.m_filename, resarray[ResUrl], arr_info(extresarray)->arr);

					vmLog("Extracting %s resource from archive", toStr(arr_info(extresarray)->arr[ResUrl]));
					AuintIdx streamsz = (AuintIdx) file_stat.m_uncomp_size;
					Value nstreamv = pushStringl(th, aNull, NULL, streamsz);
					mz_zip_reader_extract_to_mem(&zip_archive, i, (void*)toStr(nstreamv), streamsz, 0);
					str_size(nstreamv) = streamsz;
					resource_deserialize(th, arr_info(extresarray)->arr);
					popValue(th); // deserialized stream
					popValue(th); // stream
					popValue(th); // Resource array
				}
			}

			// Close the archive, freeing any resources it was using
			mz_zip_reader_end(&zip_archive);
			popValue(th); // zip resource's stream

			// Get deserialized value of the requested member of archive (if it was included!)
			pushValue(th, tblGet(th, resvalues, resarray[ResUrl]));
		}
		else
			vmLog("Resource looks like a zip archive, but won't open correctly");
	}
	return 1;
}

/** Return the symbol containing the resource's absolute url (without fragment) */
int resource_inst_url(Value th) {
	Value *resarray = arr_info(getLocal(th, 0))->arr;
	pushValue(th, resarray[ResUrl]);
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
			pushCMethod(th, resource_inst_load);
			popProperty(th, 1, "Load");
			pushCMethod(th, resource_inst_frag);
			popProperty(th, 1, "fragment");
			pushCMethod(th, resource_inst_url);
			popProperty(th, 1, "url");
		popProperty(th, 0, "_newtype");
		pushCMethod(th, resource_new);
		popProperty(th, 0, "New");
		pushTbl(th, vmlit(TypeIndexm), 16);
		popProperty(th, 0, "schemes");
		pushTbl(th, vmlit(TypeIndexm), 16);
		popProperty(th, 0, "extensions");
		pushTbl(th, vmlit(TypeIndexm), 16);
		popProperty(th, 0, "values");
		pushTbl(th, vmlit(TypeIndexm), 16);
		popProperty(th, 0, "loaders");
	popGloVar(th, "Resource");
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif

