/*
 * aerie-proc-typedefs.h
 *
 *
 *  SUMMARY
 *    Definitions of useful types for AERIE processes
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
 *    27 November, 2015
 *
 *  HISTORY
 * *  DETAILS
 *    Created on       : 27 November 2015
 *    Version          : $Revision$
 *    Last modified on : $Date$
 *    Last modified by : $Author$
 *    $Id$
 *
 *    $Log$
 *
 */

#ifndef __PRAERIE_PROC_TYPEDEFS_H__
#define __PRAERIE_PROC_TYPEDEFS_H__

#include <common-typedefs.h>
#include <praerie-typedefs.h>
#include "PRAERIE-consts.h"

/* spike types. These are slightly different from the implementation in
   neuron-typedefs.h, essentially because the spinn_event_t type is always a 
   structure. We do not attempt to define at compile time whether spikes will
   or will not have payloads.
*/
typedef struct spinn_event_t {
        uint32_t key;
        uint32_t payload;
} spinn_event_t;

typedef enum pause_state_t {
        STATE_RESUME = -1,
        STATE_RUN = 0,
        STATE_PAUSE = 1
} pause_state_t;

typedef enum buf_req_en_t {
        REQ_NONE = 0,
        REQ_NO_PYLD = 1,
        REQ_WITH_PYLD = 2,
        REQ_BOTH = 3
} buf_req_en_t;

typedef enum sys_config_t {
        N_PYLD_BLKS = NO_PAYLOAD_BLOCKS,    
        W_PYLD_BLKS = WITH_PAYLOAD_BLOCKS, 
        SEND_BUF_SZ = SEND_BUFFER_SIZE,     
        STIM_ONSET = STIMULUS_ONSET_TIME,  
        SIM_TIME = SIMULATION_TIME,      
        TICK_PRD = TICK_PERIOD,          
        INF_RUN = INFINITE_RUN,         
        DFLT_TAG = DEFAULT_TAG,          
        SDP_R_PORT = SDP_RECV_SIM_PORT,    
        BUF_RQ_EN = BUFFER_REQ_EN,        
        BUF_RQ_INTV = BUF_REQ_INTERVAL,     
        SDRAM_REQ_BLKS = SDRAM_MIN_REQ_BLOCKS, 
        REC_FLGS = RECORDING_FLAGS      
} sys_config_t;

typedef struct data_spec_layout_t {
        address_t data_addr;
        address_t regions[NUM_DATA_SPEC_REGIONS];
} data_spec_layout_t;

typedef struct seq_entry_t {
    uint8_t tag;
    bool req_enable;
    uint8_t last_seq;
    uint32_t seq_wd[8];
} seq_entry_t;

// operator type for timestamp comparisons (should this use uint32_t)?
typedef bool (*t_comp) (timer_t, timer_t);

//typedef struct spike_t {
//    key_t key;
//    payload_t payload;
//} spike_t;

// buffer structure for the SDP message buffer. Note that this needs both a
// pointer to the local store for the buffer and the local store itself, because
// for especially HOST_SEND_SEQUENCED_DATA we may want to juggle the effective
// start position of the message without actually copying the whole thing into
// a new container. 
typedef struct sdp_msg_buf_t {
    uint tag;      // Tag indicates current status: free, loading, queued, active.
    uint transfer_ID; // DMA TID, used if buffer DMA'd in. Otherwise set to 0.
    sdp_msg_t* mailbox; // mailbox where the SDP message was copied from. 
    sdp_msg_t* msg; // local buffer where the SDP message was copied to.
    sdp_msg_t sdp_msg; // physical buffer for the message.
} sdp_msg_buf_t;

// linked list structure for the SDP message buffer
typedef struct sdp_ll_t {
    uchar seq;             // PRAERIE sequence number
    uchar tag;             // SDP tag
    sdp_msg_buf_t* msg;    // associated message buffer
    struct sdp_ll_t* next; // pointer to next element
} sdp_ll_t;


