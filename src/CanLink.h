#pragma once
#include <stdint.h>

void canLinkBegin();
void sendIsoTp(uint16_t id, const uint8_t *payload, uint8_t len);
