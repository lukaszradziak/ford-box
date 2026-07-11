#include "CanLink.h"
#include <STM32_CAN.h>
#include <Arduino.h>

#define STB_MM PB4
#define MM_BAUD 125000
#define CF_INTERVAL_MS 20

static STM32_CAN canMM(PB5, PB6); // OBD 1/8

void canLinkBegin() {
    pinMode(STB_MM, OUTPUT);
    digitalWrite(STB_MM, LOW);
    canMM.begin();
    canMM.setBaudRate(MM_BAUD);
}

void sendIsoTp(uint16_t id, const uint8_t *payload, uint8_t len) {
    CAN_message_t msg;
    msg.id = id;
    msg.len = 8;

    msg.buf[0] = 0x10 | ((len >> 8) & 0x0F);
    msg.buf[1] = len & 0xFF;
    uint8_t chunk = (len < 6) ? len : 6;
    for (uint8_t i = 0; i < chunk; i++) msg.buf[2 + i] = payload[i];
    for (uint8_t i = chunk; i < 6; i++) msg.buf[2 + i] = 0x00;
    canMM.write(msg);

    uint8_t sent = chunk;
    uint8_t seq = 1;
    while (sent < len) {
        delay(CF_INTERVAL_MS);
        msg.buf[0] = 0x20 | (seq & 0x0F);
        uint8_t remaining = len - sent;
        uint8_t n = (remaining < 7) ? remaining : 7;
        for (uint8_t i = 0; i < n; i++) msg.buf[1 + i] = payload[sent + i];
        for (uint8_t i = n; i < 7; i++) msg.buf[1 + i] = 0x00;
        canMM.write(msg);
        sent += n;
        seq++;
    }
}
