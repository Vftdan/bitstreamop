#include "expression.h"
#include "common.h"

#define UNPACK(...) __VA_ARGS__

__attribute__((noreturn)) static void
die(char * msg)
{
	fprintf(stderr, "Error: %s\n", msg);
	exit(1);
}

WidthInteger
evaluate_expression(InterpContext * __context, const ExprNode * __expr)
{
	struct {
		InterpContext *context;
		const ExprNode *expression;
		WidthInteger result;
	} evaluate_expression__locals = {
		.context = __context,
		.expression = __expr,
		.result = {0, 0},
	};
	{
		switch (evaluate_expression__locals.expression->node_type) {
#define EVALUATE(subexpr) evaluate_expression(evaluate_expression__locals.context, &(subexpr))
#define BITSTREAMOP_EXPRNODE(name, elements, self, ctx, result, evalimpl, printer, printimpl) case EXPRNODE_##name: { \
		InterpContext *const ctx = evaluate_expression__locals.context; \
		WidthInteger *const result = &evaluate_expression__locals.result; \
		const name##ExprNode *const self = &evaluate_expression__locals.expression->as_##name; \
		(void) ctx; \
		(void) result; \
		(void) self; \
		UNPACK evalimpl \
	} break;
#include "expression.cc"
#undef BITSTREAMOP_EXPRNODE
#undef EVALUATE
		}
	}
	return evaluate_expression__locals.result;
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
#define BITSTREAMOP_EXPRNODE(name, elements, self, ctx, result, evalimpl, printer_var, printimpl) case EXPRNODE_##name: { \
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
