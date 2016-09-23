/*! \file
 *
 *  \brief EIEIO interface Header File
 *
 *  DESCRIPTION
 *    Specifies functions that can be used to read and write EIEIO data
 *    and command packets
 *
 */

#ifndef _EIEIO_SPINN_INTERFACE_H_
#define _EIEIO_SPINN_INTERFACE_H_

#include "common-typedefs.h"
#include "aerie-typedefs.h"

//! \brief takes a memory address and translates the next 2 bytes into
eieio_hdr_t eieio_interface_get_eieio_header(
    address_t header_start_address);

//! \brief packs the header information from a structure into the 2-byte physical header 
void eieio_interface_pack_eieio_header(
     address_t header_start_address, eieio_hdr_t header_info);

//! \brief computes expected size of a received packet
uint16_t calculate_eieio_packet_size(eieio_msg_t eieio_msg_ptr);

//! \brief prints out the received packet
void print_packet(eieio_msg_t eieio_msg_ptr);

//!  \brief raises an error and prints out the offending packet
void signal_software_error(eieio_msg_t eieio_msg_ptr, uint16_t length);

//!  \brief gives the currently setup transfer parameters of the EIEIO interface
bool read_parameters(address_t region_address);

//!  \brief record the number of lost packets
void record_provenance_data(void);

//! \brief finds available SDRAM space for eieio packets
uint32_t get_sdram_buffer_space_available(void);

//! \brief determines if there are still packets to process in the buffer
bool is_eieio_packet_in_buffer(void);

//! \brief request space in SDRAM
void send_buffer_request_pkt(void);

//! \brief inserts a packet into the SDRAM buffer
bool add_eieio_packet_to_sdram(eieio_msg_t eieio_msg_ptr, uint32_t length);

//! \brief get the assumed current time value from an eieio packet
uint32_t extract_time_from_eieio_msg(eieio_msg_t eieio_msg_ptr);

//! \brief the core input function that processes received data packets 
bool eieio_data_parse_packet(eieio_msg_t eieio_msg_ptr, uint32_t length);

//! \brief the core input function that processes received command packets
bool eieio_commmand_parse_packet(eieio_msg_t eieio_msg_ptr, uint16_t length);

//! \brief top-level EIEIO packet handler
void fetch_and_process_packet();

//! \brief top-level EIEIO packet sender
void flush_events(void);

//! \brief set up the interface at the beginning of the simulation
bool initialize(uint32_t *timer_period);

//! \brief configure the basic SDP message information for outgoing packets
bool configure_sdp_msg(void);

//! brief top-level SpiNNaker packet handler
static inline void service_incoming_event(uint key, uint payload, bool has_payload)
{ if (has_payload) process_incoming_event(key) else process_incoming_event_payload(key, payload) }

#endif
