#ifndef __BYTE_MANIP_H__
#define __BYTE_MANIP_H__

#include <common-typedefs.h>
#include "praerie-typedefs.h"

// this function unpacks words from a PRAERIE stream into local structures.
uint32_t copy_PRAERIE_buf_to_local(void* to_buf, void* from_buf, uint32_t num_words, praerie_sizes size);

// this function packs words from local structures into a PRAERIE stream
uint32_t copy_local_to_PRAERIE_buf(void* to_buf, void* from_buf, uint32_t num_words, praerie_sizes size);

#endif
