#include "interp_types.h"
#include "lexer.h"
#include "parser.h"
#include "functions.h"
#include "common.h"

#define UNPACK(...) __VA_ARGS__
#define PUSH_DOWN_MEMBERS(struct_name, current_name, members) union { struct struct_name { UNPACK members } current_name; struct { UNPACK members }; };

typedef enum {
	PSMD_NORMAL,
	PSMD_WAIT_POP,
	PSMD_GROUP,
	PSMD_GET_IDENTIFIER,
	PSMD_GET_ARGS,
} ParserMode;

struct parser {
	Lexer * lexer;
	TokenData * token;
	bool code_end;  // No more code chunks expected
	PUSH_DOWN_MEMBERS(parser_stack_frame, stack_current, (
		struct parser_stack_frame *stack_parent;
		ParserMode mode;
		ExprNode * parsed_node;
		bool pop_on_comma;
		bool pop_before_semicolon;
		bool after_comma;
		bool expression_end;  // Expecting separator or closing parenthesis
		int64_t call_iteration;
		TokenData * previous_token;
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
	case EXPRNODE_UserFunctionDef:
		if (node->as_UserFunctionDef.args.entries) {
			for (uint64_t i = 0; i < node->as_UserFunctionDef.args.length; ++i) {
				if (node->as_UserFunctionDef.args.entries[i].name) {
					free(node->as_UserFunctionDef.args.entries[i].name);
				}
			}
			free(node->as_UserFunctionDef.args.entries);
			node->as_UserFunctionDef.args.entries = NULL;
		}
		destruct_expression(node->as_UserFunctionDef.body);
		free(node->as_UserFunctionDef.body);
		free(node->as_UserFunctionDef.name);
		break;
	case EXPRNODE_UserFunctionCall:
		for (uint64_t i = 0; i < node->as_UserFunctionCall.arg_count; ++i) {
			destruct_expression(&node->as_UserFunctionCall.args[i]);
		}
		free(node->as_UserFunctionCall.args);
		free(node->as_UserFunctionCall.name);
		break;
	}
}

Parser *
parser_new()
{
	Parser *parser = malloc(sizeof(Parser));
	if (!parser) {
		fprintf(stderr, "Failed to allocate parser\n");
		exit(1);
	}
	Lexer *lexer = lexer_new();
	*parser = (Parser) {
		.lexer = lexer,
		.code_end = false,
	};
	return parser;
}

void
parser_delete(Parser * parser)
{
	if (parser->lexer) {
		lexer_delete(parser->lexer);
		parser->lexer = NULL;
	}
	if (parser->parsed_node) {
		destruct_expression(parser->parsed_node);
		free(parser->parsed_node);
		parser->parsed_node = NULL;
	}
	free(parser);
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
parser_consume_token_at(TokenData **tokenptr)
{
	if (!*tokenptr) {
		return;
	}
	destruct_token_data(*tokenptr);
	free(*tokenptr);
	*tokenptr = NULL;
}

static void
parser_consume_token(Parser * parser)
{
	parser_consume_token_at(&parser->token);
}

static ExprNode *
allocate_expr_node()
{
	ExprNode * node = malloc(sizeof(ExprNode));
	if (!node) {
		fprintf(stderr, "Failed to allocate expression node\n");
		exit(1);
	}
	node->destructor = parsed_expr_node_destructor;
	return node;
}

static ExprNode *
make_noop_expr()
{
	ExprNode * statement_list_node = allocate_expr_node();
	statement_list_node->node_type = EXPRNODE_StatementList;
	statement_list_node->as_StatementList.length = 0;
	statement_list_node->as_StatementList.args = NULL;
	return statement_list_node;
}

static void
parser_before_expression_end(Parser * parser)
{
	if (!parser->previous_token) {
		return;
	}
	if (parser->previous_token->token_type != TOKENTYPE_Identifier) {
		fprintf(stderr, "Expression end after token of type #%d", parser->previous_token->token_type);
	}
	ExprNode * node = allocate_expr_node();
	node->node_type = EXPRNODE_Variable;
	char *name = strndup(parser->previous_token->as_Identifier.name.ptr, parser->previous_token->as_Identifier.name.length);
	node->as_Variable.name = name;
	parser_consume_token_at(&parser->previous_token);
	parser->parsed_node = node;
	parser->previous_token = NULL;
	parser->expression_end = true;
}

static bool
parser_ensure_token(Parser * parser)
{
	if (parser->token) {
		return true;
	}
	if (!lexer_has_token(parser->lexer)) {
		return false;
	}
	parser->token = lexer_take_token(parser->lexer);
	return true;
}

void
parser_feed(Parser * parser, char * ptr, size_t length)
{
	lexer_feed(parser->lexer, ptr, length);
	while (parser_ensure_token(parser) || parser->mode != PSMD_NORMAL) {
		switch (parser->mode) {
		case PSMD_NORMAL:
			switch (parser->token->token_type) {
			case TOKENTYPE_LParen:
				if (parser->expression_end || parser->parsed_node) {
					fprintf(stderr, "Expected a separator, got '('\n");
					exit(1);
				}
				if (parser->previous_token) {
					switch (parser->previous_token->token_type) {
					case TOKENTYPE_Keyword: {
							ExprNode * node = allocate_expr_node();
							switch (parser->previous_token->as_Keyword.keyword_type) {
							case KWTT_WHILE:
								node->node_type = EXPRNODE_LoopWhile;
								break;
							case KWTT_IF:
								node->node_type = EXPRNODE_CondIf;
								break;
							default:
								fprintf(stderr, "Unexpected keyword of type #%d before a '('\n", parser->previous_token->as_Keyword.keyword_type);
								exit(1);
							}
							parser->parsed_node = node;
							parser_consume_token_at(&parser->previous_token);
							parser->mode = PSMD_WAIT_POP;
							parser->call_iteration = 0;
							parser_push_state(parser);
							parser->pop_before_semicolon = true;
						}
						break;
					case TOKENTYPE_Identifier: {
							ExprNode * node = allocate_expr_node();
							node->node_type = EXPRNODE_FunctionApplication;
							if (!(node->as_FunctionApplication.func = find_function(parser->previous_token->as_Identifier.name.ptr))) {
								fprintf(stderr, "Unknown function %.*s\n", (int)parser->previous_token->as_Identifier.name.length, parser->previous_token->as_Identifier.name.ptr);
								exit(1);
							}
							node->as_FunctionApplication.arg_count = node->as_FunctionApplication.func->args_def.length;
							if (!(node->as_FunctionApplication.args = calloc(node->as_FunctionApplication.arg_count, sizeof(ExprNode)))) {
								fprintf(stderr, "Failed to allocate function arguments expressions\n");
								exit(1);
							}
							parser->parsed_node = node;
							parser_consume_token_at(&parser->previous_token);
							parser->mode = PSMD_WAIT_POP;
							parser_push_state(parser);
							parser->pop_on_comma = true;
						}
						break;
					default:
						fprintf(stderr, "Unexpected previous_token %d\n", parser->previous_token->token_type);
						exit(1);
					}
				} else {
					parser->mode = PSMD_GROUP;
					parser_push_state(parser);
				}
				parser_consume_token(parser);
				break;
			case TOKENTYPE_RParen: {
					parser_before_expression_end(parser);
					parser_pop_state(parser);
				}
				// don't consume character
				break;
			case TOKENTYPE_Assign: {
					if (parser->expression_end || parser->parsed_node) {
						fprintf(stderr, "Expected a separator, got assignment\n");
						exit(1);
					}
					if (!parser->previous_token) {
						fprintf(stderr, "Assignment without left-hand-size\n");
						exit(1);
					}
					ExprNode * node = allocate_expr_node();
					char *name = strndup(parser->previous_token->as_Identifier.name.ptr, parser->previous_token->as_Identifier.name.length);
					if (parser->token->as_Assign.is_reassign) {
						node->node_type = EXPRNODE_Reassign;
						node->as_Reassign.name = name;
					} else {
						node->node_type = EXPRNODE_Assign;
						node->as_Reassign.name = name;
					}
					parser_consume_token_at(&parser->previous_token);
					parser->parsed_node = node;
					parser->mode = PSMD_WAIT_POP;
					parser_push_state(parser);
					parser->pop_before_semicolon = true;
				}
				parser_consume_token(parser);
				break;
			case TOKENTYPE_Comma: {
					parser_before_expression_end(parser);
					if (parser->pop_on_comma) {
						parser_pop_state(parser);
						parser->after_comma = true;
						parser_consume_token(parser);
					} else {
						parser_pop_state(parser);
					}
				}
				break;
			case TOKENTYPE_Identifier: {
					if (parser->expression_end || parser->previous_token || parser->parsed_node) {
						fprintf(stderr, "Expecting a separator, got \"%.*s\"\n", (int) parser->token->as_Identifier.name.length, parser->token->as_Identifier.name.ptr);
						exit(1);
					}
					parser->previous_token = parser->token;
					parser->token = NULL;
				}
				break;
			case TOKENTYPE_Keyword: {
					if (parser->expression_end || parser->previous_token || parser->parsed_node) {
						fprintf(stderr, "Expecting a separator, got keyword \"%s\"\n", keyword_type_names[parser->token->as_Keyword.keyword_type]);
						exit(1);
					}
					switch (parser->token->as_Keyword.keyword_type) {
					case KWTT_WHILE:
					case KWTT_IF:
						parser->previous_token = parser->token;
						parser->token = NULL;
						break;
					case KWTT_FUNCTION:
					case KWTT_CALL:;
						ExprNode * node = allocate_expr_node();
						switch (parser->token->as_Keyword.keyword_type) {
						case KWTT_FUNCTION:
							node->node_type = EXPRNODE_UserFunctionDef;
							node->as_UserFunctionDef.name = NULL;
							node->as_UserFunctionDef.args.entries = NULL;
							node->as_UserFunctionDef.args.length = 0;
							node->as_UserFunctionDef.body = NULL;
							break;
						case KWTT_CALL:
							node->node_type = EXPRNODE_UserFunctionCall;
							node->as_UserFunctionCall.name = NULL;
							node->as_UserFunctionCall.arg_count = 0;
							node->as_UserFunctionCall.args = NULL;
							break;
						default:
							break;
						}
						parser->parsed_node = node;
						parser_consume_token(parser);
						parser->mode = PSMD_GET_IDENTIFIER;
						break;
					}
				}
				break;
			case TOKENTYPE_Number: {
					if (parser->expression_end || parser->parsed_node) {
						fprintf(stderr, "Expecting a separator, got number %lu\n", parser->token->as_Number.value.value);
						exit(1);
					}
					ExprNode * node = allocate_expr_node();
					node->node_type = EXPRNODE_Literal;
					node->as_Literal.value = parser->token->as_Number.value;
					parser->parsed_node = node;
				}
				parser_consume_token(parser);
				break;
			case TOKENTYPE_Semicolon: {
					parser_before_expression_end(parser);
					if (parser->pop_before_semicolon) {
						parser_pop_state(parser);
						break;
					}
					parser->expression_end = false;
					ExprNode * node = allocate_expr_node();
					node->node_type = EXPRNODE_StatementList;
					node->as_StatementList.length = 0;
					node->as_StatementList.args = NULL;
					if (parser->parsed_node) {
						node->as_StatementList.length = 1;
						node->as_StatementList.args = parser->parsed_node;
					}
					parser->parsed_node = node;
					parser->mode = PSMD_WAIT_POP;
					parser_push_state(parser);
					parser->pop_before_semicolon = true;
					parser_consume_token(parser);
				}
				break;
			}
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
				++parser->call_iteration;
				if (parser->after_comma) {
					parser->after_comma = false;
					parser_push_state(parser);
					parser->pop_on_comma = true;
				} else {
					parser_consume_token(parser);
					if (parser->call_iteration != (int64_t) parser->parsed_node->as_FunctionApplication.arg_count) {
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
				if (!parser->token && !parser->code_end) {
					// We don't yet know whether it's EOF or we will have semicolon in the next code chunk
					return;
				}
				if (parser->popped_parsed_node) {
					++parser->parsed_node->as_StatementList.length;
					if (!(parser->parsed_node->as_StatementList.args = reallocarray(parser->parsed_node->as_StatementList.args, parser->parsed_node->as_StatementList.length, sizeof(ExprNode)))) {
						fprintf(stderr, "Failed to reallocate statement list\n");
						exit(1);
					}
					parser->parsed_node->as_StatementList.args[parser->parsed_node->as_StatementList.length - 1] = *parser->popped_parsed_node;  // MOVE contents
					free(parser->popped_parsed_node);  // FREE extra pointer
					parser->popped_parsed_node = NULL;
				}
				if (parser->token && parser->token->token_type == TOKENTYPE_Semicolon) {
					parser_consume_token(parser);
					// Switch again
					parser_push_state(parser);
					parser->pop_before_semicolon = true;
				} else {
					parser->mode = PSMD_NORMAL;
				}
				break;
			case EXPRNODE_LoopWhile:
				if (!parser->call_iteration) {
					parser_consume_token(parser);
					parser->parsed_node->as_LoopWhile.condition = parser->popped_parsed_node;  // MOVE
					parser->popped_parsed_node = NULL;
					parser->call_iteration = 1;
					parser_push_state(parser);
					parser->pop_before_semicolon = true;
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
					parser_consume_token(parser);
					parser->parsed_node->as_CondIf.condition = parser->popped_parsed_node;  // MOVE
					parser->popped_parsed_node = NULL;
					parser->call_iteration = 1;
					parser_push_state(parser);
					parser->pop_before_semicolon = true;
					parser->mode = PSMD_NORMAL;
				} else {
					parser->parsed_node->as_CondIf.body = parser->popped_parsed_node;  // MOVE
					parser->popped_parsed_node = NULL;
					parser->expression_end = true;
					parser->mode = PSMD_NORMAL;
				}
				break;
			case EXPRNODE_UserFunctionCall:
				if (parser->popped_parsed_node) {
					uint64_t index = parser->parsed_node->as_UserFunctionCall.arg_count++;
					if (!index) {
						if (parser->parsed_node->as_UserFunctionCall.args) {
							free(parser->parsed_node->as_UserFunctionCall.args);
						}
						parser->parsed_node->as_UserFunctionCall.args = parser->popped_parsed_node;  // MOVE
					} else {
						parser->parsed_node->as_UserFunctionCall.args = reallocarray(parser->parsed_node->as_UserFunctionCall.args, index + 1, sizeof(ExprNode));
						if (!parser->parsed_node->as_UserFunctionCall.args) {
							fprintf(stderr, "Failed to resize user function call argument array\n");
							exit(1);
						}
						parser->parsed_node->as_UserFunctionCall.args[index] = *parser->popped_parsed_node;  // MOVE contents
						free(parser->popped_parsed_node);  // FREE extra pointer
					}
					parser->popped_parsed_node = NULL;
				}
				if (parser->token && parser->token->token_type == TOKENTYPE_RParen) {
					parser_consume_token(parser);
					parser->call_iteration = 0;
					parser->expression_end = true;
					parser->mode = PSMD_NORMAL;
				} else {
					parser->after_comma = false;
					parser_push_state(parser);
					parser->pop_on_comma = true;
				}
				parser->after_comma = false;
				break;
			case EXPRNODE_UserFunctionDef:
				if (!parser->call_iteration) {
					// Arguments
					// TODO somehow use PSMD_GET_IDENTIFIER instead of destroying Variable expressions?
					if (parser->popped_parsed_node) {
						if (parser->popped_parsed_node->node_type != EXPRNODE_Variable) {
							fprintf(stderr, "Expected variable expression as formal argument, got type %s\n", expr_node_types[parser->popped_parsed_node->node_type]);
							exit(1);
						}
						uint64_t index = parser->parsed_node->as_UserFunctionDef.args.length++;
						parser->parsed_node->as_UserFunctionDef.args.entries = reallocarray(parser->parsed_node->as_UserFunctionDef.args.entries, index + 1, sizeof(ArgumentsDefEntry));
						if (!parser->parsed_node->as_UserFunctionDef.args.entries) {
							fprintf(stderr, "Failed to resize user function formal argument array\n");
							exit(1);
						}
						parser->parsed_node->as_UserFunctionDef.args.entries[index] = (ArgumentsDefEntry) {
							.name = parser->popped_parsed_node->as_Variable.name,  // MOVE
						};
						free(parser->popped_parsed_node);
						parser->popped_parsed_node = NULL;
					}
					if (parser->token && parser->token->token_type == TOKENTYPE_RParen) {
						parser_consume_token(parser);
						parser->call_iteration = 1;
						parser_push_state(parser);
						parser->pop_before_semicolon = true;
						parser->mode = PSMD_NORMAL;
					} else {
						parser->after_comma = false;
						parser_push_state(parser);
						parser->pop_on_comma = true;
					}
					parser->after_comma = false;
				} else {
					// Body
					parser->parsed_node->as_UserFunctionDef.body = parser->popped_parsed_node;  // MOVE
					parser->popped_parsed_node = NULL;
					parser->expression_end = true;
					parser->mode = PSMD_NORMAL;
				}
				break;
			default:
				fprintf(stderr, "Unexpected PSMD_WAIT_POP with expression of type #%d\n", parser->parsed_node->node_type);
				exit(1);
			}
			break;
		case PSMD_GROUP:
			if (parser->token->token_type != TOKENTYPE_RParen) {
				fprintf(stderr, "Expected ')', got type %s\n", token_type_names[parser->token->token_type]);
				exit(1);
			}
			parser_consume_token(parser);
			parser->mode = PSMD_NORMAL;
			parser->parsed_node = parser->popped_parsed_node;
			parser->popped_parsed_node = NULL;
			break;
		case PSMD_GET_IDENTIFIER: {
				if (!parser->token) {
					if (!parser->code_end) {
						return;
					}
					fprintf(stderr, "Expected identifier, got end of input\n");
					exit(1);
				}
				if (parser->token->token_type != TOKENTYPE_Identifier) {
					fprintf(stderr, "Expected identifier, got type %s\n", token_type_names[parser->token->token_type]);
					exit(1);
				}
				char *name = strndup(parser->token->as_Identifier.name.ptr, parser->token->as_Identifier.name.length);
				parser_consume_token(parser);
				switch (parser->parsed_node->node_type) {
				case EXPRNODE_UserFunctionDef:
					parser->parsed_node->as_UserFunctionDef.name = name;
					break;
				case EXPRNODE_UserFunctionCall:
					parser->parsed_node->as_UserFunctionCall.name = name;
					break;
				default:
					fprintf(stderr, "Unexpected PSMD_GET_IDENTIFIER with expression of type #%d\n", parser->parsed_node->node_type);
					exit(1);
				}
				parser->mode = PSMD_GET_ARGS;
			}
			break;
		case PSMD_GET_ARGS: {
				if (!parser->token) {
					if (!parser->code_end) {
						return;
					}
					fprintf(stderr, "Expected '(', got end of input\n");
					exit(1);
				}
				if (parser->token->token_type != TOKENTYPE_LParen) {
					fprintf(stderr, "Expected '(', got type %s\n", token_type_names[parser->token->token_type]);
					exit(1);
				}
				parser_consume_token(parser);
				parser->mode = PSMD_WAIT_POP;
				parser->call_iteration = 0;
				parser_push_state(parser);
				parser->pop_on_comma = true;
			}
			break;
		}
	}
}

const ExprNode *
parser_end(Parser * parser)
{
	parser->code_end = true;
	lexer_end(parser->lexer);
	parser_feed(parser, NULL, 0);
	while (parser->stack_parent) {
		parser_before_expression_end(parser);
		parser_pop_state(parser);
		parser_feed(parser, NULL, 0);
	}
	if (!parser->parsed_node) {
		parser->parsed_node = make_noop_expr();
	}
	return parser->parsed_node;
}
