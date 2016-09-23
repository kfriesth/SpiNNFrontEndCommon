#ifndef __MC_PACKET_PROCESS_H__
#define __MC_PACKET_PROCESS_H__

#include <common-typedefs.h>

void _multicast_packet_received_callback(uint key, uint payload);
bool MC_pkt_init(void);
uint32_t mc_buffer_overflows(void); 

#endif
