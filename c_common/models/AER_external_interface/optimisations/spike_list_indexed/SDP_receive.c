#include <common-typedefs.h>
#include <debug.h>
#include <simulation.h>
#include <spin1_api.h>
#include <sark.h>
#include <data_specification.h>
#include <praerie-typedefs.h>
#include <praerie_interface.h>
#include <eieio_interface.h>
#include "PRAERIE-consts.h"
#include "PRAERIE-proc-typedefs.h"
#include "SDP_receive.h"
#include "DMA_receive_buffer.h"

#ifdef INSTRUMENTATION
#include "instrumentation.h"
#endif

sdp_ll_t* sdp_msg_ll_head; // where to remove new messages from the list
sdp_ll_t* sdp_msg_ll_tail; // where to insert new messages from the list
sdp_msg_buf_t* sdp_msg_buf; // the buffer for SDP messages
extern seq_entry_t sequence_counter[MAX_SUPPORTED_DEVICES];
extern spike_buf_t* read_pipeline[NUM_DMA_PIPE_BUFS][2];
static spike_buf_t* *const last_dma_pipe_buf[2] = {&read_pipeline[NUM_DMA_PIPE_BUFS-1][NO_PAYLOAD], &read_pipeline[NUM_DMA_PIPE_BUFS-1][WITH_PAYLOAD]};

// ------------------- sdp message buffer management ---------------------------

static bool sdp_buf_init(void)
{
       // set up the buffer linked list
       if ((sdp_msg_ll_head = sdp_msg_ll_tail = sark_alloc(AER_SEQ_WINDOW, sizeof(sdp_ll_t))) == NULL) return false;
       // and the buffers themselves. There is one more buffer than there are
       // linked-list entries because one may be being serviced by the DMA done 
       // process and can't be released back to the linked-list pool until it's
       // been completely serviced.
       if ((sdp_msg_buf = sark_alloc(AER_SEQ_WINDOW+1, sizeof(sdp_msg_buf_t))) == NULL) return false;
       // initialise the linked-list pointers and sdp buffer tags
       for (uint32_t buf_idx = 0; buf_idx < AER_SEQ_WINDOW-1; buf_idx++)
       {
           sdp_msg_ll_tail[buf_idx].next = &sdp_msg_ll_tail[buf_idx+1];
           sdp_msg_buf[buf_idx].tag = DMA_TAG_FREE;
       }
       // didn't get the last 2 tags so initialise these
       sdp_msg_buf[AER_SEQ_WINDOW-1].tag = sdp_msg_buf[AER_SEQ_WINDOW].tag = DMA_TAG_FREE;
       // last entry points to the first entry in the list
       sdp_msg_ll_tail[AER_SEQ_WINDOW-1].next = sdp_msg_ll_head;
       return true;  
}

static sdp_msg_buf_t* get_sdp_msg_buf(void)
{
     // get a fresh buffer from the internal sdp message pool
     // we could do this with a circular buffer or linked list of free pool entries,
     // but given that the pool is expected to be small it would gain very little.  
     for (sdp_msg_buf_t* free_entry = sdp_msg_buf; free_entry <= &sdp_msg_buf[AER_SEQ_WINDOW]; free_entry++)
     {
         // looking for a free entry    
         if (free_entry->tag == DMA_TAG_FREE)
	 {
	    // and allocate it if available 
            free_entry->tag = DMA_TAG_SVC;
            return free_entry;
         } 
     }
     // if no entries were available return a null pointer.
     return NULL;
}

// linked-list buffer management for incoming SDP messages

