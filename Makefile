CFLAGS = -Wall -Wextra
ifdef DEBUG
	CFLAGS += -Og -gdwarf-2 -D DEBUG
else
	CFLAGS += -O2
endif
INTERP ?=

all: bitstreamop

run: bitstreamop
	${INTERP} ./bitstreamop

.PHONY: all run

bitstreamop: bitstreamop.o bitio.o functions.o expression.o lexer.o parser.o tree_printer.o token_types.o
	$(CC) $(LDFLAGS) $^ -o $@
