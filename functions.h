#ifndef FUNCTIONS_H_
#define FUNCTIONS_H_

#include "interp_types.h"

#define BITSTREAMOP_ARG(name) WidthInteger name;
#define BITSTREAMOP_ARGLIST(args) args
#define BITSTREAMOP_FUNCTION(name, arglist, body) typedef struct { arglist } Argtype_##name; WidthInteger funcimpl_##name(InterpContext * context, Argtype_##name * args);

#include "functions.cc"

#undef BITSTREAMOP_FUNCTION
#undef BITSTREAMOP_ARGLIST
#undef BITSTREAMOP_ARG

extern FunctionTable function_table;

FunctionTableEntry *find_function(char * name);

#endif /* end of include guard: FUNCTIONS_H_ */
