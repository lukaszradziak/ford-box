#include <Arduino.h>
#include "SerialConfig.h"

#include "VehicleState.h"
#include "Payload.h"
#include "CanLink.h"

#define CAN_ID_DATA      0x2B4
#define DATA_INTERVAL_MS 1000

static uint32_t lastDataMs = 0;
static bool sendingEnabled = true;

static void printHelp() {
    Serial.println();
    Serial.println("Commands (no Enter required):");
    Serial.println("  h - show this help");
    Serial.println("  1 - toggle 0x2B4 transmission ON/OFF");
    Serial.print("0x2B4 transmission: ");
    Serial.println(sendingEnabled ? "ON" : "OFF");
}

static void pollConsole() {
    while (Serial.available()) {
        const char command = (char)Serial.read();

        if (command == 'h' || command == 'H') {
            printHelp();
        } else if (command == '1') {
            sendingEnabled = !sendingEnabled;
            if (sendingEnabled) lastDataMs = millis();
            Serial.print("0x2B4 transmission: ");
            Serial.println(sendingEnabled ? "ON" : "OFF");
        } else if (command != '\r' && command != '\n') {
            Serial.print("Unknown command: ");
            Serial.println(command);
            Serial.println("Press h for help.");
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("ford-box - HS/CAN1 500000 bps (OBD 3/11)");
    Serial.println("0x2B4 transmission: ON (press h for help)");

    canLinkBegin();
    lastDataMs = millis();
}

void loop() {
    pollConsole();

    uint32_t now = millis();
    if (sendingEnabled && now - lastDataMs >= DATA_INTERVAL_MS) {
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
