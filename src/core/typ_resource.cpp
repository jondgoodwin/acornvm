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
void newResource(Value th, const char *url, Value baseurl, Value resource) {
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
	arrSet(th, resource, ResSchemeType, tblGet(th, getFromTop(th, 1), getFromTop(th, 0)));

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
	arrSet(th, resource, ResExtType, tblGet(th, getFromTop(th, 1), getFromTop(th, 0)));
	setTop(th, stktop); // reset stack

	// **** Build the fragment symbol
	if (fragp) {
		pushSyml(th, fragp,urlscanp-fragp);
		arrSet(th, resource, ResFragment, getFromTop(th, 0));
		popValue(th);
	}
	else
		arrSet(th, resource, ResFragment, aNull);

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
	pushSyml(th, newstr, strlen(newstr));
	arrSet(th, resource, ResUrl, getFromTop(th,0));
	popValue(th);
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
	newResource(th, toStr(urlval), baseurl, resarray);
	return 1;
}

/* Attempt to resolve the method's externs
	Return number of unresolved Resources or null if all resolved */
AuintIdx resource_resolve(Value th, Value meth, Value *externp) {
	AuintIdx counter = 0;

	// Handle a static resource
	if (isArr(*externp) && ((ArrInfo*)*externp)->type == vmlit(TypeResm)) {
		Value *resarray = arr_info(*externp)->arr;
		int resourceidx = getTop(th);
		pushGloVar(th, "Resource");
		// If resource's value is known, substitute it for extern
		Value values = pushProperty(th, resourceidx, "values");
		if (tblHas(th, values, resarray[ResUrl])) {
			*externp = tblGet(th, values, resarray[ResUrl]);
			mem_markChk(th, meth, *externp);
		}
		else {
			Value loaders = pushProperty(th, resourceidx, "loaders");
			// Start loading this resource, if not already being loaded
			if (!tblHas(th, loaders, resarray[ResUrl])) {
				pushValue(th, vmlit(SymLoad));
				pushValue(th, *externp);
				getCall(th, 1, 0);
				// If the Load finished already, get its value
				if (tblHas(th, values, resarray[ResUrl])) {
					*externp = tblGet(th, values, resarray[ResUrl]);
					mem_markChk(th, meth, *externp);
				}
				// If not finished, indicate it is unresolved
				else
					counter = 1;
			}
			else
				counter = 1; // Acknowledge that still-loading resource is unresolved
			popValue(th); // .loaders
		}
		popValue(th); // .values
		popValue(th); // Resource
	}

	// Recursively link an enclosed method
	else if (isMethod(*externp)) {
		pushSym(th, "Link");
		pushValue(th, *externp);
		getCall(th, 1, 1);
		Value retcount = popValue(th);
		if (isInt(retcount))
			return toAint(retcount); // include its count in ours
	}
	return counter;
}

/** Set the resolved value for a resource in Resource.values. It also:
	- removes its loader from Resources.loaders.
	- Tries to resolve the value for any loaders who want to link 
	  but whose references have not all been loaded
	  (i.e., any method that might have been waiting for this resource's value to be determined) */
void resource_setvalue(Value th, Value *resarray, Value val) {
	int resourceidx = getTop(th);
	pushGloVar(th, "Resource");
	Value resvalues = pushProperty(th, resourceidx, "values");
	Value resloaders = pushProperty(th, resourceidx, "loaders");

	// Store value in Resource.values
	tblSet(th, resvalues, resarray[ResUrl], val);

	// Get rid of resource's loader in Resource.loaders (we are done)
	tblRemove(th, resloaders, resarray[ResUrl]);

	// Check all in-process resource loaders with unfinished "link" status: can we now finish link?
	// Note - this is a rather inefficient way to do it, scanning all in-process loaders every time a resource resolves
	// It might be more efficient to build a dependency list within every loader
	bool scanloaders = true;
	while (scanloaders) {
		scanloaders = false; // In most cases only need one scan
		Value key = aNull;
		while ((key = tblNext(resloaders, key)) != aNull) {
			Value loader = tblGet(th, resloaders, key);
			Value r = pushSym(th, "resource");
			Value url = (arr_info(tblGet(th, loader, r))->arr)[ResUrl];
			popValue(th); // r

			// If loader has a 'value' property, it is in 'link' status
			Value method = tblGet(th, loader, vmlit(SymValue));
			if (isMethod(method)) {
				// Try to complete link
				pushSym(th, "Link");
				pushValue(th, method);
				getCall(th, 1, 1);
				// If link resolved all dependencies...
				if (popValue(th)==aNull) {
					//  run the method
					pushValue(th, method);
					pushValue(th, aNull);
					getCall(th, 1, 1);

					// vmLog("Value returned from running %s-->\n%s", toStr(url), toStr(pushSerialized(th, getFromTop(th, 0)))); popValue(th);

					// and then save return value in "values" and delete loader
					// For efficiency, we won't recursively call ourself (setvalue)
					// But then we must set the flag to re-scan all loaders,
					// since already scanned resources might have needed this value
					scanloaders = true;  // Force re-scan of all loaders
					tblSet(th, resvalues, url, getFromTop(th, 0));
					tblRemove(th, resloaders, url);
					popValue(th); // method's run value
				}
			}
		}
	}

	// Clean up stack
	popValue(th);	// .loaders
	popValue(th);	// .values
	popValue(th);	// Resource
}

