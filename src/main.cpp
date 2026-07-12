#include <Arduino.h>
#include "SerialConfig.h"

#include "VehicleState.h"
#include "Payload.h"
#include "CanLink.h"

#define CAN_ID_DATA      0x2B4
#define DATA_INTERVAL_MS 1000

static uint32_t lastDataMs = 0;

void setup() {
    Serial.begin(115200);
    Serial.println("ford-box - HS/CAN1 500000 bps (OBD 3/11)");

    canLinkBegin();
    lastDataMs = millis();
}

void loop() {
    uint32_t now = millis();
    if (now - lastDataMs >= DATA_INTERVAL_MS) {
        lastDataMs = now;
        updateVehicleState();

        uint8_t payload[PAYLOAD_LEN];
        buildPayload(payload);
        sendIsoTp(CAN_ID_DATA, payload, PAYLOAD_LEN);

        Serial.print("TX 0x2B4  TPMS=");
        Serial.print(state.tpmsFrontLeft);
        Serial.print("/");
        Serial.print(state.tpmsFrontRight);
        Serial.print("/");
        Serial.print(state.tpmsRearRight);
        Serial.print("/");
        Serial.print(state.tpmsRearLeft);
        Serial.print("  speed=");
        Serial.print(state.speed);
        Serial.print("  rpm=");
        Serial.print(state.rpm);
        Serial.print("  fuel=");
        Serial.print(state.fuelPercent);
        Serial.println("%");
    }
}
