/*-----------------------------------------------------------------------------
/
/
/------------------------------------------------------------------------------
/ <ihsan@kehribar.me>
/----------------------------------------------------------------------------*/

#include <avr/io.h> 
#include <util/delay.h>
#include "xmega_digital.h"
#include "ringBuffer.h"
#include <stdio.h>
#include "op.h"
#include <avr/interrupt.h>

void init_dac();
void init_uart();
void sendch(uint8_t ch);
void initClock_32Mhz();
void dbg(uint8_t ch);
void init_hardware();