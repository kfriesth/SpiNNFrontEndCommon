#ifndef __DMA_RECEIVE_BUFFER_PROCESS_H__
#define __DMA_RECEIVE_BUFFER_PROCESS_H__

#include <common-typedefs.h>
#include <spin1_api.h>
#include <praerie-typedefs.h>
#include "../../../neural_modelling/src/common/neuron-typedefs.h"
#include "PRAERIE-proc-typedefs.h"

// number of free DMA write/readback buffers
uint8_t dma_free_bufs(void); 
// number of write/readback buffers actively engaged in DMA transfers
// (or definitely about to be so) 
uint8_t dma_committed_pipe_bufs(void);
// send all buffered spikes for the current timestamp 
void send_old_spikes(void);
// initialises DMA buffers, states.
bool DMA_init(uint32_t no_payload_blk_count, uint32_t with_payload_blk_count);
// main callback for the DMA event
void _dma_done_callback(uint transfer_ID, uint tag);
// a pair of functions to transfer data manually if DMA
// is not available
void _initiate_manual_write(intptr_t src, intptr_t dst);
void _initiate_manual_read(intptr_t src, intptr_t dst);

#endif
