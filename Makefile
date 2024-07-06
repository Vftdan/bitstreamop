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

bitstreamop: bitstreamop.o bitio.o functions.o expression.o parser.o tree_printer.o
	$(CC) $(LDFLAGS) $^ -o $@
