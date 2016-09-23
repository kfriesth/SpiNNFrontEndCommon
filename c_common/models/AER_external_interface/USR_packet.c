#include <common-typedefs.h>
#include <debug.h>
#include <simulation.h>
#include <spin1_api.h>
#include <sark.h>
#include <data_specification.h>
#include <praerie-typedefs.h>
#include <praerie_interface.h>
#include <eieio_interface.h>
#include <circular_buffer.h>
#include "PRAERIE-consts.h"
#include "PRAERIE_codec.h"
#include "USR_packet.h"
#ifdef INSTRUMENTATION
#include "instrumentation.h"
#endif

#ifdef TRANSMITTER 
/*
  State variables. Many need to be global in order to be persistent across calls
  of the user callback, which in turn they need to be so that they don't
  disappear if the send buffer is already full, because the dequeue process from
  the input circular buffer is irreversible.
*/
sdp_msg_t* sdp_msg_build; // current SDP message being built
praerie_hdr_t aer_hdr_build; // current PRAERIE header for packet being built
eieio_hdr_t legacy_hdr_build; // any legacy header being built
spinn_event_t current_spike; // dequeued spike.
extern volatile bool mc_pkt_busy; // flag that says there are spikes to process
extern const praerie_hdr_t aer_hdr_null; // blank header defined so we can
static const praerie_hdr_t *const p_aer_hdr_null = &aer_hdr_null; // take a pointer to it
extern circular_buffer in_spike_buf; // queue of incoming spikes
static circular_buffer send_buf; // queue of messages to send
static short send_buf_full; // flag to buffer sender to resume packet process

/* get a new sdp message for the sender to use. The returned message needs to
   be assigned to an internal sdp message buffer
*/
static inline sdp_msg_t* start_new_sdp_msg(void)
{
       sdp_msg_t* new_msg;
       if ((new_msg = spin1_msg_get()) == NULL)
       {
          // If the send process is active we can wait for it to resume and
	  // try again, but if there's nothing to send, the input is overloaded. 
          if (send_buf_full == SEND_BUF_EMPTY) 
          {
             // little choice but to dump all queued MC packets 
             // since we are still operating in high priority  
             uint32_t new_mc_drops = 1;
             while (circular_buffer_get_next(in_spike_buf, &current_spike.key)) new_mc_drops++;
#ifdef INSTRUMENTATION
             // track multicast drops for post-mortem reporting
             incr_prov(MC_IN_DROPS, new_mc_drops);
#else
             log_warning("spike %u dropped (payload %u) in user dequeue", current_spike.key, current_spike.payload);
#endif
             // reset the header to clean out for fresh packets
             aer_hdr_build = aer_hdr_null;
          }
       }
       return new_msg;
}
#endif

/* INTERFACE FUNCTIONS - cannot be static */

// initialisation of the user process
bool USR_init(uint32_t send_buf_size)
{
#ifdef TRANSMITTER
     // failure to initialise the circular send buffer is fatal
     if ((send_buf = circular_buffer_initialize(send_buf_size)) == NULL)
        return false;
     else
     {
        // otherwise initialise all the internal variables to a blank condition
        send_buf_full = SEND_BUF_EMPTY;
        sdp_msg_build = NULL;
        aer_hdr_build = aer_hdr_null;
        // then turn on the callback
        spin1_callback_on(USER_EVENT, _user_event_callback, CB_PRIORITY_USR);
        return true;
     }
#else
     use(send_buf_size);
     spin1_callback_on(USER_EVENT, _user_event_callback, CB_PRIORITY_USR);
     return true;
#endif
}

/* CALLBACK FUNCTIONS - cannot be static */

