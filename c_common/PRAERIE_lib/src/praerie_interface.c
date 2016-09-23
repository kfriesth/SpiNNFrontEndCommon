#include "praerie_interface.h"
#include "byte_manip.h"

// constant default headers for some simple types.

const praerie_cmd_t command_eieio = {0x0, SIZE_LEGACY, 0, CMD_TYPE_LEGACY, CMD_DIR_LEGACY, 0, 0};

const praerie_hdr_t praerie_legacy_hdr = {0x0, ADDR_TYPE_LEGACY, TSTP_TYPE_LEGACY, PYLD_TYPE_LEGACY, SIZE_LEGACY, SIZE_LEGACY, SIZE_LEGACY, 0, 0, 0, 0, 0, 0, PRAERIE_NULL_CMD, EIEIO_NULL_HEADER};

const praerie_hdr_t praerie_cmd_hdr = {0x0, ADDR_TYPE_CMD, TSTP_TYPE_NONE, PYLD_TYPE_NONE, SIZE_NONE, SIZE_NONE, SIZE_NONE, 0, 0, 0, 0, 0, 0, PRAERIE_NULL_CMD, EIEIO_NULL_HEADER};

static void get_normal_prefixes(uintmax_t* prefix, uintmax_t extract_field, praerie_sizes size)
{
       switch(size)
       {  
             case SIZE_16:
                  *prefix = (extract_field >> PREFIX_SHIFT_16) & UINT16_MAX;
                  return;
             case SIZE_32:
                  *prefix = (extract_field >> PREFIX_SHIFT_32) & UINT32_MAX;
                  return;
             case SIZE_64:
                  *prefix = (extract_field >> PREFIX_SHIFT_64) & UINT64_MAX;
                  return;
             default:
                  *prefix = 0;
       }
}

// decode the basic binary header into a structure
praerie_hdr_t praerie_decode_hdr(uint32_t binary_hdr)
{
        praerie_hdr_t hdr_in_build;
        // is the packet actually a legacy EIEIO packet? 
        if (((binary_hdr & PRAERIE_MASK_ADDR_SIZE) >> PRAERIE_ADDR_SIZE_POS) == SIZE_COMPAT)
	{
	   // Yes. Decode according to EIEIO.
           // Use only the upper 16 bits to represent the basic binary. 
           // any prefixes will be decoded later.
           // First set the PRAERIE fields to some fixed values
           hdr_in_build = praerie_legacy_hdr;
           hdr_in_build.praerie_binary_hdr = binary_hdr & (PRAERIE_MASK_EIEIO_HDR);
           // decode any command if present
           if ((((binary_hdr >> PRAERIE_EIEIO_HDR_POS) & EIEIO_MASK_PF) >> EIEIO_F_BIT_POS) == EIEIO_PF_CMD)
	      hdr_in_build.command = praerie_decode_cmd(binary_hdr & PRAERIE_MASK_COMMAND);
           else 
              hdr_in_build.command.binary_cmd = NULL_CMD;
           // then build the actual EIEIO header.
           hdr_in_build.legacy_hdr = eieio_decode_hdr((uint16_t)((binary_hdr & PRAERIE_MASK_PKT_DECODE) >> PRAERIE_EIEIO_HDR_POS));
        }
        else
        {
           // get the address type which is fundamental for further decode
           hdr_in_build.address_type = (praerie_address_types)((binary_hdr & PRAERIE_MASK_ADDR_TYPE) >> PRAERIE_ADDR_TYPE_POS);
           // decode any command if present
           if (hdr_in_build.address_type == ADDR_TYPE_CMD)
	   {
              // initialise the header to a default command header.
	      hdr_in_build = praerie_cmd_hdr;
	      // extract the command itself 
	      hdr_in_build.command = praerie_decode_cmd(binary_hdr & PRAERIE_MASK_COMMAND);
           }
           else
	   {
              // Data packet. Get the individual fields by mask-shift operations
              hdr_in_build.timestamp_type = (praerie_timestamp_types)((binary_hdr & PRAERIE_MASK_TSTP_TYPE) >> PRAERIE_TSTP_TYPE_POS);
              hdr_in_build.payload_type = (praerie_payload_types)((binary_hdr & PRAERIE_MASK_PYLD_TYPE) >> PRAERIE_PYLD_TYPE_POS);
              hdr_in_build.address_size = (praerie_sizes)((binary_hdr & PRAERIE_MASK_ADDR_SIZE) >> PRAERIE_ADDR_SIZE_POS);
              hdr_in_build.timestamp_size = (praerie_sizes)((binary_hdr & PRAERIE_MASK_TSTP_SIZE) >> PRAERIE_TSTP_SIZE_POS);
              hdr_in_build.payload_size = (praerie_sizes)((binary_hdr & PRAERIE_MASK_PYLD_SIZE) >> PRAERIE_PYLD_SIZE_POS);
              hdr_in_build.count = (uint8_t)(binary_hdr & PRAERIE_MASK_COUNT) >> PRAERIE_COUNT_POS;
              hdr_in_build.seq = (uint8_t)(binary_hdr & PRAERIE_MASK_SEQ) >> PRAERIE_SEQ_POS;
              hdr_in_build.expansion = (binary_hdr & PRAERIE_MASK_EXT);
              hdr_in_build.command.binary_cmd = NULL_CMD; // not a command so invalidate the command extension
           }
           // not an EIEO packet so no legacy header information
           hdr_in_build.legacy_hdr.tag = EIEIO_TAG_NONE;
	   // copy the literal binary header for later reference
           hdr_in_build.praerie_binary_hdr = binary_hdr;
        }
        return hdr_in_build;
}

