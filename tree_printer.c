#include <stdarg.h>
#include <stdio.h>

#include "tree_printer.h"

static void
filetp_printf(FileTreePrinter * self, const char * format, ...)
{
	va_list args;
	va_start(args, format);

	vfprintf(self->file, format, args);
}

static void
filetp_start_field(FileTreePrinter * self)
{
	for (int64_t i = 0; i < self->current_level; ++i) {
		fprintf(self->file, "%s", self->str_indent);
	}
}

static void
filetp_end_field(FileTreePrinter * self)
{
	fprintf(self->file, "%s", self->str_endl);
}

static void
filetp_start_child(FileTreePrinter * self)
{
	++(self->current_level);
}

static void
filetp_end_child(FileTreePrinter * self)
{
	--(self->current_level);
}

void
init_file_tree_printer(FileTreePrinter * self, FILE * file)
{
	self->printf = &filetp_printf;
	self->start_field = &filetp_start_field;
	self->end_field = &filetp_end_field;
	self->start_child = &filetp_start_child;
	self->end_child = &filetp_end_child;
	self->file = file;
	self->str_endl = "\n";
	self->str_indent = "\t";
	self->current_level = 0;
}
