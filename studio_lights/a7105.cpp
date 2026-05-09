#include "a7105.h"
#include <SPI.h>
#include <string.h>

static const uint8_t REMOTE_ID[4] = {0xD2, 0x53, 0x69, 0x4D};

// Register init table from protocol doc Section 9.1, with one change:
// 0x0B = 0x19 to put the A7105 in 4-wire SPI mode (GIO1 = SDO).
static const struct { uint8_t reg; uint8_t val; } INIT_TABLE[] = {
    {0x01, 0x42}, {0x02, 0x00}, {0x03, 0x05}, {0x04, 0x00},
    {0x07, 0x00}, {0x08, 0x00}, {0x09, 0x00}, {0x0A, 0x00},
    {0x0B, 0x19}, {0x0C, 0x09}, {0x0D, 0x05}, {0x0E, 0x04},
    {0x10, 0x9E}, {0x11, 0x4B}, {0x12, 0x00}, {0x13, 0x02},
    {0x14, 0x16}, {0x15, 0x2B}, {0x16, 0x12}, {0x17, 0x00},
    {0x18, 0x62}, {0x19, 0x80}, {0x1A, 0x80}, {0x1B, 0x00},
    {0x1C, 0x0A}, {0x1D, 0x32}, {0x1E, 0xC3}, {0x1F, 0x07},
    {0x20, 0x16}, {0x21, 0x00}, {0x22, 0x00}, {0x24, 0x13},
    {0x25, 0x00}, {0x26, 0x3B}, {0x27, 0x00}, {0x28, 0x17},
    {0x29, 0x07}, {0x2A, 0x80}, {0x2B, 0x03}, {0x2C, 0x01},
    {0x2D, 0x45}, {0x2E, 0x18}, {0x2F, 0x00}, {0x30, 0x01},
    {0x31, 0x0F},
};

A7105::A7105(int sck, int miso, int mosi, int cs)
    : _sck(sck), _miso(miso), _mosi(mosi), _cs(cs) {}

void A7105::cs_low()  { digitalWrite(_cs, LOW); }
void A7105::cs_high() { digitalWrite(_cs, HIGH); }

void A7105::write_reg(uint8_t reg, uint8_t val) {
    cs_low();
    SPI.transfer(reg & 0x3F);
    SPI.transfer(val);
    cs_high();
}

uint8_t A7105::read_reg(uint8_t reg) {
    cs_low();
    SPI.transfer(0x40 | (reg & 0x3F));
    uint8_t v = SPI.transfer(0x00);
    cs_high();
    return v;
}

void A7105::write_reg_multi(uint8_t reg, const uint8_t* data, size_t n) {
    cs_low();
    SPI.transfer(reg & 0x3F);
    for (size_t i = 0; i < n; i++) SPI.transfer(data[i]);
    cs_high();
}

void A7105::strobe(uint8_t cmd) {
    cs_low();
    SPI.transfer(cmd);
    cs_high();
}

void A7105::init_chip() {
    write_reg(0x00, 0x00);
    delay(10);

    write_reg(0x0B, 0x19);

    write_reg_multi(0x06, REMOTE_ID, 4);

    write_reg(0x0F, 0x07);

    for (size_t i = 0; i < sizeof(INIT_TABLE)/sizeof(INIT_TABLE[0]); i++) {
        write_reg(INIT_TABLE[i].reg, INIT_TABLE[i].val);
    }

    write_reg(0x02, 0x01);
    uint32_t t0 = millis();
    while (read_reg(0x02) & 0x07) {
        if (millis() - t0 > 50) break;
        delayMicroseconds(100);
    }

    write_reg(0x24, 0x13);
    write_reg(0x25, 0x09);
}

bool A7105::begin() {
    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);

    SPI.begin(_sck, _miso, _mosi, _cs);
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

    init_chip();

    uint8_t id[4];
    cs_low();
    SPI.transfer(0x46);
    for (int i = 0; i < 4; i++) id[i] = SPI.transfer(0x00);
    cs_high();
    return memcmp(id, REMOTE_ID, 4) == 0;
}

void A7105::transmit_one(uint8_t channel_n, const uint8_t payload[6]) {
    write_reg_multi(0x06, REMOTE_ID, 4);
    write_reg(0x0F, (uint8_t)(4 * channel_n - 1));
    strobe(0xE0);
    write_reg_multi(0x05, payload, 6);
    strobe(0xD0);
}

void A7105::send_command(uint8_t channel_n, const uint8_t payload[6],
                        int bursts, int gap_ms) {
    for (int i = 0; i < bursts; i++) {
        transmit_one(channel_n, payload);
        if (i + 1 < bursts) delay(gap_ms);
    }
}
