#include <arduino.h>
#define PHOTO_PIN 26   // ADC0
#define DIGITAL_PIN 16 // digital input
#define SAMPLES 64

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(DIGITAL_PIN, INPUT);
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
  long raw = readAveraged();
  float voltage = raw * 3.3 / 4095.0;
  int gpio16 = digitalRead(DIGITAL_PIN);

  Serial.print("photo_raw: "); Serial.print(raw);
  Serial.print("  voltage: "); Serial.print(voltage, 4);
  Serial.print("  gpio16: "); Serial.println(gpio16);

  delay(10);
}