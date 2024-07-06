#include "bitio.h"
#include "common.h"

#include <assert.h>
#include <string.h>
#include <endian.h>
#include <stdbool.h>

// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59098
#define COMBINE(mask, ifzero, ifone) ((mask) ? ((typeof(mask)) ~(mask)) ? (((mask) & (ifone)) | (~(mask) & (ifzero))) : (ifone) : (ifzero))
#define CYC_LSHFT(bits, value, amount) COMBINE((1 << (amount)) - 1, (value) << (amount), (value) >> ((bits) - (amount)))
#define CYC_RSHFT(bits, value, amount) COMBINE((1 << ((bits) - (amount))) - 1, (value) << ((bits) - (amount)), (value) >> (amount))
#define BYTE_OF_BIT(ptr, offset) (*(((uint8_t*) (ptr)) + ((offset) >> 3)))
#define BYTE_OF_START(slc) BYTE_OF_BIT((slc).ptr, (slc).offset)
#define LONG_OF_BIT(ptr, offset) (*(((uint64_t*) (ptr)) + ((offset) >> 6)))
#define LONG_OF_START(slc) LONG_OF_BIT((slc).ptr, (slc).offset)

inline static void
copy_inside_byte_bitaligned(BitSlice * dstptr, BitConstSlice * srcptr, BitSSize * remainingptr, BitUSize advance)
{
	uint8_t c = BYTE_OF_START(*srcptr);
	if ((BitUSize) *remainingptr < advance)
		advance = *remainingptr;
	int mask_shift = (dstptr->offset + advance) & 7;
	uint8_t mask = 0xff >> (8 - advance);
	mask = CYC_RSHFT(8, mask, mask_shift);
	BYTE_OF_START(*dstptr) = COMBINE(mask, BYTE_OF_START(*dstptr), c);
	*remainingptr -= advance;
	BIT_SLICE_ADVANCE_INPLACE(*srcptr, advance);
	BIT_SLICE_ADVANCE_INPLACE(dstptr->as_const, advance);
}

inline static void
copy_inside_byte(BitSlice * dstptr, BitConstSlice * srcptr, BitSSize * remainingptr, BitUSize advance)
{
	BitUSize imbalance = (srcptr->offset - dstptr->offset) & 7;
	uint8_t c = BYTE_OF_START(*srcptr);  // dst-aligned
	c = CYC_LSHFT(8, c, imbalance);
	if ((BitUSize) *remainingptr < advance)
		advance = *remainingptr;
	int mask_shift = (dstptr->offset + advance) & 7;
	uint8_t mask = 0xff >> (8 - advance);
	mask = CYC_RSHFT(8, mask, mask_shift);
	BYTE_OF_START(*dstptr) = COMBINE(mask, BYTE_OF_START(*dstptr), c);
	*remainingptr -= advance;
	BIT_SLICE_ADVANCE_INPLACE(*srcptr, advance);
	BIT_SLICE_ADVANCE_INPLACE(dstptr->as_const, advance);
}

inline static void
copy_inside_long(BitSlice * dstptr, BitConstSlice * srcptr, BitSSize * remainingptr, BitUSize advance)
{
	BitUSize imbalance = (srcptr->offset - dstptr->offset) & 63;
	uint64_t c = LONG_OF_START(*srcptr);
	c = CYC_RSHFT(64, c, imbalance);
	if ((BitUSize) *remainingptr < advance)
		advance = *remainingptr;
	int mask_shift = (dstptr->offset + advance) & 63;
	uint64_t mask = 0xffffffffffffffffULL >> (64 - advance);
	mask = CYC_RSHFT(64, mask, mask_shift);
	mask = htobe64(mask);
	LONG_OF_START(*dstptr) = COMBINE(mask, LONG_OF_START(*dstptr), c);
	*remainingptr -= advance;
	BIT_SLICE_ADVANCE_INPLACE(*srcptr, advance);
	BIT_SLICE_ADVANCE_INPLACE(dstptr->as_const, advance);
}

BitUSize
bit_slice_copy(BitSlice dst, BitConstSlice src)
{
	BitUSize length = dst.length;
	if (length > src.length)
		length = src.length;
	BitSSize remaining = length;
	BitUSize imbalance = (src.offset - dst.offset) & 7;
	if ((imbalance == 0) && false) {
		// offset & 7 = 0 => mask = 0xff
		// offset & 7 = 7 => mask = 0x01
		copy_inside_byte_bitaligned(&dst, &src, &remaining, 8 - (src.offset & 7));

		assert((src.offset & 7) == 0);
		assert((dst.offset & 7) == 0);
		size_t whole_bytes = remaining >> 3;
		memcpy(&BYTE_OF_START(dst), &BYTE_OF_START(src), whole_bytes);
		BitUSize advance = whole_bytes << 3;
		remaining -= advance;
		BIT_SLICE_ADVANCE_INPLACE(src, advance);
		BIT_SLICE_ADVANCE_INPLACE(dst.as_const, advance);

		assert(remaining < 8);
		assert(remaining >= 0);
		assert((src.offset & 7) == 0);
		assert((dst.offset & 7) == 0);
		copy_inside_byte(&dst, &src, &remaining, remaining);
	} else {
		while (remaining >= 64) {
			BitUSize maxadv_src = 64 - (src.offset & 63);
			BitUSize maxadv_dst = 64 - (dst.offset & 63);
			BitUSize maxadv = maxadv_src > maxadv_dst ? maxadv_dst : maxadv_src;
			copy_inside_long(&dst, &src, &remaining, maxadv);
		}
		while (remaining > 0) {
			BitUSize maxadv_src = 8 - (src.offset & 7);
			BitUSize maxadv_dst = 8 - (dst.offset & 7);
			BitUSize maxadv = maxadv_src > maxadv_dst ? maxadv_dst : maxadv_src;
			copy_inside_byte(&dst, &src, &remaining, maxadv);
		}
	}
	assert(remaining == 0);
	return length - (remaining > 0 ? remaining : 0);
}

