/** Implements the Resource type.
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
void newResource(Value th, Value url, Value baseurl, Value *resarray) {
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
	urlbeg = urlscanp = authp = toStr(url);
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

/** Use the normalized url, scheme type and extension type to get the resource.
  It will be a Text value found at the top of the stack. */
void getResource(Value th, Value *resarray) {
	// If it was retrieved already and is found in the cache, return it
	pushGloVar(th, "Resource");
	Value cache = pushProperty(th, getTop(th)-1, "cache");
	Value resval = tblGet(th, cache, resarray[ResNormUrl]);
	popValue(th);
	popValue(th);
	if (resval!=aNull) {
		pushValue(th, resval);
		return;
	}

	// equivalent to: +(ResExtType)((ResSchemeType)(ResNormUrl), ResNormUrl, ResFragment)
	vmLog("Retrieving resource: %s", toStr(resarray[ResNormUrl]));
	pushValue(th, vmlit(SymNew));
	pushValue(th, resarray[ResExtType]);
	pushValue(th, vmlit(SymParas));
	pushValue(th, resarray[ResSchemeType]);
	pushValue(th, resarray[ResNormUrl]);
	getCall(th, 2, 1);
	pushValue(th, resarray[ResNormUrl]);
	pushValue(th, resarray[ResFragment]);
	getCall(th, 4, 1);

	// Save it in cache
	if (getFromTop(th, 0)!=aNull)
		tblSet(th, cache, resarray[ResNormUrl], getFromTop(th, 0));
}

/** Create a new Resource using a url string and baseurl context */
int typ_resource_new(Value th) {
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
	newResource(th, urlval, baseurl, arr_info(resarray)->arr);
	return 1;
}

/** Load and decode a resource using a url string and baseurl context, returning its value */
int typ_resource_get(Value th) {
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
	newResource(th, urlval, baseurl, arr_info(resarray)->arr);
	getResource(th, arr_info(resarray)->arr);
	return 1;
}

/** Load and decode a resource instance, returning its value */
int typ_resource_inst_get(Value th) {
	getResource(th, arr_info(getLocal(th, 0))->arr);
	return 1;
}

/** Return the symbol containing the resource's normalized url */
int typ_resource_inst_url(Value th) {
	Value *resarray = arr_info(getLocal(th, 0))->arr;
	pushValue(th, resarray[ResNormUrl]);
	return 1;
}

/** Return the symbol containing the resource's fragment */
int typ_resource_inst_frag(Value th) {
	Value *resarray = arr_info(getLocal(th, 0))->arr;
	pushValue(th, resarray[ResFragment]);
	return 1;
}

/** Initialize the Resource type */
void typ_resource_init(Value th) {
	vmlit(TypeResc) = pushType(th, vmlit(TypeType), 6);
		pushSym(th, "Resource");
		popProperty(th, 0, "_name");
		vmlit(TypeResm) = pushMixin(th, vmlit(TypeType), aNull, 5);
			pushSym(th, "*Resource");
			popProperty(th, 1, "_name");
			pushCMethod(th, typ_resource_inst_get);
			popProperty(th, 1, "()");
			pushCMethod(th, typ_resource_inst_frag);
			popProperty(th, 1, "fragment");
			pushCMethod(th, typ_resource_inst_url);
			popProperty(th, 1, "url");
		popProperty(th, 0, "_newtype");
		pushCMethod(th, typ_resource_new);
		popProperty(th, 0, "New");
		pushCMethod(th, typ_resource_get);
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

