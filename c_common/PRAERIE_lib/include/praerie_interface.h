/*
 * praerie-interface.h
 *
 *
 *  SUMMARY
 *    Function interface for PRAERIE decode
 *
 *  AUTHOR
 *    Alex Rast (rasta@cs.man.ac.uk)
 *
 *  COPYRIGHT
 *    Copyright (c) Alex Rast and The University of Manchester, 2015.
 *    All rights reserved.
 *    SpiNNaker Project
 *    Advanced Processor Technologies Group
 *    School of Computer Science
 *    The University of Manchester
 *    Manchester M13 9PL, UK
 *
 *  DESCRIPTION
 *
 *
 *  CREATION DATE
 *    4 October, 2015
 *
 *  HISTORY
 * *  DETAILS
 *    Created on       : 4 October 2015
 *    Version          : $Revision$
 *    Last modified on : $Date$
 *    Last modified by : $Author$
 *    $Id$
 *
 *    $Log$
 *
 */

#ifndef __PRAERIE_INTERFACE_H__
#define __PRAERIE_INTERFACE_H__

#include <common-typedefs.h>
#include "praerie-typedefs.h"
#include "eieio_interface.h"

inline bool praerie_pkt_has_timestamp(praerie_hdr_t* hdr)
{
       return (hdr->timestamp_type != TSTP_TYPE_NONE) || ((hdr->timestamp_type == TSTP_TYPE_LEGACY) && (hdr->legacy_hdr.payload_as_timestamp && (hdr->legacy_hdr.payload_apply_prefix || (hdr->legacy_hdr.packet_type == KEY_PAYLOAD_16_BIT || hdr->legacy_hdr.packet_type == KEY_PAYLOAD_32_BIT))));
}

inline bool praerie_pkt_has_payload(praerie_hdr_t* hdr)
{
       return (hdr->timestamp_type != TSTP_TYPE_NONE) || ((hdr->timestamp_type == TSTP_TYPE_LEGACY) && (!hdr->legacy_hdr.payload_as_timestamp && (hdr->legacy_hdr.payload_apply_prefix || (hdr->legacy_hdr.packet_type == KEY_PAYLOAD_16_BIT || hdr->legacy_hdr.packet_type == KEY_PAYLOAD_32_BIT))));
} 
   
// decode the basic binary header into a structure
praerie_hdr_t praerie_decode_hdr(uint32_t binary_hdr);

// decode a command into a structure
praerie_cmd_t praerie_decode_cmd(uint32_t command);

// decode any prefixes from a binary stream into a header
bool praerie_decode_prefixes(void* buf, praerie_hdr_t* hdr);

// encode the header into binary and place in the decoded header structure
bool praerie_encode_hdr(praerie_hdr_t* hdr);

// encode a command into binary and place in the decoded command structure
bool praerie_encode_cmd(praerie_cmd_t* cmd);

// get the size in bytes for each of: address
size_t praerie_address_len(const praerie_hdr_t* hdr);

// payload,
size_t praerie_payload_len(const praerie_hdr_t* hdr);

// and timestamp
size_t praerie_timestamp_len(const praerie_hdr_t* hdr);

// get the number of bytes occupied by the praerie header. Used for buffer offset.
uint32_t praerie_hdr_len(const praerie_hdr_t* hdr);

// get the byte size of each event for the current header
inline uint32_t praerie_event_len(const praerie_hdr_t* hdr) 
       {return praerie_address_len(hdr)+praerie_payload_len(hdr)+praerie_timestamp_len(hdr);}

// gets prefixes from events that have them embedded.
praerie_event_t praerie_extract_prefixes(const praerie_hdr_t* hdr, praerie_event_t event);

// compares headers for compatible decode
bool praerie_hdr_type_equiv(const praerie_hdr_t* hdr1, const praerie_hdr_t* hdr2);

// compare commands for equivalence - do they do the same thing?
bool praerie_cmd_equiv(const praerie_cmd_t* cmd1, const praerie_cmd_t* cmd2);

// compares binary headers for compatible decode
inline bool praerie_hdr_binary_equiv(const uint32_t hdr1, const uint32_t hdr2)
       {return ((hdr1 ^ hdr2) & (PRAERIE_MASK_PKT_DECODE | PRAERIE_MASK_EXT)) == 0;}

// binary encodes the header including prefixes into a buffer
uint32_t praerie_pkt_encode_hdr(void* buf, praerie_hdr_t* hdr);

// binary encodes an event into a buffer with header present
uint32_t praerie_pkt_encode_event(void* buf, praerie_hdr_t* hdr, praerie_event_t event, bool check_prefixes);

// binary encodes a block of events into a buffer
uint32_t praerie_pkt_encode_block(void* buf,  praerie_hdr_t* hdr, praerie_event_t* event_buf, bool check_prefixes, uint8_t num_events);

// binary encodes a complete packet from header and a list of events
inline uint32_t praerie_pkt_encode(void* buf, praerie_hdr_t* hdr, praerie_event_t* event_buf, bool check_prefixes, uint8_t num_events)
       {return praerie_pkt_encode_hdr(buf, hdr) + praerie_pkt_encode_block(buf, hdr, event_buf, check_prefixes, num_events);}

// decodes an entire header including prefixes from a buffer
praerie_hdr_t praerie_pkt_decode_hdr(void* buf);

// extracts a specific event from a PRAERIE buffer
praerie_event_t praerie_pkt_decode_event(void* buf, const praerie_hdr_t* hdr, uint8_t event_num);

// extracts a block of events from a PRAERIE buffer into an event buffer
uint8_t praerie_pkt_decode_block(void* buf, praerie_hdr_t* hdr, praerie_event_t* event_buf, uint8_t num_events, uint8_t start_event);

// extracts an entire packet including header and events into respective buffers
bool praerie_pkt_decode(void* buf, praerie_hdr_t* hdr, praerie_event_t* event_buf);

// get the praerie configuration (interfaces enabled/disabled, supported sizes etc.)
praerie_if_cfg praerie_get_config(void);

// set all or part of the praerie configuration. Not all options need be supported.
bool praerie_set_config(praerie_if_cfg* praerie_config);

#endif
