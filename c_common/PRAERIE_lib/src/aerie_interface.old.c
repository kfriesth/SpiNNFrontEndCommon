#include "aerie_interface.h"


// decode the basic binary header into a structure
aerie_hdr_t aerie_decode_hdr(uint32_t binary_hdr)
{
        aerie_hdr_t hdr_in_build;
        // is the packet actually a legacy EIEIO packet? 
        if (((binary_hdr & AERIE_MASK_ADDR_SIZE) >> AERIE_ADDR_SIZE_POS) == SIZE_COMPAT)
	{
	   // Yes. Decode according to EIEIO.
           // Use only the upper 16 bits to represent the basic binary. 
           // any prefixes will be decoded later.
           hdr_in_build.aerie_binary_hdr = binary_hdr & (AERIE_MASK_EIEIO_HDR);
           // First set the AERIE fields to some fixed values
           hdr_in_build.address_type = ADDR_TYPE_LEGACY;
           hdr_in_build.timestamp_type = TSTP_TYPE_LEGACY;
           hdr_in_build.payload_type = PYLD_TYPE_LEGACY;
           hdr_in_build.address_size = SIZE_LEGACY;
           hdr_in_build.timestamp_size = SIZE_LEGACY;
           hdr_in_build.payload_size = SIZE_LEGACY;
           hdr_in_build.count = 0;
           hdr_in_build.seq = 0;
           hdr_in_build.expansion = 0;
           // decode any command if present
           if (((binary_hdr & EIEIO_MASK_PF) >> EIEIO_F_BIT_POS) == EIEIO_PF_CMD)
	      hdr_in_build.command = (binary_hdr & EIEIO_MASK_COMMAND) >> EIEIO_CMD_POS;
           // then build the actual EIEIO header.
           hdr_in_build.legacy_hdr = &eieio_decode_hdr((uint16_t)((binary_hdr & AERIE_MASK_PKT_DECODE) >> AERIE_EIEIO_HDR_POS));
        }
        else
        {
	   // copy the literal binary header for later reference
           hdr_in_build.aerie_binary_hdr = binary_hdr;
           // then get the individual fields by mask-shift operations
           hdr_in_build.address_type = (aerie_address_types)((binary_hdr & AERIE_MASK_ADDR_TYPE) >> AERIE_ADDR_TYPE_POS);
           hdr_in_build.timestamp_type = (aerie_timestamp_types)((binary_hdr & AERIE_MASK_TSTP_TYPE) >> AERIE_TSTP_TYPE_POS);
           hdr_in_build.payload_type = (aerie_payload_types)((binary_hdr & AERIE_MASK_PYLD_TYPE) >> AERIE_PYLD_TYPE_POS);
           hdr_in_build.address_size = (aerie_sizes)((binary_hdr & AERIE_MASK_ADDR_SIZE) >> AERIE_ADDR_SIZE_POS);
           hdr_in_build.timestamp_size = (aerie_sizes)((binary_hdr & AERIE_MASK_TSTP_SIZE) >> AERIE_TSTP_SIZE_POS);
           hdr_in_build.payload_size = (aerie_sizes)((binary_hdr & AERIE_MASK_PYLD_SIZE) >> AERIE_PYLD_SIZE_POS);
           hdr_in_build.count = (uint8_t)(binary_hdr & AERIE_MASK_COUNT) >> AERIE_COUNT_POS;
           hdr_in_build.seq = (uint8_t)(binary_hdr & AERIE_MASK_SEQ) >> AERIE_SEQ_POS;
           hdr_in_build.expansion = (binary_hdr & AERIE_MASK_EXT);
           // decode any command if present
           if (hdr_in_build.address_type == ADDR_TYPE_CMD)
	      hdr_in_build.command = binary_hdr & AERIE_MASK_COMMAND;
           // not an EIEO packet so no legacy header information
           hdr_in_build.legacy_hdr = NULL;
        }
        return hdr_in_build;
}
// decode any prefixes from a binary stream into a header
bool aerie_decode_prefixes(void* buf, aerie_hdr_t* hdr)
{
  void* prefix_offset = ((uint32_t*)buf+1);
  // handle the EIEIO legacy case. The address size field is definitive
  // Catch both SIZE_LEGACY, the expected case, and SIZE_COMPAT, the 
  // unformatted case.
  if (hdr->address_size == SIZE_LEGACY || hdr->address_size == SIZE_COMPAT)
  {
     // in this situation the AERIE prefixes are nonexistent
     hdr->address_prefix = 0;
     hdr->payload_prefix = 0;
     hdr->timestamp_prefix = 0;
     // and the prefixes are kept in the EIEIO header
     return eieio_decode_prefixes(buf, hdr->legacy_hdr);
  }
  // start checking for prefixes, beginning with the address
  if (hdr->address_type == ADDR_TYPE_PREFIX_ONLY || hdr->address_type == ADDR_TYPE_ADDR_PREFIX)
  {
     switch(hdr->address_size)
     {
           /* these tricky statements handle the variable length in the bitstream 
              of prefixes. We cast the offset to the expected type for this prefix,
              then convert the extracted result to the standard type for the field
              in the aerie header structure (which is a 64-bit int). Then we
              increment the bitstream offset by the appropriate length to prepare
              for the next potential prefix (which could be a different length)
	   */
           case SIZE_16:
	        // could do the following for the next 2 lines but it looks even more obscure
	        //hdr->address_prefix = (uint64_t)(*(((uint16_t*)(prefix_offset))++));

                hdr->address_prefix = (uint64_t)(*((uint16_t*)(prefix_offset)));
		prefix_offset = ((uint16_t*)prefix_offset+1);
                break;
           case SIZE_32:
                hdr->address_prefix = (uint64_t)(*((uint32_t*)(prefix_offset)));
		prefix_offset = ((uint32_t*)prefix_offset+1);
                break;
           case SIZE_64:
                hdr->address_prefix = (*((uint64_t*)(prefix_offset)));
		prefix_offset = ((uint64_t*)prefix_offset+1);
                break;
           default:
	        hdr->address_prefix = 0;
     }
  }
  // timestamp prefix is next
  if (hdr->timestamp_type == TSTP_TYPE_PREFIX_ONLY || hdr->timestamp_type == TSTP_TYPE_TSTP_PREFIX)
  {
     // this and payload prefix might have their length set by the address length.
     // Ugly, but a ternary value in the switch statement seems the best way to 
     // cope with this. It would be nice if this could be done with a case in
     // the switch statement itself which altered the value of the actual switch
     // test, but C doesn't have any way to express this.
     switch(hdr->timestamp_size == SIZE_COMPAT? hdr->address_size : hdr_timestamp_size)
     {
           case SIZE_16:
                hdr->timestamp_prefix = (uint64_t)(*((uint16_t*)(prefix_offset)));
		prefix_offset = ((uint16_t*)prefix_offset+1);
                break;
           case SIZE_32:
                hdr->timestamp_prefix = (uint64_t)(*((uint32_t*)(prefix_offset)));
		prefix_offset = ((uint32_t*)prefix_offset+1);
                break;
           case SIZE_64:
                hdr->timestamp_prefix = (*((uint64_t*)(prefix_offset)));
		prefix_offset = ((uint64_t*)prefix_offset+1);
                break;
           default:
	        hdr->timestamp_prefix = 0;
     }
  }
  // finally get any payload prefix
  if (hdr->payload_type == PYLD_TYPE_PREFIX_ONLY || hdr->payload_type == PYLD_TYPE_PYLD_PREFIX)
  {
     switch(hdr->payload_size == SIZE_COMPAT? hdr->address_size : hdr_payload_size)
     {
           case SIZE_16:
                hdr->payload_prefix = (uint64_t)(*((uint16_t*)(prefix_offset)));
		prefix_offset = ((uint16_t*)prefix_offset+1);
                break;
           case SIZE_32:
                hdr->payload_prefix = (uint64_t)(*((uint32_t*)(prefix_offset)));
		prefix_offset = ((uint32_t*)prefix_offset+1);
                break;
           case SIZE_64:
                hdr->payload_prefix = (*((uint64_t*)(prefix_offset)));
		prefix_offset = ((uint64_t*)prefix_offset+1);
                break;
           default:
	        hdr->payload_prefix = 0;
     }
  }
  // if the prefix offset is unchanged from the end of the decode part of the
  // header there were no prefixes. Otherwise there were.
  return (prefix_offset == ((uint32_t*)buf+1));
}