/* User callback. This is triggered when new MC packets have arrived and need to
   be serviced. The MC packet process can either trigger a direct callback at 
   non-queuable (pre-emptive) priority or queuable priority. In the latter case,
   to avoid complications from further events arriving, the process immediately 
   will raise itself to non-queuable status. The process builds an outgoing SDP
   message: a PRAERIE packet bundled in SDP. As long as there are similar spikes
   to process (i.e. they can be included in the same header) this process will
   bundle them all in. The MC packet process being able to interrupt this one,
   this means that new arriving spikes may be bundled into the same message even
   if it was already being built. This callback will also relinquish control to 
   the buffer sender if there are no more SDP messages available - indicating it
   has already used them all up. The sender will then send the messages and 
   release the buffers, returning control to this function to continue packing
   spikes. There is a bit of an awkward detail in that the incoming MC packet
   buffer uses an irreversible dequeue mechanism, so if this (user) process 
   dequeues an event and then can't pack it into an SDP message it's got to hold
   the event temporarily while waiting for old messages to be sent. Likewise if
   a newly-packed message can't be sent because the send buffer is already full,
   it's got to wait until some messages (only 1 is requred) have been sent before
   it can be queued for sending. When exiting the process, since it is always 
   waiting for the possibility of a new MC event arriving, we need a critical 
   section because the decision to exit means we break out of the
   packet-processing loop and if a spike arrives right then (and triggers the MC
   packet process), it would otherwise think this one still 'alive' and then the
   event would languish in the buffer.

   This callback can also be triggered from the timer callback, for recording.
   A bit ugly here; it would be preferable simply to invoke the recording
   function directly from timer. But because the current implementation of
   recording requires that there be no pre-emption of the update by an SDP
   packet arriving (which like this function is non-queuable) we have to put it
   here so it will stay non-queuable and thus not be susceptible to SDP 
   packets arriving.
 */

void _user_event_callback(uint call_priority, uint time_if_timer_called) 
{ 
     praerie_hdr_t hdr_encode; // header template looked up from CAM
     
#ifdef INSTRUMENTATION
     // call priority for timer indicates we are invoking the recording
     // functionality.
     if (call_priority == CB_PRIORITY_TMR)
     {
        recording_do_timestep_update(time_if_timer_called);
        return;
     }
#endif
#ifdef TRANSMITTER
     // call priority indicating we have deferred the callback by queueing it.
     if (call_priority == CB_PRIORITY_USR_DEFER)
     {
        // immediately escalate to non-queueable state 
        if (spin1_trigger_user_event(CB_PRIORITY_USR, NULL_ARG) == FAILURE)
	{
	   // if this fails something very wrong has happened, most likely
           // a bad exit from a previous user event.  
	   rt_error(RTE_SWERR);
        }
        // otherwise the call should have handled everything and we are exiting.
        else return;
     }
     if (send_buf_full == SEND_BUF_FULL) return; // Send buffer is full. Wait for it to empty.     
     // first thing: queue existing ready-to-send messages 
     if ((sdp_msg_build != NULL) && !circular_buffer_add(send_buf, (intptr_t)sdp_msg_build))
     {
        // if the buffer is full pass control to the sender 
        send_buf_full = SEND_BUF_FULL;
        return;
     }
     // was there a spike waiting to be packed because the SDP pool was exhausted? 
     if (!praerie_hdr_type_equiv(&aer_hdr_build, p_aer_hdr_null))
     {
        // Yes. Get a new message, but pass control back to the sender if
        // no message could be retrieved. 
        if ((sdp_msg_build = start_new_sdp_msg()) == NULL) return;
        // Pack the spike into a packet 
        sdp_aer_pkt_encode(sdp_msg_build, current_spike, &aer_hdr_build);
     }
     // main loop. Continue as long as there are still spikes to pack.
     while (mc_pkt_busy)
     {
           // critical section because no spikes might mean we will exit. 
           uint curr_cpsr = spin1_int_disable();
           if (circular_buffer_get_next(in_spike_buf, &current_spike.key)) // dequeue spike buffer  
           {
              // still more spikes. Can end critical section. 
              spin1_mode_restore(curr_cpsr);
              if (mc_praerie_hdr_lookup(&hdr_encode, current_spike)) // associate packet with header
              {
                 // header same as the packet already being built?
                 if (praerie_hdr_type_equiv(&aer_hdr_build, &hdr_encode))
                 {
                    // Yes. Add to existing packet
                    sdp_aer_pkt_encode(sdp_msg_build, current_spike, &aer_hdr_build);  
                 }
                 else
                 {
                    // No. Send existing packet and start building a new one.
                    aer_hdr_build = hdr_encode; // copy the new header
                    // Only send if there is an existing packet
                    if (sdp_msg_build != NULL)
                    {
		      if (!circular_buffer_add(send_buf, (intptr_t)sdp_msg_build))
                       {
                          // if the buffer is full pass control to the sender 
                          send_buf_full = SEND_BUF_FULL;
                          return;
                       }
                       else
                       {
                          // if the buffer is empty need to trigger the sender 
                          if (send_buf_full == SEND_BUF_EMPTY)
                          {
                             // and set the flag.
                             send_buf_full = SEND_BUF_NONFULL; 
                             spin1_schedule_callback(_buffer_sender, 0, 0, CB_PRIORITY_SDP_SEND);
                          }
                       } // ends (circular_buffer_add) - send any existing packet
                    } // ends (sdp_msg_build != NULL) - existing packet ready to send 
                    // then get a new packet from the message pool
                    // pass control to sender if no messages are available
                    if ((sdp_msg_build = start_new_sdp_msg()) == NULL) return;
                    //  otherwise pack the new spike into the packet 
                    sdp_aer_pkt_encode(sdp_msg_build, current_spike, &aer_hdr_build);
                 } // ends (!praerie_hdr_type_equiv) - different spike destination than last
              } // ends (mc_praerie_hdr_lookup(...)) - spike is valid in outgoing table
           } // ends if (circular_buffer_get_next(...)) - MC buffer dequeuer
           else
           {
              // check for exit condition: no more packets and sdp_msg_build
              // has been cleared. Still in critical section. 
              if (sdp_msg_build == NULL)
              {
                 // exiting. Clear the process-active flag.  
                 mc_pkt_busy = false;
                 // end critical section
                 spin1_mode_restore(curr_cpsr);
                 return;
              }
              else
              {
                 // not exiting. Can end critical section.
                 spin1_mode_restore(curr_cpsr);
                 // Last packet. Clear the aer header in preparation for new packets
                 aer_hdr_build = aer_hdr_null;
                 // try to send the last packet
                 if (!circular_buffer_add(send_buf, (intptr_t)sdp_msg_build))
                 {
                    // but if the send buffer is full pass control to the sender 
                    send_buf_full = SEND_BUF_FULL;
                    return;
                 }
                 else
                 {
                    if (send_buf_full == SEND_BUF_EMPTY)
                    {
                       // if the sender process is not yet active start it and set the flag.
                       send_buf_full = SEND_BUF_NONFULL;
                       spin1_schedule_callback(_buffer_sender, 0, 0, CB_PRIORITY_SDP_SEND);
                    }
                    // clear the current sdp message which has been sent.
                    sdp_msg_build = NULL;
                 } // ends circular_buffer_add() - queued SDP message
              } // ends (sdp_msg_build != NULL) - last packet to send via SDP 
           } // ends !circular_buffer_get_next() - no more MC packets
     }  // ends while (mc_pkt_busy) - main loop
#endif
}

