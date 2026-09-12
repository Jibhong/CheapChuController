#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include "serial_link.h"

// Key Mappings based on provided layout (Set key to 0 or 0x00 to disable it)

// // Slider Front (Even pads 0, 2, 4 ... 30)
// static const WORD slider_keys_even[16] = {
//     0, 0, 0, 0, 0, 0, 0, 0,
//     0, 0, 0, 0, 0, 0, 0, 0
// };

// // Slider Back (Odd pads 1, 3, 5 ... 31)
// static const WORD slider_keys_odd[16] = {
//     0, 0, 0, 0, 'L', 'K', 'J', 'H',
//     'G', 'F', 'D', 'S', 0, 0, 0, 0
// };

// // Air Sensors 0-5
// static const WORD air_keys[6] = {
//     0, 0, 0, 0, 0, 0
// };

// Slider Front (Even pads 0, 2, 4 ... 30)
static const WORD slider_keys_even[16] = {
    '9', 'K', 'M', 'J', 'N', 'H', 'B', 'G',
    'V', 'F', 'C', 'D', 'X', 'S', 'Z', 'A'
};

// Slider Back (Odd pads 1, 3, 5 ... 31)
static const WORD slider_keys_odd[16] = {
    'I', '8', 'U', '7', 'Y', '6', 'T', '5',
    'R', '4', 'E', '3', 'W', '2', 'Q', '1'
};

// Air Sensors 0-5
static const WORD air_keys[6] = {
    '0', 'O', 'L', 'P', VK_OEM_COMMA, VK_OEM_PERIOD
};

// System Buttons
static const WORD test_key = VK_F1;
static const WORD service_key = VK_F2;
static const WORD coin_key = VK_F3;

// Previous frame state
static bool last_slider[32] = {false};
static bool last_air[6] = {false};
static bool last_test = false;
static bool last_service = false;
static bool last_coin = false;

// Helper to simulate a key press or release using hardware scancodes
static void send_key(WORD vk, bool down) {
    if (vk == 0) {
        return; // Key disabled / blank in settings
    }
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    // Map the virtual key to a hardware scan code
    input.ki.wScan = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    input.ki.wVk = 0; // Ignored when using KEYEVENTF_SCANCODE, but good practice to clear
    
    // Set the flag to use the scancode
    input.ki.dwFlags = KEYEVENTF_SCANCODE;
    
    if (!down) {
        input.ki.dwFlags |= KEYEVENTF_KEYUP;
    }
    SendInput(1, &input, sizeof(INPUT));
}

int main(void) {
    HANDLE hSerial = INVALID_HANDLE_VALUE;
    sl_frame_t frame;

    printf("keyboardio starting...\n");
    printf("Mapping Controller to Keyboard.\n");
    printf("Press Ctrl+C to exit.\n\n");

    while (1) {
        if (hSerial == INVALID_HANDLE_VALUE) {
            int port = sl_find_pico_port();
            if (port < 0) {
                printf("waiting for Pico (VID_%04X&PID_%04X)...\n", PICO_USB_VID, PICO_USB_PID);
                Sleep(1000);
                continue;
            }

            printf("found Pico on COM%d, connecting...\n", port);
            hSerial = sl_open_port(port);
            if (hSerial == INVALID_HANDLE_VALUE) {
                printf("failed to open COM%d, retrying...\n", port);
                Sleep(1000);
                continue;
            }

            printf("connected to COM%d\n", port);
        }

        if (!sl_read_frame(hSerial, &frame)) {
            DWORD errors;
            COMSTAT status;
            if (!ClearCommError(hSerial, &errors, &status)) {
                printf("serial link lost, will rescan\n");
                CloseHandle(hSerial);
                hSerial = INVALID_HANDLE_VALUE;
            }
            continue;
        }

        // Process System Buttons
        bool current_test = frame.test_btn;
        if (current_test != last_test) {
            send_key(test_key, current_test);
            last_test = current_test;
        }

        bool current_service = frame.service_btn;
        if (current_service != last_service) {
            send_key(service_key, current_service);
            last_service = current_service;
        }

        bool current_coin = frame.coin_btn;
        if (current_coin != last_coin) {
            send_key(coin_key, current_coin);
            last_coin = current_coin;
        }

        // Process Air Sensors
        for (int i = 0; i < 6; i++) {
            bool current_air = frame.air[i];
            if (current_air != last_air[i]) {
                send_key(air_keys[i], current_air);
                last_air[i] = current_air;
            }
        }

        // Process Slider
        for (int i = 0; i < 32; i++) {
            bool current_slider = (frame.slider_bits[i / 8] & (1 << (i % 8))) != 0;
            if (current_slider != last_slider[i]) {
                WORD vk = (i % 2 == 0) ? slider_keys_even[i / 2] : slider_keys_odd[i / 2];
                send_key(vk, current_slider);
                last_slider[i] = current_slider;
            }
        }
    }

    if (hSerial != INVALID_HANDLE_VALUE) {
        CloseHandle(hSerial);
    }

    return 0;
}
