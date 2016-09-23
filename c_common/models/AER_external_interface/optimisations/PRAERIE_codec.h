#ifndef __PRAERIE_CODEC_H__
#define __PRAERIE_CODEC_H__

#include <common-typedefs.h>
#include <spin1_api.h>
#include "PRAERIE-proc-typedefs.h"
#include <praerie-typedefs.h>

// function to decode the PRAERIE header from an SDP message.
// at this point this is only a wrapper for the general praerie interface.
// however if endianness means the sdp buffer information doesn't preserve
// byte ordering from cmd_rc forward some juggling may be necessary here.
inline praerie_hdr_t sdp_aer_hdr_decode(sdp_msg_buf_t* sdp_buf)
{ return praerie_pkt_decode_hdr(&(sdp_buf->msg->cmd_rc)); }

// the basic function to encode a praerie event from a received MC spike.
praerie_event_t aer_spike_encode(spinn_event_t spike);
// function to encode an mc event into an SDP message for sending.
void sdp_aer_pkt_encode(sdp_msg_t* sdp_msg, spinn_event_t spike, praerie_hdr_t* aer_hdr);
// function to decode an event from an SDP-encapsulated PRAERIE buffer
praerie_event_t sdp_aer_pkt_decode(praerie_hdr_t* hdr, sdp_msg_buf_t* sdp_buf, pkt_props_t* packet_properties);
// look up a PRAERIE header from an associative table.
bool mc_praerie_hdr_lookup(praerie_hdr_t* hdr, spinn_event_t spike);
// initialise the PRAERIE header MC lookup table
bool codec_init(address_t mc_LUT_init);

#endif
