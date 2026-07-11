#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SEGGER_RTT_Init(void);
int  SEGGER_RTT_Write(unsigned BufferIndex, const char* buf, unsigned len);
int  SEGGER_RTT_WriteString(unsigned BufferIndex, const char* s);
int  SEGGER_RTT_printf(unsigned BufferIndex, const char* sFormat, ...);
int  SEGGER_RTT_Available(unsigned BufferIndex);
int  SEGGER_RTT_Read(unsigned BufferIndex);

#ifdef __cplusplus
}
#endif
