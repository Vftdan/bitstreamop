#include "expression.h"
#include "common.h"

#define UNPACK(...) __VA_ARGS__

__attribute__((noreturn)) static void
die(char * msg)
{
	fprintf(stderr, "Error: %s\n", msg);
	exit(1);
}

#define BITSTREAMOP_EXPRNODE(name, elements, co_locals_def, self, co_locals_var, ctx, result, evalimpl, printer_var, printimpl) typedef struct { UNPACK co_locals_def } name##EvaluateExpressionLocals;
#include "expression.cc"
#undef BITSTREAMOP_EXPRNODE

typedef struct evaluate_expression_locals {
	struct evaluate_expression_locals *caller;
	InterpContext *context;
	const ExprNode *expression;
	WidthInteger result;
	bool evaluated;
	int entry;
	bool finished;
	WidthInteger * parent_result_address;
	union {
#define BITSTREAMOP_EXPRNODE(name, elements, co_locals_def, self, co_locals_var, ctx, result, evalimpl, printer_var, printimpl) name##EvaluateExpressionLocals as_##name;
#include "expression.cc"
#undef BITSTREAMOP_EXPRNODE
	};
} EvaluateExpressionLocals;

static void
push_evaluate_expression_locals(EvaluateExpressionLocals ** ptrptr, const ExprNode * expr)
{
	EvaluateExpressionLocals * new_ptr = malloc(sizeof(EvaluateExpressionLocals));
	EvaluateExpressionLocals * caller = *ptrptr;
	*new_ptr = (EvaluateExpressionLocals) {
		.caller = caller,
		.context = caller ? caller->context : NULL,
		.expression = expr,
		.result = {0, 0},
		.evaluated = false,
		.entry = 0,
		.finished = false,
	};
	*ptrptr = new_ptr;
}

static void
pop_evaluate_expression_locals(EvaluateExpressionLocals ** ptrptr)
{
	EvaluateExpressionLocals * child = *ptrptr;
	EvaluateExpressionLocals * caller = child->caller;
	free(child);
	*ptrptr = caller;
}

WidthInteger
evaluate_expression(InterpContext * __context, const ExprNode * __expr)
{
	EvaluateExpressionLocals * evaluate_expression__locals = NULL;
	push_evaluate_expression_locals(&evaluate_expression__locals, __expr);
	evaluate_expression__locals->context = __context;
	while (true) {
evaluate_expression__next_iter:
		if (evaluate_expression__locals->finished) {
			if (!evaluate_expression__locals->caller) {
				WidthInteger result = evaluate_expression__locals->result;
				pop_evaluate_expression_locals(&evaluate_expression__locals);
				return result;
			}
			if (evaluate_expression__locals->parent_result_address) {
				*evaluate_expression__locals->parent_result_address = evaluate_expression__locals->result;
			}
			pop_evaluate_expression_locals(&evaluate_expression__locals);
		}
		switch (evaluate_expression__locals->expression->node_type) {
#define EVALUATE(retvar, subexpr, reentry) if (!evaluate_expression__locals->evaluated) { WidthInteger * retvar_ptr = &(retvar); evaluate_expression__locals->entry = reentry; evaluate_expression__locals->evaluated = true; push_evaluate_expression_locals(&evaluate_expression__locals, &(subexpr)); evaluate_expression__locals->parent_result_address = retvar_ptr; goto evaluate_expression__next_iter; } else { evaluate_expression__locals->evaluated = false; }
#define CONTINUATION(n) /* FALLTHROUGH */ case n:;
#define BITSTREAMOP_EXPRNODE(name, elements, co_locals_def, self, co_locals_var, ctx, result, evalimpl, printer_var, printimpl) case EXPRNODE_##name: { \
		InterpContext *const ctx = evaluate_expression__locals->context; \
		WidthInteger *const result = &evaluate_expression__locals->result; \
		const name##ExprNode *const self = &evaluate_expression__locals->expression->as_##name; \
		name##EvaluateExpressionLocals *const co_locals_var = &evaluate_expression__locals->as_##name; \
		(void) ctx; \
		(void) result; \
		(void) self; \
		(void) co_locals_var; \
		switch (evaluate_expression__locals->entry) { \
		CONTINUATION(0) \
			UNPACK evalimpl \
			evaluate_expression__locals->finished = true; \
		} \
	} break;
#include "expression.cc"
#undef BITSTREAMOP_EXPRNODE
#undef EVALUATE
		}
	}
}

void
print_expression(TreePrinter * __printer, const ExprNode * __expr)
{
	if (!__expr) {
		__printer->start_field(__printer);
		__printer->printf(__printer, "NULL");
		__printer->end_field(__printer);
		return;
	}
	struct {
		TreePrinter *printer;
		const ExprNode *expression;
	} print_expression__locals = {
		.printer = __printer,
		.expression = __expr,
	};
	{
		switch (print_expression__locals.expression->node_type) {
#define PRINT_CHILD(subexpr) print_expression__locals.printer->start_child(print_expression__locals.printer); print_expression(print_expression__locals.printer, &(subexpr)); print_expression__locals.printer->end_child(print_expression__locals.printer)
#define BITSTREAMOP_EXPRNODE(name, elements, co_locals_def, self, co_locals_var, ctx, result, evalimpl, printer_var, printimpl) case EXPRNODE_##name: { \
		TreePrinter *const printer_var = print_expression__locals.printer; \
		const name##ExprNode *const self = &print_expression__locals.expression->as_##name; \
		printer_var->start_field(printer_var); \
		printer_var->printf(printer_var, "node_type = EXPRNODE_%s", #name); \
		printer_var->end_field(printer_var); \
		UNPACK printimpl \
	} break;
#include "expression.cc"
#undef BITSTREAMOP_EXPRNODE
#undef PRINT_CHILD
		}
	}
}

char *expr_node_types[] = {
#define BITSTREAMOP_EXPRNODE(name, elements, co_locals_def, self, co_locals_var, ctx, result, evalimpl, printer, printimpl) #name,
#include "expression.cc"
#undef BITSTREAMOP_EXPRNODE
};
