#ifndef __PRAERIE_CONSTS_H__
#define __PRAERIE_CONSTS_H__

#include <praerie-typedefs.h>

// some constants for error messages
#define NULL_ARG 0
#define ERR_NONE 0
#define ERR_SYSTEM_INIT 1
#define ERR_PRAERIE_IF 2

// configuration layout constants
#define MAX_SUPPORTED_DEVICES 4
#define NUM_DATA_SPEC_REGIONS 12
#define NUM_CONFIG_PARAMS 13
#define NUM_RECORDING_REGIONS 4

// send buffer state for SDP sends
#define SEND_BUF_EMPTY -1
#define SEND_BUF_NONFULL 0
#define SEND_BUF_FULL 1

// memory regions in core configuration
// the values in this set the bit-positions
// of the recording flags.
#define SYSTEM_REGION 0
#define CONFIGURATION_REGION 1
#define SEQUENCE_COUNTER_REGION 2
#define MC_LOOKUP_REGION 3
#define BUFFERING_OUT_CONTROL_REGION 4
#define PROVENANCE_REGION 5
#define NO_PAYLOAD_BUFFER_REGION 6
#define WITH_PAYLOAD_BUFFER_REGION 7
#define OUT_SPIKE_NO_PYLD_RECORD_REGION 8
#define OUT_SPIKE_WITH_PYLD_RECORD_REGION 9
#define IN_SPIKE_NO_PYLD_RECORD_REGION 10
#define IN_SPIKE_WITH_PYLD_RECORD_REGION 11

// region offsets in CONFIGURATION_REGION that hold the location of the 
// simulation parameter in concern
#define NO_PAYLOAD_BLOCKS    0  // number of buffer-sized SDRAM blocks for packets without payloads
#define WITH_PAYLOAD_BLOCKS  1  // number of buffer-sized SDRAM blocks for packets with payloads
#define SEND_BUFFER_SIZE     2  // size of the buffer for outbound packets (triggered by MC events)
#define STIMULUS_ONSET_TIME  3  // effective start time of the simulation
#define SIMULATION_TIME      4  // number of ticks to run (including stimulus offset)
#define TICK_PERIOD          5  // interval in microseconds between ticks
#define INFINITE_RUN         6  // never stop (SIMULATION TIME ignored)
#define DEFAULT_TAG          7  // default outward tag for SDP replies
#define SDP_RECV_SIM_PORT    8  // virtual SDP port for simulation library
#define BUFFER_REQ_EN        9  // enable buffer requests to host
#define BUF_REQ_INTERVAL     10 // number of ticks between buffer requests 
#define SDRAM_MIN_REQ_BLOCKS 11 // minimum number of free SDRAM blocks to trigger a request
#define RECORDING_FLAGS      12 // a bitmap of output types to record

// provenance regions
#define SDP_IN_DROPS       0  // incoming SDP messages dropped because queue was full
#define SDP_OUT_DROPS      SDP_IN_DROPS+1 // outgoing SDP messages dropped after multiple send failures
#define MC_IN_DROPS        SDP_OUT_DROPS+1 // incoming MC drops because queue was full
#define MC_OUT_DROPS       MC_IN_DROPS+1  // outgoing MC drops with and without payloads
#define ABANDONED_SPIKES   MC_OUT_DROPS+2 // late spikes with and without payloads
#define ABANDONED_BUF_REQS ABANDONED_SPIKES+2 // buffer requests dropped after multiple send failures
#define ABANDONED_RECORDS  ABANDONED_BUF_REQS+2 // events not recorded 

// total number of provenance regions (the last field, ABANDONED_RECORDS, 
// has a region for each recording region)
#define NUM_PROV_REGIONS ABANDONED_RECORDS+NUM_RECORDING_REGIONS 

// callback priorities
#define CB_PRIORITY_MC_PKT -1     // multicast as always is FIQ
#define CB_PRIORITY_SDP_RECV 0    // sdp received is non-queueable normal IRQ
#define CB_PRIORITY_USR 0         // so is user event (MC buffer dequeue)
#define CB_PRIORITY_USR_DEFER 1   // user is queued if a trigger fails
#define CB_PRIORITY_DMA_DONE 1    // dma done can be queued
#define CB_PRIORITY_MANUAL_XFER 2 // manual transfers are lower than DMA 
#define CB_PRIORITY_SDP_SEND 2    // sdp send can be issued at low priority
#define CB_PRIORITY_TMR 3         // timer is last so that packets can be sent

#define SDP_SEND_TIMEOUT 1 // timeout in milliseconds for SDP send
#define SDP_SEND_RETRIES 3 // number of retries on SDP send failure
#define SDP_RETRY_DELAY  1 // delay between SDP retries

