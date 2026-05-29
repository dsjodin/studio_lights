#pragma once
#include <Arduino.h>

// Weeylite RB9 transmitter. The lights listen for non-connectable BLE
// iBeacon advertisements where the 16-byte UUID encodes the command.
// See PROTOCOL.md in the weeylite repo for the full encoding.
//
// channel: 1..19. group: 0 = ALL, 1..6 = group index.
namespace Weeylite {

bool begin();
void end();

// Call from loop(). Turns advertising off once the post-command hold
// window elapses, so the HTTP handler can fire-and-forget.
void tick();

void power_on (uint8_t channel);
void power_off(uint8_t channel);
void set_cct  (uint8_t channel, uint8_t group, uint16_t kelvin, uint8_t brightness);
void set_hsi  (uint8_t channel, uint8_t group, uint16_t hue,
               uint8_t saturation, uint8_t brightness);

}
