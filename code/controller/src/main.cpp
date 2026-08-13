

// static void i2cScan() {
//   Serial.println("Scanning Wire1 I2C bus...");
//   uint8_t found = 0;

//   for (uint8_t addr = 1; addr < 127; addr++) {
//     Wire1.beginTransmission(addr);
//     uint8_t err = Wire1.endTransmission();
//     if (err == 0) {
//       Serial.print("  Device found at 0x");
//       if (addr < 16)
//         Serial.print("0");
//       Serial.println(addr, HEX);
//       found++;
//     }
//   }

//   if (found == 0) {
//     Serial.println("  No I2C devices found!");
//   }
//   Serial.println("Scan complete.");
// }

#include <Arduino.h>
#include <USB.h>

#include <Wire.h>
extern "C" {
#include "mpr121.h"
}

// # Custom VID PID so chuniio.dll can find the device
#define PICO_USB_VID 0x2E8A
#define PICO_USB_PID 0x000A

#define MPR121_ADDR_A 0x5B
#define MPR121_ADDR_B 0x5A

// ---------------------------------------------------------------------
// Packet framing constants
// ---------------------------------------------------------------------
static constexpr uint8_t SYNC0 = 0xAA;
static constexpr uint8_t SYNC1 = 0x55;
static constexpr size_t PACKET_SIZE =
    17; // 2 sync + 1 seq + 4 slider + 6 air + 3 btn + 1 checksum

static uint8_t seq_counter = 0;

// # Sensor read
static uint8_t slider_bits[4] = {0};
static uint8_t air_beams[6] = {0};
static uint8_t test_btn = 0;
static uint8_t service_btn = 0;
static uint8_t coin_btn = 0;

static void scanReboot() {
  static bool isInit = 0;
  if (!isInit) {
    pinMode(16, INPUT_PULLUP);
    isInit = 1;
  }
  if (digitalRead(16) == LOW) {
    delay(20);
    if (digitalRead(16) == LOW) {
      rp2040.reboot();
    }
  }
}

static void initSlider() {
  Wire1.setSDA(26);
  Wire1.setSCL(27);
  Wire1.begin();

  if (!mpr121_init(MPR121_ADDR_A)) {
    Serial.println("MPR121 A not found");
    while (1) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(200);

      if (digitalRead(16) == LOW) {
        delay(20);
        if (digitalRead(16) == LOW) {
          rp2040.reboot();
        }
      }
    }
  }
  if (!mpr121_init(MPR121_ADDR_B)) {
    Serial.println("MPR121 B not found");
    while (1) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(200);

      if (digitalRead(16) == LOW) {
        delay(20);
        if (digitalRead(16) == LOW) {
          rp2040.reboot();
        }
      }
    }
  }
}

// fill slider_bits[0..3] with 32 bits of pressure status.
static void scan_slider() {
  // Get the touch status of all 12 pins at once
  uint16_t touchedRegistersA = mpr121_touched(MPR121_ADDR_A);
  uint16_t touchedRegistersB = mpr121_touched(MPR121_ADDR_B);

  for (int i = 0; i < 4; i++)
    slider_bits[i] = 0;

  // Loop through all 12 pins (0 to 11)
  for (uint8_t i = 0; i < 8; i++) {
    if (touchedRegistersA & (1 << i)) {
      uint8_t idx = 31 - (i * 2);
      slider_bits[idx / 8] |= (1 << (idx % 8));
    }
  }

  for (uint8_t i = 4; i < 12; i++) {
    if (touchedRegistersB & (1 << i)) {
      uint8_t idx = 31 - ((i - 4 + 8) * 2);
      slider_bits[idx / 8] |= (1 << (idx % 8));
    }
  }
}

// fill air_beams[0..5] with 0/1.
void scan_air() {
  static uint32_t last_air_time = 0;
  static bool air_is_on = false;
  static uint8_t current_sensor = 0;

  if (!air_is_on) {
    digitalWrite(current_sensor, HIGH);
    last_air_time = micros();
    air_is_on = true;
    return; // Don't block, let the loop continue
  }

  if (micros() - last_air_time < 1000) {
    return; // Wait until 1000us has passed
  }

  // 1 = hand not block, 0 = hand block
  // Assuming the receiver pulls LOW when receiving IR light (not blocked).
  // Swap 1 and 0 below if your sensor logic is inverted!
  if (digitalRead(6 + current_sensor) == LOW) {
    air_beams[current_sensor] = 1;
  } else {
    air_beams[current_sensor] = 0;
  }

  digitalWrite(current_sensor, LOW);
  air_is_on = false;
  
  current_sensor++;
  if (current_sensor > 5) {
    current_sensor = 0;
  }
}

void scan_buttons() {
  if (digitalRead(13) == LOW)
    test_btn = 1;
  else
    test_btn = 0;

  if (digitalRead(14) == LOW)
    service_btn = 1;
  else
    service_btn = 0;

  if (digitalRead(15) == LOW)
    coin_btn = 1;
  else
    coin_btn = 0;
}

// # Build and send one packet over USB serial.
void send_packet() {
  uint8_t pkt[PACKET_SIZE];
  size_t i = 0;

  pkt[i++] = SYNC0;
  pkt[i++] = SYNC1;
  pkt[i++] = seq_counter++;

  memcpy(&pkt[i], slider_bits, 4);
  i += 4;

  memcpy(&pkt[i], air_beams, 6);
  i += 6;

  pkt[i++] = test_btn;
  pkt[i++] = service_btn;
  pkt[i++] = coin_btn;

  // checksum
  uint8_t checksum = 0;
  for (size_t j = 2; j < i; j++) {
    checksum ^= pkt[j];
  }
  pkt[i++] = checksum;

  Serial.write(pkt, PACKET_SIZE);
}

void setup() {

  USB.disconnect();
  // Custom VID PID so chuniio.dll can find the device
  USB.setVIDPID(PICO_USB_VID, PICO_USB_PID);
  USB.setManufacturer("Jibhong");
  USB.setProduct("ChuniSliderPico");
  USB.connect();

  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(16, INPUT_PULLUP);

  for (int i = 0; i <= 5; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }
  for (int i = 6; i <= 11; i++) {
    pinMode(i, INPUT_PULLUP);
  }

  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
  pinMode(15, INPUT_PULLUP);

  initSlider();
}

void loop() {
  scan_slider();
  scan_air();
  scan_buttons();

  send_packet();

  scanReboot();

  delay(1);
}