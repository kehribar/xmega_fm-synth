// Copyright 2011 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// -----------------------------------------------------------------------------
//
// A set of basic operands, especially useful for fixed-point arithmetic, with
// fast ASM implementations.

#ifndef AVRLIBX_UTILS_OP_H_
#define AVRLIBX_UTILS_OP_H_

#include <avr/pgmspace.h>
#include <stdint.h>
#include <stdlib.h>

typedef union {
  uint16_t value;
  uint8_t bytes[2];
} Word;

typedef union {
  uint32_t value;
  uint16_t words[2];
  uint8_t bytes[4];
} LongWord;

typedef struct uint24_t {
  uint16_t integral;
  uint8_t fractional;
} uint24_t;

typedef struct uint24c_t {
  uint8_t carry;
  uint16_t integral;
  uint8_t fractional;
} uint24c_t;

static inline uint24c_t U24AddC(uint24c_t a, uint24_t b) {
  uint16_t a_int = a.integral;
  uint16_t b_int = b.integral;
  uint8_t a_frac = a.fractional;
  uint8_t b_frac = b.fractional;
  uint8_t a_carry = 0;
  uint24c_t result;
  asm(
    "add %0, %6"      "\n\t"
    "adc %A1, %A7"    "\n\t"
    "adc %B1, %B7"    "\n\t"
    "adc %2, r1"      "\n\t"
    : "=r" (a_frac), "=r" (a_int), "=r" (a_carry)
    : "0" (a_frac), "1" (a_int), "2" (a_carry), "a" (b_frac), "a" (b_int)
  );
  result.integral = a_int;
  result.fractional = a_frac;
  result.carry = a_carry;
  return result;
}

static inline uint24_t U24Add(uint24_t a, uint24_t b) {
  uint16_t a_int = a.integral;
  uint16_t b_int = b.integral;
  uint8_t a_frac = a.fractional;
  uint8_t b_frac = b.fractional;
  uint24_t result;
  asm(
    "add %0, %4"      "\n\t"
    "adc %A1, %A5"    "\n\t"
    "adc %B1, %B5"    "\n\t"
    : "=r" (a_frac), "=r" (a_int)
    : "0" (a_frac), "1" (a_int), "a" (b_frac), "a" (b_int)
  );
  result.integral = a_int;
  result.fractional = a_frac;
  return result;
}

static inline uint24_t U24Sub(uint24_t a, uint24_t b) {
  uint16_t a_int = a.integral;
  uint16_t b_int = b.integral;
  uint8_t a_frac = a.fractional;
  uint8_t b_frac = b.fractional;
  uint24_t result;
  asm(
    "sub %0, %4"      "\n\t"
    "sbc %A1, %A5"    "\n\t"
    "sbc %B1, %B5"    "\n\t"
    : "=r" (a_frac), "=r" (a_int)
    : "0" (a_frac), "1" (a_int), "a" (b_frac), "a" (b_int)
  );
  result.integral = a_int;
  result.fractional = a_frac;
  return result;
}

static inline uint24_t U24ShiftRight(uint24_t a) {
  uint16_t a_int = a.integral;
  uint8_t a_frac = a.fractional;
  uint24_t result;
  asm(
    "lsr %B1"      "\n\t"
    "ror %A1"    "\n\t"
    "ror %0"    "\n\t"
    : "=r" (a_frac), "=r" (a_int)
    : "0" (a_frac), "1" (a_int)
  );
  result.integral = a_int;
  result.fractional = a_frac;
  return result;
}

static inline uint24_t U24ShiftLeft(uint24_t a) {
  uint16_t a_int = a.integral;
  uint8_t a_frac = a.fractional;
  uint24_t result;
  asm(
    "lsl %0"    "\n\t"
    "rol %A1"    "\n\t"
    "rol %B1"      "\n\t"
    : "=r" (a_frac), "=r" (a_int)
    : "0" (a_frac), "1" (a_int)
  );
  result.integral = a_int;
  result.fractional = a_frac;
  return result;
}

