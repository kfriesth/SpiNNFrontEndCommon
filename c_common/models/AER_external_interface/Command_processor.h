#ifndef __COMMAND_PROCESSOR_H__
#define __COMMAND_PROCESSOR_H__

#include <common-typedefs.h>
#include <spin1_api.h>
#include <praerie-typedefs.h>
#include "PRAERIE-proc-typedefs.h"

#ifdef RECEIVER
// servicing for the buffer request command
bool sdram_buffer_request(sdp_msg_buf_t* sdp_msg, uint8_t with_payload);
#endif
// use the tag provided to set the current sequence number for it
// This function, the inverse of get seq_from_tag, is considerably
// more elaborate so inlining is probably costly.
bool set_seq_with_tag(uint8_t seq, uint8_t tag);
// issue an SDP reply message to a device-based request
bool send_read_response(sdp_msg_t* sdp_msg, ushort length);
// top-level command handler. Usually invoked from the DMA receive process.
bool handle_command(praerie_hdr_t* pkt_hdr, sdp_msg_buf_t* sdp_buf);
// initialise the command interface
void CMD_if_init(uint32_t* sys_config, uint8_t* seq_tags);
// callback for new pause/resume functionality
void _cmd_resume_callback(); 

#endif
