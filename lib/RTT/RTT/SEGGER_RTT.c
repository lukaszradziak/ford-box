#include "SEGGER_RTT.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define RTT_UP_SIZE   512u
#define RTT_DOWN_SIZE 64u

typedef struct {
    const char*    sName;
    char*          pBuffer;
    uint32_t       SizeOfBuffer;
    uint32_t       WrOff;       /* written by target */
    uint32_t       RdOff;       /* written by host (OpenOCD) */
    uint32_t       Flags;
} RTT_CHANNEL;

typedef struct {
    char         acID[16];          /* "SEGGER RTT" + nulls */
    uint32_t     MaxNumUpBuffers;
    uint32_t     MaxNumDownBuffers;
    RTT_CHANNEL  Up[1];
    RTT_CHANNEL  Down[1];
} RTT_CB;

static char _UpBuf[RTT_UP_SIZE];
static char _DownBuf[RTT_DOWN_SIZE];

/* Nie inicjalizujemy tutaj "SEGGER RTT" - zeby uniknac falszywie pozytywnego
   wykrycia przez OpenOCD przed wywolaniem SEGGER_RTT_Init(). */
RTT_CB _SEGGER_RTT;

void SEGGER_RTT_Init(void) {
    memset(&_SEGGER_RTT, 0, sizeof(_SEGGER_RTT));

    /* Inicjalizuj ID dopiero tutaj - OpenOCD znajdzie blok dopiero po tym */
    _SEGGER_RTT.acID[0]  = 'S'; _SEGGER_RTT.acID[1]  = 'E';
    _SEGGER_RTT.acID[2]  = 'G'; _SEGGER_RTT.acID[3]  = 'G';
    _SEGGER_RTT.acID[4]  = 'E'; _SEGGER_RTT.acID[5]  = 'R';
    _SEGGER_RTT.acID[6]  = ' '; _SEGGER_RTT.acID[7]  = 'R';
    _SEGGER_RTT.acID[8]  = 'T'; _SEGGER_RTT.acID[9]  = 'T';
    /* [10..15] = 0 (juz wyzerowane przez memset) */

    _SEGGER_RTT.MaxNumUpBuffers   = 1;
    _SEGGER_RTT.MaxNumDownBuffers = 1;

    _SEGGER_RTT.Up[0].sName        = "Terminal";
    _SEGGER_RTT.Up[0].pBuffer      = _UpBuf;
    _SEGGER_RTT.Up[0].SizeOfBuffer = RTT_UP_SIZE;
    _SEGGER_RTT.Up[0].WrOff        = 0;
    _SEGGER_RTT.Up[0].RdOff        = 0;
    _SEGGER_RTT.Up[0].Flags        = 0;  /* SKIP gdy pelny */

    _SEGGER_RTT.Down[0].sName        = "Terminal";
    _SEGGER_RTT.Down[0].pBuffer      = _DownBuf;
    _SEGGER_RTT.Down[0].SizeOfBuffer = RTT_DOWN_SIZE;
    _SEGGER_RTT.Down[0].WrOff        = 0;
    _SEGGER_RTT.Down[0].RdOff        = 0;
    _SEGGER_RTT.Down[0].Flags        = 0;
}

int SEGGER_RTT_Write(unsigned idx, const char* buf, unsigned len) {
    RTT_CHANNEL* ch = &_SEGGER_RTT.Up[idx];
    uint32_t wr  = ch->WrOff;
    uint32_t rd  = ch->RdOff;
    int written  = 0;

    while (len--) {
        uint32_t next = (wr + 1u) % ch->SizeOfBuffer;
        if (next == rd) break;
        ch->pBuffer[wr] = *buf++;
        wr = next;
        written++;
    }
    ch->WrOff = wr;
    return written;
}

int SEGGER_RTT_WriteString(unsigned idx, const char* s) {
    RTT_CHANNEL* ch = &_SEGGER_RTT.Up[idx];
    uint32_t wr  = ch->WrOff;
    uint32_t rd  = ch->RdOff;  /* snapshot raz — OpenOCD pisze RdOff */
    int written  = 0;

    while (*s) {
        uint32_t next = (wr + 1u) % ch->SizeOfBuffer;
        if (next == rd) break;  /* pelny - pominij reszte */
        ch->pBuffer[wr] = *s++;
        wr = next;
        written++;
    }
    /* Jeden atomiczny zapis WrOff na koncu — OpenOCD nie widzi czesciowych stringow */
    ch->WrOff = wr;
    return written;
}

/* Down channel: host (OpenOCD) pisze WrOff, target czyta RdOff */
int SEGGER_RTT_Available(unsigned idx) {
    RTT_CHANNEL* ch = &_SEGGER_RTT.Down[idx];
    uint32_t wr = ch->WrOff;
    uint32_t rd = ch->RdOff;
    if (wr >= rd) return (int)(wr - rd);
    return (int)(ch->SizeOfBuffer - rd + wr);
}

int SEGGER_RTT_Read(unsigned idx) {
    RTT_CHANNEL* ch = &_SEGGER_RTT.Down[idx];
    uint32_t wr = ch->WrOff;
    uint32_t rd = ch->RdOff;
    if (wr == rd) return -1;
    int c = (unsigned char)ch->pBuffer[rd];
    ch->RdOff = (rd + 1u) % ch->SizeOfBuffer;
    return c;
}

int SEGGER_RTT_printf(unsigned idx, const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return SEGGER_RTT_WriteString(idx, buf);
}