static inline uint8_t S16ClipU8(int16_t value) {
  uint8_t result;
  asm(
    "mov %0, %A1"   "\n\t"  // by default, copy the value.
    "or %B1, %B1"   "\n\t"  // load H to set flags.
    "brpl .+4"      "\n\t"  // if positive, skip
    "ldi %0, 0"     "\n\t"  // set to 0.
    "rjmp .+4"      "\n\t"  // and jump.
    "breq .+2"      "\n\t"  // if null, skip
    "ldi %0, 255"   "\n\t"  // set to 255
    : "=r" (result)
    : "a" (value)
  );
  return result;
}

static inline int8_t S16ClipS8(int16_t value) {
  return S16ClipU8(value + 128) + 128;
}

static inline uint8_t U8Mix_balance(uint8_t a, uint8_t b, uint8_t balance) {
  Word sum;
  asm(
    "mul %3, %2"      "\n\t"  // b * balance
    "movw %A0, r0"    "\n\t"  // to sum
    "com %2"          "\n\t"  // 255 - balance
    "mul %1, %2"      "\n\t"  // a * (255 - balance)
    "com %2"          "\n\t"  // reset balance to its previous value
    "add %A0, r0"     "\n\t"  // add to sum L
    "adc %B0, r1"     "\n\t"  // add to sum H
    "eor r1, r1"      "\n\t"  // reset r1 after multiplication
    : "&=r" (sum)
    : "a" (a), "a" (balance), "a" (b)
    );
  return sum.bytes[1];
}

static inline uint8_t U8Mix_gain(
    uint8_t a, uint8_t b,
    uint8_t gain_a, uint8_t gain_b) {
  Word sum;
  asm(
    "mul %3, %4"      "\n\t"  // b * gain_b
    "movw %A0, r0"    "\n\t"  // to sum
    "mul %1, %2"      "\n\t"  // a * gain_a
    "add %A0, r0"     "\n\t"  // add to sum L
    "adc %B0, r1"     "\n\t"  // add to sum H
    "eor r1, r1"      "\n\t"  // reset r1 after multiplication
    : "&=r" (sum)
    : "a" (a), "a" (gain_a), "a" (b), "a" (gain_b)
    );
  return sum.bytes[1];
}

static inline int8_t S8Mix(
    int8_t a, int8_t b,
    uint8_t gain_a, uint8_t gain_b) {
  Word sum;
  asm(
    "mulsu %3, %4"    "\n\t"  // b * gain_b
    "movw %A0, r0"    "\n\t"  // to sum
    "mulsu %1, %2"    "\n\t"  // a * gain_a
    "add %A0, r0"     "\n\t"  // add to sum L
    "adc %B0, r1"     "\n\t"  // add to sum H
    "eor r1, r1"      "\n\t"  // reset r1 after multiplication
    : "&=r" (sum)
    : "a" (a), "a" (gain_a), "a" (b), "a" (gain_b)
    );
  return sum.bytes[1];
}

static inline uint16_t U8MixU16(uint8_t a, uint8_t b, uint8_t balance) {
  Word sum;
  asm(
    "mul %3, %2"      "\n\t"  // b * balance
    "movw %A0, r0"    "\n\t"  // to sum
    "com %2"          "\n\t"  // 255 - balance
    "mul %1, %2"      "\n\t"  // a * (255 - balance)
    "com %2"          "\n\t"  // reset balance to its previous value
    "add %A0, r0"     "\n\t"  // add to sum L
    "adc %B0, r1"     "\n\t"  // add to sum H
    "eor r1, r1"      "\n\t"  // reset r1 after multiplication
    : "&=r" (sum)
    : "a" (a), "a" (balance), "a" (b)
    );
  return sum.value;
}

