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
#include "DMA_receive_buffer.h"
#include "SDP_receive.h"
#include "Command_processor.h"
#include "PRAERIE_codec.h"

#ifdef INSTRUMENTATION
#include "instrumentation.h"
#endif


spike_buf_t* immediate_inject_buf[2]; // injection buffers, with and without payloads
spike_buf_t* read_pipeline[NUM_DMA_PIPE_BUFS][2]; // DMA pipeline, with/without payloads 

// variable retaining number of spikes remaining and payload/timestamp
// for current sdp packet being decoded
static pkt_props_t sdp_pkt_properties;

static praerie_hdr_t packet_header;
static eieio_hdr_t pkt_legacy_header;
static sdp_msg_buf_t* active_sdp_buf;
static spike_ll_t* sdram_evt_buf[2]; // start of SDRAM buffer region
static spike_ll_t* sdram_end_buf[2]; // end of SDRAM_buffer region
spike_ll_t* sdram_w_buf[2]; // current position in SDRAM to write to via DMA
spike_ll_t* sdram_r_buf[2]; // current position in SDRAM to read from via DMA
bool sdram_has_buffers[2]; // flags indicating SDRAM has some buffers stored

extern sdp_msg_buf_t* sdp_msg_buf; // the buffer for SDP messages
extern timer_t local_timestamp;
// top-level structures to contain the memory layout of the PRAERIE transceiver
extern data_spec_layout_t data_spec_layout;

// time-comparison operators. Static for the moment; if these have more 
// general use they can be placed in DMA_receive_buffer.h
static inline bool t_geq(timer_t ts1, timer_t ts2)
{return ts1 >= ts2;}

static inline bool t_leq(timer_t ts1, timer_t ts2)
{return ts1 <= ts2;}

static inline bool t_lt(timer_t ts1, timer_t ts2)
{return ts1 < ts2;}

static inline bool t_always(timer_t ts1, timer_t ts2)
{return true;}

static inline bool t_never(timer_t ts1, timer_t ts2)
{return false;}

// functions to get the event at certain fixed positions in a buffer
static inline praerie_event_t latest_spike(spike_buf_t* spike_buf)
{return spike_buf->spikes[spike_buf->spikes[0].link.last].spike;}

static inline praerie_event_t earliest_spike(spike_buf_t* spike_buf)
{return spike_buf->spikes[spike_buf->spikes[0].link.first].spike;}

// functions to get the pipeline buffers

// initialisation for spike buffer linked lists
static void spike_ll_init(spike_ll_t* buf)
{
     // set up the start indices into the linked list
     buf[0].link.first = 0;
     buf[0].link.last = 0;
     buf[0].link.next = 1;
     buf[0].link.prev = 1;
     // then initialise the rest of the linked-list indices.
     for (uint event = 1; event < SPIKE_BUF_MAX_SPIKES; event++)
     {
         buf[event].link.next = event+1;
         buf[event].link.prev = event-1;
     }
}

// initialisation for the event buffers
static void spike_buf_init(spike_buf_t* buf, uchar buf_num)
{
     // set which pipeline stage this is
     buf->pipe_stage = buf_num;
     // and initialise the tag. Only buffer 0 is initially at FILL_READ
     if (buf_num) buf->tag = DMA_TAG_IN_PIPELINE | DMA_TAG_READY;
     else buf->tag = DMA_TAG_IN_PIPELINE | DMA_TAG_FILL;
     // then initialise the spike linked list
     spike_ll_init(buf->spikes);
}

static bool buffer_init(uint32_t* payload_sz)
{
     // initialise buffers dependent on the type expected. Zero size for the
     // SDRAM allocation indicates no packets are expected of this type. First
     // do packets without payloads
     for (uint32_t with_payload = NO_PAYLOAD; with_payload <= WITH_PAYLOAD; with_payload++)
     {
         if (payload_sz[with_payload]) 
         {
            // allocate pipeline buffers in SDRAM
            // old xalloc method superseded by dsg pre-allocation        
            // if ((sdram_w_buf[NO_PAYLOAD] = sdram_r_buf[NO_PAYLOAD] = sark_xalloc(sv->sdram_heap, no_payload_sz*sizeof(spike_ll_t), TAG_NOPAYLOAD_ALLOC, ALLOC_ID)) == NULL) return false;
            sdram_evt_buf[with_payload] = sdram_w_buf[with_payload] = sdram_r_buf[with_payload] = (spike_ll_t*)(data_spec_layout.regions[NO_PAYLOAD_BUFFER_REGION+with_payload]);
            sdram_end_buf[with_payload] = sdram_evt_buf[with_payload] + payload_sz[with_payload];
            // clear the flag to indicate the SDRAM doesn't have buffers written.
            sdram_has_buffers[with_payload] = false;
            // and then buffers in DTCM. 
            // first do the immediate injection buffer. Allocate the memory
            if ((immediate_inject_buf[with_payload] = sark_alloc(1, sizeof(spike_buf_t))) == NULL) return false;
            // then initialise the linked list
            spike_ll_init(immediate_inject_buf[with_payload]->spikes);
            // and then the pipeline buffers. Minor memory leak: as implemented here
            // we are not freeing previously-allocated buffers if a new allocation
            // fails, but this is a fatal error anyway and the entire application will 
            // abort.
            for (uchar buf = 0; buf < NUM_DMA_PIPE_BUFS; buf++)
	    {
                if ((read_pipeline[buf][with_payload] = sark_alloc(1, sizeof(spike_buf_t))) == NULL) return false;
                // after allocating memory initialise the buffer
                spike_buf_init(read_pipeline[buf][with_payload], buf);
            } 
         }
     }
     return true;
}

static inline spike_buf_t* next_free_buf(uint32_t with_payload)
{
       for (uint offset = 0; offset < NUM_DMA_PIPE_BUFS; offset++)
       {
           if (read_pipeline[offset][with_payload]->tag == DMA_TAG_FREE) return read_pipeline[offset][with_payload];
       }
       return NULL;
}

// 3 simple convenience functions to determine buffer status

static inline bool dma_buf_full(spike_ll_t* buf) 
{return buf[buf[0].link.first].link.prev == SPIKE_BUF_MAX_SPIKES;}

static inline bool dma_buf_empty(spike_ll_t* buf) 
{return buf[buf[0].link.first].link.prev == 0;}

static inline spike_buf_t* cur_read_buf(uint32_t with_payload) 
{
       if (!(read_pipeline[EARLY_BUF][with_payload]->tag & (DMA_TAG_FILL | DMA_TAG_FULL))) return NULL;
       else return read_pipeline[EARLY_BUF][with_payload];
}

