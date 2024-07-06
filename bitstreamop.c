#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <endian.h>

#include "common.h"
#include "bitio.h"
#include "functions.h"
#include "expression.h"
#include "parser.h"

int
main(int argc, char ** argv)
{
	(void) argc;
	(void) argv;

	BitIO io_in = file_to_bit_io(stdin, 16, 0);
	BitIO io_out = file_to_bit_io(stdout, 0, 16);

	InterpContext ctx = {
		.io_in = &io_in,
		.io_out = &io_out,
		.scope = {NULL, NULL},
	};

	char code[] = "while (not(readeof())) (r = mul(read(1), 255); g = mul(read(1), 255); b = mul(read(1), 255); write(width(8, r)); write(width(8, g)); write(width(8, b));)";
	Parser * parser = parser_new();
	parser_feed(parser, code, sizeof(code) - 1);
	const ExprNode * parsed_program = parser_end(parser);
	evaluate_expression(&ctx, parsed_program);
	parser_delete(parser);
	bit_io_flush(&io_out);
	free_bit_io(io_in);
	free_bit_io(io_out);

	return 0;
}