static inline uint8_t U8U4MixU8(uint8_t a, uint8_t b, uint8_t balance) {
  uint16_t sum;
  asm(
    "mul %2, %1"      "\n\t"  // b * balance
    "movw %A3, r0"    "\n\t"  // to sum
    "com %1"          "\n\t"  // 255 - balance
    "subi %1, 240"    "\n\t"  // 15 - balance
    "mul %0, %1"      "\n\t"  // a * (15 - balance)
    "subi %1, 16"     "\n\t"
    "com %1"          "\n\t"  // reset balance to its previous value
    "add %A3, r0"     "\n\t"  // add to sum L
    "adc %B3, r1"     "\n\t"  // add to sum H
    "eor r1, r1"      "\n\t"  // reset r1 after multiplication
    "andi %B3, 15"    "\n\t"  // keep 4 lowest bits of H
    "andi %A3, 240"   "\n\t"  // keep 4 highest bits of L 
    "or %B3, %A3"     "\n\t"  // copy 4 high bits of L to H -> LLLLHHHH
    "swap %B3"        "\n\t"  // swap to get HHHHLLLL
    "mov %0, %B3"     "\n\t"  // move to output
    : "=r" (a)
    : "a" (balance), "a" (b), "a" (sum)
    );
  return a;
}

static inline uint16_t U8U4MixU12(uint8_t a, uint8_t b, uint8_t balance) {
  uint16_t sum;
  asm(
    "mul %3, %2"      "\n\t"  // b * balance
    "movw %A0, r0"    "\n\t"  // to sum
    "com %2"          "\n\t"  // 255 - balance
    "subi %2, 240"    "\n\t"  // 15 - balance
    "mul %1, %2"      "\n\t"  // a * (15 - balance)
    "subi %2, 16"     "\n\t"
    "com %2"          "\n\t"  // reset balance to its previous value
    "add %A0, r0"     "\n\t"  // add to sum L
    "adc %B0, r1"     "\n\t"  // add to sum H
    "eor r1, r1"      "\n\t"  // reset r1 after multiplication
    : "&=r" (sum)
    : "a" (a), "a" (balance), "a" (b)
    );
  return sum;
}

static inline uint8_t U8ShiftLeft4(uint8_t a) {
  uint8_t result;
  asm(
    "mov %0, %1"      "\n\t"
    "swap %0"         "\n\t"
    "andi %0, 240"     "\n\t"
    : "=r" (result)
    : "a" (a)
    );
  return result;
}

static inline uint8_t U8Swap4(uint8_t a) {
  uint8_t result;
  asm(
    "mov %0, %1"      "\n\t"
    "swap %0"         "\n\t"
    : "=r" (result)
    : "a" (a)
    );
  return result;
}

static inline uint8_t U8ShiftRight4(uint8_t a) {
  uint8_t result;
  asm(
    "mov %0, %1"      "\n\t"
    "swap %0"         "\n\t"
    "andi %0, 15"     "\n\t"
    : "=r" (result)
    : "a" (a)
    );
  return result;
}

static inline uint16_t U16ShiftRight4(uint16_t a) {
  uint16_t result;
  asm(
    "movw %A0, %A1" "\n\t"
    "lsr %B0"      "\n\t"
    "ror %A0"      "\n\t"
    "lsr %B0"      "\n\t"
    "ror %A0"      "\n\t"
    "lsr %B0"      "\n\t"
    "ror %A0"      "\n\t"
    "lsr %B0"      "\n\t"
    "ror %A0"      "\n\t"
    : "=r" (result)
    : "a" (a)
    );
  return result;
}


static inline uint8_t U8U8MulShift8(uint8_t a, uint8_t b) {
  uint8_t result;
  asm(
    "mul %1, %2"      "\n\t"
    "mov %0, r1"      "\n\t"
    "eor r1, r1"      "\n\t"
    : "=r" (result)
    : "a" (a), "a" (b)
  );
  return result;
}

static inline int8_t S8U8MulShift8(int8_t a, uint8_t b) {
  uint8_t result;
  asm(
    "mulsu %1, %2"    "\n\t"
    "mov %0, r1"      "\n\t"
    "eor r1, r1"      "\n\t"
    : "=r" (result)
    : "a" (a), "a" (b)
  );
  return result;
}

static inline int16_t S8U8Mul(int8_t a, uint8_t b) {
  int16_t result;
  asm(
    "mulsu %1, %2"    "\n\t"
    "movw %0, r0"      "\n\t"
    "eor r1, r1"      "\n\t"
    : "=r" (result)
    : "a" (a), "a" (b)
  );
  return result;
}

static inline int16_t S8S8Mul(int8_t a, int8_t b) {
  int16_t result;
  asm(
    "muls %1, %2"    "\n\t"
    "movw %0, r0"      "\n\t"
    "eor r1, r1"      "\n\t"
    : "=r" (result)
    : "a" (a), "a" (b)
  );
  return result;
}