/** Deserialize the stream and remember its value */
void resource_deserialize(Value th, Value stream, Value *resarray) {
	// Convert stream into usable content, using extension's type
	int64_t start = vmStartTimer();
	pushValue(th, vmlit(SymNew));
	pushValue(th, resarray[ResExtType]);
	pushValue(th, stream);
	pushValue(th, resarray[ResUrl]);
	pushValue(th, resarray[ResFragment]);
	getCall(th, 4, 1);
	vmLog("Deserialization took %f seconds", vmEndTimer(start));

	// Are we done and can save value, or is it a method that needs linking?
	Value decodedval = getFromTop(th, 0);
	if (!isMethod(decodedval)) // non-methods are done
		resource_setvalue(th, resarray, decodedval);
	else {
		// A method may need a link step to resolve the values
		// of all static resources it references
		pushSym(th, "Link");
		pushValue(th, decodedval);
		getCall(th, 1, 1);
		if (popValue(th)==aNull) {
			// Fully linked method is done
			pushValue(th, decodedval);
			pushValue(th, aNull);
			getCall(th, 1, 1);
			// vmLog("Value returned from running %s-->\n%s", toStr(resarray[ResUrl]), toStr(pushSerialized(th, getFromTop(th, 0)))); popValue(th);
			resource_setvalue(th, resarray, getFromTop(th, 0));
			popValue(th);
		}
		else {
			// Link is unfinished as some static references are still loading
			// Store method in loader's 'value' property, signalling a loader in 'link' status
			int resourceidx = getTop(th);
			pushGloVar(th, "Resource");
			Value loaders = pushProperty(th, resourceidx, "loaders");
			Value loader = tblGet(th, loaders, resarray[ResUrl]);
			tblSet(th, loader, vmlit(SymValue), decodedval);
			popValue(th); // .loaders
			popValue(th); // Resource
		}
	}

	// Finalize: Save resource's value in values cache
	popValue(th); // deserialized stream
}

/** Handle completed 'get' of resource's stream using scheme.
	self is null. First parameter is the stream.
	If stream is null, get failed and second parm is the error string. */
