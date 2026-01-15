#pragma once

#include "redox.h"

#if defined(KEYBOARD_redox_rev1_proton_c)
#    include "proton_c.h"
#endif

#include "quantum.h"

#ifdef USE_I2C
#include <stddef.h>
#ifdef __AVR__
  #include <avr/io.h>
  #include <avr/interrupt.h>
#endif
#endif
