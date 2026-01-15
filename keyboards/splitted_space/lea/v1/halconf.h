#pragma once

#ifndef HAL_USE_SERIAL
#define HAL_USE_SERIAL  TRUE
#endif

#ifndef HAL_USE_PWM
#define HAL_USE_PWM     FALSE
#endif

#ifndef HAL_USE_I2C
#define HAL_USE_I2C     TRUE
#endif

#ifndef HAL_USE_SPI
#define HAL_USE_SPI     TRUE
#endif

#define SPI_USE_WAIT    TRUE
#define SPI_SELECT_MODE SPI_SELECT_MODE_PAD

#include_next <halconf.h>