static uint8_t dma_open_write_bufs(uint32_t with_payload)
{
       uint8_t num_open = 0;
       for (uint32_t buf = 0; buf < NUM_DMA_PIPE_BUFS; buf++)
       {
           if (read_pipeline[buf][with_payload]->tag & DMA_TAG_FILL) num_open++;
           if (with_payload == DONT_CARE_PAYLOAD && read_pipeline[buf][NO_PAYLOAD]->tag & DMA_TAG_FILL) num_open++;
       }
       return num_open;
}

// function to make sure the spike is not too late: compare against the current
// earliest buffer in comp_buf.
bool spike_is_too_late(praerie_event_t* spike, spike_buf_t* comp_buf, bool latest, uint32_t has_payload)
{
     // latest indicates a stricter condition wherein the spike needs to be later
     // than *any* spike in the current earliest buffer (probably because the
     // current one is full)
     if (latest)
     { 
        if (spike->timestamp >= latest_spike(comp_buf).timestamp) return false;
     }
     // otherwise the more relaxed condition applies where it just needs to be 
     // at least as late as the earliest one recorded.
     else if (spike->timestamp >= earliest_spike(comp_buf).timestamp) return false;
     // if not then dump it and indicate that the spike-processing machinery can
     // grind on.
#ifdef INSTRUMENTATION
     incr_prov(ABANDONED_SPIKES+has_payload, 1);  
#else
     log_warning("buffered spike %u too late: needed at time %u, current buffer time %u", spike->address, spike->timestamp, (latest? latest_spike(comp_buf).timestamp : earliest_spike(comp_buf).timestamp));
#endif
     sdp_pkt_properties.remaining_spikes--;
     return true;
}
							    
/*
   function to insert a new event into a DMA buffer. Buffers function as 
   indexed linked-lists so that an insert is possible. In most situations
   the insert is likely to be at the end so usually this function will be
   efficient; only on a genuine in-the-middle insert will it slow because
   it will have to traverse the list looking for the insert point. Returns
   the number of events in the buffer
*/
static uchar dma_buf_insert(spike_ll_t* buf, praerie_event_t event)
{
       // initialise to the first entry
       uchar insert_point = buf[0].link.first; 
       // first entry's previous link is the count
       uchar buffer_count = buf[insert_point].link.prev;
       // exit if the buffer is full 
       if (buffer_count == SPIKE_BUF_MAX_SPIKES) return 0;
       // otherwise insert the event at the bottom of the array 
       buf[buffer_count].spike = event;
       // most common case: timestamp is at least as late as the latest.
       // insert at the end.
       if (event.timestamp >= buf[buf[0].link.last].spike.timestamp)
       {
	  // make the now-penultimate entry's next link valid
          buf[buf[0].link.last].link.next = buffer_count; 
          // and make the previous link in the last element the old last
          buf[buffer_count].link.prev = buf[0].link.last;
          // then update the last link entry in the top array element
          buf[0].link.last = buffer_count;
          // and update the count
          buf[insert_point].link.prev++;
       }
       // another simple case: timestamp is at least as early as the earliest. 
       else if (event.timestamp <= buf[insert_point].spike.timestamp)
       {
	  // new count will reside in the new first element 
          buf[buffer_count].link.prev = buffer_count+1;
          // and its next link will be to the old first element
          buf[buffer_count].link.next = insert_point;
          // update the first link entry in the top array element
          buf[0].link.first = buffer_count;
          // and finally the old first's previous link will be the new first
          buf[buf[insert_point].link.next].link.prev = insert_point;
       }
       else
       {
	  // will need to search through the list.
          // first, update the count 
	  buf[insert_point].link.prev++;
          // optional: for slightly more efficient average-case times, search
          // from the side of the buffer 'nearer' to the current event according
          // to timstamp
          if (event.timestamp-buf[insert_point].spike.timestamp > buf[buf[0].link.last].spike.timestamp-event.timestamp)
	  {
             // timestamp was closer to the last timestamp. Go through until the
             // last element with a matching or lesser timestamp is found
	     insert_point = buf[0].link.last;
             while (buf[insert_point].spike.timestamp > event.timestamp)
                   insert_point = buf[insert_point].link.prev;
          }
          else
          { 
             // timestamp was closer to the first timestamp. Go through until the
             // first element with a matching or greater timestamp is found
	     while (buf[insert_point].spike.timestamp < event.timestamp) 
                   insert_point = buf[insert_point].link.next;
          }
          // at this point insert the new element and update its next and 
          // previous links
          buf[buffer_count].link.prev = buf[insert_point].link.prev;
          buf[buffer_count].link.next = insert_point;
          // then update the next link of the now-previous element          
          buf[buf[insert_point].link.prev].link.next = buffer_count;
          // and then the previous link of the now-next element 
          buf[insert_point].link.prev = buffer_count;  
       }
       return buf[buf[0].link.first].link.prev;
}

/* function that performs the inner condition processing on the spike buffering
   decision tree. It consists of 2 parts: a comparison to make sure the spike
   is within the needed timestamp range (as against a comparison buffer in
   comp_buf) and the insertion into the buffers with appropriate updating of
   buffer state. The comparison expects an operator to be passed in comp to
   determine how to compare timestamps (which can be 'always' or 'never' to
   bypass the comparison altogether). curr_buf is the buffer spikes will be
   placed in and next_buf indicates a candidate new buffer if the existing
   buffer fills. If next_buf is void this indicates either that we know the 
   update will not fill the buffer or that there are no more available buffers. 
*/
static bool dma_buf_update(praerie_event_t* spike, spike_buf_t* comp_buf, spike_buf_t* curr_buf, spike_buf_t* next_buf, uint32_t with_payload, t_comp comp, bool latest)
{
     // this is the earliest/latest spike test
     timer_t buf_timestamp = latest ? latest_spike(comp_buf).timestamp : earliest_spike(comp_buf).timestamp;
     // is the spike within the needed timestamp range?
     if (!comp(spike->timestamp, buf_timestamp)) return false;

     // this piece is DMA_buf_update
     // if in range, place the event in the buffer
     if (dma_buf_insert(curr_buf->spikes, *spike) == SPIKE_BUF_MAX_SPIKES)
     {
        // if this fills the buffer,
        // if there is a free buffer,
        if (next_buf != NULL)
        {
	   // make the free one available
           next_buf->tag = DMA_TAG_IN_PIPELINE | DMA_TAG_FILL | (with_payload ? DMA_TAG_W_PYLD : DMA_TAG_NO_PYLD);
        }
        // mark the existing one as filled
        curr_buf->tag = DMA_TAG_IN_PIPELINE | DMA_TAG_FULL | (with_payload ? DMA_TAG_W_PYLD : DMA_TAG_NO_PYLD);
     }
     // one less spike to do.
     sdp_pkt_properties.remaining_spikes--;
     return true;
}

