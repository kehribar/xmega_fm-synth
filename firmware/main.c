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
#include "lut.h"
#include "synth.h"
#include "hardwareLayer.h"

/*---------------------------------------------------------------------------*/
#define MAX_VOICE 6
#define LOWPASS_ORDER 5

/*---------------------------------------------------------------------------*/
t_lfo outputLfo;
t_envSetting ampEnvSetting;
t_envSetting modEnvSetting;

RingBuffer_t midi_ringBuf;
uint8_t midi_ringBufData[256];

/*---------------------------------------------------------------------------*/
volatile uint8_t g_fRatio;
volatile uint8_t g_cutoff;
volatile uint8_t envCounter;
volatile uint8_t g_resonance;
volatile uint8_t g_maxFmDepth;
volatile uint8_t g_modulationIndex;
volatile uint8_t depth_mod[MAX_VOICE];
volatile uint8_t depth_amp[MAX_VOICE];
volatile struct t_key voice[MAX_VOICE];
volatile int16_t history[LOWPASS_ORDER+1];

/*---------------------------------------------------------------------------*/
static void midi_controlMessageHandler(const uint8_t id, const uint8_t data)
{
  switch(id)
  {
    case 12:
    {
      g_cutoff = data << 1;
      break;
    }
    case 13:
    {
      g_resonance = data << 1;
      break;
    }
    case 14:
    {
      ampEnvSetting.attackRate.value = (uint16_t)data * 5;
      modEnvSetting.attackRate.value = (uint16_t)data * 15;
      break;
    }
    case 15:
    {
      ampEnvSetting.decayRate.value = data;
      ampEnvSetting.releaseRate.value = ampEnvSetting.decayRate.value * 5;
      modEnvSetting.decayRate.value = (uint16_t)data * 3;
      modEnvSetting.releaseRate.value = modEnvSetting.decayRate.value * 5;
      break;
    }
    case 16:
    {
      g_maxFmDepth = data << 1;
      break;
    }
  }
}

/*---------------------------------------------------------------------------*/
static void midi_noteOnMessageHandler(const uint8_t note,const uint8_t velocity)
{
  /* Lokup table has limited range */
  if((note > 20) && (note < 109))
  {
    uint8_t k;

    /* Search for an silent empty slot */
    for (k = 0; k < MAX_VOICE; ++k)
    {
      if((voice[k].lastnote == 0) && (voice[k].noteState == NOTE_SILENT))
      {       
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
          voice[k].freqTone = noteToFreq[note - 21];
          voice[k].freqMod = (voice[k].freqTone) * g_fRatio;                    
          voice[k].lastnote = note;
          voice[k].keyVelocity = velocity << 1;  
          voice[k].noteState = NOTE_TRIGGER;
        }      
        break;         
      }
    }

    /* If no silent empty slots found, use one of the decaying notes */
    if(k == MAX_VOICE)
    {
      for (k = 0; k < MAX_VOICE; ++k)
      {
        if(voice[k].lastnote == 0)
        {
          ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
          {
            voice[k].freqTone = noteToFreq[note - 21];
            voice[k].freqMod = (voice[k].freqTone) * g_fRatio;                    
            voice[k].lastnote = note;
            voice[k].keyVelocity = velocity << 1;  
            voice[k].noteState = NOTE_TRIGGER;
          }      
          break;         
        }
      }
    }
  }  
}

/*---------------------------------------------------------------------------*/
static void midi_noteOffMessageHandler(const uint8_t note)
{
  uint8_t k;

  /* Search for previously triggered key */
  for (k = 0; k < MAX_VOICE; ++k)
  {
    if(voice[k].lastnote == note)
    {
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
      {
        voice[k].lastnote = 0;
        voice[k].noteState = NOTE_DECAY;         
      }
      break;
    }
  }          
}

