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

#define NOTE_SILENT 0 
#define NOTE_TRIGGER 1
#define NOTE_DECAY 2

typedef struct t_envelope {
  uint8_t state;
  Word fallRate;
  uint8_t lastOutput;
  Word envelopeCounter;
} t_envelope;

typedef struct t_envSetting {
  Word attackRate;
  Word decayRate;
  Word releaseRate;
} t_envSetting;

typedef struct t_lfo {
  Word phaseCounter;
  uint16_t freq;
  int16_t outSignal;
  uint8_t depth;
} t_lfo;

typedef struct t_key {
  uint8_t noteState;
  uint8_t noteState_d;
  uint8_t lastnote;
  uint8_t keyVelocity;
  uint8_t maxModulation;  
  uint16_t freqTone;
  uint16_t freqMod;  
  Word phaseCounterTone;  
  Word phaseCounterMod;     
  t_envelope modEnvelope;
  t_envelope ampEnvelope;
} t_key;