// function that gets the next event from the spike buffer 
static praerie_event_t dma_buf_remove(spike_ll_t* buf)
{
       // get the next available entry from the event linked list 
       spike_ll_t event = buf[buf[0].link.first];
       // but if the list was empty, invalidate the returned event by giving
       // it a timestamp before the current local time (which means it will 
       // be discarded)
       if (event.link.prev == 0) event.spike.timestamp = local_timestamp-1;
       else
       {
	  // otherwise decrement the count
          uchar new_count = event.link.prev-1;
          if (new_count)
          {
	     // and if there are still values in the linked list,
             // update the first entry to link to the next entry
             // and update the stored count value
             buf[0].link.first = event.link.next;
             buf[event.link.next].link.prev = new_count;
          }
          else
          {
	     // meanwhile if that emptied the linked list set all the initial
             // links to 0 so that it will start clean
	     buf[0].link.first = 0;
             buf[0].link.last = 0;
             buf[0].link.next = 0;
             buf[0].link.prev = 0;
          }
       }
       // and return the event
       return event.spike;
}

/* a bulk spike-injection routine to send all the spikes in a buffer. These may
   have been retrieved via DMA at the start of a timer tick from buffered
   time-delayed input, or dumped directly from an SDP message into the immediate
   injection buffer. The process will retry the send if it fails with a tunable
   number of resends and resend delays as compile-time parameters. (A CSMA-style
   random backoff would actually be a somewhat more sophisticated way of
   achieving more reliable injections but this seems like overkill). The function 
   returns the number of spikes actually sent.
*/
static uint32_t inject_spikes(spike_ll_t* spike_buffer, uint32_t with_payload)
{
         praerie_event_t spike;
         uint32_t count = 0;
         // go through all the spikes to be sent
         while ((spike_buffer[0].link.first <= spike_buffer[0].link.last) && (spike_buffer[spike_buffer[0].link.first].spike.timestamp <= local_timestamp))
	 {
               // dequeue the event
               spike = dma_buf_remove(spike_buffer);
               // dump any events that are too old
               if (spike.timestamp < local_timestamp)
#ifdef INSTRUMENTATION             
                  incr_prov(ABANDONED_SPIKES+with_payload, 1);
#else
                  log_warning("spike %u too late: needed at %u, time is now %u", spike.address, spike.timestamp, local_timestamp);
#endif
               else
               {
                  // try to send the packet, incrementing the retry count on each
                  // failure. Abort if the retry exceeds the limit.
                  uint32_t retry_count = 0;
	          while (!spin1_send_mc_packet((key_t)spike.address, (payload_t)spike.payload, with_payload) && retry_count++ <= NUM_MC_SEND_RETRIES) spin1_delay_us(MC_SEND_RETRY_DELAY);
                  // retry less than maximum indicates the packet was sent. 
                  if (retry_count <= NUM_MC_SEND_RETRIES)
		  {
#ifdef INSTRUMENTATION                     
                     // record the spike if desired
                     instrument_event_recording(RECORD_DIR_IN, with_payload, &spike, sizeof(praerie_event_t));
#endif
                     // Increment the send count and proceed to the next spike.
                     count++;
                  }
                  else
	          {
	             // otherwise reinsert the packet and end injection, hoping for
                     // a better moment later. If the reinsert fails, drop the spike.
                     // (CAUTION: watch for compiler optimisations that reorder the
                     // instruction sequence: this particular test is explicitly 
                     // critical on the value of the spike buffer's count being read
                     // first). 
	             if (spike_buffer[spike_buffer[0].link.first].link.prev == dma_buf_insert(spike_buffer, spike))
#ifdef INSTRUMENTATION
                        incr_prov(MC_OUT_DROPS+with_payload, 1);
#else
                        log_warning("spike %u at timestamp %u (payload %u) could not be sent", spike.address, spike.timestamp, spike.payload);
#endif
                     break;
                  }
               }
         }
         // return the final count of sent packets.
         return count;
}

// starts a DMA if one is asked for and updates the necessary state variables
// to track it. Returns false if the transfer could not be started, true
// otherwise. Both this and the next function assume a fixed block size of
// SPIKE_BUF_MAX_SPIKES. This can easily be made adaptive by reading from
// src_data's length field in the first element's prev field: 
// src_data->spikes[src_data->spikes[0].link.first].link.prev
static inline bool initiate_dma_write(void* dst_addr, spike_buf_t* src_data)
{
       if ((src_data->transfer_ID = spin1_dma_transfer(src_data->tag, dst_addr, src_data->spikes, DMA_WRITE, SPIKE_BUF_MAX_SPIKES*sizeof(spike_ll_t))) == FAILURE) return (spin1_schedule_callback(_initiate_manual_write, (intptr_t)src_data, (intptr_t)dst_addr, CB_PRIORITY_MANUAL_XFER) == SUCCESS);
       else return true;
}

static inline bool initiate_dma_read(spike_buf_t* dst_data, void* src_addr)
{
       if ((dst_data->transfer_ID = spin1_dma_transfer(dst_data->tag, src_addr, dst_data->spikes, DMA_READ, SPIKE_BUF_MAX_SPIKES*sizeof(spike_ll_t))) == FAILURE) return (spin1_schedule_callback(_initiate_manual_read, (intptr_t)src_addr, (intptr_t)dst_data, CB_PRIORITY_MANUAL_XFER) == SUCCESS);
       else return true;
}

