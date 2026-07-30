// Pico 2 -> USB HID keyboard slider test (earlephilhower Arduino core)
// -----------------------------------------------------------------------
// Plug the Pico 2 directly into the PC via USB. It enumerates as a
// plain USB keyboard -- no server, no driver, true plug-and-play.
//
// segatools reads Chunithm's 32 slider cells as individual keyboard
// keys (see the [slider] section of segatools.ini). This firmware
// dedicates one keycode per zone (1-32); slider_press(n) presses and
// releases that zone's key. Bind each zone in segatools.ini to match
// ZONE_KEYS below.

#include <Arduino.h>
#include <Keyboard.h>  // earlephilhower built-in HID keyboard

// ---------------------------------------------------------------------
// Zone -> Keyboard key map, one distinct key per of the 32 zones.
// Edit this table to match whatever keys you bind on the PC side in
// segatools.ini.
// ---------------------------------------------------------------------
static constexpr char ZONE_KEYS[32] = {
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
    'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
    'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
    'y', 'z', '1', '2', '3', '4', '5', '6',
};

// -----------------------------------------------------------------
// Test entry point: press slider zone `zone` (1-32) for `hold_ms`
// milliseconds, then release it.
// -----------------------------------------------------------------
void slider_press(int zone, uint32_t hold_ms = 50) {
    if (zone < 1 || zone > 32) {
        Serial.print("slider_press: invalid zone ");
        Serial.print(zone);
        Serial.print(" (must be 1-32)\n");
        return;
    }

    char key = ZONE_KEYS[zone - 1];

    digitalWrite(LED_BUILTIN, HIGH);

    // Press and release the key
    Keyboard.press(key);
    delay(hold_ms);
    Keyboard.release(key);

    digitalWrite(LED_BUILTIN, LOW);

    Serial.print("slider_press: zone ");
    Serial.print(zone);
    Serial.print(" -> ");
    Serial.print(key);
    Serial.print("\n");
}

// Debug helper: press every zone 1..32 in order.
void slider_sweep(uint32_t delay_ms = 100) {
    for (int z = 1; z <= 32; z++) {
        slider_press(z);
        delay(delay_ms);
    }
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200);

    // Initialize HID keyboard
    Keyboard.begin();

    // Wait a moment for USB enumeration
    delay(2000);

    Serial.println("USB HID keyboard ready.");
}

void loop() {
    // --- Manual test: press zone 1, wait, press zone 32 ---
    delay(1000);
    slider_press(1);
    delay(1000);
    slider_press(32);
}