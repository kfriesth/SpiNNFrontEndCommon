#ifndef __USR_PACKET_PROCESS_H__
#define __USR_PACKET_PROCESS_H__

#include <common-typedefs.h>

bool USR_init(uint32_t send_buf_size);
void _user_event_callback(uint call_priority, uint time_if_timer_called);
#ifdef TRANSMITTER
void _buffer_sender(uint unused0, uint unused1);
#endif

#endif
