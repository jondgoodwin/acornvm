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

/* **********************
 * Parsing routines
 * **********************/

// Prototypes for functions used before they are defined
void parseBlock(CompInfo* comp, Value astseg);
void parseExp(CompInfo* comp, Value astseg);

/** Parse an atomic value: literal, variable or pseudo-variable */
void parseValue(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	if (comp->lex->toktype == Lit_Token) {
		astAddSeg2(th, astseg, vmlit(SymLit), comp->lex->token);
		lexGetNextToken(comp->lex);
	}
	else if (comp->lex->toktype == Name_Token) {
		const char first = (toStr(comp->lex->token))[0];
		if (first=='$' || (first>='A' && first<='Z'))
			astAddSeg2(th, astseg, vmlit(SymGlobal), comp->lex->token);
		else {
			astAddSeg2(th, astseg, vmlit(SymLocal), anInt(genLocalVar(comp, comp->lex->token)));
		}
		lexGetNextToken(comp->lex);
	}
	else if (lexMatchNext(comp->lex, "baseurl")) {
		astAddValue(th, astseg, vmlit(SymBaseurl));
	}
	else if (lexMatchNext(comp->lex, "this")) {
		astAddValue(th, astseg, vmlit(SymThis));
	}
	else if (lexMatchNext(comp->lex, "self")) {
		astAddValue(th, astseg, vmlit(SymSelf));
	}
	else if (lexMatchNext(comp->lex, "(")) {
		parseExp(comp, astseg);
		if (!lexMatchNext(comp->lex, ")"))
			lexLog(comp->lex, "Expected ')'.");
	}
	// A method definition starts its own compilation context
	else if (lexMatch(comp->lex, "[")) {
		pushValue(th, vmlit(SymNew));
		pushGloVar(th, "Method");
		pushValue(th, comp);
		getCall(th, 2, 1);
		astAddSeg2(th, astseg, vmlit(SymLit), getFromTop(th, 0));
		popValue(th);
	}
	return;
}

/** Parse a compound term, handling new and suffixes */
void parseTerm(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	bool newflag = lexMatchNext(comp->lex, "+");
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
			else
				lexLog(comp->lex, "Expected property expression after '.'");
		}
		// Process parameter list
		if (lexMatchNext(comp->lex, "(")) {
			astSetValue(th, propseg, 0, vmlit(SymCallProp)); // adjust because of parms
			parseExp(comp, propseg);

			while (lexMatchNext(comp->lex, ","))
				parseExp(comp, propseg);
			if (!lexMatchNext(comp->lex, ")"))
				lexLog(comp->lex, "Expected ')' at end of parameter list.");
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
	else if (lexMatchNext(comp->lex, "@")) {
		// ('callprop', ('callprop', glo'Resource', lit'new', parsed-value, 'baseurl'), '()')
		Value getseg = astAddSeg(th, astseg, vmlit(SymCallProp), 3);
		Value newseg = astAddSeg(th, getseg, vmlit(SymCallProp), 5);
		astAddSeg2(th, newseg, vmlit(SymGlobal), vmlit(SymResource));
		astAddSeg2(th, newseg, vmlit(SymLit), vmlit(SymNew));
		parsePrefixExp(comp, newseg);
		astAddValue(th, newseg, vmlit(SymBaseurl));
		astAddSeg2(th, getseg, vmlit(SymLit), vmlit(SymParas));
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

/* Parse 'or' conditional logic operator */
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

/** Parse a 'this' expression/block or append/prepend operator */
void parseThisExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	parseCompExp(comp, astseg);
	bool appendflag = lexMatchNext(comp->lex, "<<");
	if (lexMatch(comp->lex, "{")) {
		astseg = astInsSeg(th, astseg, vmlit(SymThisBlock), 3);
		astAddValue(th, astseg, appendflag? vmlit(SymAppend) : aNull);
		parseBlock(comp, astseg);
	}
}

/** Parse an assignment or property setting expression */
void parseAssgnExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	parseThisExp(comp, astseg);
	if (lexMatchNext(comp->lex, "=")) {
		astseg = astInsSeg(th, astseg, vmlit(SymAssgn), 3);
		parseThisExp(comp, astseg);
	}
	else if (lexMatchNext(comp->lex, ":")) {
		// ('=', ('activeprop', 'this', property), value)
		astseg = astInsSeg(th, astseg, vmlit(SymAssgn), 3);
		astInsSeg2(th, astseg, vmlit(SymActProp), vmlit(SymThis), 3);
		parseThisExp(comp, astseg);
	}
	else if (lexMatchNext(comp->lex, ":=")) {
		// ('=', ('rawprop', 'this', property), value)
		astseg = astInsSeg(th, astseg, vmlit(SymAssgn), 3);
		astInsSeg2(th, astseg, vmlit(SymRawProp), vmlit(SymThis), 3);
		parseThisExp(comp, astseg);
	}
}

/** Parse an expression */
void parseExp(CompInfo* comp, Value astseg) {
	parseAssgnExp(comp, astseg);
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
		else if (lexMatchNext(comp->lex, "while")) {
			newseg = astAddSeg(th, astseg, vmlit(SymWhile), 3);
			parseLogicExp(comp, newseg);
			parseBlock(comp, newseg);
			parseSemi(comp, astseg);
		}
		else {
			parseExp(comp, astseg);
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
	astAddValue(th, comp->ast, vmlit(SymMethod));
	genAddParm(comp, vmlit(SymSelf));
	if (lexMatchNext(comp->lex, "[")) {
		do {
			if (comp->lex->toktype == Name_Token) {
				const char first = (toStr(comp->lex->token))[0];
				if (first=='$' || (first>='A' && first<='Z'))
					lexLog(comp->lex, "A global name may not be a method parameter");
				else {
					genAddParm(comp, comp->lex->token);
				}
				lexGetNextToken(comp->lex);
			}
		} while (lexMatchNext(comp->lex, ","));
		lexMatchNext(comp->lex, "]");
		parseBlock(comp, comp->ast);
	}
	else
		parseStmts(comp, comp->ast);
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif


