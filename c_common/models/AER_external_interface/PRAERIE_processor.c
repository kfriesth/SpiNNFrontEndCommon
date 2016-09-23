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
#include "Command_processor.h"
#include "Timer.h"
#ifdef RECEIVER
#include "SDP_receive.h"
#include "DMA_receive_buffer.h"
#endif
#ifdef TRANSMITTER
#include "MC_packet.h"
#include "PRAERIE_codec.h"
#endif
#if (defined TRANSMITTER || defined INSTRUMENTATION)
#include "USR_packet.h"
#endif
#ifdef INSTRUMENTATION
#include "instrumentation.h"
#else
// state of the command processor
extern cmd_IF_state_t cmd_state;
#endif

/* validates that the model being compiled does indeed contain an application
   magic number. This 'magic number' is produced in an obscure way: within
   the Makefile.SpiNNFrontEndCommon which is included in the Makefile structure
   there is an inserted compiler directive introduced by the
   CFLAGS += -DAPPLICATION_NAME_HASH=<value> method. Will this be found or
   understood by the ordinary (or sensible!) mortal? Probably not...hence the 
   comment here. Again, not my code. (ADR)*/
#ifndef APPLICATION_NAME_HASH
#error APPLICATION_NAME_HASH was undefined.  Make sure you define this constant.
#endif

const praerie_hdr_t aer_hdr_null = AER_NULL_HEADER; // blank header defined so we can take a pointer to it elsewhere
// top-level structures to contain the memory layout of the PRAERIE transceiver
data_spec_layout_t data_spec_layout;
// the system configuration,
uint32_t system_configuration[NUM_CONFIG_PARAMS];
// and the tag table for inputs where sequence numbers are expected.
uint8_t seq_tags[MAX_SUPPORTED_DEVICES];


// read in all the configuration parameters. For flexibility this function
// uses a generic loop to copy the parameters over based on their offsets.
// see the definition of sys_config_t for the parameters themselves.
static inline void get_config_params(address_t param_addr)
{
       for (sys_config_t param = N_PYLD_BLKS; param <= RECORDING_FLAGS; param++) 
           system_configuration[param] = param_addr[param];
}

// anotehr loop to get the sequence tags for packets with sequence numbers
static inline void get_seq_tags(address_t seq_tag_addr)
{
       for (uint8_t device = 0; device < MAX_SUPPORTED_DEVICES; device++)
	   seq_tags[device] = (uint8_t)seq_tag_addr[device]; 
}

// a similar generic loop to that above to read in the data spec into an
// internal structure.
static inline void get_data_spec_regions(data_spec_layout_t* data_spec)
{
       for (uint region = SYSTEM_REGION; region < NUM_DATA_SPEC_REGIONS; region++)
           data_spec->regions[region] = data_specification_get_region(region, data_spec->data_addr);
}

// application configuration initialisation. Gets the data spec and configures
// application parameters. 
static bool app_config_init(uint32_t* sys_config)
{
       data_spec_layout.data_addr = data_specification_get_data_address();
       
       if (!data_specification_read_header(data_spec_layout.data_addr)) return false;
       else get_data_spec_regions(&data_spec_layout);
       // hyper-ugly: APPLICATION_NAME_HASH is a compiler directive expected
       // to be defined via a compiler command-line directive append (-D xxx).
       // gcc documentation doesn't indicate whether a #define in a header file
       // will be processed before or after -D so it's not safe to define this
       // internally in a header. Not my code...
#ifdef INSTRUMENTATION 
       if (!simulation_initialise(data_spec_layout.regions[SYSTEM_REGION], 
                                  APPLICATION_NAME_HASH,
                                  &sys_config[TICK_PRD],
                                  &sys_config[SIM_TIME],
                                  &sys_config[INF_RUN],
                                  CB_PRIORITY_SDP_RECV,
                                  praerie_if_provenance,
                                  data_spec_layout.regions[PROVENANCE_REGION]))
          return false;
#else 
       if (!simulation_initialise(data_spec_layout.regions[SYSTEM_REGION], 
                                  APPLICATION_NAME_HASH,
                                  &sys_config[TICK_PRD],
                                  &sys_config[SIM_TIME],
                                  &sys_config[INF_RUN],
                                  CB_PRIORITY_SDP_RECV,
                                  NULL,
                                  data_spec_layout.regions[PROVENANCE_REGION]))
          return false;
#endif
       // now read in the basic parameters
       get_config_params(data_spec_layout.regions[CONFIGURATION_REGION]);
       // and get the region for the tags associated with inputs where sequence numbers are expected
       get_seq_tags(data_spec_layout.regions[SEQUENCE_COUNTER_REGION]);
       return true;
}

void c_main(void)
{
     if (!app_config_init(system_configuration)) 
     {
#ifdef INSTRUMENTATION
        instrument_error("Error during PRAERIE interface configuration\n");
#else
        rt_error(RTE_SWERR);
#endif
     }
#ifdef RECEIVER
     if ((system_configuration[N_PYLD_BLKS]+system_configuration[W_PYLD_BLKS]) && 
         (!DMA_init(system_configuration[N_PYLD_BLKS], system_configuration[W_PYLD_BLKS])))
     {
#ifdef INSTRUMENTATION
        instrument_error("Error during DMA process initialisation\n");
#else
        rt_error(RTE_SWERR);
#endif
     }
#endif
#if (defined TRANSMITTER || defined INSTRUMENTATION)
     if (!USR_init(system_configuration[SEND_BUF_SZ]))
     {
#ifdef INSTRUMENTATION
        instrument_error("Error during user-event process initialisation\n");
#else
        rt_error(RTE_SWERR);
#endif
     }
#endif
#ifdef RECEIVER
     if (!SDP_init(system_configuration[SDP_R_PORT]))
     {
#ifdef INSTRUMENTATION
        instrument_error("Error during SDP receiver initialisation\n");
#else
        rt_error(RTE_SWERR);
#endif
     }
#endif
#ifdef TRANSMITTER
     if (!codec_init(data_spec_layout.regions[MC_LOOKUP_REGION]))
     {
#ifdef INSTRUMENTATION
        instrument_error("Error during MC lookup table initialisation\n");
#else
        rt_error(RTE_SWERR);
#endif
     }
     if (!MC_pkt_init())
     {
#ifdef INSTRUMENTATION
        instrument_error("Error during MC packet process initialisation\n");
#else
        rt_error(RTE_SWERR);
#endif
     }
#endif
     TMR_init(system_configuration);
     CMD_if_init(system_configuration, seq_tags);
#ifdef INSTRUMENTATION
     if (!instrumentation_init(&data_spec_layout, system_configuration[REC_FLGS]))
     {
        log_warning("Error initialising instrumentation interface.\n The PRAERIE transceiver may still operate but there will be no recording or\n provenance data\n");
     }
#endif    
     simulation_run();
}
