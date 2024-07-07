#include "lexer.h"

typedef enum {
	LXST_NORMAL,
	LXST_COLON,
	LXST_IDENT,
	LXST_NUMBER,
} LexerState;

struct lexer {
	CharSlice untokenized;
	CharacterClass next_char_class;
	char next_char;
	bool source_end;
	LexerState state;
	TokenData * token;
};

static ssize_t
find_word_end(ConstCharSlice slc, bool last_chunk)
{
	for (size_t i = 0; i < slc.length; ++i) {
		switch (ch_classify(slc.ptr[i])) {
		case CHCLS_NUM:
		case CHCLS_ALPH:
			break;
		default:
			return i;
		}
	}
	return last_chunk ? (ssize_t) slc.length : -1;
}

static WidthInteger
parse_number(char * s, size_t length)
{
	while (*s == '0') {
		++s;
		--length;
	}
	int base = 10;
	switch (*s) {
	case 'd':
	case 'D':
		base = 10;
		++s;
		--length;
		break;
	case 'o':
	case 'O':
		base = 8;
		++s;
		--length;
		break;
	case 'b':
	case 'B':
		base = 2;
		++s;
		--length;
		break;
	case 'x':
	case 'X':
		base = 16;
		++s;
		--length;
		break;
	}
	char *endptr;
	// TODO can we expect to not have '\0' at s[length]?
	uint64_t n = strtol(s, &endptr, base);
	if (endptr < s + length) {
		fprintf(stderr, "Trailing characters in numeric literal: %.*s\n", (int) (length - (endptr - s)), endptr);
		exit(1);
	}
	return (WidthInteger) {
		.value = n,
		.width = 64,
	};
}

static void
lexer_token_destuctor(TokenData * self)
{
	switch (self->token_type) {
	case TOKENTYPE_Identifier:
		if (self->as_Identifier.name.ptr) {
			free(self->as_Identifier.name.ptr);
		}
		break;
	default:
		break;
	}
}

static TokenData *
allocate_token_data(void)
{
	TokenData * token = calloc(1, sizeof(TokenData));
	if (!token) {
		fprintf(stderr, "Failed to allocate token data\n");
		exit(1);
	}
	token->destructor = &lexer_token_destuctor;
	return token;
}

Lexer *
lexer_new()
{
	Lexer *lexer = malloc(sizeof(Lexer));
	if (!lexer) {
		fprintf(stderr, "Failed to allocate lexer\n");
		exit(1);
	}
	char *buffer = malloc(16);
	if (!buffer) {
		fprintf(stderr, "Failed to allocate lexer buffer\n");
		exit(1);
	}
	*lexer = (Lexer) {
		.untokenized = {
			.ptr = buffer,
			.length = 0,
		},
		.next_char_class = CHCLS_UNKNOWN,
		.next_char = 0,
		.source_end = false,
		.token = NULL,
	};
	return lexer;
}

void
lexer_delete(Lexer * lexer)
{
	if (lexer->untokenized.ptr) {
		free(lexer->untokenized.ptr);
		lexer->untokenized.ptr = NULL;
	}
	if (lexer->token) {
		destruct_token_data(lexer->token);
		free(lexer->token);
		lexer->token = NULL;
	}
	free(lexer);
}

bool
lexer_has_token(Lexer * lexer)
{
	return lexer->token != NULL;
}

TokenData *
lexer_take_token(Lexer * lexer)
{
	TokenData * token = lexer->token;
	lexer->token = NULL;
	if (lexer->untokenized.ptr && lexer->untokenized.length) {
		lexer_feed(lexer, NULL, 0);
	}
	return token;
}

void
lexer_end(Lexer * lexer)
{
	lexer->source_end = true;
	lexer_feed(lexer, NULL, 0);
}

static void lexer_poll_iteration(Lexer * lexer, ConstCharSlice * slc, bool source_end);

