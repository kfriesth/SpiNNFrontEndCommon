#include <common-typedefs.h>
#include <debug.h>
#include <simulation.h>
#include <spin1_api.h>
#include <sark.h>
#include <data_specification.h>
#include "PRAERIE-proc-typedefs.h"
#include <praerie_interface.h>
#include <eieio_interface.h>
#include "PRAERIE_codec.h"
#ifdef INSTRUMENTATION
#include "instrumentation.h"
#endif

// the cam for looking up a header based on MC key. 
static mc_cam_t hdr_lookup_cam[NUM_OUTGOING_PRAERIE_TYPES];

// timestamp in Timer.c
extern timer_t local_timestamp;

// blank (constant) PRAERIE header defined in PRAERIE_processor.c,
// using external linkage rather than definition in the header because
// it's a composite structure.
extern const praerie_hdr_t aer_hdr_null;

// the basic function to encode a praerie event from a received MC spike.
praerie_event_t aer_spike_encode(spinn_event_t spike)
{
              praerie_event_t event;
              event.address = (praerie_addr_t)spike.key;
              event.payload = (praerie_pyld_t)spike.payload;
              event.timestamp = (praerie_tstp_t)local_timestamp;
              return event;
}

// function to encode an mc event into an SDP message for sending.
void sdp_aer_pkt_encode(sdp_msg_t* sdp_msg, spinn_event_t spike, praerie_hdr_t* aer_hdr)
{
     // offset the insert position by the amount already filled in the sdp message. 
     uint8_t* data_insert_pos = sdp_msg->data+sdp_msg->length-(SDP_HDR_SCP_LEN);
     // encode the spike into an aer event 
     praerie_event_t praerie_spike = aer_spike_encode(spike);
     // then bundle into a packet
     // may need to fix next line - this will silently ignore the spike if 
     // prefixes don't match the existing packet being built.
     uint32_t praerie_len = praerie_pkt_encode_event(data_insert_pos, aer_hdr, praerie_spike, CHECK_PREFIXES);
     // update the size of the SDP message
     sdp_msg->length += praerie_len;
#ifdef INSTRUMENTATION
     // record if desired
     instrument_event_recording(RECORD_DIR_OUT, praerie_pkt_has_payload(aer_hdr), &praerie_spike, sizeof(praerie_event_t));
#endif
     // if this fills the message, reset the aer header for the next packet
     if (SCP_HDR_LEN+SDP_BUF_SIZE-(praerie_hdr_len(aer_hdr) + aer_hdr->count*praerie_len) < praerie_len) *aer_hdr = aer_hdr_null;     
}

// the packet decoder. Goes through each SDP buffer one by one and decodes
// the event at the appropriate position (the offset in 
// packet_properties->remaining_spikes). Returns the decoded event.
praerie_event_t sdp_aer_pkt_decode(praerie_hdr_t* hdr, sdp_msg_buf_t* sdp_buf, pkt_props_t* packet_properties)
{
       // get the starting values in the SDP buffer
       uint8_t spike_offset = hdr->count-packet_properties->remaining_spikes;
       uint8_t* data_buf = (uint8_t*)sdp_buf->msg->cmd_rc;
       // then decode the event.
       praerie_event_t event = praerie_pkt_decode_event(data_buf, hdr, spike_offset);
       // set the timestamp if there wasn't one already there.
       if (!packet_properties->with_timestamp) event.timestamp = local_timestamp;
       // and return the count.
       return event;
}

/* gets a PRAERIE header by lookup from a table associating MC keys with fixed
   headers. The number of entries in the CAM is expected to be small - no more
   than about 16. As a result sophisticated lookup methods like hash tables or
   even binary search would be ridiculous overkill. A linear search suffices.
   If in future versions it is decided that big tables are desirable then this
   function should probably use a more efficient search algorithm.
 */ 
bool mc_praerie_hdr_lookup(praerie_hdr_t* hdr, spinn_event_t spike)
{
     // only need the key to match the table 
     uint32_t key = spike.key;
     // go through the table using a linear search
     for (uint32_t entry = 0; entry < NUM_OUTGOING_PRAERIE_TYPES; entry++)
     {
         // key is maskable: apply the mask then try to match the key. 
         if ((key & hdr_lookup_cam[entry].mask) == hdr_lookup_cam[entry].match_key)
         {
            // hit. Copy the PRAERIE header block   
            *hdr = hdr_lookup_cam[entry].hdr;
            // terminate early on hit
            return true;
         } 
     }
     // miss. Packet not supported. (It will be ignored and dropped)
     *hdr = aer_hdr_null;
     return false;
}

bool codec_init(address_t mc_LUT_init)
{
     mc_cam_t* new_entry = (mc_cam_t*) mc_LUT_init;
     for (uint8_t entry = 0; entry < NUM_OUTGOING_PRAERIE_TYPES; entry++)
     {
         if ((~new_entry->mask & MC_MATCH_END) == MC_MATCH_END) return true; 
         if (~new_entry->mask & new_entry->match_key) return false;
         else hdr_lookup_cam[entry] = *new_entry;
         new_entry++;
     }
     return true;
}


