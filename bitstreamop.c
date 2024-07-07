#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <endian.h>

#include "common.h"
#include "bitio.h"
#include "functions.h"
#include "expression.h"
#ifdef LEXER_ONLY
#include "lexer.h"
#else
#include "parser.h"
#endif

void
run_program(const ExprNode * program)
{
	BitIO io_in = file_to_bit_io(stdin, 16, 0);
	BitIO io_out = file_to_bit_io(stdout, 0, 16);

	InterpContext ctx = {
		.io_in = &io_in,
		.io_out = &io_out,
		.scope = {NULL, NULL},
		.user_functions = NULL,
	};

	evaluate_expression(&ctx, program);
	bit_io_flush(&io_out);
	free_bit_io(io_in);
	free_bit_io(io_out);
	scope_clear(&ctx.scope);
	userfunclist_clear(ctx.user_functions);
}

void
dump_ast(const ExprNode * program)
{
	FileTreePrinter printer;
	init_file_tree_printer(&printer, stdout);
	print_expression(&printer.as_tree_printer, program);
}

enum main_action {
	MAINACT_RUN,
	MAINACT_DUMP,
};

int
main(int argc, char ** argv)
{
	char * code = NULL;
	enum main_action main_action = MAINACT_RUN;
	if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
		fprintf(stderr, "Usage: %s [-d] <code>\n", argc ? argv[0] : "bitstreamop");
		return 1;
	}
	code = argv[1];
	if (argc > 2 && (!strcmp(argv[1], "-d") || !strcmp(argv[1], "--dump"))) {
		main_action = MAINACT_DUMP;
		code = argv[2];
	}

#ifdef LEXER_ONLY
	Lexer * lexer = lexer_new();
	lexer_feed(lexer, code, strlen(code));
	lexer_end(lexer);
	FileTreePrinter printer;
	init_file_tree_printer(&printer, stdout);
	TokenData * token;
	while ((token = lexer_take_token(lexer))) {
		print_token_data(&printer.as_tree_printer, token);
		destruct_token_data(token);
		free(token);
	}
	lexer_delete(lexer);
#else
	Parser * parser = parser_new();
	parser_feed(parser, code, strlen(code));
	const ExprNode * parsed_program = parser_end(parser);
	switch (main_action) {
	case MAINACT_RUN:
		run_program(parsed_program);
		break;
	case MAINACT_DUMP:
		dump_ast(parsed_program);
		break;
	}
	parser_delete(parser);
#endif

	return 0;
}