// decode a command into a structure
praerie_cmd_t praerie_decode_cmd(uint32_t command)
{
            praerie_cmd_t cmd_in_build;
            // is this an eieio command?
            if ((cmd_in_build.word_size = (praerie_sizes)((command & PRAERIE_MASK_CMD_WD_SIZE) >> PRAERIE_CMD_WD_SIZE_POS)) == SIZE_COMPAT)
            {
               // configure the command to eieio boilerplate
	       cmd_in_build = command_eieio;
               // and then decode the EIEIO command
               cmd_in_build.legacy_cmd_ID = (command >> PRAERIE_EIEIO_CMD_POS) & EIEIO_MASK_COMMAND;
            }
            else
	    {
               // otherwise decode according to PRAERIE
               cmd_in_build.count = (command & PRAERIE_MASK_COUNT) >> PRAERIE_COUNT_POS;
               cmd_in_build.general_cmd = (command & PRAERIE_MASK_CMD_GEN) ? CMD_TYPE_GENERAL : CMD_TYPE_DEVICE_SPECIFIC;
               cmd_in_build.dir = (command & PRAERIE_MASK_CMD_DIR) ? CMD_DIR_WRITE : CMD_DIR_READ;
               cmd_in_build.cmd_num = (command & PRAERIE_MASK_CMD_NUM) >> PRAERIE_CMD_NUM_POS;
               if ((cmd_in_build.general_cmd == CMD_TYPE_GENERAL) && ((praerie_command_messages)cmd_in_build.cmd_num >= PRAERIE_CMD_RESERVED)) cmd_in_build.word_size = SIZE_NONE;
               cmd_in_build.legacy_cmd_ID = NULL_CMD;
            }
            cmd_in_build.binary_cmd = command;
            return cmd_in_build;
}

