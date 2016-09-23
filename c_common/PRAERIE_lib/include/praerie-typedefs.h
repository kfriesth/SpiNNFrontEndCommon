/*
 * praerie-typedefs.h
 *
 *
 *  SUMMARY
 *    Definitions of useful types and constants for PRAERIE packet decode
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

#ifndef __PRAERIE_TYPEDEFS_H__
#define __PRAERIE_TYPEDEFS_H__

#include <spin1_api.h>

# define PRAERIE_BASE_HDR_LEN 4

# define PREFIX_SHIFT_16 16
# define PREFIX_SHIFT_32 32
# define PREFIX_SHIFT_64 64

// raw header masks
#define PRAERIE_MASK_COMMAND     0xFFFFFFFF
#define PRAERIE_MASK_EIEIO_HDR   0xFFFF0000
#define PRAERIE_MASK_PKT_DECODE  0xFF000000
#define PRAERIE_MASK_ADDR_TYPE   0xC0000000
#define PRAERIE_MASK_TSTP_TYPE   0x30000000
#define PRAERIE_MASK_PYLD_TYPE   0xC000000
#define PRAERIE_MASK_ADDR_SIZE   0x3000000
#define PRAERIE_MASK_CMD_WD_SIZE 0x3000000
#define PRAERIE_MASK_COUNT       0xFF0000
#define PRAERIE_MASK_SEQ         0xFF00
#define PRAERIE_MASK_CMD_GEN     0x8000
#define PRAERIE_MASK_CMD_DIR     0x4000
#define PRAERIE_MASK_CMD_NUM     0x3FFF
#define PRAERIE_MASK_EXT         0xFF
#define PRAERIE_MASK_TSTP_SIZE   0xC0
#define PRAERIE_MASK_PYLD_SIZE   0x30
#define PRAERIE_MASK_SYNC        0x1

// raw header bit-positions
#define PRAERIE_ADDR_TYPE_POS   30
#define PRAERIE_TSTP_TYPE_POS   28
#define PRAERIE_PYLD_TYPE_POS   26
#define PRAERIE_ADDR_SIZE_POS   24
#define PRAERIE_CMD_WD_SIZE_POS 24
#define PRAERIE_COUNT_POS       16
#define PRAERIE_EIEIO_HDR_POS   16
#define PRAERIE_EIEIO_CMD_POS   16
#define PRAERIE_CMD_GEN_POS     15
#define PRAERIE_CMD_DIR_POS     14
#define PRAERIE_SEQ_POS         8
#define PRAERIE_TSTP_SIZE_POS   6
#define PRAERIE_PYLD_SIZE_POS   4
#define PRAERIE_SYNC_POS        0
#define PRAERIE_CMD_NUM_POS     0

// pseudo-masks for PRAERIE enum fields to ease type detection
#define PRAERIE_FIELD_TYPE_DT 1
#define PRAERIE_FIELD_TYPE_PF 2

// EIEIO legacy definitions
#define EIEIO_BASE_HDR_LEN 2

// EIEIO legacy decodes
#define EIEIO_TYPE_16_K            0
#define EIEIO_TYPE_16_KP           1
#define EIEIO_TYPE_32_K            2
#define EIEIO_TYPE_32_KP           3
#define EIEIO_PF_BASIC             0
#define EIEIO_PF_CMD               1
#define EIEIO_PF_PREFIX_L          2
#define EIEIO_PF_PREFIX_U          3
#define EIEIO_PREFIX_TYPE_L        0
#define EIEIO_PREFIX_TYPE_U        1
#define EIEIO_PAY_PREFIX_NONE      0
#define EIEIO_PAY_PREFIX_WITH      1
#define EIEIO_PAY_TIMESTAMP_NO     0
#define EIEIO_PAY_TIMESTAMP_YES    1
#define EIEIO_TAG_VERSION_0        0
#define EIEIO_TAG_NONE             4

// EIEIO legacy masks
#define EIEIO_MASK_PF              0xC000
#define EIEIO_MASK_P               0x8000
#define EIEIO_MASK_F               0x4000
#define EIEIO_MASK_D               0x2000
#define EIEIO_MASK_T               0x1000
#define EIEIO_MASK_TYPE            0xC000
#define EIEIO_MASK_TAG             0x300
#define EIEIO_MASK_COUNT           0xFF
#define EIEIO_MASK_PREFIX          0xFFFF
#define EIEIO_MASK_COMMAND         0x3FFF

//EIEIO legacy bit-positions
#define EIEIO_P_BIT_POS 15
#define EIEIO_F_BIT_POS 14
#define EIEIO_D_BIT_POS 13
#define EIEIO_T_BIT_POS 12
#define EIEIO_TYPE_POS 10
#define EIEIO_TAG_POS 8
#define EIEIO_COUNT_POS  0 
#define EIEIO_PREFIX_POS 0
#define EIEIO_CMD_POS 0

//EIEIO valid commands
#define NULL_CMD           0x0
#define DB_CONF            0x1
#define EVT_PAD            0x2
#define EVT_STOP_CMDS      0x3
#define STOP_SEND_REQS     0x4
#define START_SEND_REQS    0x5
#define SPINN_REQ_BUFS     0x6
#define HOST_SEND_SEQ_DAT  0x7
#define SPINN_REQ_READ_DAT 0x8
#define HOST_DAT_READ      0x9
#define RSVD_EXP           0xA

// pseudo-masks for PRAERIE enum fields to ease type detection
#define EIEIO_DTYPE_PY 1
#define EIEIO_DTYPE_SZ 2
#define EIEIO_PTYPE_DC 1
#define EIEIO_PTYPE_PF 2

// blank command
#define PRAERIE_NULL_CMD {0, SIZE_NONE, 0, CMD_TYPE_DEVICE_SPECIFIC, CMD_DIR_READ, 0, 0}

// blank header for EIEIO
// Version (tag) field is 4 (SIZE_LEGACY == TAG_NONE) to prevent use over the wire 
#define EIEIO_NULL_HEADER {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4}

// blank header for PRAERIE
#define AER_NULL_HEADER {0, ADDR_TYPE_NONE, TSTP_TYPE_NONE, PYLD_TYPE_NONE, SIZE_NONE, SIZE_NONE, SIZE_NONE, 0, 0, 0, 0, 0, 0, PRAERIE_NULL_CMD, EIEIO_NULL_HEADER}

// type for generic (size-neutral) efficient bulk copy operations with shift
typedef void (*cpy_func) (void*, void*, uint32_t, int8_t);

//! fixed typedefs to enforce correct decode
typedef enum praerie_address_types {
  ADDR_TYPE_CMD, ADDR_TYPE_ADDR_ONLY, ADDR_TYPE_PREFIX_ONLY, ADDR_TYPE_ADDR_PREFIX, ADDR_TYPE_LEGACY, ADDR_TYPE_NONE
} praerie_address_types;

typedef enum praerie_timestamp_types {
  TSTP_TYPE_NONE, TSTP_TYPE_TSTP_ONLY, TSTP_TYPE_PREFIX_ONLY, TSTP_TYPE_TSTP_PREFIX, TSTP_TYPE_LEGACY
} praerie_timestamp_types;

typedef enum praerie_payload_types {
  PYLD_TYPE_NONE, PYLD_TYPE_PYLD_ONLY, PYLD_TYPE_PREFIX_ONLY, PYLD_TYPE_PYLD_PREFIX, PYLD_TYPE_LEGACY
} praerie_payload_types;

typedef enum praerie_sizes {
  SIZE_COMPAT, SIZE_16, SIZE_32, SIZE_64, SIZE_LEGACY, SIZE_NONE
} praerie_sizes;

//! human readable form of the different eieio mesage types
typedef enum eieio_data_message_types {
    KEY_16_BIT, KEY_PAYLOAD_16_BIT, KEY_32_BIT, KEY_PAYLOAD_32_BIT
} eieio_data_message_types;

typedef enum eieio_packet_types {
    PKT_TYPE_BASIC, PKT_TYPE_CMD, PKT_TYPE_PRE_LOW, PKT_TYPE_PRE_UP
} eieio_packet_types;

typedef enum praerie_command_messages {
    PRAERIE_CMD_NULL_COMMAND = 0, // Do nothing
    PRAERIE_CMD_REAL_TIME    = 1, // Receive/report wall-clock time
    PRAERIE_CMD_RESET        = 2, // Reset the interface, discarding any exisiting packets
    PRAERIE_CMD_INFO         = 3, // Retrieve device capabilities and ID 
    PRAERIE_CMD_PAUSE_RESUME = 4, // Temporarily disable an interface or reenable it
    PRAERIE_CMD_RESERVED     = 5  // Higher commands reserved
} praerie_command_messages;

/* bodgy: commented out for now to avoid conflict with buffered_eieio_defs.h
   file in front_end_common_lib (which is needed by recording.c). Eventually
   the current header (praerie-typedefs) should entirely replace the ones in
   front_end_common_lib relating to EIEIO but this would involve modifying
   recording.c which for the moment we would rather avoid doing. It should be
   noted that given that this is the PRAERIE library it should be independent
   of implementation-dependent issues like this (this particular problem only
   applies to SpiNNaker) but the recording interface for SpiNNaker has been
   written in such a way as to bind these things together. 
//! human readable forms of the different command message ids.
typedef enum eieio_command_messages {
    NULL_COMMAND                = NULL_CMD, // Do nothing 
    DATABASE_CONFIRMATION       = DB_CONF, // Database handshake with visualiser
    EVENT_PADDING               = EVT_PAD, // Fill in buffer area with padding
    EVENT_STOP_COMMANDS         = EVT_STOP_CMDS,  // End of all buffers, stop execution
    STOP_SENDING_REQUESTS       = STOP_SEND_REQS, // Stop announcing that there is sdram free space for buffers
    START_SENDING_REQUESTS      = START_SEND_REQS, // Start announcing that there is sdram free space for buffers
    SPINNAKER_REQUEST_BUFFERS   = SPINN_REQ_BUFS, // Spinnaker requesting new buffers for spike source population
    HOST_SEND_SEQUENCED_DATA    = HOST_SEND_SEQ_DAT, // Buffers being sent from host to SpiNNaker
    SPINNAKER_REQUEST_READ_DATA = SPINN_REQ_READ_DAT, // Buffers available to be read from a buffered out vertex
    HOST_DATA_READ              = HOST_DAT_READ, // Host confirming data being read form SpiNNaker memory
    RESERVED_EXPANSION          = RSVD_EXP  // Future use
} eieio_command_messages;
*/

