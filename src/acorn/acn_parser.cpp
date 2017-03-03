/** Parser for Acorn compiler. See Acorn documentation for syntax diagrams.
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
 */

#include "acorn.h"
#include <stdio.h>

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/* **********************
 * Abstract Syntax Tree construction helpers for parser
 * (isolates that we are working with arrays to encode s-expressions)
 * **********************/

/** Append a value to AST segment - growing as needed */
#define astAddValue(th, astseg, val) (arrAdd(th, astseg, val))

/** Get a value within the AST segment */
#define astGet(th, astseg, idx) (arrGet(th, astseg, idx))

/** Set a value within the AST segment */
#define astSetValue(th, astseg, idx, val) (arrSet(th, astseg, idx, val))

/** Create and append a new AST segment (of designated size) to current segment.
 * Append the AST op to the new segment, then return it */
Value astAddSeg(Value th, Value oldseg, Value astop, AuintIdx size) {
	Value newseg = pushArray(th, aNull, size);
	arrAdd(th, oldseg, newseg);
	popValue(th);
	arrAdd(th, newseg, astop);
	return newseg;
}

/** Create and append a new AST segment (with two slots) to current segment.
 * Append the AST op and val to the new segment, then return it */
Value astAddSeg2(Value th, Value oldseg, Value astop, Value val) {
	Value newseg = pushArray(th, aNull, 2);
	arrAdd(th, oldseg, newseg);
	popValue(th);
	arrAdd(th, newseg, astop);
	arrAdd(th, newseg, val);
	return newseg;
}

/** Get last node from ast segment */
#define astGetLast(th, astseg) (arrGet(th, astseg, arr_size(astseg)-1))

/** Create a new segment of designated size to replace last value of oldseg.
 * Append the AST op and the value from the oldseg to the new segment,
 * then return it. */
Value astInsSeg(Value th, Value oldseg, Value astop, AuintIdx size) {
	AuintIdx oldpos = arr_size(oldseg)-1;
	Value saveval = arrGet(th, oldseg, oldpos);
	Value newseg = pushArray(th, aNull, size);
	arrSet(th, oldseg, oldpos, newseg);
	popValue(th);
	arrAdd(th, newseg, astop);
	arrAdd(th, newseg, saveval);
	return newseg;
}

/** Create a new segment of designated size to replace last value of oldseg.
 * Append the AST op, propval, and the value from the oldseg to the new segment,
 * then return it. */
Value astInsSeg2(Value th, Value oldseg, Value astop, Value propval, AuintIdx size) {
	AuintIdx oldpos = arr_size(oldseg)-1;
	Value saveval = arrGet(th, oldseg, oldpos);
	Value newseg = pushArray(th, aNull, size);
	arrSet(th, oldseg, oldpos, newseg);
	popValue(th);
	arrAdd(th, newseg, astop);
	if (isSym(propval)) {
		if (propval == vmlit(SymThis))
			arrAdd(th, newseg, propval);
		else {
			Value propseg = astAddSeg(th, newseg, vmlit(SymLit), 2); // Assume propval is a literal symbol
			arrAdd(th, propseg, propval);
		}
	}
	arrAdd(th, newseg, saveval);
	return newseg;
}

/** Create a new untethered, sized AST segment that has astop as first element */
Value astPushNew(Value th, Value astop, AuintIdx size) {
	Value newseg = pushArray(th, aNull, size);
	arrAdd(th, newseg, astop);
	return newseg;
}

/** Attach newseg into last slot of oldseg, whose old value is appended to newseg */
void astPopNew(Value th, Value oldseg, Value newseg) {
	AuintIdx oldpos = arr_size(oldseg)-1;
	Value saveval = arrGet(th, oldseg, oldpos);
	arrSet(th, oldseg, oldpos, newseg);
	arrAdd(th, newseg, saveval);
	popValue(th);
}

/** Return true if ast segment can be assigned a value: variable or settable property/method */
bool astIsLval(Value th, Value astseg) {
	if (!isArr(astseg))
		return false;
	Value op = astGet(th, astseg, 0);
	return op==vmlit(SymLocal) || op==vmlit(SymGlobal) || op==vmlit(SymActProp) || op==vmlit(SymRawProp) || op==vmlit(SymCallProp);
}

/* **********************
 * Parsing routines
 * **********************/

// Prototypes for functions used before they are defined
void parseBlock(CompInfo* comp, Value astseg);
void parseAppendExp(CompInfo* comp, Value astseg);
void parseExp(CompInfo* comp, Value astseg);

/* Add a url literal and return its index */
bool resource_equal(Value res1, Value res2);
int genAddUrlLit(CompInfo *comp, Value val) {
	BMethodInfo* f = comp->method;

	// See if we already have resource with same url
	int i = f->nbrlits;
	while (i-- > 0)
		if (resource_equal(f->lits[i],val))
			return i;

	// If not found, add it
	mem_growvector(comp->th, f->lits, f->nbrlits, f->litsz, Value, INT_MAX);
	f->lits[f->nbrlits] = val;
	mem_markChk(comp->th, comp, val);
	return f->nbrlits++;
}

