#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "a7105.h"
#include "weeylite.h"
#include "index_html.h"

// --- A7105 wiring ---
#define PIN_SCK   4
#define PIN_MISO  5
#define PIN_MOSI  6
#define PIN_CS    7

// --- WiFi ---
#define WIFI_SSID     "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"
#define WIFI_HOSTNAME "studio-lights"
#define WIFI_CONNECT_TIMEOUT_MS 20000

// --- Light kinds / modes ---
#define KIND_A7105    0   // Neewer 288ARC over A7105 SPI radio
#define KIND_WEEYLITE 1   // Weeylite RB9 over BLE iBeacon

#define MODE_CCT 0
#define MODE_HSI 1

// --- Lights ---
#define NUM_LIGHTS   4
#define MAX_NAME_LEN 31

struct Light {
    uint8_t  kind;
    String   name;
    uint8_t  channel;     // a7105: 1..15, weeylite: 1..19
    uint8_t  group;       // a7105: 0=A,1=B; weeylite: 0=ALL,1..6
    uint8_t  brightness;  // 0..100
    uint16_t kelvin;      // a7105: 3200..5600; weeylite: 2800..8500
    uint16_t hue;         // 0..360 (weeylite HSI)
    uint8_t  saturation;  // 0..100 (weeylite HSI)
    uint8_t  mode;        // CCT or HSI (weeylite only)
    bool     power;       // not persisted - no feedback channel from lights
};

static Light lights[NUM_LIGHTS] = {
    { KIND_A7105,    "288 Light 1", 2, 0, 50, 5600,   0,   0, MODE_CCT, false },
    { KIND_A7105,    "288 Light 2", 2, 0, 50, 5600,   0,   0, MODE_CCT, false },
    { KIND_WEEYLITE, "RB9 Light 1", 1, 1, 50, 5500,   0,   0, MODE_CCT, false },
    { KIND_WEEYLITE, "RB9 Light 2", 1, 1, 50, 5500,   0,   0, MODE_CCT, false },
};

