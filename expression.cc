#ifdef BITSTREAMOP_EXPRNODE

BITSTREAMOP_EXPRNODE(FunctionApplication, (
	uint64_t arg_count;
	FunctionTableEntry *func;
	struct expression_node *args;
), (
	size_t n, i;
	WidthInteger *arg_values;
), self, L, ctx, result, (
	L->n = self->arg_count;
	if (L->n != self->func->args_def.length) {
		die("Wrong argument count");
	}
	L->arg_values = calloc(L->n, sizeof(WidthInteger));
	if (!L->arg_values) {
		die("Failed to allocate argument values");
	}
	L->i = 0;
CONTINUATION(1)
	for (; L->i < L->n; ++L->i) {
		EVALUATE(L->arg_values[L->i], self->args[L->i], 1);
	}
	scope_push(&ctx->scope);
	*result = self->func->impl(ctx, L->arg_values);
	scope_pop(&ctx->scope);
	free(L->arg_values);
), printer, (
	printer->start_field(printer);
	printer->printf(printer, "arg_count = %llu", self->arg_count);
	printer->end_field(printer);

	printer->start_field(printer);
	printer->printf(printer, "func = %p", self->func);
	if (self->func) {
		printer->printf(printer, " (name = %s, impl = %p)", self->func->name, self->func->impl);
	}
	printer->end_field(printer);

	if (!self->args) {
		printer->start_field(printer);
		printer->printf(printer, "args = %p", self->args);
		printer->end_field(printer);
	} else {
		for (uint64_t i = 0; i < self->arg_count; ++i) {
			printer->start_field(printer);
			printer->printf(printer, "args[%llu] =", i);
			printer->end_field(printer);
			PRINT_CHILD(self->args[i]);
		}
	}
))

BITSTREAMOP_EXPRNODE(Literal, (
	WidthInteger value;
), (), self, L, ctx, result, (
	*result = self->value;
), printer, (
	printer->start_field(printer);
	printer->printf(printer, "value = (value = %lld, width = %zu)", self->value.value, self->value.width);
	printer->end_field(printer);
))

BITSTREAMOP_EXPRNODE(Assign, (
	char *name;
	struct expression_node *rhs;
), (
	WidthInteger value;
), self, L, ctx, result, (
	EVALUATE(L->value, *self->rhs, 0);
	scope_assign_variable(&ctx->scope, self->name, L->value);
	*result = L->value;
), printer, (
	printer->start_field(printer);
	printer->printf(printer, "name = %s", self->name);
	printer->end_field(printer);

	printer->start_field(printer);
	printer->printf(printer, "rhs = %p", self->rhs);
	printer->end_field(printer);
	if (self->rhs) {
		PRINT_CHILD(*self->rhs);
	}
))

// Works similar to python's nonlocal
BITSTREAMOP_EXPRNODE(Reassign, (
	char *name;
	struct expression_node *rhs;
), (
	WidthInteger value;
), self, L, ctx, result, (
	EVALUATE(L->value, *self->rhs, 0);
	WidthInteger *ptr = scope_find_variable(&ctx->scope, self->name);
	if (ptr) {
		*ptr = L->value;
	} else {
		InterpScope * scope = &ctx->scope;
		while (scope->call_parent) {
			scope = scope->call_parent;
		}
		scope_assign_variable(scope, self->name, L->value);
	}
	*result = L->value;
), printer, (
	printer->start_field(printer);
	printer->printf(printer, "name = %s", self->name);
	printer->end_field(printer);

	printer->start_field(printer);
	printer->printf(printer, "rhs = %p", self->rhs);
	printer->end_field(printer);
	if (self->rhs) {
		PRINT_CHILD(*self->rhs);
	}
))

BITSTREAMOP_EXPRNODE(Variable, (
	char *name;
), (), self, L, ctx, result, (
	WidthInteger *ptr = scope_find_variable(&ctx->scope, self->name);
	if (!ptr) {
		die("Variable not found");
	}
	*result = *ptr;
), printer, (
	printer->start_field(printer);
	printer->printf(printer, "name = %s", self->name);
	printer->end_field(printer);
))

BITSTREAMOP_EXPRNODE(StatementList, (
	uint64_t length;
	struct expression_node *args;
), (
	WidthInteger value;
	uint64_t i;
), self, L, ctx, result, (
	L->i = 0;
CONTINUATION(1)
	if (self->length) {
		for (; L->i < self->length; ++L->i) {
			EVALUATE(L->value, self->args[L->i], 1);
		}
		*result = L->value;
	}
), printer, (
	printer->start_field(printer);
	printer->printf(printer, "length = %llu", self->length);
	printer->end_field(printer);

	if (!self->args) {
		printer->start_field(printer);
		printer->printf(printer, "args = %p", self->args);
		printer->end_field(printer);
	} else {
		for (uint64_t i = 0; i < self->length; ++i) {
			printer->start_field(printer);
			printer->printf(printer, "args[%llu] =", i);
			printer->end_field(printer);
			PRINT_CHILD(self->args[i]);
		}
	}
))