/* Add a method literal and return its index */
int genAddMethodLit(CompInfo *comp, Value val) {
	BMethodInfo* f = comp->method;
	mem_growvector(comp->th, f->lits, f->nbrlits, f->litsz, Value, INT_MAX);
	f->lits[f->nbrlits] = val;
	mem_markChk(comp->th, comp, val);
	return f->nbrlits++;
}

/* Look for variable in locvars: return index if found, otherwise -1 */
int findBlockVar(Value th, Value locvars, Value varnm) {
	int nbrlocals = arr_size(locvars);
	for (int idx = nbrlocals - 1; idx > 1; idx--) {
		if (arrGet(th, locvars, idx) == varnm)
			return idx-2+toAint(arrGet(th, locvars, 1)); // relative to base index
	}
	return -1;
}

/* Look for local variable. Returns idx if found, -1 otherwise. */
int findLocalVar(CompInfo *comp, Value varnm) {
	assert(isSym(varnm));

	Value th = comp->th;
	Value locvars = comp->locvarseg;
	do {
		// Look to see if variable already defined as local
		// Ignore first two values (link pointer and base index number)
		int nbrlocals = arr_size(locvars);
		for (int idx = nbrlocals - 1; idx > 1; idx--) {
			if (arrGet(th, locvars, idx) == varnm)
				return idx-2+toAint(arrGet(th, locvars, 1)); // relative to base index
		}
		locvars = arrGet(th, locvars, 0); // link to prior local variables
	} while (locvars!=aNull);
	return -1;
}

/* Look for closure variable. Returns idx if found, -1 otherwise. */
int findClosureVar(CompInfo *comp, Value varnm) {
	assert(isSym(varnm));

	// Look to see if variable already defined as closure
	int nbrclosures = arr_size(comp->clovarseg);
	for (int idx = nbrclosures - 1; idx >= 0; idx--) {
		if (arrGet(comp->th, comp->clovarseg, idx) == varnm)
			return idx;
	}
	return -1;
}

/** If variable not declared already, declare it */
void declareLocal(CompInfo *comp, Value varnm) {
	// If explicit 'local' declaration, declare if not found in block list
	if (comp->forcelocal) {
		if (findBlockVar(comp->th, comp->locvarseg, varnm))
			arrAdd(comp->th, comp->locvarseg, varnm);
	}
	// If implicit variable, declare as local or closure, if not found in this or any outer block
	else if (findLocalVar(comp, varnm)==-1 && findClosureVar(comp, varnm)==-1)
		// Declare as closure var if we are not forcing local and if found as local in outer method. Otherwise, declare as local
		arrAdd(comp->th, comp->prevcomp!=aNull && findLocalVar((CompInfo*)comp->prevcomp, varnm)!=-1? 
			comp->clovarseg : comp->locvarseg, varnm);
}

/** Parse an atomic value: literal, variable or pseudo-variable */
void parseValue(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	// Literal token (number, symbol, string, true, false, null)
	if (comp->lex->toktype == Lit_Token) {
		astAddSeg2(th, astseg, vmlit(SymLit), comp->lex->token);
		lexGetNextToken(comp->lex);
	}
	// Static unquoted @url
	else if (comp->lex->toktype == Url_Token) {
		astAddSeg2(th, astseg, vmlit(SymExt), anInt(genAddUrlLit(comp, comp->lex->token)));
		lexGetNextToken(comp->lex);
	}
	// Local or global variable / name token
	else if (comp->lex->toktype == Name_Token) {
		const char first = (toStr(comp->lex->token))[0];
		if (first=='$' || (first>='A' && first<='Z'))
			astAddSeg2(th, astseg, vmlit(SymGlobal), comp->lex->token);
		else {
			declareLocal(comp, comp->lex->token); // declare local if not already declared
			// We do not resolve to index until gen because of else clauses (declaration after use)
			astAddSeg2(th, astseg, vmlit(SymLocal), comp->lex->token);
		}
		lexGetNextToken(comp->lex);
	}
	// 'baseurl' pseudo-variable
	else if (lexMatchNext(comp->lex, "baseurl")) {
		astAddValue(th, astseg, vmlit(SymBaseurl));
	}
	// 'this' pseudo-variable
	else if (lexMatchNext(comp->lex, "this")) {
		astAddValue(th, astseg, vmlit(SymThis));
	}
	// 'self' pseudo-variable
	else if (lexMatchNext(comp->lex, "self")) {
		astAddValue(th, astseg, vmlit(SymSelf));
	}
	// '...' splat
	else if (lexMatchNext(comp->lex, "...")) {
		astAddValue(th, astseg, vmlit(SymSplat));
	}
	// parenthetically-enclosed expression
	else if (lexMatchNext(comp->lex, "(")) {
		parseExp(comp, astseg);
		if (!lexMatchNext(comp->lex, ")"))
			lexLog(comp->lex, "Expected ')'.");
	}
	// Method definition
	else if (lexMatch(comp->lex, "[")) {
		// New compilation context for method's parms and code block
		pushValue(th, vmlit(SymNew));
		pushGloVar(th, "Method");
		pushValue(th, comp);
		getCall(th, 2, 1);
		// Stick returned compiled method reference in extern section of this method's literals
		astAddSeg2(th, astseg, vmlit(SymExt), anInt(genAddMethodLit(comp, getFromTop(th, 0))));
		popValue(th);
	}
	return;
}

