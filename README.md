# studio_lights

ESP32-C3 web portal that controls both **Neewer 288ARC** lights (over a
2.4 GHz A7105 SPI radio) and **Weeylite RB9** lights (over BLE iBeacon
advertisements) from a single board and a single web UI.

## Hardware

- ESP32-C3 dev board (e.g. `esp32-c3-devkitm-1`).
- A7105 SPI module wired per `esp32_a7105_hardware_setup.md` in the
  `neewer_light_288ARC` repo:
  - SCK  -> GPIO 4
  - MISO -> GPIO 5
  - MOSI -> GPIO 6
  - CS   -> GPIO 7
- No extra BLE hardware - the C3's built-in radio handles iBeacon TX.

## Build (Arduino IDE)

1. Install board support: `Boards Manager` -> `esp32` by Espressif.
2. Install library: `Library Manager` -> `NimBLE-Arduino` (h2zero).
3. Open `studio_lights/studio_lights.ino`.
4. Edit `WIFI_SSID` / `WIFI_PASSWORD` near the top of the sketch.
5. Select board `ESP32C3 Dev Module`, port, and upload.

## Build (PlatformIO)

```
pio run -e esp32-c3-devkitm-1 -t upload
pio device monitor
```

The serial monitor prints the IP at boot. Browse to
`http://<ip>/` or `http://studio-lights.local/`.

## HTTP API

Same shape as `neewer_light_288ARC/api.md`, extended with per-light
`kind`, `mode`, `hue`, and `saturation` fields:

- `GET  /api/state`
- `POST /api/power` form: `on=0|1`
- `POST /api/light` form: `id=N` plus any of
  `name`, `channel`, `group`, `bri`, `k`, `hue`, `sat`, `mode`, `power`

## Notes

- Default light table: 2 x 288ARC + 2 x RB9. Edit the `lights[]` array
  in `studio_lights.ino` to match your rig and reflash.
- `radio_ok` / `ble_ok` in the state JSON tell you whether each
  transmitter came up at boot.
- Each command blocks the HTTP handler for ~250-400 ms while the radio
  transmits. Serialize requests client-side.
