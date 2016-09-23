#include "eieio_interface.h"
#include "byte_manip.h"

eieio_hdr_t eieio_decode_hdr(uint16_t binary_hdr)
{
    eieio_hdr_t decoded_hdr;    
    decoded_hdr.apply_prefix = (bool)(binary_hdr & EIEIO_MASK_P);
    decoded_hdr.prefix_type = (uint8_t)((binary_hdr & EIEIO_MASK_F) >> EIEIO_F_BIT_POS);
    decoded_hdr.packet_type = (uint8_t)((binary_hdr & EIEIO_MASK_TYPE) >> EIEIO_TYPE_POS);
    decoded_hdr.key_right_shift = 0;
    decoded_hdr.payload_as_timestamp = (bool)(binary_hdr & EIEIO_MASK_T);
    decoded_hdr.payload_apply_prefix = (bool)(binary_hdr & EIEIO_MASK_D);
    decoded_hdr.count = (uint8_t)((binary_hdr & EIEIO_MASK_COUNT) >> EIEIO_COUNT_POS);
    decoded_hdr.tag = (uint8_t)((binary_hdr & EIEIO_MASK_TAG) >> EIEIO_TAG_POS);
}

bool eieio_decode_prefixes(void* buf, eieio_hdr_t* hdr)
{
     void* prefix_offset = ((uint8_t*)buf+sizeof(uint16_t));
     if (hdr->apply_prefix)
        prefix_offset += copy_PRAERIE_buf_to_local(&(hdr->prefix), prefix_offset, 1, SIZE_16);
     if (hdr->payload_apply_prefix)
     {
        switch (hdr->packet_type)
	{
	       case KEY_PAYLOAD_16_BIT:
                    prefix_offset += copy_PRAERIE_buf_to_local(&(hdr->payload_prefix), prefix_offset, 1, SIZE_16);
                    break;
	       case KEY_PAYLOAD_32_BIT:
                    prefix_offset += copy_PRAERIE_buf_to_local(&(hdr->payload_prefix), prefix_offset, 1, SIZE_32);
                    break;
	       default:
                    break;
        }
     }
     return (prefix_offset == ((uint8_t*)buf+sizeof(uint16_t)));	  
}

bool eieio_encode_hdr(eieio_hdr_t* hdr)
{
     if (hdr->tag) return false;
     hdr->eieio_binary_hdr = 0;
     if (hdr->apply_prefix) hdr->eieio_binary_hdr |= EIEIO_MASK_P;
     if (hdr->prefix_type)  hdr->eieio_binary_hdr |= EIEIO_MASK_F;
     if (hdr->payload_apply_prefix) hdr->eieio_binary_hdr |= EIEIO_MASK_D;
     if (hdr->payload_as_timestamp) hdr->eieio_binary_hdr |= EIEIO_MASK_T;
     hdr->eieio_binary_hdr |= hdr->packet_type << EIEIO_TYPE_POS;
     hdr->eieio_binary_hdr |= ((uint16_t)hdr->count) << EIEIO_COUNT_POS;
     return true;
}

bool eieio_hdr_type_equiv(const eieio_hdr_t* hdr1, const eieio_hdr_t* hdr2)
{
     if (hdr1->apply_prefix != hdr2->apply_prefix)
        return false;
     if (hdr1->payload_apply_prefix != hdr1->payload_apply_prefix)
	return false;
     if (hdr1->apply_prefix && (hdr1->prefix != hdr2->prefix))
        return false; 
     if (hdr1->payload_apply_prefix && (hdr1->payload_prefix != hdr2->payload_prefix))
        return false;
     if (hdr1->prefix_type == hdr2->prefix_type &&
        hdr1->packet_type == hdr2->packet_type &&
        hdr1->key_right_shift == hdr2->key_right_shift &&
        hdr1->payload_as_timestamp == hdr2->payload_as_timestamp)
        return true;
     return false;     
}

uint32_t eieio_hdr_len(const eieio_hdr_t* hdr)
{
         uint32_t hdr_len = EIEIO_BASE_HDR_LEN;
         if (hdr->apply_prefix) hdr_len += sizeof(uint16_t);
         if (hdr->payload_apply_prefix) hdr_len += (hdr->packet_type >= KEY_32_BIT? sizeof(uint32_t) : sizeof(uint16_t));
         return hdr_len;
}