/* buffer insertion and reordering into the DMA buffers. This is complicated by
   the fact that there may be multiple devices communicating and sending spikes
   possibly out of order. As a result each event in each packet must be inspected
   for its corresponding timestamp. There are then 4 different buffers a spike
   might be inserted into: 1) the immediate injection buffer for sending ASAP.
   2) the earliest pipeline buffer, which is the next candidate for sending. It
   won't be sent right away, but as time advances to it it will get sent. 3) the
   next-earliest buffer. This one will not be sent until the entirety of the
   earliest pipeline buffer is sent, but can be kept in DTCM until needed. 4) a
   writeback buffer. These buffers sit on the 'input' side of the SDRAM and will
   be written back via DMA when full. There are 2 such buffers to allow one to be
   actively written back while the next one remains available for filling. Note
   that while most of the cases are fairly obvious there are a few exotic
   transitional situations that can arise, mostly if there is a sudden input 
   burst of events with a similar timestamp, and then things go quiescent,
   allowing a lot of events to be issued. These unusual cases are noted below.
*/ 
static bool dma_buf_add_spike(praerie_event_t* spike) 
{
     praerie_tstp_t spike_time = spike->timestamp;
     uint32_t has_pyld = sdp_pkt_properties.with_payload;
     // place the spike in the immediate injection buffer right away if it is at
     // the current time or has no timestamp. 
     if (spike_time == local_timestamp)
     {
        // a failure to insert suggests the buffer is full, 
        while (!dma_buf_insert(immediate_inject_buf[has_pyld]->spikes, *spike))
	{
	   // so send the spikes now and then reinsert. 
#ifdef INSTRUMENTATION
           incr_prov(ABANDONED_SPIKES+has_pyld, immediate_inject_buf[has_pyld]->spikes[0].link.prev - inject_spikes(immediate_inject_buf[has_pyld]->spikes, has_pyld));
#else
           uint dropped_spikes = immediate_inject_buf[has_pyld]->spikes[0].link.prev - inject_spikes(immediate_inject_buf[has_pyld]->spikes, has_pyld);
           if (dropped_spikes)
              log_warning("%u spikes could not be immediately injected (probable fabric congestion)", dropped_spikes);
#endif
        }
        // one less spike to handle in the packet
        sdp_pkt_properties.remaining_spikes--;
        return true;
     }
     else
     {
        // this is a spike to be sent in the future. A logic tree handles the
        // various cases (this can probably be optimised with a switch statement
        // but we are using the tree for ease of understanding)
        // first see if it can be inserted in the earliest buffer
        if (read_pipeline[EARLY_BUF][has_pyld]->tag & DMA_TAG_FILL)
        {
	   // this is the only buffer with data
           if (read_pipeline[MID_BUF][has_pyld]->tag == DMA_TAG_FREE)
	   {
              // so immediately place the event in it
              if (dma_buf_update(spike, read_pipeline[EARLY_BUF][has_pyld], read_pipeline[EARLY_BUF][has_pyld], read_pipeline[MID_BUF][has_pyld], has_pyld, t_always, false)) return true;
	      

           }
           // there is later data in the next buffer
           if (read_pipeline[MID_BUF][has_pyld]->tag & (DMA_TAG_FILL | DMA_TAG_FULL))
           {
              // so need to test if the timestamp is earlier than this
              // later buffer and if so, place the event in it
              if (dma_buf_update(spike, read_pipeline[MID_BUF][has_pyld], read_pipeline[EARLY_BUF][has_pyld], NULL, has_pyld, t_leq, false)) return true;          
           }
           // the next buffer is being read back into. Can't know what its 
           // times might be,
           if (read_pipeline[MID_BUF][has_pyld]->tag & DMA_TAG_RB)
           {
	      // so the best we can do is test whether the timestamp is at
              // least as early as the latest one in the earliest buffer.
              if (dma_buf_update(spike, read_pipeline[EARLY_BUF][has_pyld], read_pipeline[EARLY_BUF][has_pyld], NULL, has_pyld, t_leq, true)) return true;       
           }
           /* an exotic case: the next buffer would have been read back into,
              but the buffer it would have read back from is not yet in SDRAM.
              This will happen if there were 2 full buffers on the 'output' 
              side of the pipeline, the first was all sent, and we moved on to
              the second, which is now the candidate for insertion, but there
              meanwhile had been a buffer on the 'input' side of the pipeline
              that filled and was being sent out via DMA to SDRAM. Aborting the
              transfer and moving the writeback buffer to the 'output' side is
              far too complex. But this means there is a sudden 'window' for 
              earlier spikes to arrive.
	   */
           if (read_pipeline[MID_BUF][has_pyld]->tag & DMA_TAG_RB_RDY)
           {
	      // We thus compare the spike's timestamp against the one sitting
              // in the buffer being written back. and if it is earlier, we can 
              // push the event into the early buffer.
              if (dma_buf_update(spike, read_pipeline[DMA_WBUF][has_pyld], read_pipeline[EARLY_BUF][has_pyld], NULL, has_pyld, t_leq, false)) return true; 
           }
        }
        // now look at the next-earliest buffer.
        if (read_pipeline[MID_BUF][has_pyld]->tag & DMA_TAG_FILL)
        {
	   // first case: early buffer is full. we need to make sure the timestamp 
           // is later than the earliest buffer's latest. If not, it's missed its
           // chance. 
           uint32_t early_buf_full = read_pipeline[EARLY_BUF][has_pyld]->tag & DMA_TAG_FULL;
           if (early_buf_full && spike_is_too_late(spike, read_pipeline[EARLY_BUF][has_pyld], true, has_pyld)) return true;
	   // second case: the earliest buffer still had space but the event is
           // later than the earliest one in the next-earliest buffer. 
           if (early_buf_full || read_pipeline[EARLY_BUF][has_pyld]->tag & DMA_TAG_FILL)
           {
              // if later buffers have data,
              if (read_pipeline[DMA_WBUF][has_pyld]->tag & (DMA_TAG_FULL | DMA_TAG_FILL))
              {
		 // if there are buffers in SDRAM, we need to consider them
                 // as part of the pipeline - as if they were the 'next'
                 // buffer. But we cannot know what timestamps they hold,
                 if (sdram_has_buffers[has_pyld])
                 {
		    // so best we can do is compare against the latest spike
                    // in this buffer.
                    if (dma_buf_update(spike, read_pipeline[MID_BUF][has_pyld], read_pipeline[MID_BUF][has_pyld], NULL, has_pyld, t_leq, true)) return true;  
                 }
                 else // no buffers in SDRAM
                 {
		    // so we can compare directly against the next buffer
                    if (dma_buf_update(spike, read_pipeline[DMA_WBUF][has_pyld], read_pipeline[MID_BUF][has_pyld], NULL, has_pyld, t_leq, false)) return true;  
                 }
              }
	      // otherwise this is the last buffer with data
	      else
              {
		 // so push the packet in immediately
                 if (dma_buf_update(spike, read_pipeline[MID_BUF][has_pyld], read_pipeline[MID_BUF][has_pyld], read_pipeline[DMA_WBUF][has_pyld], has_pyld, t_always, false)) return true;   
              }
           }
           // next case: earliest buffer is being read back into from a 
           // previous buffered spike block. This means earlier events
           // will come but we cannot know how early. This case should 
           // only happen when a rapid sending of early buffers got to 
           // the current one before any other packets needed to be stored
           // in SDRAM.
           if (read_pipeline[EARLY_BUF][has_pyld]->tag & DMA_TAG_RB)
           {
	      // So this should be the last buffer. Make sure it is 
              if (read_pipeline[DMA_WBUF][has_pyld]->tag == DMA_TAG_FREE)
              {
		 // best we can do is compare against the earliest in the 
                 // current buffer and push the event in if it's late enough
                 if (dma_buf_update(spike, read_pipeline[MID_BUF][has_pyld], read_pipeline[MID_BUF][has_pyld], read_pipeline[DMA_WBUF][has_pyld], has_pyld, t_geq, false)) return true; 
	      }
           }
           /* last case, another very exotic one, first, two earlier buffers 
              were completely sent, but in the meanwhile new events arrived
              that committed one buffer to SDRAM while another buffer was 
              partially filled. But the SDRAM writeback has not yet occurred
              so the very earliest buffer is still waiting to get its data.
              However we can know what that data will be since it's still in
              the 'later' buffer as well - again not transferring it directly
              across because this makes the buffer arrangements too complex.
              (This might be a bit inefficient, but the unlikelihood of this
              particular scenario probably justifies any potential performance
              penalty) 
	   */
           if (read_pipeline[EARLY_BUF][has_pyld]->tag & DMA_TAG_RB_RDY)
	   {
	      // make sure the next buffer is being written back,
              if (read_pipeline[DMA_WBUF][has_pyld]->tag & DMA_TAG_FULL)
              {
		 // if the time is earlier than the very earliest in the next
                 // buffer then it's missed its chance.
                 if (spike_is_too_late(spike, read_pipeline[DMA_WBUF][has_pyld], false, has_pyld)) return true;
		 // compare the timestamp against the latest one in there - 
                 // again remember in this strange 'inversion' that the later
                 // buffer contains earlier data. If the insert fills the
                 // current buffer, means the last buffer will 
                 // (hopefully very temporarily) have to become the new one
                 //  for later inserts.
                 if (dma_buf_update(spike, read_pipeline[DMA_WBUF][has_pyld], read_pipeline[MID_BUF][has_pyld], read_pipeline[DMA_TBUF][has_pyld], has_pyld, t_geq, true)) return true;  
              }
           }
        }
        /* there is another condition when the next-earliest buffer could be
           relevant: when it is ready for readback following a sequence that
           sent an earlier buffer and moved the next buffer to being sent,
           but meanwhile a buffer being sent to SDRAM has not yet been 
           written back. An opportunity exists for inserting a few lucky
           events.
	*/
        if (read_pipeline[MID_BUF][has_pyld]->tag & DMA_TAG_RB_RDY)
        {
	   // first possiblity: earliest buffer hasn't begun to send. 
           if (read_pipeline[EARLY_BUF][has_pyld]->tag & DMA_TAG_FULL)
           {
	      // this means it will not have been tested against earlier
              // so first check if it has definitely missed its chance.
              if (spike_is_too_late(spike, read_pipeline[EARLY_BUF][has_pyld], false, has_pyld)) return true;
              // can only place in the next-earliest buffer if there's nothing
              // in SDRAM - because in this latter case we can't know if there
              // are earlier spikes sitting there.
              if (!sdram_has_buffers[has_pyld])
              {
                 // otherwise make sure it's between the early and the late buffers
                 // in terms of time.  
                 if (spike_time >= latest_spike(read_pipeline[EARLY_BUF][has_pyld]).timestamp)
                 {
                    // 'lucky bastard' spike can be sent. This means also the buffer
                    // associated with it will be set to fillable.  
                    if (dma_buf_update(spike, read_pipeline[DMA_WBUF][has_pyld], read_pipeline[MID_BUF][has_pyld], NULL, has_pyld, t_leq, false))
		    { 
                       read_pipeline[MID_BUF][has_pyld]->tag = DMA_TAG_IN_PIPELINE | DMA_TAG_FILL;
                       return true;
                    }
                 }
              }				     
           }
        }   
        // now look at the first writeback buffer. These cases are considerably
        // simpler.  
        if (read_pipeline[DMA_WBUF][has_pyld]->tag & DMA_TAG_FILL)
        {
	   // next-earliest buffer is full 
           if (read_pipeline[MID_BUF][has_pyld]->tag & DMA_TAG_FULL)
           {
	      // so this is a simple case, merely check that the event is later
              // than the latest in that next-earliest buffer. If the current
              // buffer fills, the last buffer needs to be set up for use.
              if (dma_buf_update(spike, read_pipeline[MID_BUF][has_pyld], read_pipeline[DMA_WBUF][has_pyld], read_pipeline[DMA_TBUF][has_pyld], has_pyld, t_geq, true)) return true;  
           }
           else
           {
	      // all other cases require that the event be at least as late as
              // the earliest one in this buffer 
              if (dma_buf_update(spike, read_pipeline[DMA_WBUF][has_pyld], read_pipeline[DMA_WBUF][has_pyld], read_pipeline[DMA_TBUF][has_pyld], has_pyld, t_geq, false)) return true;             
           }
        }
        // finally look at the last buffer 
        if (read_pipeline[DMA_TBUF][has_pyld]->tag == DMA_TAG_FILL)
        {
	   /* That bizarre case has happened, where we had that 'very exotic' 
              situation in the next-earliest buffer, inserted a packet, this
              filled up the next-earliest buffer, and the writeback of the first
              writeback buffer still hasn't finished, but a new event has
              arrived. In this freakish situation, the first writeback buffer
              actually has earlier data than the next-earliest buffer. How many
              times will this case happen in actual use? Probably should be
              profiled! 
	   */
           if ((read_pipeline[EARLY_BUF][has_pyld]->tag & DMA_TAG_RB_RDY) && (read_pipeline[MID_BUF][has_pyld]->tag & DMA_TAG_FULL))
           {
	      // which means that if the time is earlier than what is in
              // the writeback buffer then the event has lost its chance
              if (spike_is_too_late(spike, read_pipeline[DMA_WBUF][has_pyld], false, has_pyld)) return true; 
	      // we actually have to test against the next-earliest buffer,
              // making sure the spike is later than it.
              if (dma_buf_update(spike, read_pipeline[MID_BUF][has_pyld], read_pipeline[DMA_TBUF][has_pyld], NULL, has_pyld, t_geq, true)) return true; 
	   }
	   else
           {
	      // otherwise we have the straighforward case of testing against
              // the first writeback buffer
              if (dma_buf_update(spike, read_pipeline[DMA_WBUF][has_pyld], read_pipeline[DMA_TBUF][has_pyld], NULL, has_pyld, t_geq, true)) return true; 
           }
        }
        // for all cases that couldn't be handled return indicating the event
        // was not processed, so don't give up hope but it will need to wait 
        // until something else happens to free buffers at the right position.
        return false; 
     }       
}

/* SDP queue servicing. New messages in the queue are pulled out in the order
   they have been arranged (generally, so that SEQ is preserved within a given
   input device but first-come-first-served between devices). The process will
   send any spikes in the current timestamp immediately; this includes spikes
   with no timestamp. It buffers timestamped spikes with a future time to SDRAM
   for later retrieval and will attempt to use DMA if the number of spikes is 
   reasonably large (over the compile-time parameter DMA_SPIKE_THRESHOLD) but 
   otherwise just memcpy's them. The process can pause and resume if there are
   no free buffers available for DMA and the current message has not been fully
   emptied. 
*/
static void buffer_or_send_new_spikes(void)
{
       praerie_event_t curr_event; // variable for event decoded from SDP buffer
       spike_buf_t* buf_in_svc;  // container for a buffer being serviced
       // handle new spikes. 
       // Look for unfinished SDP buffers ready to process 
       while ((active_sdp_buf = (active_sdp_buf == NULL ? sdp_buf_remove() : active_sdp_buf)) != NULL)
       {
             /* handle the currently active SDP buffer. This may be a resume
                from a paused condition or it may be a fresh, just-dequeued
                SDP message. In either case the remaining_spikes indicator will
                tell us when there's still work to do on the current buffer. 
             */
             if (sdp_pkt_properties.remaining_spikes)
             {
                /* get the event from the message. note that
                   sdp_aer_pkt_decode should insert local_timestamp into
                   the timestamp field of its structure if the packet doesn't
                   have timestamps. If the packet contains more events than
                   are still available for processing in the tick concerned
                   then only read enough to fill the available limit. 
		*/ 
                curr_event = sdp_aer_pkt_decode(&packet_header, active_sdp_buf, &sdp_pkt_properties);
                // Discard spikes with timestamps too late to be processed.
                if (curr_event.timestamp < local_timestamp)
		{
                      sdp_pkt_properties.remaining_spikes--;
#ifdef INSTRUMENTATION
                      if (praerie_pkt_has_payload(&packet_header)) incr_prov(ABANDONED_SPIKES+WITH_PAYLOAD, 1);
                      else incr_prov(ABANDONED_SPIKES+NO_PAYLOAD, 1);
#else
                      log_warning("spike %u too late: needed at time %u, current time %u", curr_event.address, curr_event.timestamp, local_timestamp);
#endif    
                }
		else
                {
                   if (dma_buf_add_spike(&curr_event))	 
		   {
		      // a spike was added. Check if a block needs to be 
                      // written to SDRAM via DMA. Continuously loop via
                      // while to write back as many buffers as needed.
                      while (((buf_in_svc = read_pipeline[DMA_WBUF][sdp_pkt_properties.with_payload])->tag & DMA_TAG_FULL) && (buf_in_svc->transfer_ID == FAILURE) && !(buf_in_svc->tag & DMA_TAG_MANUAL))
                      {
                         // new spikes to be buffered. Make sure SDRAM buffer space exists
                         if (!(sdram_has_buffers[sdp_pkt_properties.with_payload] && sdram_w_buf[sdp_pkt_properties.with_payload] == sdram_r_buf[sdp_pkt_properties.with_payload]))
			 {
			    // then start the DMA
			    initiate_dma_write(sdram_w_buf[sdp_pkt_properties.with_payload], buf_in_svc);	     
			 }
                      }   			 
		   }
                   // couldn't add a spike. So the buffers are full, at least at
                   // the relevant time point. Wait until some other event makes
                   // it possible to retry.
                   else break;
	     	}
                // at the end of the current sdp buffer,
                if (!sdp_pkt_properties.remaining_spikes)
                {
	           // release any buffer that was being serviced.
                   free_sdp_msg_buf(active_sdp_buf);
                   active_sdp_buf = NULL;
                }    			    	               
             }
             else
             {
                // once the current buffer has been processed move on to the next.
                // place the header in a convenient structure
                packet_header = sdp_aer_hdr_decode(active_sdp_buf);
                // handle commands immediately and separately
                if (packet_header.command.binary_cmd)
                   handle_command(&packet_header, active_sdp_buf);
                // and set the count for ordinary spike packets.
                else
                {
		   // update the received sequence number if enabled
		   set_seq_with_tag(get_aer_seq_from_sdp(active_sdp_buf->msg), active_sdp_buf->msg->tag);
		   sdp_pkt_properties.remaining_spikes = packet_header.address_type == ADDR_TYPE_LEGACY ? packet_header.legacy_hdr.count : packet_header.count;
                   if (praerie_pkt_has_payload(&packet_header))
	              sdp_pkt_properties.with_payload = WITH_PAYLOAD;
		   else
                      sdp_pkt_properties.with_payload = NO_PAYLOAD;  
                   if (praerie_pkt_has_timestamp(&packet_header))
		      sdp_pkt_properties.with_timestamp = WITH_TIMESTAMP;
                   else
		      sdp_pkt_properties.with_timestamp = NO_TIMESTAMP;
                }
             }
       }
       // Finally, inject all the spikes to be sent immediately.
       inject_spikes(immediate_inject_buf[NO_PAYLOAD]->spikes, NO_PAYLOAD);
       inject_spikes(immediate_inject_buf[WITH_PAYLOAD]->spikes, WITH_PAYLOAD);
}

