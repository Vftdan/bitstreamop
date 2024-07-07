#ifndef LEXER_H_
#define LEXER_H_

#include "token_types.h"

typedef enum {
	CHCLS_UNKNOWN,
	CHCLS_WS,
	CHCLS_ALPH,
	CHCLS_NUM,
	CHCLS_PUNCT,
	CHCLS_INVAL,
} CharacterClass;

typedef struct lexer Lexer;

Lexer * lexer_new(void);

void lexer_feed(Lexer * lexer, const char * ptr, size_t length);

void lexer_end(Lexer * lexer);

void lexer_delete(Lexer * lexer);

bool lexer_has_token(Lexer * lexer);

TokenData * lexer_take_token(Lexer * lexer);

__attribute__((unused)) inline static CharacterClass
ch_classify(unsigned char ch)
{
	if (ch >= 127)
		return CHCLS_INVAL;
	if (ch <= ' ')
		return CHCLS_WS;
	if ('0' <= ch && ch <= '9')
		return CHCLS_NUM;
	if (
		   (ch == '_')
		|| ('A' <= ch && ch <= 'Z')
		|| ('a' <= ch && ch <= 'z')
	)
		return CHCLS_ALPH;
	return CHCLS_PUNCT;
}

#endif /* end of include guard: LEXER_H_ */