// encode the header into binary and place in the decoded header structure
bool aerie_encode_hdr(aerie_hdr_t* hdr);

// get the size in bytes for each of: address
size_t aerie_address_len(const aerie_hdr_t* hdr)
{
       switch(hdr->address_size)
       {
             case SIZE_16: 
                  return sizeof(uint16_t);
	     case SIZE_32:
                  return sizeof(uint32_t); 
             case SIZE_64:
                  return sizeof(uint64_t);
	     case SIZE_COMPAT:  
	     case SIZE_LEGACY: 
                  return eieio_address_len(hdr->legacy_hdr);
	     default:
                  return 0;
       } 
}

// payload,
size_t aerie_payload_len(const aerie_hdr_t* hdr)
{
       switch(hdr->payload_size)
       {
             case SIZE_16: 
                  return sizeof(uint16_t);
             case SIZE_32:
                  return sizeof(uint32_t); 
             case SIZE_64:
                  return sizeof(uint64_t);
             case SIZE_COMPAT:
                  return aerie_addr_len(hdr);  
             case SIZE_LEGACY: 
                  return eieio_payload_len(hdr->legacy_hdr);
             default:
                  return 0;
       }
}

// and timestamp
size_t aerie_timestamp_len(const aerie_hdr_t* hdr)
{
       switch(hdr->timestamp_size)
       {
             case SIZE_16: 
                  return sizeof(uint16_t);
             case SIZE_32:
                  return sizeof(uint32_t); 
             case SIZE_64:
                  return sizeof(uint64_t);
             case SIZE_COMPAT:
                  return aerie_addr_len(hdr);  
             case SIZE_LEGACY: 
                  return eieio_timestamp_len(hdr->legacy_hdr);
             default:
                  return 0;
       }
}

