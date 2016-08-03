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
		astAddSeg2(th, astseg, vmlit(SymGlobal), comp->lex->token); // !! NOT RIGHT !!
		lexGetNextToken(comp->lex);
	}
	return;
}

/** Parse a property value found after '.' */
void parseSymIndex(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	if (comp->lex->toktype == Name_Token || comp->lex->toktype == Lit_Token) {
		lexGetNextToken(comp->lex);
		astAddSeg2(th, astseg, vmlit(SymLit), comp->lex->token);
	}
	else
		lexLog(comp->lex, "Expected property expression after '.'");
}

/** Parse a compound term, handling new and suffixes */
void parseTerm(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	bool newflag = lexMatchNext(comp->lex, "+") || lexMatchNext(comp->lex, "new");
	parseValue(comp, astseg);
	// Handle suffix chains
	while (newflag || lexMatch(comp->lex, ".") || lexMatch(comp->lex, "(")) {
		astseg = astInsSeg(th, astseg, vmlit(SymActProp), 4); // may adjust later
		// Determine the property
		if (newflag) {
			astAddSeg2(th, astseg, vmlit(SymLit), vmlit(SymNew));
			newflag = false; // only works once
		}
		else if (lexMatch(comp->lex, "("))
			astAddSeg2(th, astseg, vmlit(SymLit), vmlit(SymParas));
		else
			parseSymIndex(comp, astseg);
		// Process parameter list
		if (lexMatchNext(comp->lex, "(")) {
			astSetValue(th, astseg, 0, vmlit(SymCallProp)); // adjust because of parms
			parseExp(comp, astseg);
			while (lexMatchNext(comp->lex, ","))
				parseExp(comp, astseg);
			if (!lexMatchNext(comp->lex, ")"))
				lexLog(comp->lex, "Expected ')' at end of parameter list.");
		} else if (comp->lex->toktype == Lit_Token && (isStr(comp->lex->token) || isSym(comp->lex->token))) {
			astSetValue(th, astseg, 0, vmlit(SymCallProp)); // adjust because of parm
			astAddSeg2(th, astseg, vmlit(SymLit), comp->lex->token);
			lexGetNextToken(comp->lex);
		}
	}
}

/** Parse a prefix operator */
void parsePrefixExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	if (lexMatchNext(comp->lex, "-"))
		astseg = astAddSeg(th, astseg, vmlit(SymNeg), 3);
	parseTerm(comp, astseg);
}

/** Parse a 'this' expression/block or append/prepend operator */
void parseThisExp(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	parsePrefixExp(comp, astseg);
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
		astseg = astInsSeg(th, astseg, vmlit(SymColon), 3);
		parseThisExp(comp, astseg);
	}
}

/** Parse an expression */
void parseExp(CompInfo* comp, Value astseg) {
	parseAssgnExp(comp, astseg);
}

/** Parse a sequence of statements, each ending with ';' */
void parseStmts(CompInfo* comp, Value astseg) {
	Value th = comp->th;
	astseg = astAddSeg(th, astseg, vmlit(SymSemicolon), 16);
	while (comp->lex->toktype != Eof_Token && !lexMatch(comp->lex, "}")) {
		parseExp(comp, astseg);
		if (!lexMatchNext(comp->lex, ";")) {
			lexLog(comp->lex, "Unexpected token in statement. Ignoring all until block or ';'.");
			while (comp->lex->toktype != Eof_Token && !lexMatch(comp->lex, "}") && !lexMatchNext(comp->lex, ";"))
				if (lexMatch(comp->lex, "{"))
					parseBlock(comp, astseg);
				else
					lexGetNextToken(comp->lex);
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
	parseStmts(comp, comp->ast);
	if (comp->lex->toktype != Eof_Token) {
		lexLog(comp->lex, "Expected end-of-program. Ignoring everything else until found.");
		while (comp->lex->toktype != Eof_Token)
			lexGetNextToken(comp->lex);
	}
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif


