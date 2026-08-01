#include "serial_link.h"

#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "setupapi.lib")

// -----------------------------------------------------------------
// Find the COM port assigned to a USB serial device matching
// PICO_USB_VID/PICO_USB_PID by walking the Ports device class via
// SetupAPI and reading each device's hardware ID string, e.g.
//   USB\VID_2E8A&PID_000A&MI_00
// then pulling its COM port number out of the registry.
// -----------------------------------------------------------------
int sl_find_pico_port(void)
{
    char target[64];
    snprintf(target, sizeof(target), "VID_%04X&PID_%04X", PICO_USB_VID, PICO_USB_PID);

    HDEVINFO dev_info = SetupDiGetClassDevsA(&GUID_DEVCLASS_PORTS, "USB", NULL, DIGCF_PRESENT);
    if (dev_info == INVALID_HANDLE_VALUE) {
        return -1;
    }

    int found_port = -1;
    SP_DEVINFO_DATA dev_data;
    dev_data.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(dev_info, i, &dev_data); i++) {
        char hardware_id[512] = {0};
        DWORD data_type = 0;

        if (!SetupDiGetDeviceRegistryPropertyA(
                dev_info, &dev_data, SPDRP_HARDWAREID,
                &data_type, (PBYTE)hardware_id, sizeof(hardware_id), NULL)) {
            continue;
        }

        // hardware_id may contain multiple NUL-separated strings; check the first.
        if (strstr(hardware_id, target) == NULL) {
            continue;
        }

        // Found our VID/PID. Now read the friendly name or the
        // registry "PortName" value under the device's driver key to
        // get the actual COMx assignment.
        HKEY hkey = SetupDiOpenDevRegKey(
                dev_info, &dev_data, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if (hkey == INVALID_HANDLE_VALUE) {
            continue;
        }

        char port_name[32] = {0};
        DWORD port_name_size = sizeof(port_name);
        DWORD value_type = 0;
        LONG result = RegQueryValueExA(
                hkey, "PortName", NULL, &value_type, (LPBYTE)port_name, &port_name_size);
        RegCloseKey(hkey);

        if (result != ERROR_SUCCESS) {
            continue;
        }

        // port_name looks like "COM7"
        if (strncmp(port_name, "COM", 3) == 0) {
            found_port = atoi(port_name + 3);
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(dev_info);
    return found_port;
}

HANDLE sl_open_port(int com_number)
{
    char path[32];
    // \\.\COMn form is required for COM10 and above.
    snprintf(path, sizeof(path), "\\\\.\\COM%d", com_number);

    HANDLE h = CreateFileA(
            path,
            GENERIC_READ | GENERIC_WRITE,
            0,              // no sharing
            NULL,
            OPEN_EXISTING,
            0,              // synchronous I/O
            NULL);

    if (h == INVALID_HANDLE_VALUE) {
        return h;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(h, &dcb)) {
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }

    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(h, &dcb)) {
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 200;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 200;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(h, &timeouts);

    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);

    return h;
}

static bool read_exact(HANDLE h, uint8_t *buf, DWORD n)
{
    DWORD total = 0;
    while (total < n) {
        DWORD got = 0;
        if (!ReadFile(h, buf + total, n - total, &got, NULL)) {
            return false;
        }
        if (got == 0) {
            // Timeout with no data -- treat as a stall, let caller retry.
            return false;
        }
        total += got;
    }
    return true;
}

bool sl_read_frame(HANDLE hSerial, sl_frame_t *out)
{
    uint8_t b;

    // --- Resync: scan byte-by-byte until we see 0xAA 0x55. ---
    uint8_t prev = 0;
    for (;;) {
        if (!read_exact(hSerial, &b, 1)) {
            return false;
        }
        if (prev == SL_SYNC0 && b == SL_SYNC1) {
            break;
        }
        prev = b;
    }

    // --- Read the rest of the frame body: seq + slider + air + btns + checksum ---
    uint8_t body[SL_PACKET_SIZE - 2];
    if (!read_exact(hSerial, body, sizeof(body))) {
        return false;
    }

    uint8_t checksum = 0;
    for (size_t i = 0; i < sizeof(body) - 1; i++) {
        checksum ^= body[i];
    }
    uint8_t received_checksum = body[sizeof(body) - 1];
    if (checksum != received_checksum) {
        // Bad frame -- caller will call us again, which resyncs from here.
        return false;
    }

    size_t i = 0;
    out->seq = body[i++];
    memcpy(out->slider_bits, &body[i], 4);
    i += 4;
    memcpy(out->air, &body[i], 6);
    i += 6;
    out->test_btn = body[i++];
    out->service_btn = body[i++];
    out->coin_btn = body[i++];

    return true;
}