eieio_event_t eieio_extract_prefixes(const eieio_hdr_t* hdr, eieio_event_t event)
{
              eieio_event_t prefix_event;
              // may wish to mask with ((1 << hdr->key_right_shift)-1) 
              prefix_event.address = (event.address >> (hdr->prefix_type ? PREFIX_SHIFT_16 : 0)) & UINT16_MAX;
              if ((hdr->packet_type & EIEIO_DTYPE_PY) && (hdr->payload_apply_prefix))
              {
                 if (hdr->packet_type >= KEY_32_BIT)
                    prefix_event.payload = event.payload & UINT32_MAX;
                 else
                    prefix_event.payload = event.payload & UINT16_MAX;              
              }                         
              return prefix_event;
}

uint32_t eieio_pkt_encode_hdr(void* buf, eieio_hdr_t* hdr)
{
         uint8_t* write_buf = (uint8_t*) buf;
         write_buf += copy_local_to_PRAERIE_buf(write_buf, &hdr->eieio_binary_hdr, 1, SIZE_16);
         if (hdr->apply_prefix)
            write_buf += copy_local_to_PRAERIE_buf(write_buf, &hdr->prefix, 1, SIZE_16);
         if (hdr->payload_apply_prefix)
         {
	    if (hdr->packet_type >= KEY_32_BIT)
	    {
               write_buf += copy_local_to_PRAERIE_buf(write_buf, &hdr->payload_prefix, 1, SIZE_32);
            }
            else
            {
               uint16_t short_pay_prefix = (uint16_t)hdr->payload_prefix;
	       write_buf += copy_local_to_PRAERIE_buf(write_buf, &short_pay_prefix, 1, SIZE_16);
            }
         }
         return (uintptr_t)write_buf-(uintptr_t)buf;
}

uint32_t eieio_pkt_encode_event(void* buf, eieio_hdr_t* hdr, eieio_event_t event, bool check_prefixes)
{
         if (check_prefixes)
	 {
	    eieio_event_t event_prefixes = eieio_extract_prefixes(hdr, event);
            if (hdr->apply_prefix && (event_prefixes.address != hdr->prefix)) return 0;
            if (hdr->payload_apply_prefix)
            {
               if (event_prefixes.payload != hdr->payload_prefix) return 0;
            } 
         }
         uint32_t event_size = eieio_address_len(hdr)+eieio_timestamp_len(hdr)+eieio_payload_len(hdr);
         uint8_t* write_buf = (uint8_t*) buf;
	 write_buf += eieio_hdr_len(hdr)+event_size*hdr->count;
	 switch (hdr->packet_type)
	 {
	        case KEY_16_BIT:
                     write_buf += copy_local_to_PRAERIE_buf(write_buf, &(event.address), 1, SIZE_16);
                     break;
                case KEY_PAYLOAD_16_BIT:
                     write_buf += copy_local_to_PRAERIE_buf(write_buf, &(event.address), 1, SIZE_16);
                     write_buf += copy_local_to_PRAERIE_buf(write_buf, &(event.payload), 1, SIZE_16);
                     break;
                case KEY_32_BIT:
                     write_buf += copy_local_to_PRAERIE_buf(write_buf, &(event.address), 1, SIZE_32);
                     break;
                case KEY_PAYLOAD_32_BIT:
                     write_buf += copy_local_to_PRAERIE_buf(write_buf, &(event.address), 1, SIZE_32);
                     write_buf += copy_local_to_PRAERIE_buf(write_buf, &(event.payload), 1, SIZE_32);
                     break;
         }
         hdr->count += 1;
         return event_size; 
}

eieio_event_t eieio_pkt_decode_event(void* buf, const eieio_hdr_t* hdr, uint8_t event_num)
{
              eieio_event_t decoded_event;
              if (hdr->apply_prefix || !hdr->prefix_type)
              {
                 uint32_t event_size = eieio_address_len(hdr)+eieio_timestamp_len(hdr)+eieio_payload_len(hdr);
                 praerie_sizes datum_size = hdr->packet_type & EIEIO_DTYPE_SZ ? SIZE_32 : SIZE_16;
                 uint8_t* buf_pos = (uint8_t*)buf + (eieio_hdr_len(hdr)+event_num*event_size);
                 buf_pos += copy_PRAERIE_buf_to_local(&decoded_event.address, buf_pos, 1, datum_size);
                 if (hdr->packet_type & EIEIO_DTYPE_PY)
                 {    
                    buf_pos += copy_PRAERIE_buf_to_local(&decoded_event.payload, buf_pos, 1, datum_size);
                    if (hdr->payload_apply_prefix) decoded_event.payload |= hdr->payload_prefix;
                 }
                 if (hdr->apply_prefix)
                 {
                    if (hdr->prefix_type) decoded_event.address |= ((uint32_t)hdr->prefix) << 16;
                    else decoded_event.address |= (uint32_t)hdr->prefix;
                 }
              }
              return decoded_event;            
}
