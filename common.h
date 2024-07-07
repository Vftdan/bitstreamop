#ifndef COMMON_H_
#define COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define DEBUG_PRINT_VALUE(x, fmt) fprintf(stderr, #x " = " fmt "\n", x); fflush(stderr)

typedef struct {
	const char *ptr;
	size_t length;
} ConstCharSlice;

typedef struct {
	union {
		struct {
			char *ptr;
			size_t length;
		};
		ConstCharSlice as_const;
	};
} CharSlice;

#endif /* end of include guard: COMMON_H_ */
