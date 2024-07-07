#ifndef TOKEN_TYPES_H_
#define TOKEN_TYPES_H_

#include "common.h"
#include "interp_types.h"
#include "tree_printer.h"

#define TOKEN_TYPES_H__UNPACK(...) __VA_ARGS__

enum keyword_token_type {
	KWTT_WHILE,
	KWTT_IF,
	KWTT_FUNCTION,
	KWTT_CALL,
};

extern char *keyword_type_names[4];

struct token_data;

#define TOKEN_PAYLOAD_FIELD(fieldtype, fieldname) fieldtype fieldname;
#define BITSTREAMOP_TOKEN(name, elements, printimpl) typedef struct { TOKEN_TYPES_H__UNPACK elements } name##TokenData;
#include "token_types.cc"
#undef BITSTREAMOP_TOKEN
#undef TOKEN_PAYLOAD_FIELD

typedef struct token_data {
	enum token_type {
#define BITSTREAMOP_TOKEN(name, elements, printimpl) TOKENTYPE_##name,
#include "token_types.cc"
#undef BITSTREAMOP_TOKEN
	} token_type;
	void (*destructor)(struct token_data * self);
	union {
#define BITSTREAMOP_TOKEN(name, elements, printimpl) name##TokenData as_##name;
#include "token_types.cc"
#undef BITSTREAMOP_TOKEN
	};
} TokenData;

__attribute__((unused)) inline static void
destruct_token_data(TokenData * self)
{
	if (self->destructor)
		self->destructor(self);
}

void print_token_data(TreePrinter * printer, const TokenData * token);

extern char *token_type_names[];

#endif /* end of include guard: TOKEN_TYPES_H_ */
