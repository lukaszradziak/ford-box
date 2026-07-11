#pragma once
#include <stdint.h>

struct VehicleState {
    uint8_t tpmsFrontLeft;
    uint8_t tpmsFrontRight;
    uint8_t tpmsRearRight;
    uint8_t tpmsRearLeft;
    uint8_t fuelPercent;

    uint8_t fuelScale;
    uint8_t engineTemp;
    uint16_t speed;
    uint16_t odometer;
    uint8_t param2728;
    uint8_t outsideTemp;
    uint8_t param3132;
    uint8_t airTemp[3];
    uint8_t param3940;
    uint8_t param4142;
    uint16_t rpm;
    uint8_t param4748;
    uint8_t param4950;
    uint16_t speed2;
    uint8_t coolantTemp;
    uint16_t rpmCopy[2];
    uint8_t reszta[2];
};

extern VehicleState state;

void updateVehicleState();
