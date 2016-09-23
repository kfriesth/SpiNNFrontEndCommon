#include <common-typedefs.h>
#include <debug.h>
#include <simulation.h>
#include <spin1_api.h>
#include <sark.h>
#include <data_specification.h>
#include <praerie-typedefs.h>
#include <praerie_interface.h>
#include <eieio_interface.h>
#include "PRAERIE-proc-typedefs.h"
#include "Timer.h"
#include "Command_processor.h"

#ifdef INSTRUMENTATION
#include "instrumentation.h"
#endif

// command state variables
cmd_IF_state_t cmd_state;
seq_entry_t sequence_counter[MAX_SUPPORTED_DEVICES];

#ifdef RECEIVER
// variables from DMA_recieve_buffer_process.c
extern spike_ll_t* sdram_w_buf[2]; // current position in SDRAM to write to via DMA
extern spike_ll_t* sdram_r_buf[2]; // current position in SDRAM to read from via DMA
extern bool sdram_has_buffers[2]; // flags indicating SDRAM has some buffers stored
#endif

// look up in the sequence counter the current
// sequence, based on a provided tag for the device.
// this function could be in the top-level module.
static seq_entry_t* get_seq_from_tag(uint8_t tag)
{
       for (seq_entry_t* seq_entry = sequence_counter; seq_entry <= &sequence_counter[MAX_SUPPORTED_DEVICES]; seq_entry++) if (seq_entry->req_enable && seq_entry->tag == tag) return seq_entry;
       return NULL; 
}

static void init_seq_entries(uint8_t *tags)
{
  for (uint8_t entry = 0; entry < MAX_SUPPORTED_DEVICES; entry++)
       {
	   // no tag indicates end of table - terminate early.
           if (tags[entry] == TAG_NONE) return;
           sequence_counter[entry].tag = tags[entry];
           sequence_counter[entry].req_enable = true;
           sequence_counter[entry].last_seq = 0;
           for (uint8_t word = 0; word < 8; word++)
           {
	       sequence_counter[entry].seq_wd[word] = 0;
           }
       }
}

// sub-handlers for the various command classes

// This handles defined PRAERIE commands
static bool handle_standard_praerie_command(praerie_hdr_t* pkt_hdr, sdp_msg_buf_t* sdp_buf)
{
     switch ((praerie_command_messages)pkt_hdr->command.cmd_num)
     {
            case PRAERIE_CMD_NULL_COMMAND:
	    return true; 
            case PRAERIE_CMD_REAL_TIME:
	    if (pkt_hdr->command.dir == CMD_DIR_WRITE)
            return set_real_time(sdp_buf->msg->arg1);
            else
            return get_real_time(pkt_hdr, sdp_buf);
            case PRAERIE_CMD_RESET:
            case PRAERIE_CMD_INFO:
            case PRAERIE_CMD_PAUSE_RESUME:
            if (pkt_hdr->command.dir == CMD_DIR_WRITE)
	    {
               if (sdp_buf->msg->arg1)
               {
		  cmd_state.interface_paused = STATE_PAUSE;
                  return true;
               }
               else
               {
		  cmd_state.interface_paused = STATE_RESUME;
                  _cmd_resume_callback();
               }
            }
            case PRAERIE_CMD_RESERVED:
            return false;
            default:
            return false;
     }
}

// this handles SpiNNaker-specific PRAERIE commands (none defined at present
// so this is just a stub)
static bool handle_SpiNN_praerie_command(praerie_hdr_t* pkt_hdr, sdp_msg_buf_t* sdp_buf)
{
     use(pkt_hdr);
     use(sdp_buf);
     return false;
}