static A7105       radio(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
static WebServer   server(80);
static Preferences prefs;
static bool        radio_ok = false;
static bool        ble_ok   = false;

// ---------------- Persistence ----------------

static void persist_light(int id) {
    Light& L = lights[id];
    char k[8];
    snprintf(k, sizeof(k), "T%d", id); prefs.putUChar (k, L.kind);
    snprintf(k, sizeof(k), "n%d", id); prefs.putString(k, L.name);
    snprintf(k, sizeof(k), "c%d", id); prefs.putUChar (k, L.channel);
    snprintf(k, sizeof(k), "g%d", id); prefs.putUChar (k, L.group);
    snprintf(k, sizeof(k), "b%d", id); prefs.putUChar (k, L.brightness);
    snprintf(k, sizeof(k), "K%d", id); prefs.putUShort(k, L.kelvin);
    snprintf(k, sizeof(k), "H%d", id); prefs.putUShort(k, L.hue);
    snprintf(k, sizeof(k), "S%d", id); prefs.putUChar (k, L.saturation);
    snprintf(k, sizeof(k), "M%d", id); prefs.putUChar (k, L.mode);
}

static void load_lights() {
    char k[8];
    for (int i = 0; i < NUM_LIGHTS; i++) {
        Light& L = lights[i];
        snprintf(k, sizeof(k), "T%d", i); L.kind       = prefs.getUChar (k, L.kind);
        snprintf(k, sizeof(k), "n%d", i);
        String n = prefs.getString(k, "");
        if (n.length()) L.name = n;
        snprintf(k, sizeof(k), "c%d", i); L.channel    = prefs.getUChar (k, L.channel);
        snprintf(k, sizeof(k), "g%d", i); L.group      = prefs.getUChar (k, L.group);
        snprintf(k, sizeof(k), "b%d", i); L.brightness = prefs.getUChar (k, L.brightness);
        snprintf(k, sizeof(k), "K%d", i); L.kelvin     = prefs.getUShort(k, L.kelvin);
        snprintf(k, sizeof(k), "H%d", i); L.hue        = prefs.getUShort(k, L.hue);
        snprintf(k, sizeof(k), "S%d", i); L.saturation = prefs.getUChar (k, L.saturation);
        snprintf(k, sizeof(k), "M%d", i); L.mode       = prefs.getUChar (k, L.mode);
    }
}

// ---------------- A7105 packet helpers ----------------

static uint8_t make_target(uint8_t ch, uint8_t grp) {
    return (uint8_t)((grp << 6) | (ch & 0x0F));
}

static void build_power_packet(uint8_t out[6], uint8_t target, bool on) {
    out[0] = 0x7E; out[1] = 0xA4; out[2] = target;
    out[3] = on ? 0x01 : 0x00; out[4] = 0x00;
    out[5] = (uint8_t)((out[0]+out[1]+out[2]+out[3]+out[4]) & 0xFF);
}

static void build_state_packet(uint8_t out[6], uint8_t target,
                               uint8_t bri_pct, uint16_t kelvin) {
    out[0] = 0x7E; out[1] = 0xA5; out[2] = target;
    out[3] = bri_pct;
    out[4] = (uint8_t)(kelvin / 100);
    out[5] = (uint8_t)((out[0]+out[1]+out[2]+out[3]+out[4]) & 0xFF);
}

// ---------------- Dispatch ----------------

static void send_power(Light& L, bool on) {
    if (L.kind == KIND_A7105) {
        if (radio_ok) {
            uint8_t pkt[6];
            build_power_packet(pkt, make_target(L.channel, L.group), on);
            radio.send_command(L.channel, pkt);
            if (on) {
                build_state_packet(pkt, make_target(L.channel, L.group),
                                   L.brightness, L.kelvin);
                radio.send_command(L.channel, pkt);
            }
        }
    } else {
        if (ble_ok) {
            if (on) Weeylite::power_on(L.channel);
            else    Weeylite::power_off(L.channel);
        }
    }
    L.power = on;
}

static void send_state(Light& L) {
    if (L.kind == KIND_A7105) {
        if (!radio_ok) return;
        uint8_t pkt[6];
        build_state_packet(pkt, make_target(L.channel, L.group),
                           L.brightness, L.kelvin);
        radio.send_command(L.channel, pkt);
    } else {
        if (!ble_ok) return;
        if (L.mode == MODE_HSI) {
            Weeylite::set_hsi(L.channel, L.group, L.hue, L.saturation, L.brightness);
        } else {
            Weeylite::set_cct(L.channel, L.group, L.kelvin, L.brightness);
        }
    }
}

// ---------------- JSON helpers ----------------

static void json_escape(String& out, const String& s) {
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if ((uint8_t)c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
}

static const char* kind_str(uint8_t k) {
    return k == KIND_A7105 ? "a7105" : "weeylite";
}
static const char* mode_str(uint8_t m) {
    return m == MODE_HSI ? "hsi" : "cct";
}

static String state_json() {
    String j = "{\"radio_ok\":";
    j += radio_ok ? "true" : "false";
    j += ",\"ble_ok\":";
    j += ble_ok ? "true" : "false";
    j += ",\"lights\":[";
    for (int i = 0; i < NUM_LIGHTS; i++) {
        Light& L = lights[i];
        if (i) j += ",";
        j += "{\"id\":";          j += i;
        j += ",\"kind\":\"";      j += kind_str(L.kind); j += "\"";
        j += ",\"name\":\"";      json_escape(j, L.name); j += "\"";
        j += ",\"channel\":";     j += L.channel;
        j += ",\"group\":";       j += L.group;
        j += ",\"brightness\":";  j += L.brightness;
        j += ",\"kelvin\":";      j += L.kelvin;
        j += ",\"hue\":";         j += L.hue;
        j += ",\"saturation\":";  j += L.saturation;
        j += ",\"mode\":\"";      j += mode_str(L.mode); j += "\"";
        j += ",\"power\":";       j += L.power ? "true" : "false";
        j += "}";
    }
    j += "]}";
    return j;
}

// ---------------- Web handlers ----------------

static int clamp_i(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint16_t clamp_kelvin(uint8_t kind, int v) {
    if (kind == KIND_A7105) v = clamp_i(v, 3200, 5600);
    else                    v = clamp_i(v, 2800, 8500);
    return (uint16_t)((v / 100) * 100);
}

static void handle_root() {
    server.send_P(200, "text/html", INDEX_HTML);
}

static void handle_state() {
    server.send(200, "application/json", state_json());
}

static void handle_power_all() {
    bool on = server.arg("on") == "1";
    for (int i = 0; i < NUM_LIGHTS; i++) {
        send_power(lights[i], on);
    }
    server.send(200, "application/json", state_json());
}

static void handle_set_light() {
    if (!server.hasArg("id")) {
        server.send(400, "text/plain", "missing id");
        return;
    }
    int id = server.arg("id").toInt();
    if (id < 0 || id >= NUM_LIGHTS) {
        server.send(400, "text/plain", "bad id");
        return;
    }
    Light& L = lights[id];

    bool state_changed  = false;
    bool config_changed = false;
    bool do_power       = false;
    bool new_power      = L.power;

    if (server.hasArg("bri")) {
        L.brightness = (uint8_t)clamp_i(server.arg("bri").toInt(), 0, 100);
        state_changed  = true;
        config_changed = true;
    }
    if (server.hasArg("k")) {
        L.kelvin = clamp_kelvin(L.kind, server.arg("k").toInt());
        if (L.kind == KIND_WEEYLITE) L.mode = MODE_CCT;
        state_changed  = true;
        config_changed = true;
    }
    if (server.hasArg("hue")) {
        L.hue = (uint16_t)clamp_i(server.arg("hue").toInt(), 0, 360);
        if (L.kind == KIND_WEEYLITE) L.mode = MODE_HSI;
        state_changed  = true;
        config_changed = true;
    }
    if (server.hasArg("sat")) {
        L.saturation = (uint8_t)clamp_i(server.arg("sat").toInt(), 0, 100);
        if (L.kind == KIND_WEEYLITE) L.mode = MODE_HSI;
        state_changed  = true;
        config_changed = true;
    }
    if (server.hasArg("mode")) {
        String m = server.arg("mode");
        if      (m == "cct") L.mode = MODE_CCT;
        else if (m == "hsi") L.mode = MODE_HSI;
        // mode toggle alone does not trigger a packet - the next bri/k/hue/sat
        // change will TX with the new mode. Keeps the UI snappy.
        config_changed = true;
    }
    if (server.hasArg("channel")) {
        int v = server.arg("channel").toInt();
        L.channel = (uint8_t)(L.kind == KIND_A7105 ? clamp_i(v, 1, 15)
                                                   : clamp_i(v, 1, 19));
        config_changed = true;
    }
    if (server.hasArg("group")) {
        int v = server.arg("group").toInt();
        if (L.kind == KIND_A7105) {
            if (v == 0 || v == 1) { L.group = (uint8_t)v; config_changed = true; }
        } else {
            L.group = (uint8_t)clamp_i(v, 0, 6);
            config_changed = true;
        }
    }
    if (server.hasArg("name")) {
        String n = server.arg("name");
        n.trim();
        if (n.length() == 0) n = "Light " + String(id + 1);
        if (n.length() > MAX_NAME_LEN) n = n.substring(0, MAX_NAME_LEN);
        L.name = n;
        config_changed = true;
    }
    if (server.hasArg("power")) {
        new_power = server.arg("power") == "1";
        do_power  = true;
    }

    if (do_power) {
        send_power(L, new_power);
    } else if (state_changed) {
        send_state(L);
    }

    if (config_changed) persist_light(id);

    server.send(200, "application/json", state_json());
}

// ---------------- Setup / loop ----------------

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[boot] studio-lights starting");

    prefs.begin("lights", false);
    load_lights();

    // A7105 is an external SPI chip - no impact on WiFi/BLE.
    radio_ok = radio.begin();
    Serial.printf("[radio] A7105  init %s\n",
                  radio_ok ? "OK" : "FAILED (check wiring)");

    // Bring up WiFi BEFORE NimBLE. On ESP32-C3 the BLE controller and
    // the WiFi STA share a single 2.4 GHz radio; initializing BLE first
    // has been observed to keep STA from associating on some networks.
    // WiFi-first lets the controller's coexistence scheduler see WiFi
    // already in STA state when BLE comes up.
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(WIFI_HOSTNAME);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("[wifi] joining \"%s\"...\n", WIFI_SSID);

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - t0 < WIFI_CONNECT_TIMEOUT_MS) {
        delay(250);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[wifi] connected  IP=%s  RSSI=%d dBm  host=%s\n",
                      WiFi.localIP().toString().c_str(),
                      WiFi.RSSI(),
                      WIFI_HOSTNAME);
    } else {
        Serial.println("[wifi] connect FAILED - will keep retrying in background");
        WiFi.setAutoReconnect(true);
    }

    ble_ok = Weeylite::begin();
    Serial.printf("[ble]   NimBLE init %s\n",
                  ble_ok ? "OK" : "FAILED");

    server.on("/", HTTP_GET, handle_root);
    server.on("/api/state", HTTP_GET,  handle_state);
    server.on("/api/power", HTTP_POST, handle_power_all);
    server.on("/api/light", HTTP_POST, handle_set_light);
    server.onNotFound([](){ server.send(404, "text/plain", "not found"); });
    server.begin();
    Serial.println("[http] server up on :80");
}

void loop() {
    server.handleClient();
    Weeylite::tick();
}