int resource_getCallback(Value th) {
	// Access closure's stored loader and resource url info
	int loaderidx = getTop(th);
	pushCloVar(th, 2);
	Value res = pushProperty(th, loaderidx, "resource");
	Value *resarray = arr_info(res)->arr;

	// If Get failed (stream is null), return with passed error diagnostic
	Value streamv = getLocal(th,1);
	if (streamv==aNull) {
		vmLog("Resource load failure '%s' for %s", toStr(getLocal(th, 2)), toStr(resarray[ResUrl]));
		resource_setvalue(th, resarray, aNull);
		return 0;
	}

	// Deserialize stream
	char *stream = (char*) toStr(streamv);
	if (stream) {
		// Regular stream - not a zip archive
		if (strncmp(stream, "PK\3\4", 4)!=0) {
			resource_deserialize(th, streamv, resarray);
		}
		// For zip archive, unpack it and deserialize every file as a separate resource
		else {
			mz_zip_archive zip_archive;
			memset(&zip_archive, 0, sizeof(zip_archive));
			mz_bool status = mz_zip_reader_init_mem(&zip_archive, stream, str_size(streamv), MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
			if (status) {
				// On first pass through directory, create a loader for every file
				// This will ensure referenced streams come from archive rather than be auto-loaded as resources
				int nbrfiles = (int)mz_zip_reader_get_num_files(&zip_archive);
				int resourceidx = getTop(th);
				pushGloVar(th, "Resource");
				Value loaders = pushProperty(th, resourceidx, "loaders");
				for (int i = 0; i < nbrfiles; i++) {
					// Get file's metadata: name, comment, sizes, directory flag
					mz_zip_archive_file_stat file_stat;
					if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
						vmLog("Corrupted metadata for zip archive file");
						continue;
					}

					// Create a resource instance and loader for a non-directory file's stream
					if (!mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
						// Create the loader
						int loaderidx = getTop(th);
						Value loader = pushType(th, aNull, 4);

						// Create a resource instance (an array) with absolute url
						Value extresarray = pushArray(th, vmlit(TypeResm), nResVals);
						arrSet(th, extresarray, nResVals-1, aNull); // Fill it with nulls
						newResource(th, file_stat.m_filename, resarray[ResUrl], extresarray);

						// Save resource in loader and save loader in Resource.loaders
						popProperty(th, loaderidx, "resource");
						tblSet(th, loaders, (arr_info(extresarray)->arr)[ResUrl], loader);
						popValue(th); // loader
					}
				}
				popValue(th); // .loaders
				popValue(th); // Resource

				// Now extract each stream in the archive and deserialize
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
						newResource(th, file_stat.m_filename, resarray[ResUrl], extresarray);

						vmLog("Extracting %s resource from archive", toStr(arr_info(extresarray)->arr[ResUrl]));
						AuintIdx streamsz = (AuintIdx) file_stat.m_uncomp_size;
						Value nstreamv = pushStringl(th, aNull, NULL, streamsz);
						mz_zip_reader_extract_to_mem(&zip_archive, i, (void*)toStr(nstreamv), streamsz, 0);
						str_size(nstreamv) = streamsz;
						resource_deserialize(th, nstreamv, arr_info(extresarray)->arr);
						popValue(th); // stream
						popValue(th); // Resource array
					}
				}

				// Close the archive, freeing any resources it was using
				mz_zip_reader_end(&zip_archive);
				popValue(th); // zip resource's stream
			}
			else
				vmLog("Resource looks like a zip archive, but won't open correctly");
		}
	}

	return 0;
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
	Value loaderv = pushType(th, aNull, 4);
	pushValue(th, getLocal(th, 0));
	popProperty(th, loaderidx, "resource"); // Save the resource (self)
	pushCMethod(th, resource_getCallback);
	pushValue(th, aNull);
	pushValue(th, loaderv);
	pushClosure(th, 3);
	popProperty(th, loaderidx, "callback"); // Store callback so it stays alive while needed
	tblSet(th, resloaders, resarray[ResUrl], getFromTop(th, 0));

	// Get/push a byte stream for the named resource using scheme's type
	// Since 'get' might be asynchronous, pass closures for future handling of success or failure
	int streamvidx = getTop(th);
	vmLog("Loading resource: %s", toStr(resarray[ResUrl]));
	pushValue(th, vmlit(SymGet));
	pushValue(th, resarray[ResSchemeType]);
	pushValue(th, resarray[ResUrl]);
	pushProperty(th, loaderidx, "callback");
	getCall(th, 3, 1);

	// Return deserialized value of resource
	pushValue(th, tblGet(th, resvalues, resarray[ResUrl]));
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
	vmlit(TypeResc) = pushType(th, vmlit(TypeObject), 8);
		pushSym(th, "Resource");
		popProperty(th, 0, "_name");
		vmlit(TypeResm) = pushMixin(th, vmlit(TypeObject), aNull, 5);
			pushSym(th, "*Resource");
			popProperty(th, 1, "_name");
			pushCMethod(th, resource_inst_load);
			popProperty(th, 1, "Load");
			pushCMethod(th, resource_inst_frag);
			popProperty(th, 1, "fragment");
			pushCMethod(th, resource_inst_url);
			popProperty(th, 1, "url");
		popProperty(th, 0, "traits");
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

