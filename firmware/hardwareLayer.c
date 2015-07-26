/*-----------------------------------------------------------------------------
/
/
/------------------------------------------------------------------------------
/ <ihsan@kehribar.me>
/----------------------------------------------------------------------------*/

#include "hardwareLayer.h"

void init_hardware()
{  
  initClock_32Mhz();
  init_dac(); 
  init_uart();  

  /* Software PWM timer */   
  TCC4.CTRLA = TC45_CLKSEL_DIV64_gc; /* 32 Mhz / 64 => 500 kHz */
  TCC4.PER = 39; /* 500 kHz / (39 + 1) => 12.5 kHz */
  TCC4.INTCTRLA = 0x01; /* Overflow interrupt level */

  /* Dac pins are output */
  PORTC.DIRSET = 0xFF;

  pinMode(A,7,OUTPUT); 
  pinMode(A,6,OUTPUT); 
}

void dbg(uint8_t ch)
{
  while(!(USARTC0.STATUS & USART_DREIF_bm));
  USARTC0.DATA = ch;
}

void sendch(uint8_t ch)
{
  while(!(USARTD0.STATUS & USART_DREIF_bm));
  USARTD0.DATA = ch;
}

void init_uart()
{
  /* Set UART pin driver states */
  pinMode(D,6,INPUT);
  pinMode(D,7,OUTPUT);

  /* Remap the UART pins */
  PORTD.REMAP = PORT_USART0_bm;

  /* 8bit */
  USARTD0.CTRLB = USART_RXEN_bm|USART_TXEN_bm;
  USARTD0.CTRLC = USART_CMODE_ASYNCHRONOUS_gc|USART_PMODE_DISABLED_gc|USART_CHSIZE_8BIT_gc;

  /* 115200 baud rate with 32MHz clock */
  USARTD0.BAUDCTRLA = 131; USARTD0.BAUDCTRLB = (-3 << USART_BSCALE_gp);

  /* Enable UART data reception interrupt */
  USARTD0.CTRLA |= USART_DRIE_bm;
  
  /* Set UART data reception interrupt priority */
  USARTD0.CTRLA |= (1<<5);
  USARTD0.CTRLA |= (1<<4);

  /* Set UART pin driver states */
  pinMode(C,2,INPUT);
  pinMode(C,3,OUTPUT);

  USARTC0.CTRLB = USART_RXEN_bm|USART_TXEN_bm;
  USARTC0.CTRLC = USART_CMODE_ASYNCHRONOUS_gc|USART_PMODE_DISABLED_gc|USART_CHSIZE_8BIT_gc;

  /* 115200 baud rate with 32MHz clock */
  USARTC0.BAUDCTRLA = 131; USARTC0.BAUDCTRLB = (-3 << USART_BSCALE_gp);

  /* Enable all interrupt levels */
  PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
  sei();
}

void init_dac()
{
  /* DAC is enabled on Channel 0 and Channel 1 */
  DACA.CTRLA = (1<<0)|(1<<2)|(1<<3);

  /* Single channel operation */
  DACA.CTRLB = DAC_CHSEL_DUAL_gc;

  /* External 2048 mV voltage reference + Left adj for easy 8bit resolution */
  DACA.CTRLC = DAC_REFSEL_AREFA_gc;

  /* OFfset calibration ... */
  DACA.CH0OFFSETCAL = 0;
  DACA.CH1OFFSETCAL = 0;  
}

void initClock_32Mhz()
{
  /* Generates 32Mhz clock from internal 2Mhz clock via PLL */
  OSC.PLLCTRL = OSC_PLLSRC_RC2M_gc | 16;
  OSC.CTRL |= OSC_PLLEN_bm ;
  while((OSC.STATUS & OSC_PLLRDY_bm) == 0);
  CCP = CCP_IOREG_gc;
  CLK.CTRL = CLK_SCLKSEL_PLL_gc;
}
