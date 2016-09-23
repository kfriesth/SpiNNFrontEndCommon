#ifndef __SDP_RECEIVE_PROCESS_H__
#define __SDP_RECEIVE_PROCESS_H__

#include <common-typedefs.h>
#include <spin1_api.h>
#include <praerie-typedefs.h>
#include "PRAERIE-proc-typedefs.h"


// release a buffer for use. All that needs to be done is label the buffer.
inline void free_sdp_msg_buf(sdp_msg_buf_t* msg) {if (msg != NULL) msg->tag = DMA_TAG_FREE;}

// simple function to do basic message checking when one arrives
// may need byte-swapping
inline uchar get_aer_seq_from_sdp(sdp_msg_t* msg)
{
       return (msg->cmd_rc & EIEIO_MASK_TAG) ? (uchar) ((msg->seq & PRAERIE_MASK_SEQ) >> PRAERIE_SEQ_POS) : 0;
}

sdp_msg_buf_t* sdp_buf_remove(void);
bool SDP_init(uint sdp_pkt_recv_port);
void _sdp_packet_received_callback(uint mailbox, uint dest_port);

#endif
