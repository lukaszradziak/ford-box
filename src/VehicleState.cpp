#include "VehicleState.h"

VehicleState state;

static void initState() {
    state.tpmsFrontLeft = 40;
    state.tpmsFrontRight = 40;
    state.tpmsRearRight = 40;
    state.tpmsRearLeft = 40;
    state.fuelPercent = 75;

    state.fuelScale = 150;
    state.engineTemp = 90;
    state.speed = 60;
    state.odometer = 12345;
    state.param2728 = 0;
    state.outsideTemp = 22;
    state.param3132 = 0;
    state.airTemp[0] = 22;
    state.airTemp[1] = 22;
    state.airTemp[2] = 22;
    state.param3940 = 0;
    state.param4142 = 0;
    state.rpm = 800;
    state.param4748 = 0;
    state.param4950 = 0;
    state.speed2 = 60;
    state.coolantTemp = 90;
    state.rpmCopy[0] = 800;
    state.rpmCopy[1] = 800;
    state.reszta[0] = 0;
    state.reszta[1] = 0;
}

void updateVehicleState() {
    static bool inited = false;
    if (!inited) {
        inited = true;
        initState();
    }

    static uint32_t t = 0;

    const uint8_t tpmsRaw = (uint8_t)(t % 51);
    state.tpmsFrontLeft = tpmsRaw;
    state.tpmsFrontRight = tpmsRaw;
    state.tpmsRearRight = tpmsRaw;
    state.tpmsRearLeft = tpmsRaw;

    t++;

    state.speed = 60 + (t % 40);
    state.speed2 = state.speed;
    state.rpm = 1500 + (t % 500);
    state.rpmCopy[0] = state.rpm;
    state.rpmCopy[1] = state.rpm;
    if (t % 10 == 0) state.odometer++;
}
