#include <byte_manip.h>

// 4 functions to aid in the copy operations, performing the respective
// function on different sizes of data.
static void copy_8_sh(void* to_buf, void* from_buf, uint32_t num_words, int8_t left_shift)
{
       uint8_t* to_buf_8 = (uint8_t*)to_buf;
       uint8_t* from_buf_8 = (uint8_t*)from_buf;
       while (num_words--) *to_buf_8++ |= *from_buf_8++ << left_shift;
}

static void copy_16_sh(void* to_buf, void* from_buf, uint32_t num_words, int8_t left_shift)
{
       uint16_t* to_buf_16 = (uint16_t*)to_buf;
       uint16_t* from_buf_16 = (uint16_t*)from_buf;
       while (num_words--) *to_buf_16++ |= *from_buf_16++ << left_shift;
}

static void copy_32_sh(void* to_buf, void* from_buf, uint32_t num_words, int8_t left_shift)
{
       uint32_t* to_buf_32 = (uint32_t*)to_buf;
       uint32_t* from_buf_32 = (uint32_t*)from_buf;
       while (num_words--) *to_buf_32++ |= *from_buf_32++ << left_shift;
}

static void copy_64_sh(void* to_buf, void* from_buf, uint32_t num_words, int8_t left_shift)
{
       uint64_t* to_buf_64 = (uint64_t*)to_buf;
       uint64_t* from_buf_64 = (uint64_t*)from_buf;
       while (num_words--) *to_buf_64++ |= *from_buf_64++ << left_shift;
}

// and a vector of these addresses for easy use
const cpy_func word_copy[4] = {copy_8_sh, copy_16_sh, copy_32_sh, copy_64_sh};

/* a pair of doubtlessly somewhat inefficient functions for packing/unpacking
   words between the PRAERIE bitstream and internal structures. These
   are implemented here so as not to make assumptions about what C library
   support a given device may have. In particular the memcpy operation may 
   or may not be supported on various platforms, as well as the various flavours
   of network byte-swapping operations (ntohl etc.). These functions also 
   ensure internal structures are always word-aligned to the specified word size
   while making no explicit assumptions about what alignment the buffers
   provided actually have. They will offset the beginning of writes to the first
   aligned-word boundary in the internal buffer. Note that it is up to the user
   to make sure enough buffer space is provided to contain the desired input.
   For functions within the interface library this is guaranteed. Maybe someone
   would like to optimise these 2 functions?   
 */

// this function unpacks words from a PRAERIE stream into local structures.
uint32_t copy_PRAERIE_buf_to_local(void* to_buf, void* from_buf, uint32_t num_words, praerie_sizes size)
{
        // we are copying from the PRAERIE stream which we treat as a byte buffer.
        // these variables indicate how the this aligns with the local buffer.
        uint8_t wd_size = 1;
        int8_t alignment_r_shift = 0;
        int8_t alignment_l_shift = 0;
        void* cur_from_pos;

        // The size specified in the function sets the internal type and alignment.
        if (size >= SIZE_16 && size <= SIZE_64)
        {
           wd_size = (0x1 << size); // number of bytes in a word
           // shifts to offset each piece of a word to align them in the 
           // buffers corectly.
           alignment_r_shift = -((intptr_t)from_buf & (wd_size - 1));
           if (alignment_r_shift) alignment_l_shift = wd_size + alignment_r_shift;
        }
        // all other cases do a straight byte copy.
        else size = SIZE_COMPAT;
        // first copy out any misaligned initial bytes
        for (uint8_t byte = 0; byte < alignment_l_shift; byte++)
        {
	    *(((uint8_t*)to_buf)+byte) = *(((uint8_t*)from_buf)+byte);
        }
        // now increment the read pointer to an aligned boundary
        cur_from_pos = (void*)(((uint8_t*)from_buf)+alignment_l_shift);
        // bulk copy in 2 pieces, the upper part of the word,
	word_copy[size](to_buf, cur_from_pos, num_words, 8 * alignment_l_shift);
        // and the lower part. Only need the lower part if it was misaligned,
        // because if it wasn't the first copy will have got entire words at a 
        // time.
        if (alignment_r_shift)
        {
	   // we have already byte-copied the first part of the first word in,
           // so advance the write pointer by 1, 
	   to_buf = (uint8_t*)to_buf + wd_size;
           // and copy 1 less than the total number of words
           word_copy[size](to_buf, cur_from_pos, num_words-1, 8 * alignment_r_shift);
        }
        return num_words*wd_size;
}

// this function packs words from local structures into a PRAERIE stream
uint32_t copy_local_to_PRAERIE_buf(void* to_buf, void* from_buf, uint32_t num_words, praerie_sizes size)
{
        // we are copying from the PRAERIE stream which we treat as a byte buffer.
        // these variables indicate how the this aligns with the local buffer.
        uint8_t wd_size = 1;
        int8_t alignment_r_shift = 0;
        int8_t alignment_l_shift = 0;
        void* cur_to_pos;

        // The size specified in the function sets the internal type and alignment.
        if (size >= SIZE_16 && size <= SIZE_64)
        {
           wd_size = (0x1 << size); // number of bytes in a word
           // shifts to offset each piece of a word to align them in the 
           // buffers corectly.
           alignment_r_shift = -((intptr_t)to_buf & (wd_size - 1));
           if (alignment_r_shift) alignment_l_shift = wd_size + alignment_r_shift;
        }
        // all other cases do a straight byte copy.
        else size = SIZE_COMPAT;
        // first copy out any misaligned initial bytes
        for (uint8_t byte = 0; byte < alignment_l_shift; byte++)
        {
	    *(((uint8_t*)to_buf)+byte) = *(((uint8_t*)from_buf)+byte);
        }
        // now increment the read pointer to an aligned boundary
        cur_to_pos = (void*)(((uint8_t*)to_buf)+alignment_l_shift);
        // bulk copy in 2 pieces, the lower part of the word,
	word_copy[size](cur_to_pos, from_buf, num_words, 8 * alignment_r_shift);
        // and the upper part. Only need the upper part if it was misaligned,
        // because if it wasn't the first copy will have got entire words at a 
        // time.
        if (alignment_l_shift)
        {
	   // we have already byte-copied the first part of the first word in,
           // so advance the read pointer by 1, 
	   from_buf = (uint8_t*)from_buf + wd_size;
           // and copy 1 less than the total number of words
           word_copy[size](cur_to_pos, from_buf, num_words-1, 8 * alignment_l_shift);
        }
        return num_words*wd_size;
}
