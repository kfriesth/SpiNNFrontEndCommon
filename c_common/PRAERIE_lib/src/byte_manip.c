#include "byte_manip.h"

/* a pair of doubtlessly somewhat inefficient functions for packing/unpacking
   big-ended words between the PRAERIE bitstream and internal structures. These
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
uint32_t copy_be_to_words(void* to_buf, void* from_buf, uint32_t num_words, praerie_sizes size)
{
        // we are copying from the PRAERIE stream which we treat as a byte buffer.
        uint32_t wd_count = 0;
        uint8_t* cur_from_pos = (uint8_t*)from_buf;
        // The size specified in the function sets the internal type and alignment.
        switch (size)
	{
               case SIZE_16:
               {
		    // Halfword size. Ensure halfword alignment. 
                    uint16_t* to_buf_16 = (uint16_t*)((intptr_t)to_buf + ((2 - ((intptr_t)to_buf & 1)) & 1));
                    // then copy each halfword byte-wise   
                    for (; wd_count < num_words; wd_count++)
                    {
                        *to_buf_16++ = (uint16_t)(*cur_from_pos++) << 8 | (uint16_t)(*cur_from_pos++);
                    }
                    // returning the final byte count
                    return wd_count*sizeof(uint16_t);
               }
	       case SIZE_32:
               {
		    // Word size. Ensure alignment.
                    uint32_t* to_buf_32 = (uint32_t*)((intptr_t)to_buf + ((4 - ((intptr_t)to_buf & 3)) & 3));
                    // with similar processing to halfword case.
                    for (; wd_count < num_words; wd_count++)
                    {
                        *to_buf_32++ = (uint32_t)(*cur_from_pos++) << 24 | (uint32_t)(*cur_from_pos++) << 16 | (uint32_t)(*cur_from_pos++) << 8 | (uint32_t)(*cur_from_pos++);
                    }
                    return wd_count*sizeof(uint32_t);  
               }
	       case SIZE_64:
               {
                    // Doubleword size. Ensure alignment 
                    uint64_t* to_buf_64 = (uint64_t*)((intptr_t)to_buf + ((8 - ((intptr_t)to_buf & 7)) & 7));
                    // and just as before.
                    for (; wd_count < num_words; wd_count++)
                    {
                        *to_buf_64++ = (uint64_t)(*cur_from_pos++) << 56 | (uint64_t)(*cur_from_pos++) << 48 | (uint64_t)(*cur_from_pos++) << 40 | (uint64_t)(*cur_from_pos++) << 32 | (uint64_t)(*cur_from_pos++) << 24 | (uint64_t)(*cur_from_pos++) << 16 | (uint64_t)(*cur_from_pos++) << 8 | (uint64_t)(*cur_from_pos++);
                    }
                    return wd_count*sizeof(uint32_t); 
               }
               default:
               {
		    // default assumes a straight copy of the bytes 
		    uint8_t* to_buf_8 = (uint8_t*)to_buf;
		    for (; wd_count < num_words; wd_count++) *to_buf_8++ = *cur_from_pos++;
                    return wd_count;    
               }
        }
}

// this function packs words from local structures into a PRAERIE stream
uint32_t copy_words_to_be(void* to_buf, void* from_buf, uint32_t num_words, praerie_sizes size)
{
        // The buffer we are packing into is the PRAERIE side byte buffer.
        uint32_t wd_count = 0;
        uint8_t* cur_to_pos = (uint8_t*) to_buf;
        // The size specified in the function sets the internal type and alignment.
        switch (size)
	{
               case SIZE_16:
               {
		    // Halfword size. Ensure halfword alignment.
                    uint16_t* from_buf_16 = (uint16_t*)((intptr_t)from_buf + ((2 - ((intptr_t)from_buf & 1)) & 1));
                    // Copy the bytes into the buffer.    
                    for (; wd_count < num_words; wd_count++)
                    {
		        *cur_to_pos++ = (uint8_t)(*from_buf_16 >> 8);
                        *cur_to_pos++ = (uint8_t)(*from_buf_16++);
                    }
                    // and return the number of bytes packed.
                    return wd_count*sizeof(uint16_t);
               }
	       case SIZE_32:
               {
                    // Word size. Ensure alignment. 
                    uint32_t* from_buf_32 = (uint32_t*)((intptr_t)from_buf + ((4 - ((intptr_t)from_buf & 3)) & 3));
                    // Copy the bytes into the buffer.  
                    for (; wd_count < num_words; wd_count++)
                    {
		        *cur_to_pos++ = (uint8_t)(*from_buf_32 >> 24);
                        *cur_to_pos++ = (uint8_t)(*from_buf_32 >> 16);
                        *cur_to_pos++ = (uint8_t)(*from_buf_32 >> 8);
                        *cur_to_pos++ = (uint8_t)(*from_buf_32++);
                    }
                    // and return the number of bytes packed.
                    return wd_count*sizeof(uint32_t);  
               }
	       case SIZE_64:
               {
                    // Doubleword size. Ensure alignment.
                    uint64_t* from_buf_64 = (uint64_t*)((intptr_t)from_buf + ((8 - ((intptr_t)from_buf & 7)) & 7));
                    // Copy the bytes into the buffer.  
                    for (; wd_count < num_words; wd_count++)
                    {
		        *cur_to_pos++ = (uint8_t)(*from_buf_64 >> 56);
		        *cur_to_pos++ = (uint8_t)(*from_buf_64 >> 48);
		        *cur_to_pos++ = (uint8_t)(*from_buf_64 >> 40);
		        *cur_to_pos++ = (uint8_t)(*from_buf_64 >> 32);
		        *cur_to_pos++ = (uint8_t)(*from_buf_64 >> 24);
		        *cur_to_pos++ = (uint8_t)(*from_buf_64 >> 16);
		        *cur_to_pos++ = (uint8_t)(*from_buf_64 >> 8);
		        *cur_to_pos++ = (uint8_t)(*from_buf_64++);
                    }
                    // and return the number of bytes packed.
                    return wd_count*sizeof(uint32_t); 
               }
               default:
               {
                    // default assumes a straight copy of the bytes. 
                    uint8_t* from_buf_8 = (uint8_t*)from_buf;
		    for (; wd_count < num_words; wd_count++) *cur_to_pos++ = *from_buf_8++;
                    return wd_count;    
               }
        }
}