static void
lexer_stash(Lexer * lexer, const char * ptr, size_t length, ConstCharSlice slc, bool arguments_used)
{
	if (slc.ptr && slc.length) {
		// Replace lexer->untokenized data with remaining slice
		if (slc.ptr + slc.length != lexer->untokenized.ptr + lexer->untokenized.length) {
			if (!(lexer->untokenized.ptr = realloc(lexer->untokenized.ptr, slc.length))) {
				fprintf(stderr, "Failed to resize internal lexer buffer\n");
				exit(1);
			}
		}
		if (lexer->untokenized.ptr != slc.ptr) {
			memmove(lexer->untokenized.ptr, slc.ptr, slc.length);
		}
		lexer->untokenized.length = slc.length;
	} else {
		// Clear lexer->untokenized data
		lexer->untokenized.length = 0;
	}
	if (!arguments_used && ptr && length) {
		// Append
		size_t total_length = lexer->untokenized.length + length;
		if (!(lexer->untokenized.ptr = realloc(lexer->untokenized.ptr, total_length))) {
			fprintf(stderr, "Failed to resize internal lexer buffer\n");
			exit(1);
		}
		char *dest = lexer->untokenized.ptr + lexer->untokenized.length;
		memcpy(dest, ptr, length);
	}
}

void
lexer_feed(Lexer * lexer, const char * ptr, size_t length)
{
	ConstCharSlice slc = lexer->untokenized.as_const;
	bool arguments_used = false;
	while (!lexer->token) {
		if (!slc.length || !slc.ptr) {
			if (arguments_used) {
				break;
			}
			slc.ptr = ptr;
			slc.length = length;
			arguments_used = true;
			continue;
		}
		lexer_poll_iteration(lexer, &slc, arguments_used && lexer->source_end);
		if (slc.length && slc.ptr && lexer->state != LXST_NORMAL) {
			if (arguments_used) {
				break;
			}
			lexer_stash(lexer, ptr, length, slc, arguments_used);
			arguments_used = true;
			ptr = NULL;
			length = 0;
			slc = lexer->untokenized.as_const;
			lexer_poll_iteration(lexer, &slc, lexer->source_end);
		}
	}
	lexer_stash(lexer, ptr, length, slc, arguments_used);
}

inline static void
lexer_ensure_next_character(Lexer * lexer, ConstCharSlice * slc)
{
	if (lexer->next_char_class != CHCLS_UNKNOWN) {
		return;
	}
	if (!slc->ptr || !slc->length) {
		return;
	}
	char c = *slc->ptr;
	++slc->ptr;
	--slc->length;
	lexer->next_char = c;
	lexer->next_char_class = ch_classify(c);
}

inline static void
lexer_consume_character(Lexer * lexer)
{
	lexer->next_char = 0;
	lexer->next_char_class = CHCLS_UNKNOWN;
}