// get the number of bytes occupied by the aerie header. Used for buffer offset.
size_t aerie_hdr_len(const aerie_hdr_t* hdr)
{
         size_t hdr_len = AERIE_BASE_HDR_LEN;
         switch (hdr->address_type) 
         {
                case ADDR_TYPE_CMD:
	             return AERIE_BASE_HDR_LEN;
                case ADDR_TYPE_LEGACY:
                     return eieio_hdr_len(hdr->legacy_hdr);
                case ADDR_TYPE_PREFIX_ONLY: 
                case ADDR_TYPE_ADDR_PREFIX: 
                     hdr_len += aerie_address_len(hdr);
                     break;
                default:                    
         } 
         if (hdr->payload_type == PYLD_TYPE_PREFIX_ONLY || hdr->payload_type == PYLD_TYPE_PYLD_PREFIX)
            hdr_len += aerie_payload_len(hdr);
         if (hdr->timestamp_type == TSTP_TYPE_PREFIX_ONLY || hdr->timestamp_type == TSTP_TYPE_TSTP_PREFIX)
            hdr_len += aerie_timestamp_len(hdr);
         return hdr_len;
}

// compares headers for compatible decode
bool aerie_hdr_type_equiv(const aerie_hdr_t* hdr1, const aerie_hdr_t* hdr2)
{
     if ((hdr1->timestamp_type == TSTP_TYPE_PREFIX_ONLY || 
	  hdr1->timestamp_type == TSTP_TYPE_TSTP_PREFIX) &&
         (hdr1->timestamp_type != hdr_2->timestamp_type ||
         hdr1->timestamp_prefix != hdr2->timestamp_prefix)))
        return false;
     if ((hdr1->payload_type == PYLD_TYPE_PREFIX_ONLY || 
         hdr1->payload_type == PYLD_TYPE_PYLD_PREFIX) &&
         (hdr1->payload_type != hdr_2->payload_type ||
         hdr1->payload_prefix != hdr2->payload_prefix)))
        return false;
     if (hdr1->address_type == hdr2->address_type)
     {
        switch (hdr1->address_type)
	{
              case ADDR_TYPE_CMD:
                   if (hdr1->command == hdr2->command) 
                      return true;
                   return false;
              case ADDR_TYPE_LEGACY:
                   eieio_hdr_t e_hdr_1 = hdr1->legacy_hdr;
                   eieio_hdr_t e_hdr_2 = hdr2->legacy_hdr;
                   if (e_hdr_1->apply_prefix != e_hdr_2->apply_prefix)
                      return false;
                   if (e_hdr_1->payload_apply_prefix != e_hdr_1->payload_apply_prefix)
		      return false;
		   if (e_hdr_1->apply_prefix && (e_hdr_1->prefix != e_hdr_2->prefix))
		      return false; 
                   if (e_hdr_1->payload_apply_prefix && (e_hdr_1->payload_prefix != e_hdr_2->payload_prefix))
		      return false;
                   if (e_hdr_1->prefix_type == e_hdr_2->prefix_type &&
                       e_hdr_1->packet_type == e_hdr_2->packet_type &&
                       e_hdr_1->key_right_shift == e_hdr_2->key_right_shift &&
                       e_hdr_1->payload_as_timestamp == e_hdr_2->payload_as_timestamp)
		      return true;
                   return false;              
              case ADDR_TYPE_PREFIX_ONLY:
	      case ADDR_TYPE_ADDR_PREFIX:
		   if (hdr1->address_prefix != hdr2->address_prefix)
		      return false; 
              default:
                   if (hdr1->timestamp_type == hdr2->timestamp_type &&
                       hdr1->payload_type == hdr2->payload_type &&
                       hdr1->address_size == hdr2->address_size &&
                       hdr1->timestamp_size == hdr2->timestamp_size &&
                       hdr1->payload_size == hdr2->payload_size &&
                       hdr1->expansion == hdr2->expansion) 
		      return true;
                   return false;
        }     
     }
     return false; 
}

