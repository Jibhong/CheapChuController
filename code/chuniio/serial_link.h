#ifndef SERIAL_LINK_H_INCLUDED
#define SERIAL_LINK_H_INCLUDED

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Must match firmware's USBDevice.setID() call exactly.
#define PICO_USB_VID 0x2E8A
#define PICO_USB_PID 0x000A

#define SL_SYNC0 0xAA
#define SL_SYNC1 0x55
#define SL_PACKET_SIZE 44 // 2 sync + 1 seq + 32 slider + 6 air + 2 btn + 1 checksum

typedef struct
{
    uint8_t seq;
    uint8_t slider[32];
    uint8_t air[6];
    uint8_t test_btn;
    uint8_t service_btn;
} sl_frame_t;

// Scans COM1..COM256 for a device matching PICO_USB_VID/PID via the
// registry (fast, no need to open each port) and returns the port
// number, or -1 if not found.
int sl_find_pico_port(void);

// Opens the given COM port number for exclusive read access at the
// firmware's baud rate. Returns INVALID_HANDLE_VALUE on failure.
HANDLE sl_open_port(int com_number);

// Blocking read of exactly one valid frame from an already-open serial
// handle, resyncing on the 0xAA 0x55 marker and validating checksum.
// Returns true on success, false on I/O error (caller should treat this
// as "device disconnected" and attempt to re-scan/reconnect).
bool sl_read_frame(HANDLE hSerial, sl_frame_t *out);

#ifdef __cplusplus
}
#endif

#endif // SERIAL_LINK_H_INCLUDED
