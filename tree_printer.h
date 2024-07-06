#ifndef TREE_PRINTER_H_
#define TREE_PRINTER_H_

#include <stdint.h>
#include <stdio.h>

#define TREE_PRINTER_VTABLE(TSelf) \
	void (*printf)(TSelf * self, const char * format, ...); \
	void (*start_field)(TSelf * self); \
	void (*end_field)(TSelf * self); \
	void (*start_child)(TSelf * self); \
	void (*end_child)(TSelf * self);

typedef struct tree_printer TreePrinter;
typedef struct file_tree_printer FileTreePrinter;

struct tree_printer {
	TREE_PRINTER_VTABLE(TreePrinter)
};

struct file_tree_printer {
	union {
		TreePrinter as_tree_printer;
		struct {
			TREE_PRINTER_VTABLE(FileTreePrinter)
		};
	};
	FILE * file;
	char * str_endl;
	char * str_indent;
	int64_t current_level;
};

void init_file_tree_printer(FileTreePrinter * self, FILE * file);

#endif /* end of include guard: TREE_PRINTER_H_ */
