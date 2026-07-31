#include <Arduino.h>
#include <USB.h>

// # Custom VID PID so chuniio.dll can find the device
#define PICO_USB_VID 0x2E8A
#define PICO_USB_PID 0x000A

// ---------------------------------------------------------------------
// Packet framing constants
// ---------------------------------------------------------------------
static constexpr uint8_t SYNC0 = 0xAA;
static constexpr uint8_t SYNC1 = 0x55;
static constexpr size_t  PACKET_SIZE = 44; // 2 sync + 1 seq + 32 slider + 6 air + 2 btn + 1 checksum

static uint8_t seq_counter = 0;

// # Sensor read
static uint8_t slider_pressure[32] = {0};
static uint8_t air_beams[6] = {0};
static uint8_t test_btn = 0;
static uint8_t service_btn = 0;


// fill slider_pressure[0..31] with 0-255 pressure values.
void scan_slider() {
    bool ok = digitalRead(16) == LOW;
    if(ok){
        digitalWrite(LED_BUILTIN, HIGH);
        slider_pressure[0] = 255;
        slider_pressure[1] = 255;
        slider_pressure[0] = 255;
    }
    else{
        digitalWrite(LED_BUILTIN, LOW);
        slider_pressure[0] = 0;
        slider_pressure[1] = 0;
        slider_pressure[0] = 0;
    }
}

// fill air_beams[0..5] with 0/1.
void scan_air() {
    bool ok = digitalRead(16) == LOW;
    if(ok){
        digitalWrite(LED_BUILTIN, HIGH);
        air_beams[0] = 1;
        air_beams[1] = 1;
        air_beams[0] = 1;
    }
    else{
        digitalWrite(LED_BUILTIN, LOW);
        air_beams[0] = 0;
        air_beams[1] = 0;
        air_beams[0] = 0;
    }
    
}

void scan_buttons() {
    
}

// # Build and send one packet over USB serial.
void send_packet() {
    uint8_t pkt[PACKET_SIZE];
    size_t i = 0;

    pkt[i++] = SYNC0;
    pkt[i++] = SYNC1;
    pkt[i++] = seq_counter++;

    memcpy(&pkt[i], slider_pressure, 32);
    i += 32;

    memcpy(&pkt[i], air_beams, 6);
    i += 6;

    pkt[i++] = test_btn;
    pkt[i++] = service_btn;

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
}

void loop() {
    scan_slider();
    scan_air();
    scan_buttons();

    send_packet();

    delay(1);
}
