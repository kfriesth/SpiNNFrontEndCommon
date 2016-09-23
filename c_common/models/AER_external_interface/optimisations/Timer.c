#include <common-typedefs.h>
#include <debug.h>
#include <simulation.h>
#include <spin1_api.h>
#include <sark.h>
#include <data_specification.h>
#include <praerie-typedefs.h>
#include <praerie_interface.h>
#include <eieio_interface.h>
#include "Timer.h"
#include "Command_processor.h"
#ifdef RECEIVER
#include "DMA_receive_buffer.h"
#endif
#ifdef INSTRUMENTATION
#include "instrumentation.h"
#else
extern cmd_IF_state_t cmd_state;
#endif

timer_t local_timestamp;

/* INTERFACE FUNCTIONS - cannot be static */

/* set the wall-clock real time as seen by the PRAERIE interface. This can
   have one of 2 effects: If the new time to set is in the future relative
   to the current time then it just advances to that time and will end early,
   no harm in that because all that will happen is that the interface will 
   shut down while maybe other parts of the simulation carry on - which from 
   its point of view are irrelevant. If the new time to set is in the past,
   however, then the tick count will have to be offset so that the interface
   doesn't keep injecting spikes long after the rest of the simulation thinks
   it's done. 
 */
bool set_real_time(timer_t time)
{
     // before updating the time disable interrupts - an obviously critical
     // section. 
     uint cpsr = spin1_int_disable();
     // compare the timestamp: is it in the past?
     if (local_timestamp > time)
     {
        // if so offset the tick count by the time shift
        if (cmd_state.ticks < local_timestamp-time)
	{
	   // but if this would result in a negative time the user tried to
           // perform an invalid time update. 
	   spin1_mode_restore(cpsr);
	   return false;
        }
        else cmd_state.ticks -= (local_timestamp-time);
     }
     // now update the timestamp itself
     local_timestamp = time;
     // and disable timer ticks. We are first going to do an immediate 
     // timer tick event reflecting the updated time, after which we can
     // reenable the timer tick event.
     spin1_callback_off(TIMER_TICK);
     // now we can end the critical section
     spin1_mode_restore(cpsr);
     // and schedule the immediate timer tick event
     spin1_schedule_callback(_timer_callback, local_timestamp, TIME_RESET, CB_PRIORITY_TMR);
     return true; 
}

// stub function for the moment - just indicate this isn't yet supported.
// will be supported soon (just need to send a PRAERIE reply packet)
bool get_real_time(praerie_hdr_t* in_hdr, sdp_msg_buf_t* in_msg)
{
     use(in_hdr);
     use(in_msg);
     return false;
}

// initialise the timer process. Set up timestamps and move to run state,
// then turn on the timer callback
void TMR_init(uint32_t* sys_config)
{
     local_timestamp = sys_config[STIM_ONSET];
     cmd_state.ticks = sys_config[SIMULATION_TIME];
     cmd_state.tick_period = sys_config[TICK_PERIOD];
     cmd_state.interface_paused = STATE_RUN;
     cmd_state.buf_req_timer = cmd_state.buf_req_interval = sys_config[BUF_RQ_INTV];
     spin1_callback_on(TIMER_TICK, _timer_callback, CB_PRIORITY_TMR);
}

/* CALLBACK FUNCTIONS - cannot be static */

// Called when the timer tick goes off. Needs to update the current timestamp
// and then send buffered spikes for that timestamp.
void _timer_callback(uint simulation_time, uint reset_time)
{
     use(simulation_time);
     // a pause disables the timer tick 
     if (!cmd_state.interface_paused)
     {
        // if time has been reset immediately send anything
        // eligible for injection (it's as if the timer tick 
        // just went off) 
        if (reset_time == TIME_RESET)
	{
#ifdef INSTRUMENTATION
           // handle buffer requests if needed
           // this could be in a separate module 'instrumentation.c'.
	   instrument_timer(local_timestamp);
#endif
#ifdef RECEIVER
           // and then send all the loaded spikes for that timestamp.
           send_old_spikes();
#endif
           // once injection has been completed we can reenable the timer
           // callback for events which was disabled when the set time 
           // message was received.
           spin1_callback_on(TIMER_TICK, _timer_callback, CB_PRIORITY_TMR);
        }
        // first update the timestamp
        if ((++local_timestamp >= cmd_state.ticks) && (!cmd_state.infinite_run))
        {
	   // if we have reached the end of the simulation, enter pause state
           cmd_state.interface_paused = STATE_PAUSE;
           // and register the handling for a possible restart
           simulation_handle_pause_resume(_cmd_resume_callback);
#ifdef INSTRUMENTATION           
           // finish any recording that needs to be done
           instrumentation_pause();
#endif
           // back up the timestamp so that a resume will start at the right
           // time.
           local_timestamp--;
        }
#if (defined INSTRUMENTATION || defined RECEIVER)
        else
	{
#ifdef INSTRUMENTATION
           // handle buffer requests if needed
           // this could be in a separate module 'instrumentation.c'.
	   instrument_timer(local_timestamp);
#endif
#ifdef RECEIVER
           // and then send all the loaded spikes for that timestamp.
           send_old_spikes();
#endif
        }
#endif
     }
}