#define SDP_MSG_HDR_LEN  8 // length of the SDP message info header
#define SDP_HDR_LEN      8 // length of the SDP header
#define SCP_HDR_LEN      16 // length of the SCP header
#define SDP_HDR_SCP_LEN  SDP_HDR_LEN+SCP_HDR_LEN // length of the SDP header including SCP
#define SDP_MSG_MAX_LEN  SDP_HDR_SCP_LEN+SDP_BUF_SIZE // maximum length of an SDP message
#define SDP_USR_PORT     1    // port for SDP outbound messages
#define SDP_FLAGS_NOACK  0x7  // flags when no acknowledgement of SDP expected
#define SDP_FLAGS_ACK    0x87 // flags when acknowledgement of SDP expected
#define SDP_DEST_ADDR_ROOT 0  // root chip address for outgoing SDPs

#define CHECK_PREFIXES   true

// sequence number parameters
#define AER_SEQ_WINDOW 4 // size of the internal incoming SDP message buffer

/* a tuning parameter - how far in the future do we interpret AER seqs? Seqs
   beyond the horizon (including wraparound) are interpreted as being in the
   past. This depends on how probable it is that badly out-of-order packets
   will be received; if a packet might arrive very late indeed the horizon
   should be small; if late reception is unlikely the horizon can be close to
   the maximum limit of 255.
*/
#define AER_SEQ_HORIZON       128
#define SEQ_BIT_MASK          0x1F
#define SEQ_WORD_MASK         0xFF & (~SEQ_BIT_MASK)
#define SEQ_WORD_SHIFT        5
#define SEQ_SHIFTED_WORD_MASK SEQ_WORD_MASK >> SEQ_WORD_SHIFT

// constants for buffer setup
#define NUM_SDRAM_BLOCKS 1024 // number of buffers in SDRAM. 1024 gives ~3MB.
#define SPIKE_BUF_MAX_SPIKES 255 // size of the spike buffers for incoming spikes
#define NUM_OUTGOING_PRAERIE_TYPES 8 // number of entries in the MC-AER CAM table
#define N_INCOMING_SPIKES 256 // number of spaces in the incoming MC spike buffer
#define MC_MATCH_END 0xFFFFFFFF // with mask 0 indicates no more MC lookup entries

//  DMA and SDP buffer tag codes
#define DMA_TAG_FREE          0xFFFFFFFF // Buffer available.
#define DMA_TAG_SVC           0xFFF00000 // Buffer being transferred from System RAM.
#define DMA_TAG_IN_PIPELINE   0xFF000000 // Buffer is in the DMA-service pipeline
#define DMA_TAG_READY         0x00001000 // Buffer queued and ready to process.
#define DMA_TAG_FILL          0x00002000 // Buffer is not full. 
#define DMA_TAG_FULL          0x00004000 // Buffer is full and may be being read or written back.
#define DMA_TAG_RB_RDY        0x00008000 // Buffer is expecting a readback from SDRAM.
#define DMA_TAG_RB            0x00010000 // Buffer is being filled by a readback from SDRAM.
#define DMA_TAG_W_MASK        0x000001FF // Buffer is being written to.
#define DMA_TAG_W_PYLD        0x00000100 // Buffer has valid payloads.
#define DMA_TAG_NO_PYLD       0x00000000 // Buffer does not have payloads.
#define DMA_TAG_MANUAL        0x00000200 // Buffer is being manually transferred.

// DMA buffer indices
#define EARLY_BUF 0
#define MID_BUF 1
#define DMA_WBUF 2
#define DMA_TBUF 3

// DMA process parameters
// A similar tuning parameter to AER_SEQ_HORIZON for timestamp reordering.
// Here set to half the maximum timestamp size.
#define DMA_IN_TIMESTAMP_HORIZON 0x80000000 
#define NUM_DMA_PIPE_BUFS 4  // Number of DMA pipeline buffers
#define NUM_DMA_ACTIVE_BUFS 3 // Number of potentially transferring DMA buffers 
#define DMA_BUFS_ALL NUM_DMA_PIPE_BUFS  // All buffers available
#define DMA_BUFS_NONE 0 // No buffers available       
#define DMA_MANUAL_RETRY_DELAY 1 // delay between retries on manual transfers.

// MC packet transmit parameters
#define NO_TIMESTAMP 0
#define WITH_TIMESTAMP 1
#define NO_PAYLOAD 0
#define WITH_PAYLOAD 1
#define DONT_CARE_PAYLOAD 2
#define NUM_MC_SEND_RETRIES 1
#define MC_SEND_RETRY_DELAY 1

#define HW_HIGH_BIT_SHIFT 16
#define HW_LOW_BIT_SHIFT 0

// Timer process parameters
#define MAX_TICKS 0xFFFFFFFF
#define TIME_RESET 0xFFFFFFFF

// recording parameters
#define RECORD_DIR_OUT 0
#define RECORD_DIR_IN 2 

#endif