typedef enum praerie_cmd_types {
    CMD_TYPE_DEVICE_SPECIFIC = 0,
    CMD_TYPE_GENERAL = 1, 
    CMD_TYPE_LEGACY = 2
} praerie_cmd_types;

typedef enum praerie_cmd_dirs {
    CMD_DIR_READ = 0,
    CMD_DIR_WRITE = 1,
    CMD_DIR_LEGACY = 2
} praerie_cmd_dirs;

// praerie event components
typedef uintmax_t praerie_addr_t;
typedef uintmax_t praerie_pyld_t;
typedef uintmax_t praerie_tstp_t;

typedef uint32_t eieio_addr_t;
typedef uint32_t eieio_pyld_t;

typedef struct praerie_event_t {
    praerie_addr_t address;
    praerie_pyld_t payload;
    praerie_tstp_t timestamp;
} praerie_event_t; 

typedef struct eieio_event_t {
    eieio_addr_t address;
    eieio_pyld_t payload;
} eieio_event_t;

// praerie command struct
typedef struct praerie_cmd_t {
    uint32_t binary_cmd;
    praerie_sizes word_size;
    uint8_t count;
    praerie_cmd_types general_cmd;
    praerie_cmd_dirs dir;
    uint16_t cmd_num;
    uint16_t legacy_cmd_ID;  
} praerie_cmd_t;