/** Add a list of parameters to a AST propseg */
void parseParams(CompInfo* comp, Value propseg, const char *closeparen) {
	bool saveforcelocal = comp->forcelocal;
	comp->forcelocal = false;

	parseAppendExp(comp, propseg);
	while (lexMatchNext(comp->lex, ","))
		parseAppendExp(comp, propseg);

	if (!lexMatchNext(comp->lex, closeparen))
		lexLog(comp->lex, "Expected ')' or ']' at end of parameter/index list.");

	comp->forcelocal = saveforcelocal;
}

/** Determine if token is '.' '.:' or '::' */
#define isdots(token) ((token)==vmlit(SymDot) || (token)==vmlit(SymColons) || (token)==vmlit(SymDotColon))

/** Parse a compound term, handling new and suffixes */
void parseTerm(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	// Capture whether term began with a "+" prefix
	bool newflag = lexMatchNext(comp->lex, "+");
	// Obtain base value (dots as prefix implies 'this' as base value)
	if (!newflag && isdots(comp->lex->token))
		astAddValue(th, astseg, vmlit(SymThis));
	else
		parseValue(comp, astseg);
	// Handle suffix chains
	while (newflag || isdots(comp->lex->token) || lexMatch(comp->lex, "(") || lexMatch(comp->lex, "[")) {
		bool getparms = true;
		Value propseg = astInsSeg(th, astseg, vmlit(SymActProp), 4); // may adjust op later
		// Treat '+' as calling .New
		if (newflag) {
			astAddSeg2(th, propseg, vmlit(SymLit), vmlit(SymNew));
			newflag = false; // only works once
		}
		// For pure method call, adjust to be: self.method
		else if (lexMatch(comp->lex, "(")) {
			arrSet(th, propseg, 2, arrGet(th, propseg, 1));
			arrSet(th, propseg, 1, vmlit(SymSelf));
		}
		// For indexing, adjust to: base.'[]'
		else if (lexMatchNext(comp->lex, "[")) {
			astSetValue(th, propseg, 0, vmlit(SymCallProp)); // adjust because of parms
			astAddSeg2(th, propseg, vmlit(SymLit), vmlit(SymBrackets));
			parseParams(comp, propseg, "]");
			getparms = false;
		}
		// Handle '.', '.:', and '::'
		else {
			if (lexMatch(comp->lex, ".:")) {
				astSetValue(th, propseg, 0, vmlit(SymRawProp));
				getparms = false;
			}
			else if (lexMatch(comp->lex, "::")) {
				astSetValue(th, propseg, 0, vmlit(SymCallProp));
				astAddSeg2(th, propseg, vmlit(SymLit), vmlit(SymBrackets));
				getparms = false;
			}
			lexGetNextToken(comp->lex); // scan past dot(s) operator

			// Retrieve the property specified after the dot(s) operator
			if (comp->lex->toktype == Name_Token || comp->lex->toktype == Lit_Token) {
				astAddSeg2(th, propseg, vmlit(SymLit), comp->lex->token);
				lexGetNextToken(comp->lex);
			}
			// Calculated property symbol/method value
			else if (lexMatchNext(comp->lex, "(")) {
				parseExp(comp, propseg);
				if (!lexMatchNext(comp->lex, ")"))
					lexLog(comp->lex, "Expected ')' at end of property expression.");
			}
			else {
				astAddSeg2(th, propseg, vmlit(SymLit), aNull);
				lexLog(comp->lex, "Expected property expression after '.', '.:', or '::'");
			}
		}

		// Process parameter list, if appropriate for this term suffix
		if (getparms) {
			if (lexMatchNext(comp->lex, "(")) {
				astSetValue(th, propseg, 0, vmlit(SymCallProp)); // adjust because of parms
				parseParams(comp, propseg, ")");
			}
			// Treat Text or Symbol literal as a single parameter to pass
			else if (comp->lex->toktype == Lit_Token && (isStr(comp->lex->token) || isSym(comp->lex->token))) {
				astSetValue(th, propseg, 0, vmlit(SymCallProp)); // adjust because of parm
				astAddSeg2(th, propseg, vmlit(SymLit), comp->lex->token);
				lexGetNextToken(comp->lex);
			}
		}
	}
}