/*---------------------------------------------------------------------------*/
static uint8_t envelope_iterate(t_envelope* env, const t_envSetting* setting, const uint8_t maxVal)
{  
  uint8_t output = 0;

  if(env->state == 0) /* idle */
  {
    output = 0;
    env->lastOutput = 0;
    env->envelopeCounter.value  = 0;   
  }    
  else if(env->state == 1) /* attack */
  {
    /* Max attack rate can be 32767 */
    env->envelopeCounter.value += setting->attackRate.value;
    output = env->envelopeCounter.bytes[1];
    
    /* Detect overflow ... */
    if(output < env->lastOutput)
    {    
      output = 0xFF;  
      env->state = 2;        
      env->envelopeCounter.value = 0xFFFF;
    }  
  }
  else if(env->state == 2) /* decay */
  {    
    env->envelopeCounter.value -= env->fallRate.value;
    output = env->envelopeCounter.bytes[1];

    /* Detect underflow ... */
    if(output > env->lastOutput)
    {
      output = 0;
      env->state = 0;      
    }
  }
  else
  {
    output = 0;
    env->state = 0;      
  }
 
  env->lastOutput = output;

  output = U8U8MulShift8(output,maxVal);
  
  return output;
}

/*---------------------------------------------------------------------------*/
static int8_t fm_iterate(t_key* key, const uint8_t depth_mod, const uint8_t depth_amp)
{ 
  int8_t signal_out;
  int8_t signal_mod;  
  uint8_t signal_phase;

  key->phaseCounterTone.value += key->freqTone;  
  key->phaseCounterMod.value += key->freqMod;  
  
  signal_mod = lutSin[key->phaseCounterMod.bytes[1]];    
  signal_mod = S8U8MulShift8(signal_mod,depth_mod);  
  signal_mod = signal_mod * g_modulationIndex;  
  
  signal_phase = key->phaseCounterTone.bytes[1];
  signal_phase += signal_mod;
  
  signal_out = S8U8MulShift8(lutSin[signal_phase],depth_amp);    
  
  return signal_out;
}

/*---------------------------------------------------------------------------*/
ISR(TCC4_OVF_vect)
{    
  uint8_t i;  
  int16_t totalOutput = 0;

  /* set pin high for timing debug */
  digitalWrite(A,6,HIGH);  

  /* Slow down the envelope calculations a bit */
  if(envCounter == 0)
  {
    envCounter = 5;    
  }
  else
  {
    envCounter--;
  }

  /* scan voices */
  for(i=0;i<MAX_VOICE;++i)
  {         
    /* key down */
    if((voice[i].noteState == NOTE_TRIGGER) && (voice[i].noteState_d != NOTE_TRIGGER))
    {
      voice[i].phaseCounterMod.value = 0;
      voice[i].phaseCounterTone.value = 0;

      voice[i].maxModulation = U8U8MulShift8(g_maxFmDepth,voice[i].keyVelocity);

      voice[i].ampEnvelope.state = 1;
      voice[i].ampEnvelope.fallRate = ampEnvSetting.decayRate;
      voice[i].ampEnvelope.envelopeCounter.value = 0;

      voice[i].modEnvelope.state = 1;
      voice[i].modEnvelope.fallRate = ampEnvSetting.decayRate;
      voice[i].modEnvelope.envelopeCounter.value = 0;
    }
    /* key up */
    else if((voice[i].noteState != NOTE_TRIGGER) && (voice[i].noteState_d == NOTE_TRIGGER))
    {
      voice[i].ampEnvelope.state = 2;
      voice[i].ampEnvelope.envelopeCounter.bytes[1] = voice[i].ampEnvelope.lastOutput;
      voice[i].ampEnvelope.envelopeCounter.bytes[0] = 0xFF;
      voice[i].ampEnvelope.fallRate = ampEnvSetting.releaseRate;

      voice[i].modEnvelope.state = 2;
      voice[i].modEnvelope.envelopeCounter.bytes[1] = voice[i].modEnvelope.lastOutput;
      voice[i].modEnvelope.envelopeCounter.bytes[0] = 0xFF;
      voice[i].modEnvelope.fallRate = ampEnvSetting.releaseRate;
    }

    /* calculate modulation and amplitude levels for a given key based on envelopes */
    if(envCounter == 0)
    { 
      depth_mod[i] = envelope_iterate(&(voice[i].modEnvelope),&modEnvSetting,voice[i].maxModulation);  
      depth_amp[i] = envelope_iterate(&(voice[i].ampEnvelope),&ampEnvSetting,voice[i].keyVelocity);         

      /* Simulated 'note off' signal if amplitude dropped to zero on its own */
      if(depth_amp[i] == 0)
      {
        voice[i].lastnote = 0;
        voice[i].noteState = NOTE_SILENT;
      }
    }    

    /* actual FM synthesis algorithm */
    totalOutput += fm_iterate(&(voice[i]),depth_mod[i],depth_amp[i]);    
    
    /* note state history */
    voice[i].noteState_d = voice[i].noteState;
  }

  /* Run lowpass */
  for(i=0;i<LOWPASS_ORDER;i++) 
  {
    history[i] = S16U8MulShift8(history[i],g_cutoff) + S16U8MulShift8(history[i+1],255-g_cutoff);
  }
  history[LOWPASS_ORDER] = totalOutput - S16U8MulShift8(history[0],g_resonance);
  totalOutput = history[0];

  /* generate LFO signal, multiply with the output and mix with certain ratio */
  outputLfo.phaseCounter.value += outputLfo.freq;
  outputLfo.outSignal = S16S8MulShift8(totalOutput,lutSin[outputLfo.phaseCounter .bytes[1]]);  
  totalOutput = S16U8MulShift8(outputLfo.outSignal,outputLfo.depth) + S16U8MulShift8(totalOutput,255-outputLfo.depth);

  /* add constant offset for 'zero level' */
  DACA.CH1DATA = totalOutput + 1024;  

  /* clear ISR flag */
  TCC4.INTFLAGS |= (1<<0);

  /* set pin low for timing debug */
  digitalWrite(A,6,LOW);  
}

