#ifndef INTERP_TYPES_H_
#define INTERP_TYPES_H_

#include <stdint.h>
#include <string.h>
#include "bitio.h"

typedef struct {
	uint64_t value;
	BitUSize width;
} WidthInteger;

struct varlist_node {
	struct varlist_node *next;
	char *name;
	WidthInteger value;
};

typedef struct interp_scope {
	struct interp_scope *call_parent;
	struct varlist_node *variables;
} InterpScope;

typedef struct {
	BitIO *io_in, *io_out;
	InterpScope scope;
} InterpContext;

typedef struct {
	char *name;
} ArgumentsDefEntry;

typedef struct {
	uint64_t length;
	ArgumentsDefEntry *entries;
} ArgumentsDef;

typedef struct {
	char *name;
	WidthInteger (*impl)(InterpContext * context, void * args);
	ArgumentsDef args_def;
} FunctionTableEntry;

typedef struct {
	uint64_t length;
	FunctionTableEntry *entries;
} FunctionTable;

__attribute__((unused)) inline static WidthInteger*
scope_find_variable(InterpScope * scope, char * name)
{
	while (scope) {
		struct varlist_node *varnode = scope->variables;
		while (varnode) {
			if (!strcmp(name, varnode->name)) {
				return &varnode->value;
			}
			varnode = varnode->next;
		}
		scope = scope->call_parent;
	}
	return NULL;
}

__attribute__((unused)) inline static void
scope_assign_variable(InterpScope * scope, char * name, WidthInteger value)
{
	if (!scope)
		return;
	struct varlist_node *new_node = malloc(sizeof(struct varlist_node));
	if (!new_node) {
		fprintf(stderr, "Failed to allocate variable node\n");
		exit(1);
	}
	*new_node = (struct varlist_node) {
		.next = scope->variables,
		.name = strdup(name),
		.value = value,
	};
	scope->variables = new_node;
}

__attribute__((unused)) inline static void
scope_clear(InterpScope * scope)
{
	if (!scope)
		return;
	struct varlist_node *varnode = scope->variables;
	while (varnode) {
		struct varlist_node *next = varnode->next;
		free(varnode->name);
		free(varnode);
		varnode = next;
	}
	scope->variables = NULL;
}

__attribute__((unused)) inline static void
scope_push(InterpScope * scope)
{
	InterpScope *parent = malloc(sizeof(InterpScope));
	if (!parent) {
		fprintf(stderr, "Failed to allocate scope\n");
		exit(1);
	}
	*parent = *scope;
	*scope = (InterpScope) {
		.call_parent = parent,
		.variables = NULL,
	};
}

__attribute__((unused)) inline static void
scope_pop(InterpScope * scope)
{
	scope_clear(scope);
	InterpScope parent = {NULL, NULL};
	InterpScope *parent_ptr = scope->call_parent;
	if (parent_ptr) {
		parent = *parent_ptr;
		free(parent_ptr);
	}
	*scope = parent;
}

#endif /* end of include guard: INTERP_TYPES_H_ */