// decode any prefixes from a binary stream into a header
bool praerie_decode_prefixes(void* buf, praerie_hdr_t* hdr)
{
  void* prefix_offset = ((uint8_t*)buf+sizeof(uint32_t));
  // handle the EIEIO legacy case. The address size field is definitive
  // Catch both SIZE_LEGACY, the expected case, and SIZE_COMPAT, the 
  // unformatted case.
  if (hdr->address_size == SIZE_LEGACY || hdr->address_size == SIZE_COMPAT)
  {
     // in this situation the PRAERIE prefixes are nonexistent
     hdr->address_prefix = 0;
     hdr->payload_prefix = 0;
     hdr->timestamp_prefix = 0;
     // and the prefixes are kept in the EIEIO header
     return eieio_decode_prefixes(buf, &hdr->legacy_hdr);
  }
  // start checking for prefixes, beginning with the address
  if (hdr->address_type == ADDR_TYPE_PREFIX_ONLY || hdr->address_type == ADDR_TYPE_ADDR_PREFIX)
  {
     if (hdr->address_size == SIZE_COMPAT || hdr->address_size > SIZE_64)
        hdr->address_prefix = 0;
     else
	prefix_offset = (void*)((uintptr_t)prefix_offset + copy_PRAERIE_buf_to_local(&hdr->address_prefix, prefix_offset, 1, hdr->address_size));
  }
  // timestamp prefix is next
  if (hdr->timestamp_type == TSTP_TYPE_PREFIX_ONLY || hdr->timestamp_type == TSTP_TYPE_TSTP_PREFIX)
  {
     // this and payload prefix might have their length set by the address length.
     if (hdr->timestamp_size > SIZE_64)
        hdr->timestamp_prefix = 0;
     else
        prefix_offset = (void*)((uintptr_t)prefix_offset + copy_PRAERIE_buf_to_local(&hdr->timestamp_prefix, prefix_offset, 1, hdr->timestamp_size == SIZE_COMPAT? hdr->address_size : hdr->timestamp_size));
  }
  // finally get any payload prefix
  if (hdr->payload_type == PYLD_TYPE_PREFIX_ONLY || hdr->payload_type == PYLD_TYPE_PYLD_PREFIX)
  {
     if (hdr->payload_size > SIZE_64)
        hdr->payload_prefix = 0;
     else
        prefix_offset = (void*)((uintptr_t)prefix_offset + copy_PRAERIE_buf_to_local(&hdr->payload_prefix, prefix_offset, 1, hdr->payload_size == SIZE_COMPAT? hdr->address_size : hdr->payload_size));
  }
  // if the prefix offset is unchanged from the end of the decode part of the
  // header there were no prefixes. Otherwise there were.
  return (prefix_offset == ((uint8_t*)buf+sizeof(uint32_t)));
}

// encode the header into binary and place in the decoded header structure
bool praerie_encode_hdr(praerie_hdr_t* hdr)
{
  // first, zero the binary header to get started
  hdr->praerie_binary_hdr = 0;
  // do some basic checks on values to screen out invalid cases
  // address type of none is a null header
  if (hdr->address_type == ADDR_TYPE_NONE) return false;
  // timestamps or payloads that exist need a size 
  if (hdr->timestamp_type != TSTP_TYPE_NONE && hdr->timestamp_size == SIZE_NONE) return false;
  if (hdr->payload_type != PYLD_TYPE_NONE && hdr->payload_size == SIZE_NONE) return false;
  // check for EIEIO-style legacy encode
  if (hdr->address_size != SIZE_LEGACY || hdr->address_size != SIZE_COMPAT)
  {
     // types must not be legacy for non-legacy encode
     if (hdr->address_type == ADDR_TYPE_LEGACY) return false;
     if (hdr->timestamp_type == TSTP_TYPE_LEGACY) return false;
     if (hdr->payload_type == PYLD_TYPE_LEGACY) return false; 
  }
  else 
  {
    // legacy encode the packet but if it fails indicate this in the return value
    if (!eieio_encode_hdr(&hdr->legacy_hdr))
       return false;
    else
    {
       // successful legacy encode. Copy this to the top-level binary header
       hdr->praerie_binary_hdr = hdr->legacy_hdr.eieio_binary_hdr;
       // and exit with success.
       return true;
    }
  }
  // command packets need their own separate encode
  if (hdr->address_type == ADDR_TYPE_CMD)
  {
     // all the data-packet type specifiers should be none for commands 
     if (hdr->timestamp_type != TSTP_TYPE_NONE) return false;
     if (hdr->payload_type != PYLD_TYPE_NONE) return false;
     if (hdr->address_size != SIZE_NONE) return false;
     if (hdr->timestamp_size != SIZE_NONE) return false;
     if (hdr->payload_size != SIZE_NONE) return false;
     // and the rest of the header fields should be 0.
     if (hdr->count != 0) return false;
     if (hdr->seq != 0) return false;
     if (hdr->expansion != 0) return false;
     // encode the command
     if (praerie_encode_cmd(&hdr->command))
     { 
        // and if successful dump it into the binary header and return 
        hdr->praerie_binary_hdr = hdr->command.binary_cmd;
        return true;
     }
     // otherwise indicate encoding failed.
     else return false; 
  }
  // data packets must have some size for their address field
  else if (hdr->address_size == SIZE_NONE) return false; 
  // once we have survived all the checks and handled the unusual cases, encode
  // the header as usual by bit-shift and OR.
  hdr->praerie_binary_hdr |= hdr->address_type << PRAERIE_ADDR_TYPE_POS;
  hdr->praerie_binary_hdr |= hdr->timestamp_type << PRAERIE_TSTP_TYPE_POS;
  hdr->praerie_binary_hdr |= hdr->payload_type << PRAERIE_PYLD_TYPE_POS;
  hdr->praerie_binary_hdr |= hdr->address_size << PRAERIE_ADDR_SIZE_POS;
  hdr->praerie_binary_hdr |= hdr->count << PRAERIE_COUNT_POS;
  hdr->praerie_binary_hdr |= hdr->seq << PRAERIE_SEQ_POS;
  // only fill in the expansion bits if we have to.
  if (hdr->timestamp_type != TSTP_TYPE_NONE) hdr->praerie_binary_hdr |= hdr->timestamp_size << PRAERIE_TSTP_SIZE_POS;
  if (hdr->payload_type != PYLD_TYPE_NONE) hdr->praerie_binary_hdr |= hdr->payload_size << PRAERIE_PYLD_SIZE_POS;
  if (hdr->expansion) hdr->praerie_binary_hdr |= ((hdr->expansion & PRAERIE_MASK_SYNC) << PRAERIE_SYNC_POS);
  // exit with success.
  return true; 
}