/* event handling when a pipeline writeback has completed. The event may need
   to trigger additional writebacks if there are further writebacks pending or
   even a readback if this writeback was one initiated at a point when
   immediately after the output-side buffer was emptied because all the spikes
   were sent.
*/
static void dma_writeback_done(uint8_t with_payload)
{
     spike_buf_t* xfer_buf; // convenience buffer to avoid over-referencing
     // look at the DMA write buffer (always pipeline stage 2)
     if ((xfer_buf = read_pipeline[DMA_WBUF][with_payload])->tag & DMA_TAG_FULL)
     {
        // free the existing write buffer
        xfer_buf->transfer_ID = FAILURE; 
        xfer_buf->tag = DMA_TAG_FREE;
        // did the next one have data?
        if (read_pipeline[DMA_TBUF][with_payload]->tag & (DMA_TAG_FILL | DMA_TAG_FULL))
        {
	   // if so, make it the next one eligible for writeback  
           read_pipeline[DMA_WBUF][with_payload] = read_pipeline[DMA_TBUF][with_payload];
           // and reassign the current one to the last pipeline stage.
           read_pipeline[DMA_TBUF][with_payload] = next_free_buf(with_payload); 
        }
        // update the SDRAM pointers, wrapping around if necessary.
        if ((sdram_w_buf[with_payload] += SPIKE_BUF_MAX_SPIKES) >= sdram_end_buf[with_payload]) sdram_w_buf[with_payload] = sdram_evt_buf[with_payload];
        // if there are buffered events waiting to be read back,
        if (!sdram_has_buffers[with_payload])
        {
	  // if there is a buffer available, read back into it.
           if (((xfer_buf = read_pipeline[EARLY_BUF][with_payload])->tag & DMA_TAG_RB_RDY) ||
	      ((xfer_buf = read_pipeline[MID_BUF][with_payload])->tag & DMA_TAG_RB_RDY))
	   {
              xfer_buf->tag = DMA_TAG_IN_PIPELINE | DMA_TAG_RB | (with_payload ? DMA_TAG_W_PYLD : DMA_TAG_NO_PYLD);
              initiate_dma_read(xfer_buf, sdram_r_buf[with_payload]);
           }
        }
        sdram_has_buffers[with_payload] = true;
        // this condition means that the old write buffer completed, and we 
        // have swapped over to the old tail buffer, which is also full and
        // so can be sent.
        if ((xfer_buf = read_pipeline[DMA_WBUF][with_payload])->tag == DMA_TAG_FULL)
        {
           // new spikes to be buffered. Make sure SDRAM buffer space exists
           if (!(sdram_has_buffers[with_payload] && sdram_w_buf[with_payload] == sdram_r_buf[with_payload]))
           {
              // then start the DMA
              initiate_dma_write(sdram_w_buf[with_payload], xfer_buf);	     
           }
        }
     }
}

