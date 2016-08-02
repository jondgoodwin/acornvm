/** Definitions for Acorn compiler
 *
 * @file
 *
 * This source file is part of avm - Acorn Virtual Machine.
 * See Copyright Notice in avm.h
*/

#ifndef acn_main_h
#define acn_main_h

#ifdef __cplusplus
namespace avm {
extern "C" {
#endif

/** What sort of token is in token */
enum TokenType {
	Lit_Token,		//!< Literal token: null, true, false, int, float, symbol, string
	Name_Token,		//!< Named identifier (e.g., for a variable)
	Res_Token,		//!< Reserved word or operator
	Eof_Token		//!< End of file
};
	
/** Lexer state */
typedef struct LexInfo {
	MemCommonInfo;	//!< Common header

	Value source;	//!< The source text
	Value url;		//!< The url where the source text came from
	Value token;	//!< Current token
	Value th;		//!< Current thread

	// Position info
	AuintIdx bytepos;	//!< Current byte position in source
	AuintIdx linenbr;	//!< Current line number
	AuintIdx linebeg;	//!< Beginning of current line
	AuintIdx tokbeg;	//!< Start of current token in source
	AuintIdx tokline;	//!< Line number for current token
	AuintIdx toklinepos; //!< Column position in line for current token

	// indent state
	unsigned int curindent;	//!< Current level of indentation
	unsigned int newindent; //!< Indentation level for current line

	int optype;			//!< sub-type of operator (when type==Op_Token)
	TokenType toktype;	//!< type of the current token
	bool newline;		//!< True if we just started a new non-continued line
	bool newprogram;	//!< True if we have not yet processed any token
} LexState;

/** Mark values for garbage collection 
 * Increments how much allocated memory the lexinfo uses. */
#define lexMark(th, o) \
	{mem_markobj(th, (o)->token); \
	vm(th)->gcmemtrav += sizeof(LexInfo);}

/** Free all of a lexinfo's allocated memory */
#define lexFree(th, o) \
	mem_free(th, (o));

// ***********
// Non-API C-Method functions
// ***********

/** Return a new LexInfo value, lexer context for a source program */
Value newLex(Value th, Value *dest, Value src, Value url);

/** Get the next token */
void lex_getNextToken(LexInfo *lex);

/** Match current token to a reserved symbol.
 * If it matches, advance to the next token */
bool lex_match(LexInfo *lex, const char *sym);

/** Log an compiler message */
void lex_log(LexInfo *lex, const char *msg);

/** A temporary function for generating byte-code test programs */
AVM_API Value genTestPgm(Value th, int pgm);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif