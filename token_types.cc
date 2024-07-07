#ifdef BITSTREAMOP_TOKEN

BITSTREAMOP_TOKEN(LParen, (
), (
))

BITSTREAMOP_TOKEN(RParen, (
), (
))

BITSTREAMOP_TOKEN(Assign, (
	TOKEN_PAYLOAD_FIELD(bool, is_reassign)
), (
	PRINT_FIELD("is_reassign = %d", self->is_reassign);
))

BITSTREAMOP_TOKEN(Comma, (
), (
))

BITSTREAMOP_TOKEN(Semicolon, (
), (
))

BITSTREAMOP_TOKEN(Keyword, (
	TOKEN_PAYLOAD_FIELD(enum keyword_token_type, keyword_type)
), (
	PRINT_FIELD("keyword_type = %s", keyword_type_names[self->keyword_type]);
))

BITSTREAMOP_TOKEN(Identifier, (
	TOKEN_PAYLOAD_FIELD(CharSlice, name)
), (
	PRINT_FIELD("name = %.*s", (int) self->name.length, self->name.ptr);
))

BITSTREAMOP_TOKEN(Number, (
	TOKEN_PAYLOAD_FIELD(WidthInteger, value)
), (
	PRINT_FIELD("value = %ld", self->value);
))

#endif
