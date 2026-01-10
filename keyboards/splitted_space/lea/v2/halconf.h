#pragma once

#define HAL_USE_SERIAL  TRUE
#define HAL_USE_PWM     TRUE
#define HAL_USE_I2C     TRUE

// Note: LCD keymaps override this to TRUE in their keymap-specific halconf.h
#ifndef HAL_USE_SPI
#define HAL_USE_SPI     FALSE
#endif

#define SPI_USE_WAIT    FALSE
#define SPI_SELECT_MODE SPI_SELECT_MODE_PAD

#include_next <halconf.h>
