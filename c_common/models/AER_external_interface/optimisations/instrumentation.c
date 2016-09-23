#include <spin1_api.h>
#include <sark.h>
#include <debug.h>
#include <simulation.h>
#include "PRAERIE-proc-typedefs.h"
#include "instrumentation.h"
#include "Command_processor.h"

extern timer_t local_timestamp;

// initialises instrumentation interface (provenance gathering and recording)
// This expects that the recording flags passed in have a 1 at the bit-position
// corresponding to the desired regions to record (which means what is to be
// recorded can be set up in the network script).
bool instrumentation_init(data_spec_layout_t* data_spec, uint32_t recording_flags) 
{
  
     // initialise the provenance data structure by stepping through all the elements
     for (uint prov_field = SDP_IN_DROPS; prov_field < NUM_PROV_REGIONS; prov_field++)
         prov_data[prov_field] = 0;
     // need to setup the region structures for recording initialisation. These
     // 2 temporary variables will hold the extracted region information to record
     // from recording flags.
     uint8_t num_regions = 0;
     uint8_t regions_to_record[NUM_DATA_SPEC_REGIONS];
     for (uint8_t region = SYSTEM_REGION; region < NUM_DATA_SPEC_REGIONS; region++)
     {
         if (recording_flags & (0x1 << region)) 
            regions_to_record[num_regions++] = region;
     }
     // initialise the local recording flags variable
     cmd_state.recording_flags = recording_flags;        
     if (!recording_initialize(num_regions, regions_to_record, &data_spec->regions[SYSTEM_REGION][SIMULATION_N_TIMING_DETAIL_WORDS], BUFFERING_OUT_CONTROL_REGION, &cmd_state.recording_flags)) return false;
     else return true; 
}

// provenance function stores running information of use for debug. This steps
// through the provenance data structure and copies each field into the provenance
// data region.
void praerie_if_provenance(address_t data_region)
{
     for (uint32_t field = SDP_IN_DROPS; field < NUM_PROV_REGIONS; field++)
         data_region[field] = prov_data[field];
}

void instrument_error(const char* error_msg)
{
     log_error("", error_msg);
     rt_error(RTE_SWERR);
     spin1_exit(ERR_PRAERIE_IF);
}

// General interface for recording. The involved processes indicate what 
// direction (in or out of SpiNNaker) they are operating in and whether the
// data they are providing has a payload. This function then records the 
// data, recording as provenance information any failed attempts to record.
void instrument_event_recording(uint8_t direction, bool with_payload, void* data, uint32_t size)
{
     // determine the channel to record - will be direction + payload. 
     if (with_payload) direction++;
     // Is this channel even being recorded?
     if (cmd_state.recording_flags & (0x1 << OUT_SPIKE_NO_PYLD_RECORD_REGION+direction))
     {
        // if so record the data, storing provenance if it fails. 
        if (!recording_record(direction, data, size))
           prov_data[ABANDONED_RECORDS+direction]++;
     }
}  

// instrumentation for buffer requests and recording output from timer. We
// allow for either or both of payloaded and non-payloaded buffers to 
// request more data.  
void instrument_timer(timer_t timestamp)
{
#ifdef RECEIVER
     sdp_msg_buf_t buf_req_msg;
     // check if any requests are enabled and if the timer interval has expired 
     // for sending another buffer request
     if ((cmd_state.buffer_req_en & REQ_BOTH) && !(--cmd_state.buf_req_timer)) 
     {
        // get a new SDP message container for the request
        // could also be malloc'ed which might be more efficient. 
        if ((buf_req_msg.mailbox = buf_req_msg.msg = spin1_msg_get()) != NULL)
        {
	   // then issue the request depending upon the appropriate buffer
           // being enabled for requests.
           if (cmd_state.buffer_req_en & REQ_NO_PYLD)
           {
              sdram_buffer_request(&buf_req_msg, NO_PAYLOAD);
           }
           if (cmd_state.buffer_req_en & REQ_WITH_PYLD)
           {
              sdram_buffer_request(&buf_req_msg, WITH_PAYLOAD);
           }
           // message has been sent so we can return it to the free pool.
           spin1_msg_free(buf_req_msg.msg);
        }
        // reset the timer to count the interval down again.
        cmd_state.buf_req_timer = cmd_state.buf_req_interval;
     }
#endif
     // slightly bodgy workaround for recording. The recording interface 
     // requires that neither the SDP-received event nor the timer event
     // interrupt each other with respect to their recording functionality.
     // Since Timer is set to a lower priority than SDP received, for the 
     // recording part, we escalate to a nonqueueable user event priority
     // to ensure that the critical part related to recording (updating the
     // current_read pointer for the recording channels) is not susceptible
     // to interruption. Eventually recording.c should be fixed to eliminate
     // this dependency.   
     if (cmd_state.recording_flags) spin1_trigger_user_event(CB_PRIORITY_TMR, timestamp);
}
