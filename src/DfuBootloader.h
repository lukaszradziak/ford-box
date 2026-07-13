#pragma once
#include <stdint.h>

// Reboot into the STM32F105 factory ROM bootloader (USB DFU).
void dfuRequestBootloader();

// Reset vector stored in factory system memory. It identifies known ROM
// bootloader revisions and is useful when diagnosing DFU availability.
uint32_t dfuBootloaderResetVector();
const char *dfuBootloaderVersion();