// this handles the legacy SpiNNaker-specific EIEIO commands
static bool handle_eieio_command(uint16_t command, sdp_msg_buf_t* sdp_buf)
{
     if (command >= RSVD_EXP) return false; 
     switch (command)
     {
            case NULL_CMD:
	    return true;
            case DB_CONF:
	    return false;
            case EVT_PAD:
            return false;
            case EVT_STOP_CMDS:
            cmd_state.interface_paused = STATE_PAUSE;
            return true;
#ifdef RECEIVER
            case STOP_SEND_REQS:
            // note that this is correctly an assignment. We do not want
            // == here
            return !(cmd_state.buffer_req_en = REQ_NONE);
            case START_SEND_REQS:
            // same here too. 
            return (cmd_state.buffer_req_en = REQ_NO_PYLD);
            case SPINN_REQ_BUFS:
            return sdram_buffer_request(sdp_buf, ((spinn_rq_buf_t*)(&sdp_buf->msg->cmd_rc))->with_payload);
            case HOST_SEND_SEQ_DAT:
            {
                 // get the current sequence recorded for this tag
	         seq_entry_t* old_seq = get_seq_from_tag(sdp_buf->msg->tag);
                 // no sequence has been recorded. Probably forgot to turn it on.
                 // Can't do anything with the packet.
	         if (old_seq == NULL) return false; 
                 // record the new sequence number received; this is the main
                 // functionality of HOST_SEND_SEQUENCED_DATA.
                 if (set_seq_with_tag(sdp_buf->msg->seq, sdp_buf->msg->tag))
                 {
	            // HOST_SEND_SEQUENCED_DATA merely wraps an EIEIO packet in an
                    // additional quasi-reliable transport. By pointer juggling we
                    // can strip off the transport while returning the packet to the
                    // general PRAERIE/EIEIO packet-processing function.
                    // header part needs to be copied across. 
                    sdp_msg_t* translated_buf = (sdp_msg_t*)(((intptr_t)sdp_buf->msg)+4);
                    uchar copy_buf[SDP_MSG_HDR_LEN+SDP_HDR_LEN];
                    spin1_memcpy(copy_buf, sdp_buf->msg, SDP_MSG_HDR_LEN+SDP_HDR_LEN);
                    spin1_memcpy(translated_buf, copy_buf, SDP_MSG_HDR_LEN + SDP_HDR_LEN);
                    // then the length adjusted
                    translated_buf->length -= 4;
                    // and finally reset the pointer in the structure.
                    sdp_buf->msg = translated_buf;
                    // return true only if the next expected seq was returned.
                    // otherwise buffer the sequence (no harm in that) but 
                    // report an error.
                    return sdp_buf->msg->seq == (old_seq->last_seq + 1);
                 }
                 return false; // error: no such tracked tag. (Could report) 
            }
#endif
            case SPINN_REQ_READ_DAT:
	    return false;
            case HOST_DAT_READ:
	    return false;
            default:
            return false;
     } 
}

/* INTERFACE FUNCTIONS - cannot be static */

// use the tag provided to set the current sequence number for it
bool set_seq_with_tag(uint8_t seq, uint8_t tag)
{
       for (seq_entry_t* seq_entry = sequence_counter; seq_entry <= &sequence_counter[MAX_SUPPORTED_DEVICES]; seq_entry++)
       {
           if (seq_entry->req_enable && seq_entry->tag == tag)
           {
	      // reset on wraparound - needs further thought. This naive 
              // approach may work most of the time but it will fail in some
              // cases when sequence numbers arrive badly out-of-order.
              uint8_t seq_wrap_bit = seq & SEQ_BIT_MASK;
              /*
                if seq is beyond last_seq, before last_seq+AER_SEQ_HORIZON, and
                beyond first_seq (i.e. it is interpreted as in the future, and traversing
                first_seq in its advancement from last_seq), advance last_seq and
                reset the seq received bitmaps as necessary. 
	       */
              if (((seq_entry->seq_wd[(seq & SEQ_WORD_MASK) >> SEQ_WORD_SHIFT]) & (0x1 << seq_wrap_bit)) ||
		  ((seq - seq_entry->last_seq) < AER_SEQ_HORIZON)) 
              {
		 uint8_t last_horizon = seq_entry->last_seq + AER_SEQ_HORIZON;
                 uint8_t clr_offset = seq - last_horizon;
                 uint8_t seq_clr_word = (last_horizon & SEQ_WORD_MASK) >> SEQ_WORD_SHIFT;
                 uint8_t seq_clr_bit = 32 - (last_horizon & SEQ_BIT_MASK);
                 if (clr_offset < seq_clr_bit)
		    seq_entry->seq_wd[seq_clr_word] &= ~((0xFFFFFFF >> (32 - clr_offset)) << (32 - seq_clr_bit));
                 else
                 {
                    seq_entry->seq_wd[seq_clr_word++] &= (0xFFFFFFF >> seq_clr_bit);
                    clr_offset -= seq_clr_bit;
		    while (clr_offset > seq_wrap_bit)
		    {
		          seq_entry->seq_wd[seq_clr_word++] = 0;
                          clr_offset -= 32;
                    }
                    seq_entry->seq_wd[seq_clr_word] &= 0xFFFFFFFF << seq_wrap_bit;
                 }                 
              }
	      seq_entry->last_seq = seq;
	      seq_entry->seq_wd[seq & SEQ_WORD_MASK] |= (0x1 << (seq & SEQ_BIT_MASK));
              return true;
           }
       }
       return false; 
}

// issue an SDP reply message to a device-based request
bool send_read_response(sdp_msg_t* sdp_msg, ushort length)
{
     uchar src_port = sdp_msg->dest_port;
     uchar src_addr = sdp_msg->dest_addr;
     sdp_msg->length = length;
     sdp_msg->flags = SDP_FLAGS_NOACK;
     sdp_msg->dest_port = sdp_msg->srce_port;
     sdp_msg->dest_addr = sdp_msg->srce_addr;
     sdp_msg->srce_port = src_port;
     sdp_msg->srce_addr = src_addr;
     for (uint i = 0; i < SDP_SEND_RETRIES; i++)
     {
         if (spin1_send_sdp_msg(sdp_msg, SDP_SEND_TIMEOUT))
            return true;
         else
	    spin1_delay_us(SDP_RETRY_DELAY);
     }
     return false;
}   

