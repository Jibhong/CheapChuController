#include <arduino.h>
#define PHOTO_PIN 26    // ADC0
#define DIGITAL_PIN 0   // digital input
#define TOGGLE_PIN 1    // output, toggles every 0.5s
#define SAMPLES 64

bool toggleState = false;
unsigned long lastToggleTime = 0;
const unsigned long TOGGLE_INTERVAL = 500; // ms

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(DIGITAL_PIN, INPUT);
  pinMode(TOGGLE_PIN, OUTPUT);
  digitalWrite(TOGGLE_PIN, LOW);
}

long readAveraged() {
  long sum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    sum += analogRead(PHOTO_PIN);
    delayMicroseconds(100);
  }
  return sum / SAMPLES;
}

void loop() {
  // non-blocking toggle every 0.5s
  unsigned long now = millis();
  if (now - lastToggleTime >= TOGGLE_INTERVAL) {
    lastToggleTime = now;
    toggleState = !toggleState;
    digitalWrite(TOGGLE_PIN, toggleState);
  }

  long raw = readAveraged();
  float voltage = raw * 3.3 / 4095.0;
  int gpio16 = digitalRead(DIGITAL_PIN);

  Serial.print("gpio26 photo_raw: "); Serial.print(raw);
  Serial.print("  voltage: "); Serial.print(voltage, 4);
  Serial.print(" |  gpio0: "); Serial.print(gpio16);
  Serial.print(" |  gpio1: "); Serial.println(toggleState);

  delay(10);
}