/** Parse a prefix operator */
void parsePrefixExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	if (lexMatchNext(comp->lex, "-")) {
		parsePrefixExp(comp, astseg);

		// Optimize taking the negative of a literal number
		Value selfseg = astGetLast(th, astseg);
		if (astGet(th, selfseg, 0)==vmlit(SymLit) && isFloat(astGet(th, selfseg, 1)))
			astSetValue(th, selfseg, 1, aFloat(-toAfloat(astGet(th, selfseg, 1))));
		else if (astGet(th, selfseg, 0)==vmlit(SymLit) && isInt(astGet(th, selfseg, 1)))
			astSetValue(th, selfseg, 1, anInt(-toAint(astGet(th, selfseg, 1))));
		else { // Not a literal number? Do the property call
			astseg = astInsSeg(th, astseg, vmlit(SymCallProp), 3);
			Value litseg = astAddSeg(th, astseg, vmlit(SymLit), 2);
			astAddValue(th, litseg, vmlit(SymNeg));
		}
	}
	// '@' + symbol, text or '('exp')'
	else if (lexMatchNext(comp->lex, "@")) {
		// Symbol or text: treat as static, unquoted url
		if (comp->lex->toktype == Lit_Token) {
			assert(isStr(comp->lex->token) || isSym(comp->lex->token));
			// +Resource(token,baseurl)
			pushValue(th, vmlit(SymNew));
			pushValue(th, vmlit(TypeResc));
			pushValue(th, comp->lex->token);
			pushValue(th, comp->lex->url);
			getCall(th, 3, 1);
			// ('lit', resource)
			astAddSeg2(th, astseg, vmlit(SymExt), anInt(genAddUrlLit(comp, getFromTop(th, 0))));
			popValue(th);
			lexGetNextToken(comp->lex);
		}
		else {
			// ('callprop', ('callprop', glo'Resource', lit'New', parsed-value, 'baseurl'), 'Load')
			Value loadseg = astAddSeg(th, astseg, vmlit(SymCallProp), 3);
			Value newseg = astAddSeg(th, loadseg, vmlit(SymCallProp), 5);
			astAddSeg2(th, newseg, vmlit(SymGlobal), vmlit(SymResource));
			astAddSeg2(th, newseg, vmlit(SymLit), vmlit(SymNew));
			parsePrefixExp(comp, newseg);
			astAddValue(th, newseg, vmlit(SymBaseurl));
			astAddSeg2(th, loadseg, vmlit(SymLit), vmlit(SymLoad));
		}
	}
	else
		parseTerm(comp, astseg);
}

/** Parse the '**' operator */
void parsePowerExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	parsePrefixExp(comp, astseg);
	Value op = comp->lex->token;
	while (lexMatchNext(comp->lex, "**")) {
		Value newseg = astInsSeg(th, astseg, vmlit(SymCallProp), 4);
		astAddSeg2(th, newseg, vmlit(SymLit), op);
		parsePrefixExp(comp, newseg);
	}
}

/** Parse the '*', '/' or '%' binary operator */
void parseMultDivExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	parsePowerExp(comp, astseg);
	Value op;
	while ((op=comp->lex->token) && ((lexMatchNext(comp->lex, "*")) 
		|| lexMatchNext(comp->lex, "/") || lexMatchNext(comp->lex, "%"))) {
		Value newseg = astInsSeg(th, astseg, vmlit(SymCallProp), 4);
		astAddSeg2(th, newseg, vmlit(SymLit), op);
		parsePowerExp(comp, newseg);
	}
}

/** Parse the '+' or '-' binary operator */
void parseAddSubExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	parseMultDivExp(comp, astseg);
	int isAdd;
	while ((isAdd = lexMatchNext(comp->lex, "+")) || lexMatchNext(comp->lex, "-")) {
		Value newseg = astInsSeg(th, astseg, vmlit(SymCallProp), 4);
		astAddSeg2(th, newseg, vmlit(SymLit), isAdd? vmlit(SymPlus) : vmlit(SymMinus));
		parseMultDivExp(comp, newseg);
	}
}

/** Parse the range .. constructor operator */
void parseRangeExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	parseAddSubExp(comp, astseg);
	if (lexMatchNext(comp->lex, "..")) {
		// ('CallProp', 'Range', 'New', from, to, step)
		Value newseg = astInsSeg(th, astseg, vmlit(SymCallProp), 4);
		Value from = pushValue(th, arrGet(th, newseg, 1));
		arrDel(th, newseg, 1, 1);
		astAddSeg2(th, newseg, vmlit(SymGlobal), vmlit(SymRange));
		astAddSeg2(th, newseg, vmlit(SymLit), vmlit(SymNew));
		astAddValue(th, newseg, from);
		popValue(th);
		parseAddSubExp(comp, newseg);
		if (lexMatchNext(comp->lex, ".."))
			parseAddSubExp(comp, newseg);
	}
}