/* buffer insert. This uses a linked list structure to maintain a buffer of 
   incoming SDP-bundled PRAERIE packets in sequence order. If an out-of-sequence
   packet is received it can be buffered awaiting potential late packets. However
   the process will not wait forever; if the buffer-remove process gets to the 
   out-of-order packet first, late packets that should have been before it will
   be dropped. SDP is a non-queuable callback with no interactions with the
   priority -1 MC packet-receive process. Thus while it interacts with the DMA
   process in processing an SDP buffer all its interactions require no critical
   sections. In particular the pointer juggling required to insert into the 
   linked list of SDP entries 
*/  
static sdp_ll_t* sdp_buf_insert(sdp_msg_t* sdp_msg, int aer_seq)
{
     // immediately exit if the buffer is full
     if (sdp_msg_ll_tail->next == sdp_msg_ll_head) 
        return NULL;
     // otherwise prepare a new entry
     sdp_ll_t* new_sdp_msg_ll_entry = sdp_msg_ll_tail->next;
     // again exit if the buffer has no free entries
     if ((new_sdp_msg_ll_entry->msg = get_sdp_msg_buf()) == NULL)
        return NULL; 
     // start looking for the insert position from the earliest list entry 
     sdp_ll_t* insert_pos = sdp_msg_ll_head; 
     // received a packet too late. Drop it.
     if ((sdp_msg->tag == insert_pos->tag) && ((aer_seq-insert_pos->seq >= AER_SEQ_HORIZON) && aer_seq > 0))
        return NULL;
     // search through the list looking for the insert position: where the 
     // sequence number of this packet is just before the one for the next 
     // position.
     while ((insert_pos != sdp_msg_ll_tail) && ((sdp_msg->tag != insert_pos->next->tag) || (aer_seq-insert_pos->next->seq < AER_SEQ_HORIZON)))
           insert_pos = insert_pos->next;
     // sequence number will become a DMA tag when writing to SDRAM
     new_sdp_msg_ll_entry->seq = aer_seq;
     // sdp tag will be recorded at dequeue time if recording for the tag is on.
     new_sdp_msg_ll_entry->tag = sdp_msg->tag;
     // if we are inserting the linked-list pointers need juggling
     if (insert_pos != sdp_msg_ll_tail)
     {
        // update the various linked-list pointers
        sdp_msg_ll_tail->next = new_sdp_msg_ll_entry->next; // for the last valid entry
        new_sdp_msg_ll_entry->next = insert_pos->next;      // for the new entry
        insert_pos->next = new_sdp_msg_ll_entry;            // for the insert point        
     }
     // otherwise only the tail needs updating
     else
        sdp_msg_ll_tail = new_sdp_msg_ll_entry; // update the tail list pointer
     // store the mailbox pointer so it can be released after the copy 
     new_sdp_msg_ll_entry->msg->mailbox = sdp_msg;
     // and set the internal pointer to the beginning of the structure's 
     // physical buffer. We do this so as to be able to change the effective
     // buffer start point later without actually copying the contents.
     new_sdp_msg_ll_entry->msg->msg = &new_sdp_msg_ll_entry->msg->sdp_msg;
     // copy the sdp packet into the buffer entry. First try DMA
     if ((new_sdp_msg_ll_entry->msg->transfer_ID = spin1_dma_transfer(DMA_TAG_SVC, (uint*) sdp_msg, (uint*) new_sdp_msg_ll_entry->msg->msg, DMA_READ, sizeof(sdp_msg_t))) == FAILURE)
     { 
        // but if this fails fall back on manual transfer 
        spin1_memcpy(new_sdp_msg_ll_entry->msg->msg, sdp_msg, sizeof(sdp_msg_t));
        // release the message to the SDP free pool
        spin1_msg_free(sdp_msg);
        // indicate that the buffer is immediately available
        new_sdp_msg_ll_entry->msg->tag = DMA_TAG_READY;
        /* and since the buffer is immediately available trigger the DMA dequeue
           Marginal possibility of triggering redundant DMA dequeue processes
           here if an existing DMA dequeue is in progress. Could use a DMA
           process flag if desired, but this seems unnecessary in view of the
           extreme unlikelihood that extra processes will overload the system. 
        */
        if (!spin1_schedule_callback(_dma_done_callback, FAILURE, DMA_TAG_READY, CB_PRIORITY_DMA_DONE))
        {
	   // but if the task queue is full, we can either wait for 
           // any existing DMA to finish, or if none is in progress
           // resort to desperation tactics 
           if (dma_committed_pipe_bufs())
           {
              // No DMA was in progress. We are not dead yet; try
              // scheduling a dummy DMA to give the task queue a chance to
              // empty, and then run the DMA process 
              // ***CAUTION: What should the source be? sdp_msg *might* be being actively written to
              // by an incoming packet. Previously this had the (probably out-of-date) mailbox variable.
              if (spin1_dma_transfer(DMA_TAG_READY, (uint*) sdp_msg, (uint*) (*last_dma_pipe_buf[1])->spikes, DMA_READ, sizeof(sdp_msg_t)) == FAILURE)
#ifdef INSTRUMENTATION 
                 instrument_error("Could not schedule SDP dequeue process\n");
#else
                 rt_error(RTE_SWERR);
#endif
                 // Couldn't do that either. Something has gone very wrong.
           }   
	}
        
     }
     return sdp_msg_ll_tail;
}