// binary encodes a complete packet from header and a list of events
uint32_t aerie_pkt_encode(void* buf, aerie_hdr_t* hdr, aerie_event_t* event_buf);

// binary encodes an event into a buffer with header present
uint32_t aerie_pkt_encode_event(void* buf, aerie_hdr_t* hdr, aerie_event_t event);

// binary encodes a block of events into a buffer
uint32_t aerie_pkt_encode_block(void* buf,  aerie_hdr_t* hdr, aerie_event_t* event_buf, uint8_t num_events);

// extracts an entire packet including header and events into respective buffers
bool aerie_pkt_decode(void* buf, aerie_hdr_t* hdr, aerie_event_t* event_buf);

// decodes an entire header including prefixes from a buffer
aerie_hdr_t aerie_pkt_decode_hdr(void* buf);

// extracts a specific event from an AERIE buffer
aerie_event_t aerie_pkt_decode_event(void* buf, const aerie_hdr_t* hdr, uint8_t event_num);

// extracts a block of events from an AERIE buffer into an event buffer
uint32_t aerie_pkt_decode_block(void* buf, const aerie_hdr_t* hdr, aerie_event_t* event_buf, uint8_t num_events);

void aerie_process_sdp_msg_recv_buf(void);

void aerie_process_sdp_msg_send_buf(void);

void aerie_process_mc_pkt_recv_buf(void);

void aerie_process_mc_pkt_send_buf(void);

void aerie_process_aerie_send_buf(void);

void aerie_process_aerie_recv_buf(void);

aerie_header_struct aerie_get_config_params(void);

void aerie_set_config_params(aerie_hdr_t config_params);

aerie_if_cfg aerie_enable_interfaces(aerie_if_cfg interface_enable);



aerie_header_struct aerie_interface_get_aerie_header(
    address_t header_start_address)
{
    aerie_header_struct hdr_info;
    hdr_info.address_size = *header_start_address & AERIE_MASK_ADDR_SIZE) >> AERIE_ADDR_SIZE_POS
    if (hdr_info.address_size == SIZE_COMPAT)
    {   
       if ((hdr_info & EIEIO_MASK_PF >> EIEIO_F_BIT_POS) == EIEIO_PF_CMD)
       {
	  hdr_info.command = (header_start_address & EIEIO_MASK_COMMAND) >> EIEIO_CMD_POS;
       }
       else
       {
          hdr_info.legacy_hdr = (eieio_header_struct*) sark_alloc(1, (size_t) sizeof(eieio_header_struct));
          *hdr_info.legacy_hdr = eieio_interface_get_eieio_header(header_start_address)
       }
    } 
    hdr_info.address_type = *header_start_address & EIEIO_MASK_P;
    hdr_info.timestamp_type = *header_start_address & EIEIO_MASK_PREFIX;
    hdr_info.payload_type = *header_start_address & EIEIO_MASK_F;
    hdr_info.address_size = *header_start_address & EIEIO_MASK_TYPE;
    hdr_info.timestamp_size = 0;
    hdr_info.payload_size =;
    hdr_info.count = *header_start_address & EIEIO_MASK_COUNT;
    hdr_info.seq = *header_start_address & EIEIO_MASK_T;
    hdr_info.expansion = *header_start_address & EIEIO_MASK_D;
    hdr_info.address_prefix =;
    hdr_info.timestamp_prefix =;
    hdr_info.payload_prefix =;
    switch (hdr_info.packet_type)
    {
       case EIEIO_TYPE_!6_KP:
	 hdr_info.payload_prefix = *(header_start_address+1) >> 16;
         break;
       case EIEIO_TYPE_32_KP:
         hdr_info.payload_prefix = *(header_start_address+1);
         break;
       default:
         hdr_info.payload_prefix = 0;
    }
    return hdr_info;
}

void eieio_interface_pack_eieio_header(
     address_t header_start_address, eieio_header_struct header_info)
{
     *header_start_address = 0;
     *header_start_address |= hdr_info.apply_prefix << EIEIO_P_BIT_POS;
     *header_start_address |= hdr_info.prefix_type << EIEIO_F_BIT_POS;
     *header_start_address |= hdr_info.payload_apply_prefix << EIEIO_D_BIT_POS;
     *header_start_address |= hdr_info.payload_as_time_stamp << EIEIO_T_BIT_POS;
     *header_start_address |= hdr_info.packet_type << EIEIO_TYPE_POS;
     *header_start_address |= hdr_info.tag << EIEIO_TAG_POS;
     *header_start_address |= hdr_info.count << EIEIO_COUNT_POS;
     if (hdr_info.apply_prefix) *header_start_address |= hdr_info.prefix << EIEIO_PREFIX_POS;
}