#ifdef RECEIVER
// servicing for the buffer request command
bool sdram_buffer_request(sdp_msg_buf_t* sdp_msg, uint8_t with_payload)
{
     seq_entry_t* seq_lookup;
     spinn_rq_buf_t *const req_data = (spinn_rq_buf_t*)&(sdp_msg->msg->cmd_rc);

     req_data->eieio_cmd_id = EIEIO_MASK_F | SPINN_REQ_BUFS;
     req_data->region = NO_PAYLOAD_BUFFER_REGION + with_payload;
     // space available computations had been multiplied by sizeof(spike_ll_t) but
     // with new definition of a spike_ll_t this should be unnecessary (Verify!)
     if ((sdram_w_buf[with_payload] < sdram_r_buf[with_payload]) || !sdram_has_buffers[with_payload])
        req_data->space_available = ((sdram_w_buf[with_payload] + NUM_SDRAM_BLOCKS) - sdram_r_buf[with_payload]);
     else req_data->space_available = (sdram_w_buf[with_payload] - sdram_r_buf[with_payload]);
     if (sdp_msg->mailbox == NULL)
     {   
        sdp_msg->msg->tag = cmd_state.default_tag;
        sdp_msg->msg->srce_port = PORT_ETH;
        sdp_msg->msg->srce_addr = SDP_DEST_ADDR_ROOT;
     }
     sdp_msg->msg->dest_port = (1 << PORT_SHIFT) | (req_data->processor = spin1_get_core_id());
     req_data->chip_id = sdp_msg->msg->dest_addr = spin1_get_chip_id(); 
     if ((seq_lookup = get_seq_from_tag(sdp_msg->msg->tag)) == NULL)
        req_data->with_payload = DONT_CARE_PAYLOAD;
     else
     {
        req_data->seq = (uint8_t)seq_lookup->last_seq;
        for (uint8_t word = 0; word < 8; word++)
            req_data->seq_bitmap[word] = seq_lookup->seq_wd[word];
        req_data->with_payload = with_payload;
     } 
     if (!send_read_response(sdp_msg->msg, SDP_HDR_LEN + sizeof(spinn_rq_buf_t)))
     { 
#ifdef INSTRUMENTATION
        incr_prov(ABANDONED_BUF_REQS+with_payload, 1);
#endif
        return false;
     }
     else return true;
}
#endif

// top-level command handler. Usually invoked from the DMA receive process.
bool handle_command(praerie_hdr_t* pkt_hdr, sdp_msg_buf_t* sdp_buf)
{
     if (pkt_hdr->command.word_size == SIZE_NONE) return false;
     if (pkt_hdr->command.general_cmd == CMD_TYPE_LEGACY) return handle_eieio_command(pkt_hdr->command.legacy_cmd_ID, sdp_buf);
     if (pkt_hdr->command.general_cmd == CMD_TYPE_GENERAL) return handle_standard_praerie_command(pkt_hdr, sdp_buf);
     return handle_SpiNN_praerie_command(pkt_hdr, sdp_buf);
}

// initialise the command interface
void CMD_if_init(uint32_t* sys_config, uint8_t* seq_tags)
{
     cmd_state.interface_paused = STATE_RUN;
#ifdef RECEIVER
     cmd_state.buffer_req_en = sys_config[BUFFER_REQ_EN];
     cmd_state.sdram_space_p = sys_config[WITH_PAYLOAD_BLOCKS]*sizeof(spike_ll_t);
     cmd_state.sdram_space_n = sys_config[NO_PAYLOAD_BLOCKS]*sizeof(spike_ll_t);
     cmd_state.sdram_min_req_space = sys_config[SDRAM_MIN_REQ_BLOCKS]*sizeof(spike_ll_t);
#endif
     cmd_state.default_tag = sys_config[DEFAULT_TAG];
     cmd_state.infinite_run = sys_config[INFINITE_RUN];
     init_seq_entries(seq_tags);
}

/* CALLBACK FUNCTIONS - cannot be static */

// callback for new pause/resume functionality
void _cmd_resume_callback()
{
     // pause/resume could be triggered in one of 2 ways: 
     // first, a PRAERIE PAUSE/RESUME command. This sets
     // the STATE_RESUME state in inteface_paused so
     // we resume manually.
     if (cmd_state.interface_paused == STATE_RESUME)
     {
        cmd_state.interface_paused = STATE_RUN;
        spin1_resume(SYNC_WAIT);
     }
     // alternatively via the callback associated with simulation.h when 
     // the simulation finishes its current duration and goes into pause.
     // simulation.c takes care of actually resuming things so we just need
     // to set the pause state to STATE_RUN.
     else cmd_state.interface_paused = STATE_RUN;       
}
