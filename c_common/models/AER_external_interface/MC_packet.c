#include <debug.h>
#include <spin1_api.h>
#include <circular_buffer.h>
#include "PRAERIE-consts.h"
#include "PRAERIE-proc-typedefs.h"
#include "MC_packet.h"
#include "USR_packet.h"

#ifdef INSTRUMENTATION
#include "instrumentation.h"
#endif


// True if the MC packet-receive "loop" is currently running
bool mc_pkt_busy;
circular_buffer in_spike_buf; // buffer for incoming spikes

/* CALLBACK FUNCTIONS - cannot be static */

// Called when a multicast packet is received
void _multicast_packet_received_callback(uint key, uint payload) 
{
    spinn_event_t received_spike = {key, payload};

    // If there was space to add spike to incoming spike queue
    // Ugly: circular_buffer is actually only defined for uint32_t
    // entries. So for the moment we will have to add only the key,
    // payloads not supported. (A more general version of circular
    // buffer should probably be implemented in spinn_common!
    if (circular_buffer_add(in_spike_buf, received_spike.key))
    {
        // If we're not already processing received events,
        // flag pipeline as busy and trigger a feed event
        if (!mc_pkt_busy)
        {     
            mc_pkt_busy = true; 
            if (spin1_trigger_user_event(CB_PRIORITY_USR, NULL_ARG) == FAILURE)                
            {
	       // but if this fails because an old one was in the exit window
               // schedule it as a queuable callback
               if (spin1_schedule_callback(_user_event_callback, CB_PRIORITY_USR_DEFER, NULL_ARG, CB_PRIORITY_USR_DEFER) == FAILURE)
               {
		  /* and if even this fails because the task queue was full,
                     there is little choice but to hope another MC packet arrives!
                     Note that there is the possibilty of a nasty pathology: if
                     the callback doesn't succeed, AND the MC queue is filled by
                     the newly-arrived spike, the queue will NEVER be serviced 
                     and it will be as if no further MC packets arrived. This 
                     would require the rather freakish situation that a series
                     of MC packets arrived exactly at the moment the user process
                     was exiting while simultaneously there were multiple queued
                     callbacks to service. The MC packets would fill up the
                     buffer and then cause the whole system to grind to a halt. 
                     Most likely if this were going on there is a serious problem
                     with too much traffic anyway.
		  */
		  mc_pkt_busy = false;
                  log_debug("Could not trigger user event\n");
               }
            }
        }
    }
    else
    {
        // couldn't add an mc packet. Drop it and record provenance data. 
#ifdef INSTRUMENTATION         
        incr_prov(MC_IN_DROPS, 1);
#else
        log_warning("spike %u dropped (payload %u) on MC event", received_spike.key, received_spike.payload);
#endif
    }
}

/* INTERFACE FUNCTIONS - cannot be static */

bool MC_pkt_init(void) 
{
    // Allocate incoming spike buffer. Failure to allocate is fatal.
    if ((in_spike_buf = circular_buffer_initialize(N_INCOMING_SPIKES)) == NULL)
    {
        return false;
    }
    // initialise the flag indicating there are received events being processed.
    mc_pkt_busy = false;
    // Set up the callback
    spin1_callback_on(MC_PACKET_RECEIVED, _multicast_packet_received_callback, CB_PRIORITY_MC_PKT);
    return true;
}

uint32_t mc_buffer_overflows(void) 
{
    // Check for buffer overflow
    uint32_t spike_buffer_overflows = circular_buffer_get_n_buffer_overflows(in_spike_buf);
    if (spike_buffer_overflows > 0) {
        log_warning("%u spike buffers overflowed", spike_buffer_overflows);
        //io_printf(IO_BUF, "\tWarning - %u spike buffers overflowed\n",
        //          spike_buffer_overflows);
    }
    return spike_buffer_overflows;
}