/** Parse the comparison operators */
void parseCompExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	parseRangeExp(comp, astseg);
	Value op = comp->lex->token;
	if (lexMatchNext(comp->lex, "<=>")) {
		Value newseg = astInsSeg(th, astseg, vmlit(SymCallProp), 4);
		astAddSeg2(th, newseg, vmlit(SymLit), op);
		parseRangeExp(comp, newseg);
	}
	else if (lexMatchNext(comp->lex, "===") || lexMatchNext(comp->lex, "=~")
		|| lexMatchNext(comp->lex, "==") || lexMatchNext(comp->lex, "!=")
		|| lexMatchNext(comp->lex, "<=") || lexMatchNext(comp->lex, ">=")
		|| lexMatchNext(comp->lex, "<") || lexMatchNext(comp->lex, ">")) {
		Value newseg = astInsSeg(th, astseg, op, 3);
		parseRangeExp(comp, newseg);
	}
}

/* Parse 'not' conditional logic operator */
void parseNotExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	bool takenot = false;
	while ((lexMatchNext(comp->lex, "!")) || lexMatchNext(comp->lex, "not"))
		takenot = !takenot;
	if (takenot) {
		Value newseg = astAddSeg(th, astseg, vmlit(SymNot), 2);
		parseCompExp(comp, newseg);
	}
	else
		parseCompExp(comp, astseg);
}

/* Parse 'and' conditional logic operator */
void parseAndExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	parseNotExp(comp, astseg);
	if ((lexMatchNext(comp->lex, "&&")) || lexMatchNext(comp->lex, "and")) {
		Value newseg = astInsSeg(th, astseg, vmlit(SymAnd), 3);
		do {
			parseNotExp(comp, newseg);
		} while ((lexMatchNext(comp->lex, "&&")) || lexMatchNext(comp->lex, "and"));
	}
}

/** Parse 'or' conditional logic operator */
void parseLogicExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	parseAndExp(comp, astseg);
	if ((lexMatchNext(comp->lex, "||")) || lexMatchNext(comp->lex, "or")) {
		Value newseg = astInsSeg(th, astseg, vmlit(SymOr), 3);
		do {
			parseAndExp(comp, newseg);
		} while ((lexMatchNext(comp->lex, "||")) || lexMatchNext(comp->lex, "or"));
	}
}

/** Parse '?' 'else' ternary operator */
void parseTernaryExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	parseLogicExp(comp, astseg);
	if ((lexMatchNext(comp->lex, "?"))) {
		Value newseg = astInsSeg(th, astseg, vmlit(SymQuestion), 4);
		parseLogicExp(comp, newseg);
		if (lexMatchNext(comp->lex, "else"))
			parseLogicExp(comp, newseg);
		else {
			astAddSeg2(th, newseg, vmlit(SymLit), aNull);
			lexLog(comp->lex, "Expected 'else' in ternary expression");
		}
	}
}

/** Parse append and prepend operators */
void parseAppendExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	// If prefix, assume 'this'. Otherwise get left hand value
	if (lexMatch(comp->lex, "<<") || lexMatch(comp->lex, ">>"))
		astAddValue(th, astseg, vmlit(SymThis));
	else
		parseTernaryExp(comp, astseg);

	Value op;
	while ((op=comp->lex->token) && lexMatchNext(comp->lex, "<<") || lexMatchNext(comp->lex, ">>")) {
		Value newseg = astInsSeg(th, astseg, vmlit(SymCallProp), 4);
		astAddSeg2(th, newseg, vmlit(SymLit), op);
		parseTernaryExp(comp, newseg);
	}
}

/** Parse comma separated expressions */
void parseCommaExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	parseAppendExp(comp, astseg);
	if (lexMatch(comp->lex, ",")) {
		Value commaseg = astInsSeg(th, astseg, vmlit(SymComma), 4);
		while (lexMatchNext(comp->lex, ",")) {
			parseAppendExp(comp, commaseg);
		}
	}
}

