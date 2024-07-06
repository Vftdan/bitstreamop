#include "functions.h"
#include "interp_types.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>

__attribute__((noreturn)) static void
die(char * msg)
{
	fprintf(stderr, "Error: %s\n", msg);
	exit(1);
}

static WidthInteger
fix_width(WidthInteger value)
{
	if (!value.width) {
		value.value = 0;
	}
	value.value &= (1ULL << value.width) - 1;
	return value;
}

#define UNPACK(...) __VA_ARGS__
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define MIN(a, b) ((a) > (b) ? (b) : (a))

// Implementations:
#define BITSTREAMOP_FUNCTION(name, arglist, body) WidthInteger funcimpl_##name(InterpContext * context, Argtype_##name * args) { (void) context; (void) args; UNPACK body }

#include "functions.cc"

#undef BITSTREAMOP_FUNCTION

// Table:
#define BITSTREAMOP_ARG(aname) {.name = #aname},
#define BITSTREAMOP_ARGLIST(...) {.length = sizeof((ArgumentsDefEntry[]) {__VA_ARGS__}) / sizeof(ArgumentsDefEntry), .entries = (ArgumentsDefEntry[]) {__VA_ARGS__}}
#define BITSTREAMOP_FUNCTION(fname, arglist, body) {.name = #fname, .impl = (WidthInteger (*)(InterpContext *, void *)) &funcimpl_##fname, .args_def = arglist},
static FunctionTableEntry function_table_values[] = {
#include "functions.cc"
};

FunctionTable function_table = {
	.length = (sizeof(function_table_values) / sizeof(FunctionTableEntry)),
	.entries = function_table_values,
};

FunctionTableEntry *
find_function(char * name)
{
	for (size_t i = 0; i < function_table.length; ++i) {
		if (!strcmp(function_table.entries[i].name, name)) {
			return &function_table.entries[i];
		}
	}
	return NULL;
}
