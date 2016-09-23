/*
 * eieio-interface.h
 *
 *
 *  SUMMARY
 *    Function interface for EIEIO decode
 *
 *  AUTHOR
 *    Alex Rast (rasta@cs.man.ac.uk)
 *
 *  COPYRIGHT
 *    Copyright (c) Alex Rast and The University of Manchester, 2016.
 *    All rights reserved.
 *    SpiNNaker Project
 *    Advanced Processor Technologies Group
 *    School of Computer Science
 *    The University of Manchester
 *    Manchester M13 9PL, UK
 *
 *  DESCRIPTION
 *
 *  A simple set of basic functions to do the EIEIO decode/encode process.
 *  Used for legacy support in PRAERIE, but equally well could be used to 
 *  implement EIEIO support only for non-PRAERIE devices. 
 *
 *  CREATION DATE
 *    3 Feburary, 2016
 *
 *  HISTORY
 * *  DETAILS
 *    Created on       : 3 February 2016
 *    Version          : $Revision$
 *    Last modified on : $Date$
 *    Last modified by : $Author$
 *    $Id$
 *
 *    $Log$
 *
 */

#ifndef _EIEIO_INTERFACE_H_
#define _EIEIO_INTERFACE_H_

#include <common-typedefs.h>
#include "praerie-typedefs.h"

// brief decodes an EIEIO binary header into the eieio header structure
eieio_hdr_t eieio_decode_hdr(uint16_t binary_hdr);

// brief decodes any prefixes in an EIEIO stream into the header structure
bool eieio_decode_prefixes(void* buf, eieio_hdr_t* hdr);

// brief encodes information in an eieio header structure into a binary
// suitable for insertion into an EIEIO data stream.
bool eieio_encode_hdr(eieio_hdr_t* hdr);

// brief compares 2 EIEIO headers for compatibility of data-types and prefixes
bool eieio_hdr_type_equiv(const eieio_hdr_t* hdr1, const eieio_hdr_t* hdr2);

// brief gets the word size of the address part of an event
inline uint32_t eieio_address_len(const eieio_hdr_t* hdr)
{
       if (hdr->packet_type >= KEY_32_BIT) return sizeof(uint32_t);
       else return sizeof(uint16_t);
}

// brief gets the word size of any timestamp in an event
inline uint32_t eieio_timestamp_len(const eieio_hdr_t* hdr)
{
       if (hdr->payload_as_timestamp && (hdr->packet_type & EIEIO_DTYPE_PY))
       {
	  return eieio_address_len(hdr);
       }
       else return 0;
}

// brief gets the word size of a non-timestamp payload in an event
inline uint32_t eieio_payload_len(const eieio_hdr_t* hdr)
{
       if (!hdr->payload_as_timestamp && (hdr->packet_type & EIEIO_DTYPE_PY))
       {
	  return eieio_address_len(hdr);
       }
       else return 0;
}

// brief gets the byte length of the entire header including prefixes
uint32_t eieio_hdr_len(const eieio_hdr_t* hdr);

// brief finds the prefix portion of an event to be sent over EIEIO
eieio_event_t eieio_extract_prefixes(const eieio_hdr_t* hdr, eieio_event_t event);

// brief encodes an EIEIO header into a (stream) buffer
uint32_t eieio_pkt_encode_hdr(void* buf, eieio_hdr_t* hdr);

// brief encodes an event into an EIEIO (stream) buffer
uint32_t eieio_pkt_encode_event(void* buf, eieio_hdr_t* hdr, eieio_event_t event, bool check_prefixes);

// brief decodes and event from a (stream) buffer
eieio_event_t eieio_pkt_decode_event(void* buf, const eieio_hdr_t* hdr, uint8_t event_num);


#endif