/** Parse an assignment or property setting expression */
void parseAssgnExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	bool isColonEq;

	// Get lvals (could be rvals if no assignment operator is found)
	// Presence of 'local' ensures unknown locals are declared as locals vs. closure vars
	bool saveforcelocal = comp->forcelocal;
	comp->forcelocal = lexMatchNext(comp->lex, "local");
	bool inparens = lexMatch(comp->lex, "(");
	parseCommaExp(comp, astseg);
	comp->forcelocal = saveforcelocal;

	// Process rvals depending on type of assignment
	if (lexMatch(comp->lex, "=")) {
		Value assgnseg = astInsSeg(th, astseg, vmlit(SymAssgn), 3);
		// Warn about unalterable literals or pseudo-variables to the left of "="
		Value lvalseg = arrGet(th, assgnseg, 1);
		if (arrGet(th, lvalseg, 0) == vmlit(SymComma)) {
			Value lval;
			for (Auint i = 1; i<arr_size(lvalseg); i++) {
				if (!astIsLval(th, lval = arrGet(th, lvalseg, i))) {
					lexLog(comp->lex, "Literals/pseudo-variables/expressions cannot be altered.");
					break;
				}
			}
		}
		else if (!astIsLval(th, lvalseg)) {
			lexLog(comp->lex, "Literals/pseudo-variables/expressions cannot be altered.");
		}
		lexGetNextToken(comp->lex); // Go past assignment operator
		parseAssgnExp(comp, assgnseg); // Get the values to the right
	}
	else if ((isColonEq = lexMatchNext(comp->lex, ":=")) || lexMatchNext(comp->lex, ":")) {
		// Turn variable name before ':' into a literal
		if (!inparens) {
			Value lval = arrGet(th, astseg, arr_size(astseg)-1);
			Value op = isArr(lval)? arrGet(th, lval, 0) : aNull;
			if (op==vmlit(SymLocal) || op==vmlit(SymGlobal))
				arrSet(th, lval, 0, vmlit(SymLit));
		}
		// ('=', ('activeprop'/'callprop', 'this', ('[]',) property), value)
		Value assgnseg = astInsSeg(th, astseg, vmlit(SymAssgn), 3);
		Value indexseg = astPushNew(th, vmlit(isColonEq? SymCallProp : SymActProp), 4);
		astAddValue(th, indexseg, vmlit(SymThis));
		if (isColonEq)
			astAddSeg2(th, indexseg, vmlit(SymLit), vmlit(SymBrackets));
		astPopNew(th, assgnseg, indexseg);
		parseAssgnExp(comp, assgnseg);
	}
}

/** Parse an expression */
void parseExp(CompInfo* comp, Value astseg) {
	parseAssgnExp(comp, astseg);
}

/** Set up block variable list and add it to astseg */
Value parseNewBlockVars(CompInfo *comp, Value astseg) {
	Value th = comp->th;
	// Set up block variable list
	Value blkvars = pushArray(th, vmlit(TypeListm), 8);
	arrSet(th, blkvars, 0, comp->locvarseg);
	arrSet(th, blkvars, 1, anInt(0));
	comp->locvarseg = blkvars;
	astAddValue(th, astseg, blkvars);
	popValue(th); // blkvars
	return blkvars;
}

/** Parse an expression statement / 'this' block */
void parseThisExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	Value svlocalvars = comp->locvarseg;
	parseAssgnExp(comp, astseg);
	if (lexMatchNext(comp->lex, "using")) {
		Value newseg = astInsSeg(th, astseg, vmlit(SymThisBlock), 5);
		parseNewBlockVars(comp, newseg);
		parseAssgnExp(comp, newseg);
		parseBlock(comp, newseg);
	}
	else if (lexMatch(comp->lex, "{")) {
		Value newseg = astInsSeg(th, astseg, vmlit(SymThisBlock), 5);
		parseNewBlockVars(comp, newseg);
		astAddValue(th, newseg, aNull);
		parseBlock(comp, newseg);
	}
	comp->locvarseg = svlocalvars;
}

/** Expect ';' at this point. Error if not found, then scan to find it. */
void parseSemi(CompInfo* comp, Value astseg) {
	// Allow right curly brace and end-of-file to stand in for a semi-colon
	if (!lexMatchNext(comp->lex, ";")&&!lexMatch(comp->lex, "}")&&comp->lex->toktype!=Eof_Token) {
		lexLog(comp->lex, "Unexpected token in statement. Ignoring all until block or ';'.");
		while (comp->lex->toktype != Eof_Token && !lexMatch(comp->lex, "}") && !lexMatchNext(comp->lex, ";"))
			if (lexMatch(comp->lex, "{"))
				parseBlock(comp, astseg);
			else
				lexGetNextToken(comp->lex);
	}
}

/** Parse the each clause: vars and iterator */
void parseEachClause(CompInfo *comp, Value newseg) {
	Value th = comp->th;

	// Set up block variable list
	Value blkvars = parseNewBlockVars(comp, newseg);

	// Parse list of 'each' variables (into new variable block)
	AuintIdx bvarsz=2;
	do {
		if (comp->lex->toktype==Name_Token) {
			arrSet(th, blkvars, bvarsz++, comp->lex->token);
			lexGetNextToken(comp->lex);
		}
		else
			lexLog(comp->lex, "Expected variable name");
		// Assign null variable for "key", if none specified using ':'
		if (bvarsz==3 && !lexMatch(comp->lex, ":")) {
			arrSet(th, blkvars, bvarsz++, arrGet(th, blkvars, 2));
			arrSet(th, blkvars, 2, aNull);
		}
	} while (lexMatchNext(comp->lex, ",") || lexMatchNext(comp->lex, ":"));
	astAddValue(th, newseg, anInt(bvarsz-2)); // expected number of 'each' values

	// 'in' clause
	if (!lexMatchNext(comp->lex, "in"))
		lexLog(comp->lex, "Expected 'in'");
	parseLogicExp(comp, newseg);
}