static inline uint16_t U8U8Mul(uint8_t a, uint8_t b) {
  uint16_t result;
  asm(
    "mul %1, %2"    "\n\t"
    "movw %0, r0"      "\n\t"
    "eor r1, r1"      "\n\t"
    : "=r" (result)
    : "a" (a), "a" (b)
  );
  return result;
}

static inline int8_t S8S8MulShift8(int8_t a, int8_t b) {
  uint8_t result;
  asm(
    "muls %1, %2"    "\n\t"
    "mov %0, r1"      "\n\t"
    "eor r1, r1"      "\n\t"
    : "=r" (result)
    : "a" (a), "a" (b)
  );
  return result;
}

// The code generated by gcc for >> 6 is short but uses a loop. This saves
// a couple of cycles. Note that this solution only works for operands with
// a 14-bits resolution.
static inline uint8_t U14ShiftRight6(uint16_t value) {
  uint8_t b = value >> 8;
  uint8_t a = value & 0xff;
  uint8_t result;
  asm(
    "add %1, %1"       "\n\t"
    "adc %2, %2"       "\n\t"
    "add %1, %1"       "\n\t"
    "adc %2, %2"       "\n\t"
    : "=r" (result)
    : "a" (a), "0" (b)
  );
  return result;
}

static inline uint8_t U15ShiftRight7(uint16_t value) {
  uint8_t b = value >> 8;
  uint8_t a = value & 0xff;
  uint8_t result;
  asm(
    "add %1, %1"       "\n\t"
    "adc %2, %2"       "\n\t"
    : "=r" (result)
    : "a" (a), "0" (b)
  );
  return result;
}

static inline int16_t S16U16MulShift16(int16_t a, uint16_t b) {
  int16_t result;
  int16_t tmp;
  asm(
    "eor %A1, %A1"    "\n\t"
    "mul %A2, %A3"    "\n\t"
    "mov %B1, r1"    "\n\t"
    "mulsu %B2, %B3"  "\n\t"
    "movw %A0, r0"    "\n\t"
    "mul %B3, %A2"    "\n\t"
    "add %B1, r0"     "\n\t"
    "adc %A0, r1"     "\n\t"
    "adc %B0, %A1"    "\n\t"
    "mulsu %B2, %A3"  "\n\t"
    "sbc %B0, %A1"    "\n\t"
    "add %B1, r0"     "\n\t"
    "adc %A0, r1"     "\n\t"
    "adc %B0, %A1"    "\n\t"
    "eor r1, r1"      "\n\t"
    : "=&r" (result), "=&r" (tmp)
    : "a" (a), "a" (b)
  );
  return result;
}

static inline int16_t S16U12MulShift12(int16_t a, uint16_t b) {
  int16_t result;
  int16_t tmp;
  asm(
    "add %A3, %A3"    "\n\t"
    "adc %B3, %B3"    "\n\t"
    "add %A3, %A3"    "\n\t"
    "adc %B3, %B3"    "\n\t"
    "add %A3, %A3"    "\n\t"
    "adc %B3, %B3"    "\n\t"
    "add %A3, %A3"    "\n\t"
    "adc %B3, %B3"    "\n\t"
    "eor %A1, %A1"    "\n\t"
    "mul %A2, %A3"    "\n\t"
    "mov %B1, r1"    "\n\t"
    "mulsu %B2, %B3"  "\n\t"
    "movw %A0, r0"    "\n\t"
    "mul %B3, %A2"    "\n\t"
    "add %B1, r0"     "\n\t"
    "adc %A0, r1"     "\n\t"
    "adc %B0, %A1"    "\n\t"
    "mulsu %B2, %A3"  "\n\t"
    "sbc %B0, %A1"    "\n\t"
    "add %B1, r0"     "\n\t"
    "adc %A0, r1"     "\n\t"
    "adc %B0, %A1"    "\n\t"
    "eor r1, r1"      "\n\t"
    : "=&r" (result), "=&r" (tmp)
    : "a" (a), "a" (b)
  );
  return result;
}

