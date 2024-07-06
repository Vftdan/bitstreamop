#include "parser.h"
#include "functions.h"
#include "common.h"

#define UNPACK(...) __VA_ARGS__
#define PUSH_DOWN_MEMBERS(struct_name, current_name, members) union { struct struct_name { UNPACK members } current_name; struct { UNPACK members }; };

typedef struct {
	char *ptr;
	size_t length;
} CharSlice;

typedef enum {
	CHCLS_UNKNOWN,
	CHCLS_WS,
	CHCLS_ALPH,
	CHCLS_NUM,
	CHCLS_PUNCT,
	CHCLS_INVAL,
} CharacterClass;

typedef enum {
	PSMD_NORMAL,
	PSMD_WAIT_POP,
	PSMD_GROUP,
} ParserMode;

struct parser {
	CharSlice untokenized;
	CharacterClass next_char_class;
	char next_char;
	bool code_end;  // No more code chunks expected
	bool after_ident;  // We have parsed an identifier, but we don't yet know whether it is variable reference, assignment, or function call
	PUSH_DOWN_MEMBERS(parser_stack_frame, stack_current, (
		struct parser_stack_frame *stack_parent;
		ParserMode mode;
		ExprNode * parsed_node;
		CharSlice parsed_identifier;
		char stack_terminator;
		bool pop_on_comma;
		bool pop_before_semicolon;
		bool after_comma;
		bool expression_end;  // Expecting separator or closing parenthesis
		int64_t call_iteration;
	))
	ExprNode * popped_parsed_node;
};

static void
parsed_expr_node_destructor(ExprNode * node)
{
	switch (node->node_type) {
	case EXPRNODE_FunctionApplication:
		for (uint64_t i = 0; i < node->as_FunctionApplication.arg_count; ++i) {
			destruct_expression(&node->as_FunctionApplication.args[i]);
		}
		free(node->as_FunctionApplication.args);
		break;
	case EXPRNODE_Literal:
		break;
	case EXPRNODE_Assign:
		destruct_expression(node->as_Assign.rhs);
		free(node->as_Assign.rhs);
		free(node->as_Assign.name);
		break;
	case EXPRNODE_Reassign:
		destruct_expression(node->as_Reassign.rhs);
		free(node->as_Reassign.rhs);
		free(node->as_Assign.name);
		break;
	case EXPRNODE_Variable:
		free(node->as_Variable.name);
		break;
	case EXPRNODE_StatementList:
		for (uint64_t i = 0; i < node->as_StatementList.length; ++i) {
			destruct_expression(&node->as_StatementList.args[i]);
		}
		free(node->as_StatementList.args);
		break;
	case EXPRNODE_LoopWhile:
		destruct_expression(node->as_LoopWhile.body);
		destruct_expression(node->as_LoopWhile.condition);
		free(node->as_LoopWhile.body);
		free(node->as_LoopWhile.condition);
		break;
	case EXPRNODE_CondIf:
		destruct_expression(node->as_CondIf.body);
		destruct_expression(node->as_CondIf.condition);
		free(node->as_CondIf.body);
		free(node->as_CondIf.condition);
		break;
	}
}

static char
chslc_shift(CharSlice * slc)
{
	if (!slc->ptr || !slc->length)
		return 0;
	char c = *slc->ptr;
	++(slc->ptr);
	--(slc->length);
	return c;
}

static CharacterClass
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

static ssize_t
find_word_end(CharSlice slc, bool last_chunk)
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

Parser *
parser_new()
{
	Parser *parser = malloc(sizeof(Parser));
	if (!parser) {
		fprintf(stderr, "Failed to allocate parser\n");
		exit(1);
	}
	char *buffer = malloc(16);
	if (!buffer) {
		fprintf(stderr, "Failed to allocate parser buffer\n");
		exit(1);
	}
	*parser = (Parser) {
		.untokenized = {
			.ptr = buffer,
			.length = 0,
		},
		.next_char_class = CHCLS_UNKNOWN,
		.next_char = 0,
	};
	return parser;
}

void
parser_delete(Parser * parser)
{
	if (parser->untokenized.ptr) {
		free(parser->untokenized.ptr);
		parser->untokenized.ptr = NULL;
	}
	if (parser->parsed_node) {
		destruct_expression(parser->parsed_node);
		free(parser->parsed_node);
		parser->parsed_node = NULL;
	}
	free(parser);
}