/** Parse 'if', 'while' or 'each' statement clauses */
void parseClause(CompInfo* comp, Value astseg, AuintIdx stmtvarsz) {
	Value th = comp->th;
	Value svlocalvars = comp->locvarseg; // Statement's local block
	Value deeplocalvars = aNull; // First/deepest clause's local block
	Value inslocalvars = aNull;  // prior/deeper clause's local block
	// Handle multiple clauses so they execute in reverse order
	while (lexMatch(comp->lex, "if") || lexMatch(comp->lex, "while") || lexMatch(comp->lex, "each")) {
		Value ctlseg;
		Value ctltype = comp->lex->token;
		if (lexMatchNext(comp->lex, "if") || lexMatchNext(comp->lex, "while")) {
			// Wrap 'if' single statement in a block (so that fixing implicit returns works)
			if (ctltype==vmlit(SymIf))
				astInsSeg(th, astseg, vmlit(SymSemicolon), 2);
			ctlseg = astPushNew(th, ctltype, 4);
			parseNewBlockVars(comp, ctlseg);
			parseLogicExp(comp, ctlseg);
		}
		else if (lexMatchNext(comp->lex, "each")) {
			ctlseg = astPushNew(th, ctltype, 5);
			parseEachClause(comp, ctlseg); // var and 'in' iterator
		}
		astPopNew(th, astseg, ctlseg); // swap in place of block

		// Linkage of variable scoping for clauses is intricate
		if (inslocalvars != aNull) // link prior clause to current
			arrSet(th, inslocalvars, 0, comp->locvarseg);
		if (deeplocalvars == aNull)
			deeplocalvars = comp->locvarseg; // Remember first/deepest
		inslocalvars = comp->locvarseg; // Remember most recent
		comp->locvarseg = svlocalvars; // Restore to statement block
	}
	parseSemi(comp, astseg);

	// Move any new locals declared in statement to deepest clause's block scope
	if (deeplocalvars!=aNull && stmtvarsz<arr_size(svlocalvars)) {
		comp->locvarseg = deeplocalvars;
		for (AuintIdx vari = arr_size(svlocalvars)-1; vari >= stmtvarsz; vari--) {
			// Pop off statement's declared local, and if not found, add to deepest block
			// Find check is needed to see each's declared variables, for example
			Value varnm = pushValue(th, arrGet(th, svlocalvars, vari));
			arrSetSize(th, svlocalvars, vari);
			if (findLocalVar(comp, varnm)==-1)
				arrAdd(th, deeplocalvars, varnm);
			popValue(th);
		}
		arrSetSize(th, svlocalvars, stmtvarsz); // Remove from outer block vars
		comp->locvarseg = svlocalvars;
	}
}

