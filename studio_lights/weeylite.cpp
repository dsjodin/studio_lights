#include "weeylite.h"
#include <NimBLEDevice.h>
#include <math.h>

namespace Weeylite {

static NimBLEAdvertising* adv = nullptr;
static uint8_t packet_id = 0;

// How long we keep the iBeacon advertising live per command. The lights
// check the rolling packet ID byte, so a single transmission is enough
// to be picked up; ~250 ms covers about 2-3 advertising intervals.
static const uint32_t TX_DURATION_MS = 250;

static uint8_t next_id() {
    packet_id = (uint8_t)((packet_id + 1) % 223);
    return packet_id;
}

static void hsv_to_rgb(uint16_t h_deg, uint8_t s_pct, uint8_t v_pct,
                       uint8_t& r, uint8_t& g, uint8_t& b) {
    float s = s_pct / 100.0f;
    float v = v_pct / 100.0f;
    float c = v * s;
    float hh = (h_deg % 360) / 60.0f;
    float x = c * (1.0f - fabsf(fmodf(hh, 2.0f) - 1.0f));
    float m = v - c;
    float rf = 0, gf = 0, bf = 0;
    if      (hh < 1) { rf = c; gf = x; bf = 0; }
    else if (hh < 2) { rf = x; gf = c; bf = 0; }
    else if (hh < 3) { rf = 0; gf = c; bf = x; }
    else if (hh < 4) { rf = 0; gf = x; bf = c; }
    else if (hh < 5) { rf = x; gf = 0; bf = c; }
    else             { rf = c; gf = 0; bf = x; }
    r = (uint8_t)((rf + m) * 255.0f);
    g = (uint8_t)((gf + m) * 255.0f);
    b = (uint8_t)((bf + m) * 255.0f);
}

static void build_uuid(uint8_t out[16], uint8_t channel, uint8_t group,
                       uint8_t mode, uint8_t b3, uint8_t b4, uint8_t b5,
                       uint8_t b6, uint8_t b7, uint8_t b8, uint8_t b9,
                       uint8_t b10) {
    out[0]  = 0xEF;
    out[1]  = (uint8_t)((channel << 3) | (group & 0x07));
    out[2]  = mode;
    out[3]  = b3;
    out[4]  = b4;
    out[5]  = b5;
    out[6]  = b6;
    out[7]  = b7;
    out[8]  = b8;
    out[9]  = b9;
    out[10] = b10;
    out[11] = 0x11;
    out[12] = 0x22;
    out[13] = 0xFE;
    out[14] = next_id();
    out[15] = 0x00;
}

static void transmit(const uint8_t uuid[16]) {
    if (!adv) return;

    // BLE manufacturer-specific data layout for an Apple iBeacon:
    //   company id LE: 4C 00
    //   iBeacon prefix: 02 15
    //   UUID (16): command payload
    //   major BE: 00 0A (10)
    //   minor BE: 00 6E (110)
    //   tx power: C5 (-59 dBm)
    std::string mfg;
    mfg.reserve(25);
    mfg += (char)0x4C; mfg += (char)0x00;
    mfg += (char)0x02; mfg += (char)0x15;
    mfg.append((const char*)uuid, 16);
    mfg += (char)0x00; mfg += (char)0x0A;
    mfg += (char)0x00; mfg += (char)0x6E;
    mfg += (char)0xC5;

    NimBLEAdvertisementData data;
    data.setFlags(0x04); // BR/EDR not supported
    data.setManufacturerData(mfg);

    adv->stop();
    adv->setAdvertisementData(data);
    adv->start();
    delay(TX_DURATION_MS);
    adv->stop();
}

bool begin() {
    NimBLEDevice::init("");
    adv = NimBLEDevice::getAdvertising();
    if (!adv) return false;
    // 100 ms advertising interval, matching the Android app's LOW_LATENCY
    // mode. Units are 0.625 ms, so 0xA0 = 160 = 100 ms.
    adv->setMinInterval(0xA0);
    adv->setMaxInterval(0xA0);
    return true;
}

void end() {
    if (adv) adv->stop();
    NimBLEDevice::deinit(true);
    adv = nullptr;
}

void power_on(uint8_t channel) {
    uint8_t u[16];
    build_uuid(u, channel, 0, 4, 0x00, 0x64, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00);
    transmit(u);
}

void power_off(uint8_t channel) {
    uint8_t u[16];
    build_uuid(u, channel, 0, 3, 0x00, 0x64, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00);
    transmit(u);
}

void set_cct(uint8_t channel, uint8_t group, uint16_t kelvin, uint8_t brightness) {
    int kv = (int)(kelvin / 100);
    if (kv < 28) kv = 28;
    if (kv > 85) kv = 85;
    if (brightness > 100) brightness = 100;
    uint8_t u[16];
    build_uuid(u, channel, group, 0,
               (uint8_t)kv, brightness, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00);
    transmit(u);
}

void set_hsi(uint8_t channel, uint8_t group, uint16_t hue,
             uint8_t saturation, uint8_t brightness) {
    if (hue > 360) hue = 360;
    if (saturation > 100) saturation = 100;
    if (brightness > 100) brightness = 100;
    uint8_t r, g, b;
    hsv_to_rgb(hue, saturation, brightness, r, g, b);
    uint8_t u[16];
    build_uuid(u, channel, group, 1,
               0x99, brightness, r, g, b,
               (uint8_t)((hue >> 8) & 0xFF),
               (uint8_t)(hue & 0xFF),
               saturation);
    transmit(u);
}

}