/*---------------------------------------------------------------------------*/
ISR(USARTD0_RXC_vect)
{   
  uint8_t data = USARTD0.DATA;    
  if(!RingBuffer_IsFull(&midi_ringBuf))
  {
    RingBuffer_Insert(&midi_ringBuf,data);
  }
}

/*---------------------------------------------------------------------------*/
int main()
{   
  uint8_t data;  
  uint8_t midibuf[3];

  init_hardware();  

  RingBuffer_InitBuffer(&midi_ringBuf, midi_ringBufData, sizeof(midi_ringBufData));
  
  ampEnvSetting.attackRate.value = 10000;
  ampEnvSetting.decayRate.value = 10;
  ampEnvSetting.releaseRate.value = 300;

  modEnvSetting.attackRate.value = 20000;
  modEnvSetting.decayRate.value = 20;
  modEnvSetting.releaseRate.value = 600;
  
  outputLfo.depth = 50;
  outputLfo.freq = 10;

  g_fRatio = 2;
  g_maxFmDepth = 100;
  g_modulationIndex = 3;

  while(1)
  { 
    /* Analyse MIDI input stream */
    while(RingBuffer_GetCount(&midi_ringBuf))
    {       
      data = RingBuffer_Remove(&midi_ringBuf);

      /* Sliding buffer ... */
      midibuf[0] = midibuf[1];
      midibuf[1] = midibuf[2];
      midibuf[2] = data;

      if((midibuf[1] < 128) && (midibuf[2] < 128))
      {      
        /* Note on */
        if((midibuf[0] & 0xF0) == 0x90)
        {
          /* If velocity is zero, treat it like 'note off' */
          if(midibuf[2] == 0x00)
          {
            midi_noteOffMessageHandler(midibuf[1]);
          }
          else
          {
            midi_noteOnMessageHandler(midibuf[1],midibuf[2]);
          }
        }
        /* Note off */
        else if((midibuf[0] & 0xF0) == 0x80)
        {
          midi_noteOffMessageHandler(midibuf[1]);
        }
        /* Control message */
        else if((midibuf[0] & 0xF0) == 0xB0)
        {
          midi_controlMessageHandler(midibuf[1],midibuf[2]);            
        }
      }
    }    
  }

  return 0;
}