static char *
extend_untokenized_buffer(Parser * parser, size_t saved_last, size_t add_length)
{
	size_t new_length = saved_last + add_length;
	if (!(parser->untokenized.ptr = realloc(parser->untokenized.ptr, new_length))) {
		fprintf(stderr, "Failed to resize internal parser buffer\n");
		exit(1);
	}
	if (saved_last) {
		memmove(parser->untokenized.ptr, parser->untokenized.ptr + parser->untokenized.length - saved_last, saved_last);
	}
	parser->untokenized.length = new_length;
	return parser->untokenized.ptr + saved_last;
}

static void
parser_push_state(Parser * parser)
{
	struct parser_stack_frame *prev = malloc(sizeof(struct parser_stack_frame));
	if (!prev) {
		fprintf(stderr, "Failed to allocate parser stack frame\n");
		exit(1);
	}
	*prev = parser->stack_current;
	parser->stack_current = (struct parser_stack_frame) {};
	parser->stack_parent = prev;
}

static void
parser_pop_state(Parser * parser)
{
	if (parser->parsed_node) {
		if (parser->popped_parsed_node) {
			fprintf(stderr, "Warning: overwriting parser->popped_parsed_node\n");
		}
		parser->popped_parsed_node = parser->parsed_node;
	}
	struct parser_stack_frame *prev = parser->stack_parent;
	if (!prev) {
		fprintf(stderr, "Trying to pop empty parser stack\n");
		exit(1);
	}
	parser->stack_current = *prev;
	free(prev);
}

static void
parser_before_expression_end(Parser * parser)
{
	if (!parser->after_ident) {
		return;
	}
	ExprNode * variable_node = malloc(sizeof(ExprNode));
	if (!variable_node) {
		fprintf(stderr, "Failed to allocate expression node\n");
		exit(1);
	}
	variable_node->destructor = parsed_expr_node_destructor;
	variable_node->node_type = EXPRNODE_Variable;
	variable_node->as_Variable.name = parser->parsed_identifier.ptr;
	parser->parsed_identifier = (CharSlice) { NULL, 0 };
	parser->parsed_node = variable_node;
	parser->after_ident = false;
	parser->expression_end = true;
}