/* INTERFACE FUNCTIONS - cannot be static */

/* buffer remove. This has the rather simpler job of pulling out entries from 
   the linked list. It is expected that this will be called from the DMA callback
   process which will need a short critical section because the SDP received
   callback may insert a new entry into the linked list. When an entry is removed
   and thus ready for DMA it is marked by setting the tag to the seq value. So
   the buffer manager (in DMA_receive_buffer_process.c) can know exactly what
   state a given message in the SDP buffer is: available for use, waiting for
   service in the input linked-list, or pending DMA.   
 */

/* Note that if DMA-active flags are kept, to prevent redundant DMA
   invocations in the case where the SDP process failed in its attempt
   to retrieve the SDP message via DMA, resorted to manual transfer,
   and triggered another DMA request itself (which may have been done
   while an existing DMA complete was being serviced, e.g. during read
   operations at the beginning of every timestep), then NULL returns will
   have to be within a critical section and the DMA active flag cleared here,
   because this result indicates the DMA process will definitely be
   exiting, and if the SDP receive process decides at this exact point
   to insert a new SDP, it would detect DMA done as active, not schedule
   another call of the process, and the newly-inserted message would 
   languish in the buffer until some other DMA completed (if ever). The 
   dynamic conditions required for such an eventuality are, however, 
   improbable and exotic.
*/ 
sdp_msg_buf_t* sdp_buf_remove(void)
{
     // immediately exit if the buffer is empty
     if (sdp_msg_ll_head->msg == NULL)
        return NULL;
     // otherwise get the message at the link location
     sdp_msg_buf_t* removed_entry = sdp_msg_ll_head->msg;
     // only remove if it's ready for service
     if (removed_entry->tag != DMA_TAG_READY)
        return NULL;
     // set the tag so that the entry is now ready for DMA
     removed_entry->tag = (uint) sdp_msg_ll_head->seq;
     // invalidate the message in the linked list     
     sdp_msg_ll_head->msg = NULL;
     // Brief critical section.
     uint cpsr = spin1_irq_disable();
     // update the head pointer. 
     sdp_msg_ll_head = sdp_msg_ll_head->next;
     spin1_mode_restore(cpsr);
     return removed_entry;          
}

// ------------------- ends buffer management section --------------------------

// initialisation for SDP receive process
bool SDP_init(uint sdp_pkt_recv_port)
{
     // initialise the buffers (linked list and SDP packet bufs) 
     if (sdp_buf_init())
     {
        // and if this succeeds turn on the SDP receive callback
        simulation_sdp_callback_on(sdp_pkt_recv_port, _sdp_packet_received_callback);
        return true;
     }
     // failure to initialise buffers is fatal.
     else return false;
}

/* CALLBACK FUNCTIONS - cannot be static */

// Called when an sdp packet is received
void _sdp_packet_received_callback(uint mailbox, uint dest_port) 
{
     use(dest_port);
     sdp_msg_t* cur_msg = (sdp_msg_t*) mailbox;
     // quick check on sequence number in the message
     // then heave into the SDP buffer. Dump if the buffer is full.
     if (sdp_buf_insert(cur_msg, get_aer_seq_from_sdp(cur_msg)) != NULL) return;
#ifdef INSTRUMENTATION
     // couldn't insert message, record in provenance
     incr_prov(SDP_IN_DROPS, 1);
#else
     log_warning("sdp message dropped in receive process: buffer full");
#endif
     // free message and exit 
     spin1_msg_free(cur_msg);       
}
