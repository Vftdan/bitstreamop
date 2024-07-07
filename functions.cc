#if defined(BITSTREAMOP_FUNCTION)

BITSTREAMOP_FUNCTION(read, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(amount)), (
	BitUSize amount = (BitUSize) args->amount.value;
	if (amount > 64)
		die("Cannot read more than 64 bits");
	uint64_t result_n = 0;
	BitSlice result_slice = BIT_SLICE_REFERENCE_INT(result_n);
	bit_io_read(context->io_in, &result_slice, amount);
	result_n = be64toh(result_n) >> (64 - amount);
	return (WidthInteger) {
		.value = result_n,
		.width = amount,
	};
))

BITSTREAMOP_FUNCTION(write, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(value)), (
	uint64_t amount = args->value.width;
	uint64_t value_n = args->value.value;
	value_n = htobe64(value_n << (64 - amount));
	BitConstSlice value_slice = BIT_SLICE_REFERENCE_INT(value_n);
	bit_io_write(context->io_out, &value_slice, amount);
	return (WidthInteger) {
		.value = 0,
		.width = 0,
	};
))

BITSTREAMOP_FUNCTION(readeof, BITSTREAMOP_ARGLIST(), (
	uint64_t result_n = 0;
	result_n = feof(context->io_in->file) && !context->io_in->in_buffer.io_length;
	if (context->io_in->in_eof) {
		result_n = 1;
	}
	return (WidthInteger) {
		.value = result_n,
		.width = 1,
	};
))

BITSTREAMOP_FUNCTION(not, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(value)), (
	return (WidthInteger) {
		.value = (bool) !args->value.value,
		.width = 1,
	};
))

BITSTREAMOP_FUNCTION(and, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return (WidthInteger) {
		.value = (bool) args->lhs.value && args->rhs.value,
		.width = 1,
	};
))

BITSTREAMOP_FUNCTION(or, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return (WidthInteger) {
		.value = (bool) args->lhs.value || args->rhs.value,
		.width = 1,
	};
))

BITSTREAMOP_FUNCTION(xor, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return (WidthInteger) {
		.value = !!args->lhs.value != !!args->rhs.value,
		.width = 1,
	};
))

BITSTREAMOP_FUNCTION(bit_not, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(value)), (
	return fix_width((WidthInteger) {
		.value = ~args->value.value,
		.width = args->value.width,
	});
))

BITSTREAMOP_FUNCTION(bit_and, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return (WidthInteger) {
		.value = args->lhs.value & args->rhs.value,
		.width = MIN(args->lhs.width, args->rhs.width),
	};
))

BITSTREAMOP_FUNCTION(bit_or, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return (WidthInteger) {
		.value = args->lhs.value | args->rhs.value,
		.width = MAX(args->lhs.width, args->rhs.width),
	};
))

BITSTREAMOP_FUNCTION(bit_xor, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return (WidthInteger) {
		.value = args->lhs.value ^ args->rhs.value,
		.width = MAX(args->lhs.width, args->rhs.width),
	};
))

BITSTREAMOP_FUNCTION(shl, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return fix_width((WidthInteger) {
		.value = args->lhs.value << args->rhs.value,
		.width = args->lhs.width,
	});
))

BITSTREAMOP_FUNCTION(shr, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return fix_width((WidthInteger) {
		.value = args->lhs.value >> args->rhs.value,
		.width = args->lhs.width,
	});
))

BITSTREAMOP_FUNCTION(width, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(new_width) BITSTREAMOP_ARG(value)), (
	return fix_width((WidthInteger) {
		.value = args->value.value,
		.width = args->new_width.value,
	});
))

BITSTREAMOP_FUNCTION(sig_width, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(new_width) BITSTREAMOP_ARG(value)), (
	return fix_width((WidthInteger) {
		.value = sigextend_value(args->value),
		.width = args->new_width.value,
	});
))

BITSTREAMOP_FUNCTION(add, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return fix_width((WidthInteger) {
		.value = args->lhs.value + args->rhs.value,
		.width = MAX(args->lhs.width, args->rhs.width),
	});
))

BITSTREAMOP_FUNCTION(sub, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return fix_width((WidthInteger) {
		.value = args->lhs.value - args->rhs.value,
		.width = MAX(args->lhs.width, args->rhs.width),
	});
))

BITSTREAMOP_FUNCTION(mul, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return fix_width((WidthInteger) {
		.value = args->lhs.value * args->rhs.value,
		.width = MAX(args->lhs.width, args->rhs.width),
	});
))

BITSTREAMOP_FUNCTION(div, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return fix_width((WidthInteger) {
		.value = args->lhs.value / args->rhs.value,
		.width = args->lhs.width,
	});
))

BITSTREAMOP_FUNCTION(sig_div, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return fix_width((WidthInteger) {
		.value = sigextend_value(args->lhs) / sigextend_value(args->rhs),
		.width = args->lhs.width,
	});
))

BITSTREAMOP_FUNCTION(lt, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return (WidthInteger) {
		.value = args->lhs.value < args->rhs.value,
		.width = 1,
	};
))

BITSTREAMOP_FUNCTION(gt, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return (WidthInteger) {
		.value = args->lhs.value > args->rhs.value,
		.width = 1,
	};
))

BITSTREAMOP_FUNCTION(le, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return (WidthInteger) {
		.value = args->lhs.value <= args->rhs.value,
		.width = 1,
	};
))

BITSTREAMOP_FUNCTION(ge, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return (WidthInteger) {
		.value = args->lhs.value >= args->rhs.value,
		.width = 1,
	};
))

BITSTREAMOP_FUNCTION(sig_lt, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return (WidthInteger) {
		.value = sigextend_value(args->lhs) < sigextend_value(args->rhs),
		.width = 1,
	};
))

BITSTREAMOP_FUNCTION(sig_gt, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return (WidthInteger) {
		.value = sigextend_value(args->lhs) > sigextend_value(args->rhs),
		.width = 1,
	};
))

BITSTREAMOP_FUNCTION(sig_le, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return (WidthInteger) {
		.value = sigextend_value(args->lhs) <= sigextend_value(args->rhs),
		.width = 1,
	};
))

BITSTREAMOP_FUNCTION(sig_ge, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return (WidthInteger) {
		.value = sigextend_value(args->lhs) >= sigextend_value(args->rhs),
		.width = 1,
	};
))

BITSTREAMOP_FUNCTION(eq, BITSTREAMOP_ARGLIST(BITSTREAMOP_ARG(lhs) BITSTREAMOP_ARG(rhs)), (
	return (WidthInteger) {
		.value = args->lhs.value == args->rhs.value,
		.width = 1,
	};
))

#endif
