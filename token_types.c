#include "token_types.h"
#include "common.h"

#define UNPACK(...) __VA_ARGS__

void
print_token_data(TreePrinter * __printer, const TokenData * __token)
{
	if (!__token) {
		__printer->start_field(__printer);
		__printer->printf(__printer, "NULL");
		__printer->end_field(__printer);
		return;
	}
	struct {
		TreePrinter *printer;
		const TokenData *token;
	} print_token_data__locals = {
		.printer = __printer,
		.token = __token,
	};
	{
		switch (print_token_data__locals.token->token_type) {
#define PRINT_FIELD(...) printer->start_field(printer); printer->printf(printer, __VA_ARGS__); printer->end_field(printer);
#define BITSTREAMOP_TOKEN(name, elements, printimpl) case TOKENTYPE_##name: { \
		TreePrinter *const printer = print_token_data__locals.printer; \
		const name##TokenData *const self = &print_token_data__locals.token->as_##name; \
		(void) self; \
		PRINT_FIELD("token_type = TOKENTYPE_%s", #name); \
		UNPACK printimpl \
	} break;
#include "token_types.cc"
#undef BITSTREAMOP_TOKEN
		}
	}
}
