#ifdef BITSTREAMOP_EXPRNODE

BITSTREAMOP_EXPRNODE(FunctionApplication, (
	uint64_t arg_count;
	FunctionTableEntry *func;
	struct expression_node *args;
), self, ctx, result, (
	size_t n = self->arg_count;
	if (n != self->func->args_def.length) {
		die("Wrong argument count");
	}
	WidthInteger *arg_values = calloc(n, sizeof(WidthInteger));
	if (!arg_values) {
		die("Failed to allocate argument values");
	}
	for (uint64_t i = 0; i < n; ++i) {
		arg_values[i] = EVALUATE(self->args[i]);
	}
	scope_push(&ctx->scope);
	*result = self->func->impl(ctx, arg_values);
	scope_pop(&ctx->scope);
	free(arg_values);
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
), self, ctx, result, (
	*result = self->value;
), printer, (
	printer->start_field(printer);
	printer->printf(printer, "value = (value = %lld, width = %zu)", self->value.value, self->value.width);
	printer->end_field(printer);
))

BITSTREAMOP_EXPRNODE(Assign, (
	char *name;
	struct expression_node *rhs;
), self, ctx, result, (
	WidthInteger value = EVALUATE(*self->rhs);
	scope_assign_variable(&ctx->scope, self->name, value);
	*result = value;
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
), self, ctx, result, (
	WidthInteger value = EVALUATE(*self->rhs);
	WidthInteger *ptr = scope_find_variable(&ctx->scope, self->name);
	if (ptr) {
		*ptr = value;
	} else {
		scope_assign_variable(&ctx->scope, self->name, value);
	}
	*result = value;
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
), self, ctx, result, (
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
), self, ctx, result, (
	if (self->length) {
		WidthInteger value;
		for (uint64_t i = 0; i < self->length; ++i) {
			value = EVALUATE(self->args[i]);
		}
		*result = value;
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
), self, ctx, result, (
	scope_push(&ctx->scope);
	WidthInteger condition = EVALUATE(*self->condition);
	while (condition.value) {
		*result = EVALUATE(*self->body);
		condition = EVALUATE(*self->condition);
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

#endif