static inline uint16_t U16U16MulShift16(uint16_t a, uint16_t b) {
  uint16_t result;
  uint16_t tmp;
  asm(
    "eor %A1, %A1"    "\n\t"
    "mul %A2, %A3"    "\n\t"
    "mov %B1, r1"     "\n\t"
    "mul %B2, %B3"    "\n\t"
    "movw %A0, r0"    "\n\t"
    "mul %B3, %A2"    "\n\t"
    "add %B1, r0"     "\n\t"
    "adc %A0, r1"     "\n\t"
    "adc %B0, %A1"    "\n\t"
    "mul %B2, %A3"    "\n\t"
    "add %B1, r0"     "\n\t"
    "adc %A0, r1"     "\n\t"
    "adc %B0, %A1"    "\n\t"
    "eor r1, r1"      "\n\t"
    : "=&r" (result), "=&r" (tmp)
    : "a" (a), "a" (b)
  );
  return result;
}

static inline int16_t S16U8MulShift8(int16_t a, uint8_t b) {
  int16_t result;
  asm(
    "eor %B0, %B0"    "\n\t"
    "mul %A1, %A2"    "\n\t"
    "mov %A0, r1"     "\n\t"
    "mulsu %B1, %A2"  "\n\t"
    "add %A0, r0"     "\n\t"
    "adc %B0, r1"     "\n\t"
    "eor r1, r1"      "\n\t"
    : "=&r" (result)
    : "a" (a), "a" (b)
  );
  return result;
}

static inline uint16_t U16U8MulShift8(uint16_t a, uint8_t b) {
  uint16_t result;
  asm(
    "eor %B0, %B0"    "\n\t"
    "mul %A1, %A2"    "\n\t"
    "mov %A0, r1"     "\n\t"
    "mul %B1, %A2"  "\n\t"
    "add %A0, r0"     "\n\t"
    "adc %B0, r1"     "\n\t"
    "eor r1, r1"      "\n\t"
    : "=&r" (result)
    : "a" (a), "a" (b)
  );
  return result;
}

static inline int16_t S16S8MulShift8(int16_t a, int8_t b) {
  int16_t result;
  asm(
    "eor %B0, %B0"    "\n\t"
    "muls %A2, %B1"   "\n\t"
    "movw %A0, r0"    "\n\t"
    "mulsu %A2, %A1"  "\n\t"
    "eor r0, r0"      "\n\t"
    "sbc %B0, r0"     "\n\t"
    "add %A0, r1"     "\n\t"
    "adc %B0, r0"     "\n\t"
    "eor r1, r1"      "\n\t"
    : "=&r" (result)
    : "a" (a), "a" (b)
  );
  return result;
}

static inline uint8_t InterpolateSample(
    const prog_uint8_t* table,
    uint16_t phase) __attribute__((always_inline));

static inline uint8_t InterpolateSample(
    const prog_uint8_t* table,
    uint16_t phase) {
  uint8_t result;
  uint8_t work;
  asm(
    "movw r30, %A2"           "\n\t"  // copy base address to r30:r31
    "add r30, %B3"            "\n\t"  // increment table address by phaseH
    "adc r31, r1"             "\n\t"  // just carry
    "mov %1, %A3"             "\n\t"  // move phaseL to working register
    "lpm %0, z+"              "\n\t"  // load sample[n]
    "lpm r1, z+"              "\n\t"  // load sample[n+1]
    "mul %1, r1"              "\n\t"  // multiply second sample by phaseL
    "movw r30, r0"            "\n\t"  // result to accumulator
    "com %1"                  "\n\t"  // 255 - phaseL -> phaseL
    "mul %1, %0"              "\n\t"  // multiply first sample by phaseL
    "add r30, r0"             "\n\t"  // accumulate L
    "adc r31, r1"             "\n\t"  // accumulate H
    "eor r1, r1"              "\n\t"  // reset r1 after multiplication
    "mov %0, r31"             "\n\t"  // use sum H as output
    : "=r" (result), "=r" (work)
    : "r" (table), "r" (phase)
    : "r30", "r31"
  );
  return result;
}

#endif  // AVRLIBX_UTILS_OP_H_