bool praerie_encode_cmd(praerie_cmd_t* cmd)
{
     if ((cmd->word_size == SIZE_COMPAT) || (cmd->word_size == SIZE_NONE)) return false;
     if (cmd->word_size == SIZE_LEGACY)
     {
        if ((cmd->cmd_num != 0) || (cmd->legacy_cmd_ID > EIEIO_MASK_COMMAND) || (cmd->legacy_cmd_ID & EIEIO_MASK_TAG)) return false;
        cmd->binary_cmd = EIEIO_MASK_F;
        cmd->binary_cmd |= cmd->legacy_cmd_ID;
        cmd->binary_cmd <<= PRAERIE_EIEIO_CMD_POS;
     }
     else
     {
        if ((cmd->general_cmd == CMD_TYPE_GENERAL) && ((praerie_command_messages)cmd->cmd_num >= PRAERIE_CMD_RESERVED)) return false;
        if ((cmd->binary_cmd = (cmd->cmd_num & PRAERIE_MASK_CMD_NUM) >> PRAERIE_CMD_NUM_POS) != cmd->cmd_num) return false;
        cmd->binary_cmd |= cmd->dir == CMD_DIR_WRITE? PRAERIE_MASK_CMD_DIR : 0;
        cmd->binary_cmd |= cmd->general_cmd == CMD_TYPE_GENERAL? PRAERIE_MASK_CMD_GEN : 0;
        cmd->binary_cmd |= ((uint32_t)cmd->count) << PRAERIE_COUNT_POS;
        cmd->binary_cmd |= cmd->word_size << PRAERIE_CMD_WD_SIZE_POS;
     }
     return true;
}

static size_t praerie_field_len(praerie_sizes size)
{
       switch(size)
       {
             case SIZE_16: 
                  return sizeof(uint16_t);
	     case SIZE_32:
                  return sizeof(uint32_t); 
             case SIZE_64:
                  return sizeof(uint64_t);
             default:
                  return 0; 
       }
}

// get the size in bytes for each of: address
size_t praerie_address_len(const praerie_hdr_t* hdr)
{
       if (hdr->address_size == SIZE_COMPAT || hdr->address_size == SIZE_LEGACY)
          return eieio_address_len(&hdr->legacy_hdr);
       else
	  return praerie_field_len(hdr->address_size); 
}

// payload,
size_t praerie_payload_len(const praerie_hdr_t* hdr)
{
       if (hdr->payload_size == SIZE_COMPAT)
          return praerie_address_len(hdr);  
       else if (hdr->payload_size == SIZE_LEGACY) 
          return eieio_payload_len(&hdr->legacy_hdr);
       else
          return praerie_field_len(hdr->payload_size);    
}