BITSTREAMOP_EXPRNODE(LoopWhile, (
	struct expression_node *condition, *body;
), (
	WidthInteger condition;
), self, L, ctx, result, (
	scope_push(&ctx->scope);
CONTINUATION(1)
	EVALUATE(L->condition, *self->condition, 1);
CONTINUATION(2)
	if (L->condition.value) {
		EVALUATE(*result, *self->body, 2);
		EVALUATE(L->condition, *self->condition, 1);
	}
	scope_pop(&ctx->scope);
), printer, (
	printer->start_field(printer);
	printer->printf(printer, "condition = %p", self->condition);
	printer->end_field(printer);
	if (self->condition) {
		PRINT_CHILD(*self->condition);
	}

	printer->start_field(printer);
	printer->printf(printer, "body = %p", self->body);
	printer->end_field(printer);
	if (self->body) {
		PRINT_CHILD(*self->body);
	}
))

BITSTREAMOP_EXPRNODE(CondIf, (
	struct expression_node *condition, *body;
), (
	WidthInteger condition;
), self, L, ctx, result, (
	scope_push(&ctx->scope);
CONTINUATION(1)
	EVALUATE(L->condition, *self->condition, 1);
CONTINUATION(2)
	if (L->condition.value) {
		EVALUATE(*result, *self->body, 2);
	}
	scope_pop(&ctx->scope);
), printer, (
	printer->start_field(printer);
	printer->printf(printer, "condition = %p", self->condition);
	printer->end_field(printer);
	if (self->condition) {
		PRINT_CHILD(*self->condition);
	}

	printer->start_field(printer);
	printer->printf(printer, "body = %p", self->body);
	printer->end_field(printer);
	if (self->body) {
		PRINT_CHILD(*self->body);
	}
))

BITSTREAMOP_EXPRNODE(UserFunctionCall, (
	uint64_t arg_count;
	char *name;
	struct expression_node *args;
), (
	struct userfunclist_node *func;
	size_t n, i;
	InterpScope caller_scope, function_scope;
	WidthInteger arg_value;
), self, L, ctx, result, (
	L->func = userfunclist_find_function(ctx->user_functions, self->name);
	if (!L->func) {
		die("User function is not defined");
	}
	L->n = self->arg_count;
	if (L->n != L->func->args_def.length) {
		die("Wrong argument count");
	}
	L->caller_scope = ctx->scope;
	scope_push(&ctx->scope);
	L->function_scope = ctx->scope;
	ctx->scope = L->caller_scope;
	L->i = 0;
CONTINUATION(1)
	for (; L->i < L->n; ++L->i) {
		EVALUATE(L->arg_value, self->args[L->i], 1);
		scope_assign_variable(&L->function_scope, L->func->args_def.entries[L->i].name, L->arg_value);
	}
	ctx->scope = L->function_scope;
CONTINUATION(2)
	EVALUATE(*result, *L->func->body, 2);
	scope_pop(&ctx->scope);
), printer, (
	printer->start_field(printer);
	printer->printf(printer, "name = %s", self->name);
	printer->end_field(printer);

	printer->start_field(printer);
	printer->printf(printer, "arg_count = %llu", self->arg_count);
	printer->end_field(printer);

	if (!self->args) {
		printer->start_field(printer);
		printer->printf(printer, "args = %p", self->args);
		printer->end_field(printer);
	} else {
		for (uint64_t i = 0; i < self->arg_count; ++i) {
			printer->start_field(printer);
			printer->printf(printer, "args[%llu] =", i);
			printer->end_field(printer);
			PRINT_CHILD(self->args[i]);
		}
	}
))

BITSTREAMOP_EXPRNODE(UserFunctionDef, (
	char *name;
	struct expression_node *body;
	ArgumentsDef args;
), (), self, L, ctx, result, (
	// TODO consider using reassign-like logic
	userfunclist_add_function(&ctx->user_functions, self->name, self->args, self->body);
), printer, (
	printer->start_field(printer);
	printer->printf(printer, "name = %s", self->name);
	printer->end_field(printer);

	printer->start_field(printer);
	printer->printf(printer, "args.length = %llu", self->args.length);
	printer->end_field(printer);

	if (!self->args.entries) {
		printer->start_field(printer);
		printer->printf(printer, "args.entries = %p", self->args);
		printer->end_field(printer);
	} else {
		for (uint64_t i = 0; i < self->args.length; ++i) {
			printer->start_field(printer);
			printer->printf(printer, "args.entries[%llu].name = %s", i, self->args.entries[i].name);
			printer->end_field(printer);
		}
	}

	printer->start_field(printer);
	printer->printf(printer, "body = %p", self->body);
	printer->end_field(printer);
	if (self->body) {
		PRINT_CHILD(*self->body);
	}
))

#endif