#ifdef TRANSMITTER
/* buffer sender is a callback without an event which is always triggered from
   the user callback. It sends SDP messages containing outgoing PRAERIE events
   (at relatively low priority). This process is given low priority so that the
   more critical input processes in either direction don't get backed up, however
   it is higher than the timer priority to ensure that packets will be sent before
   the next timer tick, if only barely. It also has to interact with the user
   callback in the 'reverse' direction, triggering it if the user callback couldn't
   bundle more packets to be sent because there were already too many pending. So
   the sender can't be starved by too many incoming packets - it will eventually
   send everything it has. 
*/ 
void _buffer_sender(uint unused0, uint unused1)
{

     sdp_msg_t* sdp_msg_send; // the sdp message to send. Retrieved from buffer.

     use(unused0);
     use(unused1);

     // keep dequeueing packets
     while (circular_buffer_get_next(send_buf, &sdp_msg_send))
     {
       // and sending them
       if (spin1_send_sdp_msg(sdp_msg_send, SDP_SEND_TIMEOUT) == FAILURE)
       {
#ifdef INSTRUMENTATION
	  // unless they can't be sent in the time allowed in which case drop. 
          incr_prov(SDP_OUT_DROPS, 1);
#else
          log_warning("sdp message dropped in user process from timeout");
#endif
       }
       // release the send messages back to the pool 
       spin1_msg_free(sdp_msg_send);
       // until all messages are sent the buffer should be set to NONFULL.
       send_buf_full = SEND_BUF_NONFULL;
       /* are there still packets to process in the input queue?
          note that this does not need a critical section. _buffer_sender is
          operating in low-priority (2) mode and the _user_event_callback is
          non-queuable. We therefore know it's not running. If perchance
          the FIQ gets there first, and triggers the callback, it can only 
          either exit by waiting for the buffer sender to dequeue packets,
          or exit with !mc_packet_busy. In the latter situation the callback
          will harmlessly bottom out of the packet-dequeue loop and return here
          anyway, no damage done. 
       */
       if (mc_pkt_busy)
       {
          // revive the MC dequeue process if it was stalled on SDP queue full.
          // we don't have to worry about the trigger failing because if we 
          // are running buffer_sender then by definition no user callback is
          // in progress.           
          spin1_trigger_user_event(CB_PRIORITY_USR, NULL_ARG);
       }
     }
     // and signal that there's nothing left to send when done.
     send_buf_full = SEND_BUF_EMPTY;
}
#endif
