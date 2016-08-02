/** Lexer for Acorn compiler
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

/* Test lexer - bye, bye soon! */
void testlexer(LexInfo* lex) {
	if (lex_match(lex, "+"))
		puts("+ matched!");
	lex_log(lex, "Actually I have no error to report here");
	while (lex->toktype!=Eof_Token) {
		switch (lex->toktype) {
		case Lit_Token: {
			pushSerialized(lex->th, lex->token);
			printf("Literal: %s\n", toStr(getFromTop(lex->th, 0)));
			popValue(lex->th);
			} break;
		case Name_Token: {
			pushSerialized(lex->th, lex->token);
			printf("Name: %s\n", toStr(getFromTop(lex->th, 0)));
			popValue(lex->th);
			} break;
		case Res_Token: {
			pushSerialized(lex->th, lex->token);
			printf("Reserved: %s\n", toStr(getFromTop(lex->th, 0)));
			popValue(lex->th);
			} break;
		}
		lex_getNextToken(lex);
	}
}

#ifdef __cplusplus
} // extern "C"
} // namespace avm
#endif