/* more complex pair of structures for a linked-list-like reorder
   buffer. This is used to be able to insert and remove events at
   arbitrary points into a list without disturbing time order, so
   that they will be sent out according to their expected time.
*/

// this defines elements in the linked list
typedef struct spike_link_t {
    praerie_event_t spike; // the actual event information
    struct spike_link_t* next; // index to the next element
} spike_link_t;

// This defines the current status of the linked list
typedef struct spike_ll_status_t {
    spike_link_t* first; // the first event in the linked list.
    spike_link_t* last;  // the last event in the linked list.
    spike_link_t* end;  // index of the final (free) element. If last == end the buffer is full. 
} spike_ll_status_t;

// this is the entire linked list
typedef struct spike_ll_t {
    spike_link_t spikes[SPIKE_BUF_MAX_SPIKES];
    spike_ll_status_t status;
} spike_ll_t;

/*
typedef struct spike_buf_t {
    uint tag;         // DMA tag for buffer. Will be last seq. 0xFFFFFFFF = free 
    uint transfer_ID; // DMA TID. Only assigned when a DMA is scheduled.
    spike_t spikes[SPIKE_BUF_MAX_SPIKES]; // the actual spikes in the buffer
} spike_buf_t;
*/

typedef struct spike_buf_t {
    uint pipe_stage;  // Which processing stage of the buffer pipeline this is
    uint tag;         // DMA tag for buffer. Will be last seq. 0xFFFFFFFF = free 
    uint transfer_ID; // DMA TID. Only assigned when a DMA is scheduled.
    spike_ll_t spike_list; // the linked-list containing the actual spikes
} spike_buf_t;

typedef struct spike_decode_t {
    praerie_event_t spike;
    uint32_t with_payload;
} spike_decode_t;

typedef struct pkt_props_t {
    uint8_t  remaining_spikes;
    uint32_t with_payload;
    uint32_t with_timestamp;
} pkt_props_t;

typedef struct cmd_IF_state_t {
    buf_req_en_t buffer_req_en; // enable external buffer request messages
    timer_t buf_req_interval; // timing interval between requests
    timer_t buf_req_timer; // countdown timer for buffer requests
    pause_state_t interface_paused;
    uint32_t sdram_space_n;
    uint32_t sdram_space_p;
    uint32_t sdram_min_req_space;
    uint32_t default_tag; // SDP tag to use by default for outbound messages
    timer_t ticks; // number of timer ticks to run before pausing
    bool infinite_run;
    uint32_t tick_period;
    uint32_t recording_flags; // a bitmap of the regions to record. See PRAERIE-consts.h
} cmd_IF_state_t;

typedef struct spinn_rq_buf_t {   
    uint16_t eieio_cmd_id;
    uint16_t chip_id;
    uint8_t processor;
    uint8_t with_payload;
    uint8_t region;
    uint8_t seq;
    uint32_t space_available;
    uint32_t seq_bitmap[8];
} spinn_rq_buf_t;

typedef struct mc_cam_t {
    uint32_t mask;
    uint32_t match_key;
    praerie_hdr_t hdr;
    eieio_hdr_t legacy_hdr;
} mc_cam_t; 

/*
typedef struct provenance_t {
    uint sdp_in_drops;  // incoming SDP messages dropped because queue was full
    uint sdp_out_drops; // outgoing SDP messages dropped after multiple send failures
    uint mc_in_drops; // incoming MC drops because queue was full
    uint mc_out_drops[2];  // outgoing MC drops with and without payloads
    uint abandoned_spikes[2]; // late spikes with and without payloads
    uint abandoned_records[NUM_RECORDING_REGIONS]; // events not recorded 
    uint abandoned_buf_reqs[2]; // buffer requests dropped after multiple send failures
} provenance_t;
*/

#endif
