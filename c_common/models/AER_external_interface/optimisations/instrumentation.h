#ifndef __INSTRUMENTATION_H__
#define __INSTRUMENTATION_H__

#include <common-typedefs.h>
#include <recording.h>
#include "PRAERIE-proc-typedefs.h"

// state of the command processor
extern cmd_IF_state_t cmd_state;
// provenance data. Found in instrument_provenance.c for very ugly reasons
// explained in that file. (Provenance interface in simulation.c ought to be
// fixed)
extern uint* prov_data;

inline void instrumentation_pause(void)
{
       if (cmd_state.recording_flags) recording_finalise();
}
inline void incr_prov(uint field, uint increment) 
{if (field < NUM_PROV_REGIONS) prov_data[field] += increment;}
bool instrumentation_init(data_spec_layout_t* data_spec, uint32_t recording_flags);
void praerie_if_provenance(address_t data_region);
void instrument_error(const char* error_msg);
void instrument_event_recording(uint8_t direction, bool with_payload, void* data, uint32_t size);
void instrument_timer(timer_t timestamp); 

#endif
