#ifndef __TIMER_PROCESS_H__
#define __TIMER_PROCESS_H__

#include <common-typedefs.h>
#include <spin1_api.h>
#include <praerie-typedefs.h>
#include "PRAERIE-proc-typedefs.h"

bool set_real_time(uint32_t time);
bool get_real_time(praerie_hdr_t* in_hdr, sdp_msg_buf_t* in_msg);
void TMR_init(uint32_t* sys_config);
void _timer_callback(uint simulation_time, uint reset_time);

#endif
