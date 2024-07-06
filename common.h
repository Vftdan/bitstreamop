#ifndef COMMON_H_
#define COMMON_H_

#include <stdio.h>

#define DEBUG_PRINT_VALUE(x, fmt) fprintf(stderr, #x " = " fmt "\n", x); fflush(stderr)

#endif /* end of include guard: COMMON_H_ */
