#ifndef BITIO_H_
#define BITIO_H_

// MSB-first

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef size_t BitUSize;
typedef ssize_t BitSSize;

typedef struct {
	const void * ptr;
	BitUSize offset, length;
} BitConstSlice;

typedef struct {
	union {
		BitConstSlice as_const;
		struct {
			void * ptr;
			BitUSize offset, length;
		};
	};
} BitSlice;

#define BIT_SLICE_REFERENCE_INT(num) {.ptr = &num, .offset = 0, .length = sizeof(num) << 3}

__attribute__((unused)) inline static void
bit_subslice_inplace(BitConstSlice * slcptr, BitUSize start, BitUSize endlen)
{
	BitUSize length = slcptr->length;
	length = length > endlen ? endlen : length;
	length = length > start ? length - start : 0;
	slcptr->offset += start;
	slcptr->length = length;
}

#define BIT_SLICE_ADVANCE_INPLACE(slc, amount) bit_subslice_inplace(&(slc), amount, (slc).length)

typedef struct {
	size_t byte_size;
	size_t io_length;  // Place into which to read?
	BitUSize used_bits;
	uint8_t * data;
} BitBuffer;

typedef struct {
	FILE * file;
	BitBuffer in_buffer, out_buffer;
	bool in_eof;
} BitIO;

BitUSize bit_slice_copy(BitSlice dst, BitConstSlice src);

__attribute__((unused)) inline static BitUSize
bit_slice_l_copy(BitSlice dst, BitConstSlice src, BitUSize length)
{
	bit_subslice_inplace(&dst.as_const, 0, length);
	return bit_slice_copy(dst, src);
}

__attribute__((unused)) inline static BitSlice
bit_buffer_remaining_to_slice(BitBuffer buf)
{
	return (BitSlice) {.ptr = buf.data, .offset = buf.used_bits, .length = (buf.byte_size << 3) - buf.used_bits};
}

__attribute__((unused)) inline static BitSlice
bit_buffer_remaining_valid_to_slice(BitBuffer buf)
{
	return (BitSlice) {.ptr = buf.data, .offset = buf.used_bits, .length = (buf.io_length << 3) - buf.used_bits};
}

__attribute__((unused)) inline static BitSlice
bit_buffer_used_to_slice(BitBuffer buf)
{
	return (BitSlice) {.ptr = buf.data, .offset = 0, .length = buf.used_bits};
}

BitIO file_to_bit_io(FILE * file, size_t in_byte_size, size_t out_byte_size);

void free_bit_io(BitIO io);

BitUSize bit_io_write(BitIO * io, BitConstSlice * slcptr, BitUSize amount);
BitUSize bit_io_read(BitIO * io, BitSlice * slcptr, BitUSize amount);
void bit_io_flush(BitIO * io);  // Dosn't call underlying flush

#endif /* end of include guard: BITIO_H_ */