//! legacy eieio header struct
typedef struct eieio_hdr_t {
    uint16_t eieio_binary_hdr; // the encoded binary of the header
    bool     apply_prefix; //! the p bit of the eieio header
    uint16_t prefix; //! prefix if needed (last 16 bits of header)
    uint8_t  prefix_type; //! prefix type if data header (F bit)
    uint8_t  packet_type; //! type of packet 16bit, payload, 32 bit payload. (type bits)
    uint32_t key_right_shift;
    bool     payload_as_timestamp; //! t bit, verifyies if payloads are timestamps
    bool     payload_apply_prefix; //! D bit
    uint32_t payload_prefix; //! payload prefix
    uint8_t  count; //! the number of elements in the header
    uint8_t tag; //! the tag bits of the eieio header
} eieio_hdr_t;

//! praerie header struct
typedef struct praerie_hdr_t {
    uint32_t praerie_binary_hdr;
    praerie_address_types   address_type;
    praerie_timestamp_types timestamp_type;
    praerie_payload_types   payload_type;
    praerie_sizes           address_size;
    praerie_sizes           timestamp_size;
    praerie_sizes           payload_size;
    uint8_t  count;
    uint8_t  seq;
    uint32_t expansion;
    uint64_t address_prefix;
    uint64_t timestamp_prefix;
    uint64_t payload_prefix;
    praerie_cmd_t command;
    eieio_hdr_t legacy_hdr;
} praerie_hdr_t;

//! praerie configuration struct
typedef struct praerie_if_cfg {
    bool praerie_input_en;
    bool praerie_output_en;
} praerie_if_cfg;

#endif
