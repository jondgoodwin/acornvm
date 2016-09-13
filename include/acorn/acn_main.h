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
	MemCommonInfoGray;	//!< Common header

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
	bool insertSemi;	//!< True if we need to insert ';' as next token
} LexInfo;

/** Mark values for garbage collection 
 * Increments how much allocated memory the lexinfo uses. */
#define lexMark(th, o) \
	{mem_markobj(th, (o)->token); \
	mem_markobj(th, (o)->source); \
	mem_markobj(th, (o)->url);}

/** Free all of a lexinfo's allocated memory */
#define lexFree(th, o) \
	mem_free(th, (o));

/** Compiler state for a method being compiled */
typedef struct CompInfo {
	MemCommonInfoGray;	//!< Common header

	Value th;				//!< Current thread
	LexInfo *lex;			//!< Lexer info for Acorn source being compiled
	Value ast;				//!< Abstract Syntax Tree
	BMethodInfo* method;	//!< Method whose byte-code is being built
	Value prevcomp;			//!< Compiler for method that defined this method

	// Generation context
	Value thisop;			//!< Operator to use on every 'this' block stmt

	unsigned int nextreg;	//!< Next register available for use
	unsigned int thisreg;	//!< Register holding 'this' value
	int whileBegIp;			//!< ip of current while block's first instruction
	int whileEndIp;			//!< ip of first jump statement to end of current while block
} CompInfo;

/** Mark values for garbage collection 
 * Increments how much allocated memory the compinfo uses. */
#define compMark(th, o) \
	{if ((o)->lex) mem_markobj(th, (o)->lex); \
	if ((void *)(o)->ast) mem_markobj(th, (o)->ast); \
	if ((o)->method) mem_markobj(th, (o)->method); \
	mem_markobj(th, (o)->prevcomp);}

/** Free all of a compinfo's allocated memory */
#define compFree(th, o) \
	mem_free(th, (o));

// ***********
// Non-API C-Method functions
// ***********

/** Method to compile an Acorn method.
   Pass it a string containing the program source and a symbol or null for the baseurl.
   It returns the compiled byte-code method. */
int acn_newmethod(Value th);

/** Method to compile and run an Acorn program.
   Pass it a string containing the program source and a symbol or null for the baseurl.
   Any additional parameters are passed to the compiled method when run. */
int acn_newprogram(Value th);

/** Return a new CompInfo value, compiler state for an Acorn method */
Value newCompiler(Value th, Value *dest, Value src, Value url);

/** Return a new LexInfo value, lexer context for a source program */
Value newLex(Value th, Value *dest, Value src, Value url);

/** Create a new bytecode method value. */
void newBMethod(Value th, Value *dest);

/** Parse an Acorn program */
void parseProgram(CompInfo* comp);

/** Generate a complete byte-code method by walking the 
 * Abstract Syntax Tree generated by the parser */
void genBMethod(CompInfo *comp);

/** Get the next token */
void lexGetNextToken(LexInfo *lex);

/** Match current token to a reserved symbol. */
bool lexMatch(LexInfo *lex, const char *sym);

/** Match current token to a reserved symbol.
 * If it matches, advance to the next token */
bool lexMatchNext(LexInfo *lex, const char *sym);

/** Log an compiler message */
void lexLog(LexInfo *lex, const char *msg);

/** A temporary function for generating byte-code test programs */
AVM_API Value genTestPgm(Value th, int pgm);

#ifdef __cplusplus
} // end "C"
} // end namespace
#endif

#endif