/* event handling when a pipeline readback has completed. The event may need
   to trigger additional readbacks if there are further writebacks pending.
*/
static void dma_readback_done(uint transfer_id, uint8_t with_payload)
{
     // make sure there was a readback to process.
     if ((read_pipeline[EARLY_BUF][with_payload]->tag & DMA_TAG_RB) || (read_pipeline[MID_BUF][with_payload]->tag & DMA_TAG_RB))
     { 
        // increment the SDRAM read pointer, wrapping around if necessary. 
        if ((sdram_r_buf[with_payload] += SPIKE_BUF_MAX_SPIKES) >= sdram_end_buf[with_payload]) sdram_r_buf[with_payload] = sdram_evt_buf[with_payload];
        // have we emptied the SDRAM? If so, clear the SDRAM flag.
        if (sdram_r_buf[with_payload] == sdram_w_buf[with_payload])
           sdram_has_buffers[with_payload] = false;
        // first check the earliest buffer and see if it was the readback target.
        if (transfer_id == read_pipeline[EARLY_BUF][with_payload]->transfer_ID)
        {
           // if so it goes to read.
           read_pipeline[EARLY_BUF][with_payload]->transfer_ID = FAILURE;
           read_pipeline[EARLY_BUF][with_payload]->tag = (DMA_TAG_IN_PIPELINE | DMA_TAG_FULL);
           // was there another readback queued? 
           if (read_pipeline[MID_BUF][with_payload]->tag & DMA_TAG_RB_RDY)
           {
              // if so send it on its way 
	      read_pipeline[MID_BUF][with_payload]->tag = DMA_TAG_IN_PIPELINE | DMA_TAG_RB | (with_payload ? DMA_TAG_W_PYLD : DMA_TAG_NO_PYLD);
              initiate_dma_read(read_pipeline[MID_BUF][with_payload], sdram_r_buf[with_payload]);
              return; // escape further processing since the next DMA is underway.
           }
        }
        // now check the next-earliest buffer. This is a much simpler case because
        // there can be no other pending readbacks to trigger. Note that this will
        // also catch the case where manual transfer was used after a readback in
        // the first buffer because it will just fall through to this point. 
        if (transfer_id == read_pipeline[MID_BUF][with_payload]->transfer_ID)
        {
           read_pipeline[MID_BUF][with_payload]->transfer_ID = FAILURE;
           read_pipeline[MID_BUF][with_payload]->tag = (DMA_TAG_IN_PIPELINE | DMA_TAG_FULL);
        }
     }
}

/* INTERFACE FUNCTIONS - cannot be static */

// simple convenience functions that determine number of free and
// number of committed buffers (ones that have alreay got a DMA in
// queue) in the pipeline

uint8_t dma_free_bufs(void)
{
       uint8_t num_free = 0;
       for (uint8_t buf = 0; buf < NUM_DMA_PIPE_BUFS; buf++)
       {
	   if (read_pipeline[buf][NO_PAYLOAD]->tag == DMA_TAG_FREE) num_free++;
           if (read_pipeline[buf][WITH_PAYLOAD]->tag == DMA_TAG_FREE) num_free++;
       }
       return num_free;
}

uint8_t dma_committed_pipe_bufs(void)
{
       uint8_t commit_count = 0;
       for (uint8_t buf = EARLY_BUF; buf <= DMA_WBUF; buf++) 
       {
           if (read_pipeline[buf][NO_PAYLOAD]->tag & DMA_TAG_RB) commit_count++;
           if (read_pipeline[buf][WITH_PAYLOAD]->tag & DMA_TAG_RB) commit_count++;
       }
       return commit_count;
}

/* DMA read pipeline servicing. This handles the sending of events from the
   available read buffers in the pipeline (which will either have been DMA'd
   in or pushed to the current read position directly from SDP receipt). The
   process goes through the read buffer at position 0 and sends any spikes 
   with the current timestamp. If position 0 is exhausted it moves on to the 
   next buffer, continuing until there are none left to empty. The process
   is triggered by the timer as well as by completed readback operations so
   that current-timestamp packets are sent ASAP. The process does NOT handle
   newly-arrived events still in SDP buffers with the current timestamp; the
   separate buffer_or_send_new_spikes function deals with these immediately
   and is triggered after all potential 'old' events (events with the current
   timestamp, that were received before the current timestamp) have been sent.
*/ 
void send_old_spikes(void)
{
       // more convenience variables to avoid too much indirection. 
       spike_buf_t* early_buf_orig;
       spike_buf_t* mid_buf_next;
       for (uint8_t with_payload = NO_PAYLOAD; with_payload <= WITH_PAYLOAD; with_payload++)
       {
           // first send all the spikes in the early buffer 
           // (which is the one eligible for injection)
           while (inject_spikes((early_buf_orig = read_pipeline[EARLY_BUF][with_payload])->spikes, with_payload) > 0)
           {
                 // if this exhausts the buffer and there's nothing left to send
                 // revert to expecting fills in the early buffer 
                 if ((early_buf_orig->spikes[early_buf_orig->spikes[0].link.first].link.prev) ||
                     ((read_pipeline[MID_BUF][with_payload])->tag == DMA_TAG_FREE))
                    early_buf_orig->tag = DMA_TAG_IN_PIPELINE | DMA_TAG_FILL | (with_payload ? DMA_TAG_W_PYLD : DMA_TAG_NO_PYLD);
                 else
                 {
                    // otherwise transfer the middle buffer into the early buffer
                    early_buf_orig->tag = DMA_TAG_FREE;
                    read_pipeline[EARLY_BUF][with_payload] = read_pipeline[MID_BUF][with_payload];
                    mid_buf_next = read_pipeline[MID_BUF][with_payload] = next_free_buf(with_payload);
                    // and if there are buffered spikes in SDRAM
                    if (sdram_has_buffers[with_payload])
                    {
		       // make the middle buffer eligible for readback.
                       mid_buf_next->tag = DMA_TAG_IN_PIPELINE | (with_payload ? DMA_TAG_W_PYLD : DMA_TAG_NO_PYLD); 
                       // 2 possibilities: a) the previous middle buffer
                       // was already being readback into. So that one is
                       // now the early buffer and the middle buffer should
                       // be scheduled for readback after the early one.
                       if (read_pipeline[EARLY_BUF][with_payload]->tag & DMA_TAG_RB)
                          mid_buf_next->tag |= DMA_TAG_RB_RDY;
                       else
		       {
			  // b) the middle buffer already had data. We can 
                          // schedule a readback for the new middle buffer now.
                          mid_buf_next->tag |= DMA_TAG_RB;
                          initiate_dma_read(mid_buf_next, sdram_r_buf[with_payload]);
                       }
                    }
                    // no buffers in SDRAM
                    else
                    {
		       /* this handles the freakish case where a readback
                          into the early buffer hadn't completed when new
                          data arrived eligible for the middle buffer. This
                          then filled and started to fill the second writeback
                          buffer. So strangely, the first writeback buffer is
                          earluer than the second, it will be read back into 
                          the early buffer, and thus we need to transfer the
                          second writeback buffer into the middle buffer.
                       */ 
                       if (read_pipeline[DMA_WBUF][with_payload]->tag & DMA_TAG_FULL)
		       {
                          read_pipeline[MID_BUF][with_payload] = read_pipeline[DMA_TBUF][with_payload];
                       }
                       // otherwise more normal behaviour: we can simply throw
                       // the first writeback buffer into the middle buffer 
                       // and it need never be written to SDRAM.
                       else if (read_pipeline[DMA_WBUF][with_payload]->tag & DMA_TAG_FILL)
                       {
                          read_pipeline[MID_BUF][with_payload] = read_pipeline[DMA_WBUF][with_payload];
                       }  
                    }
                 }
           }
           // handle any manual transfers that need to be done if initial attempts
           // to DMA them in failed.
           if (sdram_has_buffers[with_payload])
	   {
              early_buf_orig = read_pipeline[EARLY_BUF][with_payload];
              if ((early_buf_orig->tag & DMA_TAG_RB) && (early_buf_orig->transfer_ID == FAILURE) && !(early_buf_orig->tag & DMA_TAG_MANUAL))
              {
                 initiate_dma_read(early_buf_orig, sdram_r_buf[with_payload]);
              } 
           }
       }                      
}

