#include "CanLink.h"
#include <STM32_CAN.h>
#include <Arduino.h>

#define STB_CAN1 PB3
#define HS_BAUD 500000
#define CF_INTERVAL_MS 20

static STM32_CAN can1(PB8, PB9); // OBD 3/11.

void canLinkBegin() {
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    pinMode(STB_CAN1, OUTPUT);
    digitalWrite(STB_CAN1, LOW);

    can1.begin();

    __HAL_AFIO_REMAP_CAN1_2();
    can1.setBaudRate(HS_BAUD);
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
    can1.write(msg);

    uint8_t sent = chunk;
    uint8_t seq = 1;
    while (sent < len) {
        delay(CF_INTERVAL_MS);
        msg.buf[0] = 0x20 | (seq & 0x0F);
        uint8_t remaining = len - sent;
        uint8_t n = (remaining < 7) ? remaining : 7;
        for (uint8_t i = 0; i < n; i++) msg.buf[1 + i] = payload[sent + i];
        for (uint8_t i = n; i < 7; i++) msg.buf[1 + i] = 0x00;
        can1.write(msg);
        sent += n;
        seq++;
    }
}