/** Parse a sequence of statements, each ending with ';' */
void parseStmts(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	astseg = astAddSeg(th, astseg, vmlit(SymSemicolon), 16);
	Value newseg;
	while (comp->lex->toktype != Eof_Token && !lexMatch(comp->lex, "}")) {
		Value stmt = comp->lex->token;
		AuintIdx stmtvarsz = arr_size(comp->locvarseg); // Remember for clauses
		// 'if' block
		if (lexMatchNext(comp->lex, "if")) {
			newseg = astAddSeg(th, astseg, vmlit(SymIf), 4);
			Value svlocalvars = comp->locvarseg;
			parseNewBlockVars(comp, newseg);
			parseLogicExp(comp, newseg);
			parseBlock(comp, newseg);
			comp->locvarseg = svlocalvars;
			parseSemi(comp, astseg);
			while (lexMatchNext(comp->lex, "elif")) {
				parseNewBlockVars(comp, newseg);
				parseLogicExp(comp, newseg);
				parseBlock(comp, newseg);
				comp->locvarseg = svlocalvars;
				parseSemi(comp, astseg);
			}
			if (lexMatchNext(comp->lex, "else")) {
				parseNewBlockVars(comp, newseg);
				astAddValue(th, newseg, vmlit(SymElse));
				parseBlock(comp, newseg);
				comp->locvarseg = svlocalvars;
				parseSemi(comp, astseg);
			}
		}

		// 'match' block
		if (lexMatchNext(comp->lex, "match")) {
			newseg = astAddSeg(th, astseg, vmlit(SymMatch), 4);
			Value svlocalvars = comp->locvarseg;
			parseExp(comp, newseg);
			if (lexMatchNext(comp->lex, "using"))
				parseAssgnExp(comp, newseg);
			else
				astAddValue(comp, newseg, vmlit(SymMatchOp));
			parseSemi(comp, astseg);
			while (lexMatchNext(comp->lex, "with")) {
				parseNewBlockVars(comp, newseg);
				parseExp(comp, newseg);
				parseBlock(comp, newseg);
				comp->locvarseg = svlocalvars;
				parseSemi(comp, astseg);
			}
			if (lexMatchNext(comp->lex, "else")) {
				parseNewBlockVars(comp, newseg);
				astAddValue(th, newseg, vmlit(SymElse));
				parseBlock(comp, newseg);
				comp->locvarseg = svlocalvars;
				parseSemi(comp, astseg);
			}
		}

		// 'while' block
		else if (lexMatchNext(comp->lex, "while")) {
			newseg = astAddSeg(th, astseg, vmlit(SymWhile), 4);
			Value svlocalvars = comp->locvarseg;
			parseNewBlockVars(comp, newseg);
			parseLogicExp(comp, newseg);
			parseBlock(comp, newseg);
			comp->locvarseg = svlocalvars;
			parseSemi(comp, astseg);
		}

		// 'each': ('each', localvars, nretvals, iterator, block)
		else if (lexMatchNext(comp->lex, "each")) {
			newseg = astAddSeg(th, astseg, vmlit(SymEach), 5);
			Value svlocalvars = comp->locvarseg;
			parseEachClause(comp, newseg); // vars and 'in' iterator
			parseBlock(comp, newseg);
			comp->locvarseg = svlocalvars;
			parseSemi(comp, astseg);
		}

		// 'do': ('do', local, exp, block)
		else if (lexMatchNext(comp->lex, "do")) {
			newseg = astAddSeg(th, astseg, vmlit(SymDo), 4);
			Value svlocalvars = comp->locvarseg;
			parseNewBlockVars(comp, newseg);
			if (!lexMatch(comp->lex, "{"))
				parseExp(comp, newseg);
			else
				astAddValue(th, newseg, aNull);
			parseBlock(comp, newseg);
			comp->locvarseg = svlocalvars;
			parseSemi(comp, astseg);
		}

		// 'break' or 'continue' statement
		else if (lexMatchNext(comp->lex, "break") || lexMatchNext(comp->lex, "continue")) {
			astAddSeg(th, astseg, stmt, 1);
			parseClause(comp, astseg, stmtvarsz);
		}

		// 'return' statement
		else if (lexMatchNext(comp->lex, "return") || lexMatchNext(comp->lex, "yield")) {
			newseg = astAddSeg(th, astseg, stmt, 2);
			if (!lexMatch(comp->lex, ";") && !lexMatch(comp->lex, "if")
				&& !lexMatch(comp->lex, "each") && !lexMatch(comp->lex, "while"))
				parseThisExp(comp, newseg);
			else
				astAddValue(th, newseg, aNull);
			parseClause(comp, astseg, stmtvarsz);
		}

		// expression or 'this' block
		else {
			if (stmt != vmlit(SymSemicolon)) {
				parseThisExp(comp, astseg);
				parseClause(comp, astseg, stmtvarsz);
			}
		}
	}
	return;
}

/** Parse a block of statements enclosed by '{' and '}' */
void parseBlock(CompInfo* comp, Value astseg) {
	if (!lexMatchNext(comp->lex, "{"))
		return;
	parseStmts(comp, astseg);
	if (!lexMatchNext(comp->lex, "}"))
		lexLog(comp->lex, "Expected '}'");
	return;
}

/* Parse an Acorn program */
void parseProgram(CompInfo* comp) {
	Value th = comp->th;
	Value methast = comp->ast;
	astAddValue(th, methast, vmlit(SymMethod));

	// local variable list - starts with pointer to outer method's local variable list
	comp->locvarseg = astAddSeg(th, methast, aNull, 16);
	astAddValue(th, comp->locvarseg, anInt(1));

	// closure variable list
	comp->clovarseg = pushArray(th, aNull, 4);
	arrAdd(th, methast, comp->clovarseg);
	popValue(th);

	Value parminitast = astAddSeg(th, methast, vmlit(SymSemicolon), 4);

	// process parameters as local variables
	if (lexMatchNext(comp->lex, "[")) {
		do {
			if (lexMatchNext(comp->lex, "...")) {
				methodFlags(comp->method) |= METHOD_FLG_VARPARM;
				break;
			}
			else if (comp->lex->toktype == Name_Token) {
				Value symnm = comp->lex->token;
				const char first = (toStr(symnm))[0];
				if (first=='$' || (first>='A' && first<='Z'))
					lexLog(comp->lex, "A global name may not be a method parameter");
				else {
					if (findLocalVar(comp, symnm)==-1) {
						arrAdd(th, comp->locvarseg, symnm);
						methodNParms(comp->method)++;
					}
					else
						lexLog(comp->lex, "Duplicate method parameter name");
				}
				lexGetNextToken(comp->lex);

				// Handle any specified parameter default value
				if (lexMatchNext(comp->lex, "=")) {
					// Produce this ast: parm||=default-expression
					Value oreqseg = astAddSeg(th, parminitast, vmlit(SymOrAssgn), 3);
					astAddSeg2(th, oreqseg, vmlit(SymLocal), symnm);
					parseExp(comp, oreqseg);
				}
			}
		} while (lexMatchNext(comp->lex, ","));
		lexMatchNext(comp->lex, "]");
		parseBlock(comp, methast);
	}
	else
		parseStmts(comp, methast);

	comp->method->nbrlocals = arr_size(comp->locvarseg)-1;
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