bool
parser_feed(Parser * parser, char * ptr, size_t length)
{
	CharSlice slc = parser->untokenized;
	bool arguments_used = false;
	while (!arguments_used) {
		if (!slc.ptr || !slc.length) {
			arguments_used = true;
			slc.ptr = ptr;
			slc.length = length;
		}
		while (parser->mode != PSMD_NORMAL || (slc.ptr && slc.length)) {
			if (parser->next_char_class == CHCLS_UNKNOWN && slc.ptr && slc.length) {
				parser->next_char = chslc_shift(&slc);
				parser->next_char_class = ch_classify(parser->next_char);
			}
			switch (parser->mode) {
			case PSMD_NORMAL:
				switch (parser->next_char_class) {
				case CHCLS_UNKNOWN:
				case CHCLS_INVAL:
					fprintf(stderr, "Unexpectes byte: \\x%02x", parser->next_char);
					exit(1);
				case CHCLS_WS:
					break;
				case CHCLS_ALPH:
				case CHCLS_NUM:
					if (parser->expression_end) {
						fprintf(stderr, "Expecting a separator, got '%c'\n", parser->next_char);
						exit(1);
					}
	retry_token_end:;
					ssize_t token_end = find_word_end(slc, arguments_used && parser->code_end);
					if (token_end < 0) {
						if (!arguments_used) {
							// Currently, slc points to the internal buffer
							// Should we still check that arguments are non-null?
							memcpy(extend_untokenized_buffer(parser, slc.length, length), ptr, length);
							arguments_used = true;
							slc = parser->untokenized;
							goto retry_token_end;
						}
						goto save_untokenized;
					}
					char * full_token = malloc(1 + token_end + 1);  // ALLOCATE full_token
					if (!full_token) {
						fprintf(stderr, "Failed to allocate token copy\n");
						exit(1);
					}
					full_token[0] = parser->next_char;
					memcpy(full_token + 1, slc.ptr, token_end);
					full_token[1 + token_end] = '\0';
					if (parser->next_char_class == CHCLS_NUM) {
						WidthInteger literal_value = parse_number(full_token, 1 + token_end);
						ExprNode * literal_node = malloc(sizeof(ExprNode));
						if (!literal_node) {
							fprintf(stderr, "Failed to allocate expression node\n");
							exit(1);
						}
						literal_node->destructor = parsed_expr_node_destructor;
						literal_node->node_type = EXPRNODE_Literal;
						literal_node->as_Literal.value = literal_value;
						parser->parsed_node = literal_node;
						parser->expression_end = true;
						free(full_token);  // FREE full_token
					} else {
						parser->parsed_identifier.ptr = full_token;  // MOVE full_token
						parser->parsed_identifier.length = 1 + token_end;
						parser->after_ident = true;
					}
					slc.length -= token_end;
					slc.ptr += token_end;
					break;
				case CHCLS_PUNCT:
					if (parser->next_char == parser->stack_terminator) {
						parser_before_expression_end(parser);
						parser_pop_state(parser);
						// don't consume character
						continue;
					}
					switch (parser->next_char) {
					case ')':
						// We don't know to which stack frame it belongs
						parser_before_expression_end(parser);
						parser_pop_state(parser);
						// don't consume character
						continue;
					case ',':
						parser_before_expression_end(parser);
						if (parser->pop_on_comma) {
							parser_pop_state(parser);
							parser->after_comma = true;
							// Consume
							break;
						} else {
							if (parser->stack_terminator != ';') {
								// TODO can we create a more elegant way of deciding whether we can pop?
								fprintf(stderr, "Unexpected comma, expecting '%c'\n", parser->stack_terminator);
								exit(1);
							}
							parser_pop_state(parser);
							// don't consume character
							continue;
						}
					case '=':
						if (!parser->after_ident) {
							fprintf(stderr, "Assignment without left-hand-size\n");
							exit(1);
						}
						ExprNode * assignment_node = malloc(sizeof(ExprNode));
						if (!assignment_node) {
							fprintf(stderr, "Failed to allocate expression node\n");
							exit(1);
						}
						assignment_node->destructor = parsed_expr_node_destructor;
						assignment_node->node_type = EXPRNODE_Assign;
						assignment_node->as_Assign.name = parser->parsed_identifier.ptr;
						parser->parsed_node = assignment_node;
						parser->parsed_identifier = (CharSlice) { NULL, 0 };
						parser->mode = PSMD_WAIT_POP;
						parser_push_state(parser);
						parser->pop_before_semicolon = true;
						break;
					case ':':  // TODO use ":=", not ":"
						if (!parser->after_ident) {
							fprintf(stderr, "Assignment without left-hand-size\n");
							exit(1);
						}
						ExprNode * reassignment_node = malloc(sizeof(ExprNode));
						if (!reassignment_node) {
							fprintf(stderr, "Failed to allocate expression node\n");
							exit(1);
						}
						reassignment_node->destructor = parsed_expr_node_destructor;
						reassignment_node->node_type = EXPRNODE_Reassign;
						reassignment_node->as_Reassign.name = parser->parsed_identifier.ptr;
						parser->parsed_node = reassignment_node;
						parser->parsed_identifier = (CharSlice) { NULL, 0 };
						parser->mode = PSMD_WAIT_POP;
						parser_push_state(parser);
						parser->pop_before_semicolon = true;
						break;
					case ';':
						parser_before_expression_end(parser);
						if (parser->pop_before_semicolon) {
							parser_pop_state(parser);
							// Do not consume
							continue;
						}
						ExprNode * statement_list_node = malloc(sizeof(ExprNode));
						if (!statement_list_node) {
							fprintf(stderr, "Failed to allocate expression node\n");
							exit(1);
						}
						statement_list_node->destructor = parsed_expr_node_destructor;
						statement_list_node->node_type = EXPRNODE_StatementList;
						statement_list_node->as_StatementList.length = 0;
						statement_list_node->as_StatementList.args = NULL;
						if (parser->parsed_node) {
							statement_list_node->as_StatementList.length = 1;
							statement_list_node->as_StatementList.args = parser->parsed_node;
						}
						parser->parsed_node = statement_list_node;
						parser->mode = PSMD_WAIT_POP;
						parser_push_state(parser);
						parser->stack_terminator = ';';
						break;
					case '(':
						if (parser->after_ident) {
							parser->after_ident = false;
							if (!strcmp(parser->parsed_identifier.ptr, "while")) {
								ExprNode * loop_while_node = malloc(sizeof(ExprNode));
								if (!loop_while_node) {
									fprintf(stderr, "Failed to allocate expression node\n");
									exit(1);
								}
								loop_while_node->destructor = parsed_expr_node_destructor;
								loop_while_node->node_type = EXPRNODE_LoopWhile;
								parser->parsed_node = loop_while_node;
								free(parser->parsed_identifier.ptr);
								parser->parsed_identifier = (CharSlice) { NULL, 0 };
								parser->mode = PSMD_WAIT_POP;
								parser_push_state(parser);
								parser->stack_terminator = ')';
							} else if (!strcmp(parser->parsed_identifier.ptr, "if")) {
								ExprNode * cond_if_node = malloc(sizeof(ExprNode));
								if (!cond_if_node) {
									fprintf(stderr, "Failed to allocate expression node\n");
									exit(1);
								}
								cond_if_node->destructor = parsed_expr_node_destructor;
								cond_if_node->node_type = EXPRNODE_CondIf;
								parser->parsed_node = cond_if_node;
								free(parser->parsed_identifier.ptr);
								parser->parsed_identifier = (CharSlice) { NULL, 0 };
								parser->mode = PSMD_WAIT_POP;
								parser_push_state(parser);
								parser->stack_terminator = ')';
							} else {
								// Function
								ExprNode * function_application_node = malloc(sizeof(ExprNode));
								if (!function_application_node) {
									fprintf(stderr, "Failed to allocate expression node\n");
									exit(1);
								}
								function_application_node->destructor = parsed_expr_node_destructor;
								function_application_node->node_type = EXPRNODE_FunctionApplication;
								if (!(function_application_node->as_FunctionApplication.func = find_function(parser->parsed_identifier.ptr))) {
									fprintf(stderr, "Unknown function %.*s\n", (int)parser->parsed_identifier.length, parser->parsed_identifier.ptr);
									exit(1);
								}
								function_application_node->as_FunctionApplication.arg_count = function_application_node->as_FunctionApplication.func->args_def.length;
								if (!(function_application_node->as_FunctionApplication.args = calloc(function_application_node->as_FunctionApplication.arg_count, sizeof(ExprNode)))) {
									fprintf(stderr, "Failed to allocate function arguments expressions\n");
									exit(1);
								}
								parser->parsed_node = function_application_node;
								free(parser->parsed_identifier.ptr);
								parser->parsed_identifier = (CharSlice) { NULL, 0 };
								parser->mode = PSMD_WAIT_POP;
								parser_push_state(parser);
								parser->pop_on_comma = true;
								parser->stack_terminator = ')';
							}
						} else {
							parser->mode = PSMD_GROUP;
							parser_push_state(parser);
							parser->stack_terminator = ')';
						}
						break;
					}
				}
				parser->next_char = 0;
				parser->next_char_class = CHCLS_UNKNOWN;
				break;
			case PSMD_WAIT_POP:
				switch (parser->parsed_node->node_type) {
				case EXPRNODE_FunctionApplication:
					if (parser->popped_parsed_node) {
						parser->parsed_node->as_FunctionApplication.args[parser->call_iteration] = *parser->popped_parsed_node;  // MOVE contents
						free(parser->popped_parsed_node);  // FREE extra pointer
						parser->popped_parsed_node = NULL;
					} else {
						--parser->call_iteration;
					}
					if (parser->after_comma && parser->call_iteration >= (int64_t) parser->parsed_node->as_FunctionApplication.arg_count) {
						fprintf(stderr, "Too many arguments for function %s\n", parser->parsed_node->as_FunctionApplication.func->name);
						exit(1);
					}
					if (parser->after_comma) {
						parser->after_comma = false;
						++parser->call_iteration;
						parser_push_state(parser);
						parser->pop_on_comma = true;
						parser->stack_terminator = ')';
					} else {
						// Consume character
						parser->next_char = 0;
						parser->next_char_class = CHCLS_UNKNOWN;
						if (parser->call_iteration + 1 != (int64_t) parser->parsed_node->as_FunctionApplication.arg_count) {
							fprintf(stderr, "Not enough arguments for function %s, expected %ld, given %ld\n", parser->parsed_node->as_FunctionApplication.func->name, parser->parsed_node->as_FunctionApplication.arg_count, parser->call_iteration);
							exit(1);
						}
						parser->call_iteration = 0;
						parser->expression_end = true;
						parser->mode = PSMD_NORMAL;
					}
					parser->after_comma = false;
					break;
				case EXPRNODE_Assign:
					parser->parsed_node->as_Assign.rhs = parser->popped_parsed_node;  // MOVE
					parser->popped_parsed_node = NULL;
					parser->expression_end = true;
					parser->mode = PSMD_NORMAL;
					break;
				case EXPRNODE_Reassign:
					parser->parsed_node->as_Reassign.rhs = parser->popped_parsed_node;  // MOVE
					parser->popped_parsed_node = NULL;
					parser->expression_end = true;
					parser->mode = PSMD_NORMAL;
					break;
				case EXPRNODE_StatementList:
					if (parser->popped_parsed_node) {
						++(parser->parsed_node->as_StatementList.length);
						if (!(parser->parsed_node->as_StatementList.args = reallocarray(parser->parsed_node->as_StatementList.args, parser->parsed_node->as_StatementList.length, sizeof(ExprNode)))) {
							fprintf(stderr, "Failed to reallocate statement list");
							exit(1);
						}
						parser->parsed_node->as_StatementList.args[parser->parsed_node->as_StatementList.length - 1] = *parser->popped_parsed_node;  // MOVE contents
						free(parser->popped_parsed_node);  // FREE extra pointer
						parser->popped_parsed_node = NULL;
					}
					if (parser->next_char == ';') {
						// Consume character
						parser->next_char = 0;
						parser->next_char_class = CHCLS_UNKNOWN;
						// Switch again
						parser_push_state(parser);
						parser->stack_terminator = ';';
					} else {
						parser->mode = PSMD_NORMAL;
					}
					break;
				case EXPRNODE_LoopWhile:
					if (!parser->call_iteration) {
						// Consume character
						parser->next_char = 0;
						parser->next_char_class = CHCLS_UNKNOWN;
						parser->parsed_node->as_LoopWhile.condition = parser->popped_parsed_node;  // MOVE
						parser->popped_parsed_node = NULL;
						parser->call_iteration = 1;
						parser_push_state(parser);
						parser->mode = PSMD_NORMAL;
					} else {
						parser->parsed_node->as_LoopWhile.body = parser->popped_parsed_node;  // MOVE
						parser->popped_parsed_node = NULL;
						parser->expression_end = true;
						parser->mode = PSMD_NORMAL;
					}
					break;
				case EXPRNODE_CondIf:
					if (!parser->call_iteration) {
						// Consume character
						parser->next_char = 0;
						parser->next_char_class = CHCLS_UNKNOWN;
						parser->parsed_node->as_CondIf.condition = parser->popped_parsed_node;  // MOVE
						parser->popped_parsed_node = NULL;
						parser->call_iteration = 1;
						parser_push_state(parser);
						parser->mode = PSMD_NORMAL;
					} else {
						parser->parsed_node->as_CondIf.body = parser->popped_parsed_node;  // MOVE
						parser->popped_parsed_node = NULL;
						parser->expression_end = true;
						parser->mode = PSMD_NORMAL;
					}
					break;
				default:
					fprintf(stderr, "Unexpected PSMD_WAIT_POP\n");
					exit(1);
				}
				break;
			case PSMD_GROUP:
				if (parser->next_char != ')') {
					fprintf(stderr, "Expected ')', got '\\x%02x'\n", parser->next_char);
					exit(1);
				}
				parser->next_char = 0;
				parser->next_char_class = CHCLS_UNKNOWN;
				parser->mode = PSMD_NORMAL;
				parser->parsed_node = parser->popped_parsed_node;
				parser->popped_parsed_node = NULL;
				break;
			}
		}
	}
save_untokenized:
	if (slc.ptr && slc.length) {
		if (slc.ptr + slc.length == parser->untokenized.ptr + parser->untokenized.length) {
			parser->untokenized = slc;
		} else {
			if (!(parser->untokenized.ptr = realloc(parser->untokenized.ptr, slc.length))) {
				fprintf(stderr, "Failled to resize internal parser buffer\n");
				exit(1);
			}
			memcpy(parser->untokenized.ptr, slc.ptr, slc.length);
			parser->untokenized.length = slc.length;
		}
	}
	return true;
}

const ExprNode *
parser_end(Parser * parser)
{
	parser->code_end = true;
	parser_feed(parser, NULL, 0);
	while (parser->stack_parent) {
		parser_before_expression_end(parser);
		parser_pop_state(parser);
		parser_feed(parser, NULL, 0);
	}
	return parser->parsed_node;
}
