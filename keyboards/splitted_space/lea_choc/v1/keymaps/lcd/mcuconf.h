#pragma once

// LCD keymap needs SPI1 enabled for the display
#include_next <mcuconf.h>

#undef STM32_SPI_USE_SPI1
#define STM32_SPI_USE_SPI1 TRUE
