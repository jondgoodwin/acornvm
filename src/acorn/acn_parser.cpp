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
void parseTernaryExp(CompInfo* comp, Value astseg);
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

/* Look for local variable. Returns idx if found, -1 otherwise. */
int findLocalVar(CompInfo *comp, Value varnm) {
	assert(isSym(varnm));

	// Look to see if variable already defined as local
	int nbrlocals = arr_size(comp->locvarseg);
	for (int idx = nbrlocals - 1; idx > 0; idx--) {
		if (arrGet(comp->th, comp->locvarseg, idx) == varnm)
			return idx-1;
	}
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
void implicitLocal(CompInfo *comp, Value varnm) {
	// If not declared by this method as local or closure ...
	if (findLocalVar(comp, varnm)==-1 && findClosureVar(comp, varnm)==-1)
		// Declare as closure var if we are not forcing local and if found as local in outer method. Otherwise, declare as local
		arrAdd(comp->th, !comp->forcelocal && comp->prevcomp!=aNull && findLocalVar((CompInfo*)comp->prevcomp, varnm)!=-1? 
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
			implicitLocal(comp, comp->lex->token); // declare local if not already declared
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

/** Parse a compound term, handling new and suffixes */
void parseTerm(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	bool newflag = lexMatchNext(comp->lex, "+");
	if (lexMatch(comp->lex, "."))
		astAddValue(th, astseg, vmlit(SymThis));
	else
		parseValue(comp, astseg);
	// Handle suffix chains
	while (newflag || lexMatch(comp->lex, ".") || lexMatch(comp->lex, "(")) {
		Value propseg = astInsSeg(th, astseg, vmlit(SymActProp), 4); // may adjust later
		// Determine the property
		if (newflag) {
			astAddSeg2(th, propseg, vmlit(SymLit), vmlit(SymNew));
			newflag = false; // only works once
		}
		else if (lexMatch(comp->lex, "("))
			astAddSeg2(th, propseg, vmlit(SymLit), vmlit(SymParas));
		else { // '.'
			lexGetNextToken(comp->lex); // scan past '.'
			if (comp->lex->toktype == Name_Token || comp->lex->toktype == Lit_Token) {
				astAddSeg2(th, propseg, vmlit(SymLit), comp->lex->token);
				lexGetNextToken(comp->lex);
			}
			else {
				astAddSeg2(th, propseg, vmlit(SymLit), aNull);
				lexLog(comp->lex, "Expected property expression after '.'");
			}
		}
		// Process parameter list
		if (lexMatchNext(comp->lex, "(")) {
			bool saveforcelocal = comp->forcelocal;
			comp->forcelocal = false;
			astSetValue(th, propseg, 0, vmlit(SymCallProp)); // adjust because of parms
			parseTernaryExp(comp, propseg);

			while (lexMatchNext(comp->lex, ","))
				parseTernaryExp(comp, propseg);
			if (!lexMatchNext(comp->lex, ")"))
				lexLog(comp->lex, "Expected ')' at end of parameter list.");
			comp->forcelocal = saveforcelocal;
		} else if (comp->lex->toktype == Lit_Token && (isStr(comp->lex->token) || isSym(comp->lex->token))) {
			astSetValue(th, propseg, 0, vmlit(SymCallProp)); // adjust because of parm
			astAddSeg2(th, propseg, vmlit(SymLit), comp->lex->token);
			lexGetNextToken(comp->lex);
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

/** Parse the comparison operators */
void parseCompExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	parseAddSubExp(comp, astseg);
	Value op = comp->lex->token;
	if (lexMatchNext(comp->lex, "<=>")) {
		Value newseg = astInsSeg(th, astseg, vmlit(SymCallProp), 4);
		astAddSeg2(th, newseg, vmlit(SymLit), op);
		parseAddSubExp(comp, newseg);
	}
	else if (lexMatchNext(comp->lex, "===") || lexMatchNext(comp->lex, "~=")
		|| lexMatchNext(comp->lex, "==") || lexMatchNext(comp->lex, "!=")
		|| lexMatchNext(comp->lex, "<=") || lexMatchNext(comp->lex, ">=")
		|| lexMatchNext(comp->lex, "<") || lexMatchNext(comp->lex, ">")) {
		Value newseg = astInsSeg(th, astseg, op, 3);
		parseAddSubExp(comp, newseg);
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

/** Parse 'if', 'while' or 'each' suffix */
void parseSuffix(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	Value token = comp->lex->token;
	if (lexMatchNext(comp->lex, "if") || lexMatchNext(comp->lex, "while")) {
		Value newseg = astInsSeg(th, astseg, token, 4);
		parseLogicExp(comp, newseg);
		// swap elements 1 and 2, so condition follows 'if'/'while'
		arrSet(th, newseg, 3, astGet(th, newseg, 1));
		arrDel(th, newseg, 1, 1);
		// Wrap single statement in a block (so that fixing implicit returns works)
		astInsSeg(th, newseg, vmlit(SymSemicolon), 2);
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

/** Parse comma separated expressions */
void parseCommaExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	parseTernaryExp(comp, astseg);
	if (comp->lex->token==vmlit(SymComma)) {
		Value commaseg = astInsSeg(th, astseg, vmlit(SymComma), 4);
		while (lexMatchNext(comp->lex, ",")) {
			parseTernaryExp(comp, commaseg);
		}
	}
}

/** Parse an assignment or property setting expression */
void parseAssgnExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;

	// Get lvals (could be rvals if no assignment operator is found)
	// Presence of 'local' ensures unknown locals are declared as locals vs. closure vars
	bool saveforcelocal = comp->forcelocal;
	comp->forcelocal = lexMatchNext(comp->lex, "local");
	parseCommaExp(comp, astseg);
	comp->forcelocal = saveforcelocal;

	// Process rvals depending on type of assignment
	if (lexMatchNext(comp->lex, "=")) {
		Value assgnseg = astInsSeg(th, astseg, vmlit(SymAssgn), 3);
		// Warn about unalterable literals or pseudo-variables to the left of "="
		Value lvalseg = arrGet(th, assgnseg, 1);
		if (!astIsLval(th, lvalseg))
			lexLog(comp->lex, "Literals/pseudo-variables/expressions cannot be altered.");
		else if (arrGet(th, lvalseg, 0) == vmlit(SymComma)) {
			Value lval;
			for (Auint i = 1; i<arr_size(lvalseg); i++) {
				if (!astIsLval(th, lval = arrGet(th, lvalseg, i))) {
					lexLog(comp->lex, "Literals/pseudo-variables/expressions cannot be altered.");
					break;
				}
			}
		}
		parseAssgnExp(comp, assgnseg); // Get the values to the right
	}
	else if (lexMatchNext(comp->lex, ":")) {
		// ('=', ('activeprop', 'this', property), value)
		astseg = astInsSeg(th, astseg, vmlit(SymAssgn), 3);
		astInsSeg2(th, astseg, vmlit(SymActProp), vmlit(SymThis), 3);
		parseAssgnExp(comp, astseg);
	}
	else if (lexMatchNext(comp->lex, ":=")) {
		// ('=', ('rawprop', 'this', property), value)
		astseg = astInsSeg(th, astseg, vmlit(SymAssgn), 3);
		astInsSeg2(th, astseg, vmlit(SymRawProp), vmlit(SymThis), 3);
		parseAssgnExp(comp, astseg);
	}
}

/** Parse a 'this' expression/block or append/prepend operator */
void parseThisExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	parseAssgnExp(comp, astseg);
	Value op;
	bool opflag = (op=comp->lex->token) 
		&& (lexMatchNext(comp->lex, "<<") || lexMatchNext(comp->lex, ">>"));
	if (lexMatch(comp->lex, "{")) {
		Value newseg = astInsSeg(th, astseg, vmlit(SymThisBlock), 3);
		astAddValue(th, newseg, opflag? op : aNull);
		parseBlock(comp, newseg);
	}
	else if (opflag) {
		do {
			Value newseg = astInsSeg(th, astseg, vmlit(SymCallProp), 4);
			astAddSeg2(th, newseg, vmlit(SymLit), op);
			parseAssgnExp(comp, newseg);
		} while ((op=comp->lex->token) 
		&& (lexMatchNext(comp->lex, "<<") || lexMatchNext(comp->lex, ">>")));
	}
}

/** Parse an expression */
void parseExp(CompInfo* comp, Value astseg) {
	parseThisExp(comp, astseg);
}

void parseSemi(CompInfo* comp, Value astseg) {
	if (!lexMatchNext(comp->lex, ";")&&comp->lex->toktype!=Eof_Token) {
		lexLog(comp->lex, "Unexpected token in statement. Ignoring all until block or ';'.");
		while (comp->lex->toktype != Eof_Token && !lexMatch(comp->lex, "}") && !lexMatchNext(comp->lex, ";"))
			if (lexMatch(comp->lex, "{"))
				parseBlock(comp, astseg);
			else
				lexGetNextToken(comp->lex);
	}
}

/** Parse a sequence of statements, each ending with ';' */
void parseStmts(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	astseg = astAddSeg(th, astseg, vmlit(SymSemicolon), 16);
	Value newseg;
	while (comp->lex->toktype != Eof_Token && !lexMatch(comp->lex, "}")) {
		Value stmt = comp->lex->token;
		// 'if' block
		if (lexMatchNext(comp->lex, "if")) {
			newseg = astAddSeg(th, astseg, vmlit(SymIf), 3);
			parseLogicExp(comp, newseg);
			parseBlock(comp, newseg);
			parseSemi(comp, astseg);
			while (lexMatchNext(comp->lex, "elif")) {
				parseLogicExp(comp, newseg);
				parseBlock(comp, newseg);
				parseSemi(comp, astseg);
			}
			if (lexMatchNext(comp->lex, "else")) {
				astAddValue(th, newseg, vmlit(SymElse));
				parseBlock(comp, newseg);
				parseSemi(comp, astseg);
			}
		}

		// 'while' block
		else if (lexMatchNext(comp->lex, "while")) {
			newseg = astAddSeg(th, astseg, vmlit(SymWhile), 3);
			parseLogicExp(comp, newseg);
			parseBlock(comp, newseg);
			parseSemi(comp, astseg);
		}

		// 'break' or 'continue' statement
		else if (lexMatchNext(comp->lex, "break") || lexMatchNext(comp->lex, "continue")) {
			if (lexMatchNext(comp->lex, "if")) {
				newseg = astAddSeg(th, astseg, vmlit(SymIf), 3); 
				parseLogicExp(comp, newseg);
			}
			else
				newseg = astseg;
			astAddSeg(th, newseg, stmt, 1);
			parseSemi(comp, astseg);
		}

		// 'return' statement
		else if (lexMatchNext(comp->lex, "return")) {
			newseg = astAddSeg(th, astseg, vmlit(SymReturn), 2);
			if (comp->lex->token != vmlit(SymSemicolon) && comp->lex->token != vmlit(SymIf))
				parseExp(comp, newseg);
			else
				astAddValue(th, newseg, aNull);
			if (comp->lex->token == vmlit(SymIf))
				parseSuffix(comp, astseg);
			parseSemi(comp, newseg);
		}

		// expression or 'this' block
		else {
			parseThisExp(comp, astseg);
			parseSuffix(comp, astseg);
			parseSemi(comp, astseg);
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
	comp->locvarseg = astAddSeg(th, methast, comp->prevcomp==aNull? aNull : ((CompInfo*)comp->prevcomp)->locvarseg, 16);

	// closure variable list
	comp->clovarseg = pushArray(th, aNull, 4);
	arrAdd(th, methast, comp->clovarseg);
	popValue(th);

	Value parminitast = astAddSeg(th, methast, vmlit(SymSemicolon), 4);

	// Declare self as first implicit parameter
	arrAdd(th, comp->locvarseg, vmlit(SymSelf));
	methodNParms(comp->method)=1;

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

					/* // Produce this ast: parm=default-expression if parm==null
					Value ifseg = astAddSeg(th, parminitast, vmlit(SymIf), 3);
					Value condseg = astAddSeg(th, ifseg, vmlit(SymEquiv), 3);
					astAddSeg2(th, condseg, vmlit(SymLocal), symnm);
					astAddSeg2(th, condseg, vmlit(SymLit), aNull);
					Value assgnseg = astAddSeg(th, ifseg, vmlit(SymAssgn), 3);
					astAddSeg2(th, assgnseg, vmlit(SymLocal), symnm);
					parseExp(comp, assgnseg);*/
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