// and timestamp
size_t praerie_timestamp_len(const praerie_hdr_t* hdr)
{

       if (hdr->timestamp_size == SIZE_COMPAT)
          return praerie_address_len(hdr);  
       else if (hdr->timestamp_size == SIZE_LEGACY)
          return eieio_timestamp_len(&hdr->legacy_hdr);
       else
          return praerie_field_len(hdr->timestamp_size); 
}


// get the number of bytes occupied by the praerie header. Used for buffer offset.
size_t praerie_hdr_len(const praerie_hdr_t* hdr)
{
         size_t hdr_len = PRAERIE_BASE_HDR_LEN;
         switch (hdr->address_type) 
         {
                case ADDR_TYPE_CMD:
	             return PRAERIE_BASE_HDR_LEN;
                case ADDR_TYPE_LEGACY:
                     return eieio_hdr_len(&hdr->legacy_hdr);
                case ADDR_TYPE_PREFIX_ONLY: 
                case ADDR_TYPE_ADDR_PREFIX: 
                     hdr_len += praerie_address_len(hdr);
                     break;
                default:
                     break;                    
         } 
         if (hdr->payload_type == PYLD_TYPE_PREFIX_ONLY || hdr->payload_type == PYLD_TYPE_PYLD_PREFIX)
            hdr_len += praerie_payload_len(hdr);
         if (hdr->timestamp_type == TSTP_TYPE_PREFIX_ONLY || hdr->timestamp_type == TSTP_TYPE_TSTP_PREFIX)
            hdr_len += praerie_timestamp_len(hdr);
         return hdr_len;
}

// gets prefixes from events that have them embedded.
praerie_event_t praerie_extract_prefixes(const praerie_hdr_t* hdr, praerie_event_t event)
{
              praerie_event_t prefix_event;
              //switch(hdr->address_size)
              if (hdr->address_size == SIZE_COMPAT || hdr->address_size == SIZE_LEGACY)
              {
                    /*
                    case SIZE_COMPAT:
                    case SIZE_LEGACY:
		    {
                         eieio_event_t e_event;
                         e_event.address = (eieio_addr_t)event.address;
                         e_event.payload = (eieio_pyld_t)(hdr->legacy_hdr.payload_as_timestamp ? event.timestamp : event.payload);
                         eieio_event_t e_prefix_event = eieio_extract_prefixes(&hdr->legacy_hdr, e_event);
                         prefix_event.address = e_prefix_event.address;
                         if (hdr->legacy_hdr.payload_as_timestamp)
                         {
                            prefix_event.timestamp = e_prefix_event.payload;
                            prefix_event.payload = 0;
                         }
                         else
                         {
                            prefix_event.payload = e_prefix_event.payload;
                            prefix_event.timestamp = 0;
                         }                       
                         return prefix_event;
                    }
                    */
                 eieio_event_t e_event;
                 e_event.address = (eieio_addr_t)event.address;
                 e_event.payload = (eieio_pyld_t)(hdr->legacy_hdr.payload_as_timestamp ? event.timestamp : event.payload);
                 eieio_event_t e_prefix_event = eieio_extract_prefixes(&hdr->legacy_hdr, e_event);
                 prefix_event.address = e_prefix_event.address;
                 if (hdr->legacy_hdr.payload_as_timestamp)
                 {
                    prefix_event.timestamp = e_prefix_event.payload;
                    prefix_event.payload = 0;
                 }
                 else
                 {
                    prefix_event.payload = e_prefix_event.payload;
                    prefix_event.timestamp = 0;
                 }                       
                 return prefix_event;   
              }
              else
              {
                 get_normal_prefixes(&prefix_event.address, event.address, hdr->address_size);
                 get_normal_prefixes(&prefix_event.timestamp, event.timestamp, hdr->timestamp_size == SIZE_COMPAT ? hdr->address_size : hdr->timestamp_size);
                 get_normal_prefixes(&prefix_event.payload, event.payload, hdr->payload_size == SIZE_COMPAT ? hdr->address_size : hdr->payload_size);
                 return prefix_event;    
              }
              /*
              switch(hdr->timestamp_size == SIZE_COMPAT ? hdr->address_size : hdr->timestamp_size)
              {
                    case SIZE_16:
                         prefix_event.timestamp = (event.timestamp >> PREFIX_SHIFT_16) & UINT16_MAX;
                         break;
                    case SIZE_32:
                         prefix_event.timestamp = (event.timestamp >> PREFIX_SHIFT_32) & UINT32_MAX;
                         break;
                    case SIZE_64:
                         prefix_event.timestamp = (event.timestamp >> PREFIX_SHIFT_64) & UINT64_MAX;
                         break;
                    default:
                         prefix_event.timestamp = 0;
              }
              switch(hdr->payload_size == SIZE_COMPAT ? hdr->address_size : hdr->payload_size)
              {
                    case SIZE_16:
                         prefix_event.payload = (event.payload >> PREFIX_SHIFT_16) & UINT16_MAX;
                         break;
                    case SIZE_32:
                         prefix_event.payload = (event.payload >> PREFIX_SHIFT_32) & UINT32_MAX;
                         break;
                    case SIZE_64:
                         prefix_event.payload = (event.payload >> PREFIX_SHIFT_64) & UINT64_MAX;
                         break;
                    default:
                         prefix_event.payload = 0;
              }                  
              return prefix_event;
              */
}

