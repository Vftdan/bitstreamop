#ifndef PARSER_H_
#define PARSER_H_

#include <stdbool.h>
#include "common.h"
#include "expression.h"

struct parser;

typedef struct parser Parser;

Parser * parser_new(void);

bool parser_feed(Parser * parser, char * ptr, size_t length);

const ExprNode * parser_end(Parser * parser);

void parser_delete(Parser * parser);

#endif /* end of include guard: PARSER_H_ */