static void
lexer_poll_iteration(Lexer * lexer, ConstCharSlice * slc, bool source_end)
{
	switch (lexer->state) {
	case LXST_NORMAL:
		while (!lexer->token && lexer->state == LXST_NORMAL) {
			lexer_ensure_next_character(lexer, slc);
			switch (lexer->next_char_class) {
				case CHCLS_UNKNOWN:
					return;
				case CHCLS_INVAL:
					fprintf(stderr, "Unexpected byte: \\x%02x\n", lexer->next_char);
					exit(1);
				case CHCLS_WS:
					lexer_consume_character(lexer);
					continue;
				case CHCLS_ALPH:
					lexer->state = LXST_IDENT;
					return lexer_poll_iteration(lexer, slc, source_end);
				case CHCLS_NUM:
					lexer->state = LXST_NUMBER;
					return lexer_poll_iteration(lexer, slc, source_end);
				case CHCLS_PUNCT:
					switch (lexer->next_char) {
					case '(':
						lexer->token = allocate_token_data();
						lexer->token->token_type = TOKENTYPE_LParen;
						lexer_consume_character(lexer);
						break;
					case ')':
						lexer->token = allocate_token_data();
						lexer->token->token_type = TOKENTYPE_RParen;
						lexer_consume_character(lexer);
						break;
					case '=':
						lexer->token = allocate_token_data();
						lexer->token->token_type = TOKENTYPE_Assign;
						lexer->token->as_Assign.is_reassign = false;
						lexer_consume_character(lexer);
						break;
					case ':':
						lexer->state = LXST_COLON;
						lexer_consume_character(lexer);
						return lexer_poll_iteration(lexer, slc, source_end);
					case ',':
						lexer->token = allocate_token_data();
						lexer->token->token_type = TOKENTYPE_Comma;
						lexer_consume_character(lexer);
						break;
					case ';':
						lexer->token = allocate_token_data();
						lexer->token->token_type = TOKENTYPE_Semicolon;
						lexer_consume_character(lexer);
						break;
					default:
						fprintf(stderr, "Unexpected punctuation: '%c'\n", lexer->next_char);
						exit(1);
					}
					break;
			}
			break;
		}
		break;
	case LXST_COLON:
		lexer_ensure_next_character(lexer, slc);
		if (lexer->next_char_class == CHCLS_UNKNOWN) {
			return;
		}
		switch (lexer->next_char) {
		case '=':
			lexer->token = allocate_token_data();
			lexer->token->token_type = TOKENTYPE_Assign;
			lexer->token->as_Assign.is_reassign = true;
			lexer_consume_character(lexer);
			lexer->state = LXST_NORMAL;
			break;
		default:
			fprintf(stderr, "Expected '=', got: '\\x%02x'\n", lexer->next_char);
		}
		break;
	case LXST_IDENT:
	case LXST_NUMBER:;
		ssize_t token_end = find_word_end(*slc, source_end);
		if (token_end < 0) {
			return;
		}
		char * full_token = malloc(1 + token_end + 1);  // ALLOCATE full_token
		if (!full_token) {
			fprintf(stderr, "Failed to allocate token copy\n");
			exit(1);
		}
		full_token[0] = lexer->next_char;
		memcpy(full_token + 1, slc->ptr, token_end);
		full_token[1 + token_end] = '\0';
		if (lexer->state == LXST_NUMBER) {
			WidthInteger literal_value = parse_number(full_token, 1 + token_end);
			lexer->token = allocate_token_data();
			lexer->token->token_type = TOKENTYPE_Number;
			lexer->token->as_Number.value = literal_value;
			free(full_token);  // FREE full_token
		} else {
			if (!strcmp(full_token, "while")) {
				lexer->token = allocate_token_data();
				lexer->token->token_type = TOKENTYPE_Keyword;
				lexer->token->as_Keyword.keyword_type = KWTT_WHILE;
				free(full_token);  // FREE full_token
			} else if (!strcmp(full_token, "if")) {
				lexer->token = allocate_token_data();
				lexer->token->token_type = TOKENTYPE_Keyword;
				lexer->token->as_Keyword.keyword_type = KWTT_IF;
				free(full_token);  // FREE full_token
			} else if (!strcmp(full_token, "function")) {
				lexer->token = allocate_token_data();
				lexer->token->token_type = TOKENTYPE_Keyword;
				lexer->token->as_Keyword.keyword_type = KWTT_FUNCTION;
				free(full_token);  // FREE full_token
			} else if (!strcmp(full_token, "call")) {
				lexer->token = allocate_token_data();
				lexer->token->token_type = TOKENTYPE_Keyword;
				lexer->token->as_Keyword.keyword_type = KWTT_CALL;
				free(full_token);  // FREE full_token
			} else {
				lexer->token = allocate_token_data();
				lexer->token->token_type = TOKENTYPE_Identifier;
				lexer->token->as_Identifier.name = (CharSlice) {
					.ptr = full_token,  // MOVE full_token
					.length = 1 + token_end,
				};
			}
		}
		slc->length -= token_end;
		slc->ptr += token_end;
		lexer_consume_character(lexer);;
		lexer->state = LXST_NORMAL;
		break;
	}
}