BitIO
file_to_bit_io(FILE * file, size_t in_byte_size, size_t out_byte_size)
{
	if (!in_byte_size)
		in_byte_size = 1;
	if (!out_byte_size)
		out_byte_size = 1;
	void * in_data = calloc(1, in_byte_size);
	void * out_data = calloc(1, out_byte_size);
	if (!in_data || !out_data) {
		fprintf(stderr, "Failed to allocate IO buffer!\n");
		exit(1);
	}
	BitIO io = {
		.file = file,
		.in_buffer = {
			.data = in_data,
			.byte_size = in_byte_size,
			.io_length = 0,
			.used_bits = 0,
		},
		.out_buffer = {
			.data = out_data,
			.byte_size = out_byte_size,
			.io_length = 0,
			.used_bits = 0,
		},
	};
	return io;
}

void
free_bit_io(BitIO io)
{
	free(io.in_buffer.data);
	free(io.out_buffer.data);
}

BitUSize
bit_io_write(BitIO * io, BitConstSlice * slcptr, BitUSize amount)
{
	BitConstSlice slc = *slcptr;
	if (amount > slc.length)
		amount = slc.length;
	BitSSize remaining = amount;
	while (remaining > 0) {
		BitSlice io_slice = bit_buffer_remaining_to_slice(io->out_buffer);
		BitUSize advance = bit_slice_l_copy(io_slice, slc, remaining);
		remaining -= advance;
		BIT_SLICE_ADVANCE_INPLACE(slc, advance);
		BIT_SLICE_ADVANCE_INPLACE(io_slice.as_const, advance);
		io->out_buffer.used_bits += advance;
		if (io_slice.length <= 8) {
			size_t to_delete_bytes = io->out_buffer.io_length;
			BitUSize to_delete = to_delete_bytes << 3;
			io->out_buffer.used_bits -= to_delete;
			memmove(io->out_buffer.data, io->out_buffer.data + to_delete_bytes, io->out_buffer.byte_size - to_delete_bytes);
			io->out_buffer.io_length -= to_delete_bytes;
		}
		size_t write_bytes = (io->out_buffer.used_bits >> 3) - io->out_buffer.io_length;
		if (write_bytes) {
			write_bytes = fwrite(io->out_buffer.data + io->out_buffer.io_length, 1, write_bytes, io->file);
			io->out_buffer.io_length += write_bytes;
		}
	}
	assert(remaining == 0);
	return amount - remaining;
}

BitUSize
bit_io_read(BitIO * io, BitSlice * slcptr, BitUSize amount)
{
	BitSlice slc = *slcptr;
	if (amount > slc.length)
		amount = slc.length;
	BitSSize remaining = amount;
	while (remaining > 0) {
		BitSSize read_bits = remaining - ((io->in_buffer.io_length << 3) - io->in_buffer.used_bits);
		if (read_bits < 0)
			read_bits = 0;
		size_t read_bytes = read_bits ? ((read_bits - 1) >> 3) + 1 : 0;
		size_t available_space = io->in_buffer.byte_size - io->in_buffer.io_length;
		if (read_bytes > available_space)
			read_bytes = available_space;
		if (read_bytes) {
			size_t read_bytes_success = fread(io->in_buffer.data + io->in_buffer.io_length, 1, read_bytes, io->file);
			if (!read_bytes_success) {
				memset(io->in_buffer.data + io->in_buffer.io_length, 0, read_bytes);
				read_bytes_success = read_bytes;
				io->in_eof = true;
			}
			io->in_buffer.io_length += read_bytes_success;
		}
		BitConstSlice io_slice = bit_buffer_remaining_valid_to_slice(io->in_buffer).as_const;
		BitUSize advance = bit_slice_l_copy(slc, io_slice, remaining);
		remaining -= advance;
		BIT_SLICE_ADVANCE_INPLACE(slc.as_const, advance);
		io->in_buffer.used_bits += advance;
		size_t to_delete_bytes = io->in_buffer.used_bits >> 3;
		BitUSize to_delete = to_delete_bytes << 3;
		io->in_buffer.used_bits -= to_delete;
		io->in_buffer.io_length -= to_delete_bytes;
		memmove(io->out_buffer.data, io->out_buffer.data + to_delete_bytes, io->out_buffer.byte_size - to_delete_bytes);
	}
	assert(remaining == 0);
	return amount - remaining;
}

void
bit_io_flush(BitIO * io)
{
	BitUSize unfinished_bits = io->out_buffer.used_bits & 7;
	if (unfinished_bits) {
		uint64_t zero = 0;
		BitConstSlice zero_slice = BIT_SLICE_REFERENCE_INT(zero);
		bit_io_write(io, &zero_slice, 8 - unfinished_bits);
	}
	size_t write_bytes = (io->out_buffer.used_bits >> 3) - io->out_buffer.io_length;
	if (write_bytes) {
		write_bytes = fwrite(io->out_buffer.data + io->out_buffer.io_length, 1, write_bytes, io->file);
		io->out_buffer.io_length += write_bytes;
	}
}
