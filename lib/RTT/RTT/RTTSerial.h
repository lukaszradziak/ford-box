#pragma once
#ifdef ARDUINO
#include <Arduino.h>
#include "SEGGER_RTT.h"

class RTTSerial_ : public Stream {
public:
    void begin(unsigned long)            { SEGGER_RTT_Init(); }
    void begin(unsigned long, uint8_t)   { SEGGER_RTT_Init(); }

    size_t write(uint8_t c) {
        SEGGER_RTT_Write(0, (const char*)&c, 1);
        return 1;
    }
    size_t write(const uint8_t* buf, size_t size) {
        return (size_t)SEGGER_RTT_Write(0, (const char*)buf, (unsigned)size);
    }
    using Print::write;

    int available() { return SEGGER_RTT_Available(0); }
    int read()      { return SEGGER_RTT_Read(0); }
    int peek()      { return -1; }  /* RTT nie wspiera peek bez konsumpcji */
    void flush()    {}

    operator bool() { return true; }
};

extern RTTSerial_ RTTSerial;

// Nadpisuje Serial frameworka (USART1) na RTT przez SWD
#undef Serial
#define Serial RTTSerial

#endif // ARDUINO
