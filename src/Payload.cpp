#include "Payload.h"
#include "VehicleState.h"

static void writeHex8(uint8_t *out, int offset, uint8_t value) {
    static const char digits[] = "0123456789ABCDEF";
    out[offset]     = digits[(value >> 4) & 0xF];
    out[offset + 1] = digits[value & 0xF];
}

static void writeHex16(uint8_t *out, int offset, uint16_t value) {
    writeHex8(out, offset, (uint8_t)(value >> 8));
    writeHex8(out, offset + 2, (uint8_t)(value & 0xFF));
}

void buildPayload(uint8_t *out) {
    out[0] = 0x3B;
    out[1] = 0x01;
    out[2] = 0x01;
    out[3] = 0x01;
    out[4] = 0xDA;

    writeHex8(out, 5, state.tpmsFrontLeft);
    writeHex8(out, 7, state.tpmsFrontRight);
    writeHex8(out, 9, state.tpmsRearRight);
    writeHex8(out, 11, state.tpmsRearLeft);
    writeHex8(out, 13, state.fuelPercent);
    writeHex8(out, 15, state.fuelScale);
    writeHex8(out, 17, state.engineTemp);
    writeHex16(out, 19, state.speed);
    writeHex16(out, 23, state.odometer);
    writeHex8(out, 27, state.param2728);
    writeHex8(out, 29, state.outsideTemp);
    writeHex8(out, 31, state.param3132);
    writeHex8(out, 33, state.airTemp[0]);
    writeHex8(out, 35, state.airTemp[1]);
    writeHex8(out, 37, state.airTemp[2]);
    writeHex8(out, 39, state.param3940);
    writeHex8(out, 41, state.param4142);
    writeHex16(out, 43, state.rpm);
    writeHex8(out, 47, state.param4748);
    writeHex8(out, 49, state.param4950);
    writeHex16(out, 51, state.speed2);
    writeHex8(out, 55, state.coolantTemp);
    writeHex16(out, 57, state.rpmCopy[0]);
    writeHex16(out, 61, state.rpmCopy[1]);
    writeHex8(out, 65, state.reszta[0]);
    writeHex8(out, 67, state.reszta[1]);
}
