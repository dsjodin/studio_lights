#pragma once
#include <Arduino.h>

class A7105 {
public:
    A7105(int sck, int miso, int mosi, int cs);

    // Init SPI + chip. Returns true if the 4-byte ID register reads back the
    // expected pairing ID (D2 53 69 4D).
    bool begin();

    // Send the same 6-byte payload as a burst of `bursts` packets, `gap_ms`
    // apart, on remote channel `channel_n` (1..N).
    void send_command(uint8_t channel_n, const uint8_t payload[6],
                      int bursts = 8, int gap_ms = 50);

private:
    int _sck, _miso, _mosi, _cs;

    void cs_low();
    void cs_high();
    void write_reg(uint8_t reg, uint8_t val);
    uint8_t read_reg(uint8_t reg);
    void write_reg_multi(uint8_t reg, const uint8_t* data, size_t n);
    void strobe(uint8_t cmd);
    void init_chip();
    void transmit_one(uint8_t channel_n, const uint8_t payload[6]);
};