// compares headers for compatible decode
bool praerie_hdr_type_equiv(const praerie_hdr_t* hdr1, const praerie_hdr_t* hdr2)
{
     if ((hdr1->timestamp_type == TSTP_TYPE_PREFIX_ONLY || 
	  hdr1->timestamp_type == TSTP_TYPE_TSTP_PREFIX) &&
         (hdr1->timestamp_type != hdr2->timestamp_type ||
         hdr1->timestamp_prefix != hdr2->timestamp_prefix))
        return false;
     if ((hdr1->payload_type == PYLD_TYPE_PREFIX_ONLY || 
         hdr1->payload_type == PYLD_TYPE_PYLD_PREFIX) &&
         (hdr1->payload_type != hdr2->payload_type ||
         hdr1->payload_prefix != hdr2->payload_prefix))
        return false;
     if (hdr1->address_type == hdr2->address_type)
     {
        switch (hdr1->address_type)
	{
              case ADDR_TYPE_CMD:
                   if (praerie_cmd_equiv(&hdr1->command, &hdr2->command)) 
                      return true;
                   return false;
              case ADDR_TYPE_LEGACY:
		   return eieio_hdr_type_equiv(&hdr1->legacy_hdr, &hdr2->legacy_hdr);              
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

// compare commands for equivalence - do they do the same thing?
bool praerie_cmd_equiv(const praerie_cmd_t* cmd1, const praerie_cmd_t* cmd2)
{
    // word size needs to be identical; immediately fail if not.
    if (cmd1->word_size != cmd2->word_size) return false;
    // is this an EIEIO command? 
    if (cmd1->word_size == SIZE_LEGACY)
    {
       // if so the only thing that matters is the legacy command
       if (cmd1->legacy_cmd_ID != cmd2->legacy_cmd_ID) return false;
       else return true;
    }
    // otherwise the command number, type, and direction must match
    if ((cmd1->general_cmd != cmd1->general_cmd) ||
        (cmd1->dir != cmd2->dir) ||
        (cmd1->cmd_num != cmd2->cmd_num)) 
       return false;
    else return true; 
}

// binary encodes the header including prefixes into a buffer
// copy_local_to_PRAERIE_buf(void* to_buf, void* from_buf, uint32_t num_words, praerie_sizes size)
uint32_t praerie_pkt_encode_hdr(void* buf, praerie_hdr_t* hdr)
{
     if (!praerie_encode_hdr(hdr)) return 0;
     if (hdr->address_size == SIZE_LEGACY || hdr->address_size == SIZE_COMPAT)
        return eieio_pkt_encode_hdr(buf, &hdr->legacy_hdr);
     else
     {
        uint8_t* write_buf = (uint8_t*) buf;
        write_buf += copy_local_to_PRAERIE_buf(write_buf, &(hdr->praerie_binary_hdr), 1, SIZE_32);
        if (hdr->address_type == ADDR_TYPE_PREFIX_ONLY || hdr->address_type == ADDR_TYPE_ADDR_PREFIX)
           write_buf+= copy_local_to_PRAERIE_buf(write_buf, &(hdr->address_prefix), 1, hdr->address_size);
        if (hdr->timestamp_type == TSTP_TYPE_PREFIX_ONLY || hdr->timestamp_type == TSTP_TYPE_TSTP_PREFIX)
           write_buf+= copy_local_to_PRAERIE_buf(write_buf, &(hdr->timestamp_prefix), 1, hdr->timestamp_size == SIZE_COMPAT? hdr->address_size : hdr->timestamp_size);     
        if (hdr->payload_type == PYLD_TYPE_PREFIX_ONLY || hdr->payload_type == PYLD_TYPE_PYLD_PREFIX)
           write_buf+= copy_local_to_PRAERIE_buf(write_buf, &(hdr->payload_prefix), 1, hdr->payload_size == SIZE_COMPAT? hdr->address_size : hdr->payload_size);
        return (uintptr_t)write_buf-(uintptr_t)buf;
     }
}

// binary encodes an event into a buffer with header present
uint32_t praerie_pkt_encode_event(void* buf, praerie_hdr_t* hdr, praerie_event_t event, bool check_prefixes)
{
         if (hdr->address_size == SIZE_NONE) return 0;
         if (hdr->address_size == SIZE_LEGACY || hdr->address_size == SIZE_COMPAT);
	 {
            eieio_event_t e_event;
            e_event.address = (eieio_addr_t)event.address;
            e_event.payload = (eieio_pyld_t)(hdr->legacy_hdr.payload_as_timestamp ? event.timestamp : event.payload); 
	    return eieio_pkt_encode_event(buf, &hdr->legacy_hdr, e_event, check_prefixes);
         }
         if (check_prefixes)
	 {
	    praerie_event_t event_prefixes = praerie_extract_prefixes(hdr, event);
            if (event_prefixes.address != hdr->address_prefix || event_prefixes.timestamp != hdr->timestamp_prefix || event_prefixes.payload != hdr->payload_prefix) return 0;
         }
         uint32_t event_size = praerie_address_len(hdr)+praerie_timestamp_len(hdr)+praerie_payload_len(hdr);
         uint8_t* write_buf = (uint8_t*) buf;
         write_buf += praerie_hdr_len(hdr)+event_size*hdr->count;
         if (hdr->address_type == ADDR_TYPE_ADDR_ONLY || hdr->address_type == ADDR_TYPE_ADDR_PREFIX)
            write_buf += copy_local_to_PRAERIE_buf(write_buf, &(event.address), 1, hdr->address_size);
         if (hdr->timestamp_type == TSTP_TYPE_TSTP_ONLY || hdr->timestamp_type == TSTP_TYPE_TSTP_PREFIX)
            write_buf += copy_local_to_PRAERIE_buf(write_buf, &(event.timestamp), 1, hdr->timestamp_size == SIZE_COMPAT? hdr->address_size : hdr->timestamp_size);
         if (hdr->payload_type == PYLD_TYPE_PYLD_ONLY || hdr->payload_type == PYLD_TYPE_PYLD_PREFIX)
            write_buf += copy_local_to_PRAERIE_buf(write_buf, &(event.payload), 1, hdr->payload_size == SIZE_COMPAT? hdr->address_size : hdr->payload_size);
         hdr->count += 1;
         return event_size;
}

// binary encodes a block of events into a buffer
uint32_t praerie_pkt_encode_block(void* buf,  praerie_hdr_t* hdr, praerie_event_t* event_buf, bool check_prefixes, uint8_t num_events)
{
         uint32_t bytes_encoded = 0;
         uint32_t bytes_this_event;
         uint32_t evt = 0;
         while (evt < num_events && (bytes_this_event = praerie_pkt_encode_event(buf, hdr, event_buf[evt++], check_prefixes))) bytes_encoded += bytes_this_event;
         if (bytes_this_event == 0) return 0;
         else return bytes_encoded;
}

// decodes an entire header including prefixes from a buffer
praerie_hdr_t praerie_pkt_decode_hdr(void* buf)
{
            uint32_t raw_binary_hdr;
            praerie_hdr_t new_hdr = AER_NULL_HEADER;
            if (copy_PRAERIE_buf_to_local(&raw_binary_hdr, buf, 1, SIZE_32) == sizeof(uint32_t))
	    {
	       new_hdr = praerie_decode_hdr(raw_binary_hdr);
               praerie_decode_prefixes(buf, &new_hdr);
            }
            return new_hdr;              
}

// extracts a specific event from an PRAERIE buffer. This function assumes the buffer is
// not a command. If called with a command packet unexpected behaviour will likely result
// and certainly the returned 'event' will be meaningless.
praerie_event_t praerie_pkt_decode_event(void* buf, const praerie_hdr_t* hdr, uint8_t event_num)
{
              praerie_event_t decoded_event;
              if (hdr->address_size == SIZE_LEGACY || hdr->address_size == SIZE_COMPAT)
              {
                 eieio_event_t e_event = eieio_pkt_decode_event(buf, &hdr->legacy_hdr, event_num);
                 decoded_event.address = (praerie_addr_t)e_event.address;
                 if (hdr->legacy_hdr.payload_as_timestamp)
                 {
                    decoded_event.timestamp = (praerie_tstp_t)e_event.payload;
                 }
                 else
                 {
                    decoded_event.payload = (praerie_pyld_t)e_event.payload;
                 }          
              }
              else
              {
                 uint32_t event_size = praerie_address_len(hdr)+praerie_timestamp_len(hdr)+praerie_payload_len(hdr);
		 uint8_t* buf_pos = (uint8_t*)buf + (praerie_hdr_len(hdr)+event_num*event_size);
                 if (hdr->address_type & PRAERIE_FIELD_TYPE_DT)
                    buf_pos += copy_PRAERIE_buf_to_local(&decoded_event.address, buf_pos, 1, hdr->address_size);
                 if (hdr->address_type & PRAERIE_FIELD_TYPE_PF)
                    decoded_event.address |= hdr->address_prefix << (8*(1 << hdr->address_size));
                 if (hdr->timestamp_type & PRAERIE_FIELD_TYPE_DT)
                    buf_pos += copy_PRAERIE_buf_to_local(&decoded_event.timestamp, buf_pos, 1, hdr->timestamp_size == SIZE_COMPAT ? hdr->address_size : hdr->timestamp_size);
                 if (hdr->timestamp_type & PRAERIE_FIELD_TYPE_PF)
                    decoded_event.timestamp |= hdr->timestamp_prefix << (8*(1 << (hdr->timestamp_size == SIZE_COMPAT ? hdr->address_size : hdr->timestamp_size)));
                 if (hdr->payload_type & PRAERIE_FIELD_TYPE_DT)
                    buf_pos += copy_PRAERIE_buf_to_local(&decoded_event.payload, buf_pos, 1, hdr->payload_size == SIZE_COMPAT ? hdr->address_size : hdr->payload_size);
                 if (hdr->payload_type & PRAERIE_FIELD_TYPE_PF)
                    decoded_event.payload |= hdr->payload_prefix << (8*(1 << (hdr->payload_size == SIZE_COMPAT ? hdr->address_size : hdr->payload_size)));
              }
              return decoded_event;
}

// extracts a block of events into an event buffer
uint8_t praerie_pkt_decode_block(void* buf, praerie_hdr_t* hdr, praerie_event_t* event_buf, uint8_t num_events, uint8_t start_event)
{
        // filter out commands which have device_specific decode
        if (hdr->address_type == ADDR_TYPE_CMD || hdr->address_type == ADDR_TYPE_NONE ||
	    (hdr->address_type == ADDR_TYPE_LEGACY && !hdr->legacy_hdr.apply_prefix && 
             hdr->legacy_hdr.prefix_type == EIEIO_PREFIX_TYPE_U)
	   ) return 0;
         uint8_t this_event = 0;
         for (; this_event < num_events; this_event++)
	   event_buf[this_event+start_event] = praerie_pkt_decode_event(buf, hdr, this_event+start_event);
         return this_event;
}             

// extracts an entire packet including header and events into respective buffers
bool praerie_pkt_decode(void* buf, praerie_hdr_t* hdr, praerie_event_t* event_buf)
{
     // decode the header
     *hdr = praerie_pkt_decode_hdr(buf);
     {
        // then decode the block of events
        if (praerie_pkt_decode_block(buf, hdr, event_buf, hdr->count, 0) > 0) return true;
     }
     return false;
}


// get the praerie configuration (interfaces enabled/disabled, supported sizes etc.)
praerie_if_cfg praerie_get_config(void);

// set all or part of the praerie configuration. Not all options need be supported.
bool praerie_set_config(praerie_if_cfg* praerie_config);