// overall DMA initialisation
bool DMA_init(uint32_t no_payload_blk_count, uint32_t with_payload_blk_count)
{
     uint32_t blk_count[2] = {(SPIKE_BUF_MAX_SPIKES*no_payload_blk_count), 
			    (SPIKE_BUF_MAX_SPIKES*with_payload_blk_count)};
     // initialise the buffers and if this succeeds,
     if (buffer_init(blk_count))
     {
        // set current remaining spikes to 0 so an SDP message will be dequeued 
        sdp_pkt_properties.remaining_spikes = 0;
        // no current SDP buffer being processed
        active_sdp_buf = NULL;
        // turn on the DMA callback
        spin1_callback_on(DMA_TRANSFER_DONE, _dma_done_callback, CB_PRIORITY_DMA_DONE);
        return true;
     }
     // failed buffer initialisation is fatal.
     else return false;
} 
          
/* CALLBACK FUNCTIONS - cannot be static */

// Called when a DMA transfer completes. Also called if buffers are available for
// such a transfer, and new SDP message (i.e. a bundle of spikes) has arrived.
void _dma_done_callback(uint transfer_ID, uint tag)
{
     // Now find out what sort of transaction just completed
     // first handle DMA's transferring SDP from mailbox to local
     if (tag == (DMA_TAG_SVC))
     {
          // find the buffer responsible, 
          for (uint8_t msg_buf_idx = 0; msg_buf_idx <= AER_SEQ_WINDOW; msg_buf_idx++)
          {
	      if (sdp_msg_buf[msg_buf_idx].tag == DMA_TAG_SVC && sdp_msg_buf[msg_buf_idx].transfer_ID == transfer_ID)
              {
		 // then free the SDP mailbox and mark the buffer as ready 
		 spin1_msg_free(sdp_msg_buf[msg_buf_idx].mailbox);
                 sdp_msg_buf[msg_buf_idx].tag = DMA_TAG_READY;
              }
          }
     }
     // next search the read buffers.
     else if (tag & DMA_TAG_RB)
             dma_readback_done(transfer_ID, tag & DMA_TAG_W_PYLD ? WITH_PAYLOAD : NO_PAYLOAD);
     // then search the write buffers.
     else if (tag & DMA_TAG_FULL)
             dma_writeback_done(DMA_TAG_W_PYLD ? WITH_PAYLOAD : NO_PAYLOAD);
     // A received tag DMA_TAG_READY indicates the SDP process completed an SDP
     // copy manually and a buffer is available for service now.
     // Once preliminaries have been handled, service the SDP received queue.
     buffer_or_send_new_spikes();
     // then send any old spikes still to be processed. This could be executed
     // at a lower priority level by using spin1_schedule_callback(_send_old_spikes...)
     send_old_spikes();
}              

// These callbacks initiate manual copies if DMA fails. With directive
// SLOW_MANUAL_XFER defined, they will actually copy items one by one
// (could be VERY slow with large buffers, and no guarantee of success)
// With the directive left undefined, however, the process simply waits
// for a while and then retries.
void _initiate_manual_write(intptr_t src, intptr_t dst)
{
     // we are passing in the pointers as ints so reconvert 
     spike_buf_t* local_buf = (spike_buf_t*) src;
     spike_ll_t* sdram_buf = (spike_ll_t*) dst; 
     // tag the buffer as doing a manual transfer   
     local_buf->tag |= DMA_TAG_MANUAL;
#ifdef SLOW_MANUAL_XFER
     // and copy across one at a time.
     uint buf_count = local_buf->spikes[local_buf->spikes[0].link.first].link.prev;
     spin1_memcpy(sdram_buf, local_buf->spikes, buf_count*sizeof(spike_ll_t));
     // this is equivalent to a DMA completing with a FAILURE code
     spin1_schedule_callback(_dma_done_callback, FAILURE, local_buf->tag, CB_PRIORITY_DMA_DONE);
#else
     // if no slow transfer just wait and try again to DMA.
     spin1_delay_us(DMA_MANUAL_RETRY_DELAY);
     initiate_dma_write(sdram_buf, local_buf);
#endif
}

void _initiate_manual_read(intptr_t src, intptr_t dst)
{
     spike_buf_t* local_buf = (spike_buf_t*) dst;
     spike_ll_t* sdram_buf = (spike_ll_t*) src; 
     local_buf->tag |= DMA_TAG_MANUAL;
#ifdef SLOW_MANUAL_XFER
     // exactly like the write case except reversed source and destination
     uint buf_count = local_buf->spikes[local_buf->spikes[0].link.first].link.prev;
     spin1_memcpy(local_buf->spikes, sdram, buf_count*sizeof(spike_ll_t));
     spin1_schedule_callback(_dma_done_callback, FAILURE, local_buf->tag, CB_PRIORITY_DMA_DONE);
#else
     spin1_delay_us(DMA_MANUAL_RETRY_DELAY);
     initiate_dma_read(local_buf, sdram_buf);
#endif
}
