#ifndef EXPRESSION_H_
#define EXPRESSION_H_

#include "interp_types.h"
#include "tree_printer.h"

#define EXPRESSION_H__UNPACK(...) __VA_ARGS__

struct expression_node;

#define BITSTREAMOP_EXPRNODE(name, elements, self, ctx, result, evalimpl, printer, printimpl) typedef struct { EXPRESSION_H__UNPACK elements } name##ExprNode;
#include "expression.cc"
#undef BITSTREAMOP_EXPRNODE

typedef struct expression_node {
	enum expr_node_type {
#define BITSTREAMOP_EXPRNODE(name, elements, self, ctx, result, evalimpl, printer, printimpl) EXPRNODE_##name,
#include "expression.cc"
#undef BITSTREAMOP_EXPRNODE
	} node_type;
	void (*destructor)(struct expression_node * self);
	union {
#define BITSTREAMOP_EXPRNODE(name, elements, self, ctx, result, evalimpl, printer, printimpl) name##ExprNode as_##name;
#include "expression.cc"
#undef BITSTREAMOP_EXPRNODE
	};
} ExprNode;

WidthInteger evaluate_expression(InterpContext * context, const ExprNode * expr);

void print_expression(TreePrinter * printer, const ExprNode * expr);

__attribute__((unused)) inline static void
destruct_expression(struct expression_node * self)
{
	if (self->destructor)
		self->destructor(self);
}

#undef EXPRESSION_H__UNPACK

#endif /* end of include guard: EXPRESSION_H_ */
