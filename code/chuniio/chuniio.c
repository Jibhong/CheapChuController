// chuniio.dll -- Raspberry Pi Pico serial backend
// -----------------------------------------------------------------------
// Drop-in replacement for segatools' chuniio.dll. Implements the same
// exported interface as Brokenithm-Android-Server's chuniio.c, but
// instead of reading a Windows shared-memory block filled by a UDP/TCP
// server, it auto-detects a Pico by USB VID/PID, opens its COM port,
// and reads a small binary frame (slider pressure + air beams + test/
// service buttons) directly over serial.
//
// Reference behavior matched from Brokenithm-Android-Server/segatools/
// chuniio/chuniio.c:
//   - chuni_io_get_api_version() returns 0x0101
//   - chuni_io_jvs_poll() reports opbtn bits 0x01 (test) / 0x02 (service)
//     and a 6-bit air beam mask
//   - chuni_io_slider_start() spins a thread that calls the callback
//     with a fresh uint8_t[32] pressure array roughly every 1ms
//   - chuni_io_slider_set_leds() is a stub here (no LED path back to
//     the Pico yet -- see note at bottom of file)
// -----------------------------------------------------------------------

#include <windows.h>
#include <process.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "chuniio.h"
#include "serial_link.h"

static unsigned int __stdcall chuni_io_slider_thread_proc(void *ctx);
static void chuni_io_log(const char *fmt, ...);

// -----------------------------------------------------------------------
// Shared state between the JVS poll calls and the slider thread. All of
// this is written only by chuni_io_slider_thread_proc and read by the
// jvs_* functions, so a lightweight critical section protects it -- the
// data is small and polled frequently, a spinlock-style CS is fine.
// -----------------------------------------------------------------------
static CRITICAL_SECTION chuni_io_state_lock;
static bool chuni_io_state_lock_initialized;

static uint8_t chuni_io_latest_air[6];
static uint8_t chuni_io_latest_test_btn;
static uint8_t chuni_io_latest_service_btn;
static bool chuni_io_device_connected;

static HANDLE chuni_io_slider_thread;
static bool chuni_io_slider_stop_flag;

static bool chuni_io_coin;
static uint16_t chuni_io_coins;

// -----------------------------------------------------------------------
// Logging: writes to a plain text file next to the DLL so you can debug
// connection/auto-scan issues without attaching a debugger to the game.
// -----------------------------------------------------------------------
static void chuni_io_log(const char *fmt, ...)
{
    FILE *f = fopen("chuniio_pico.log", "a");
    if (!f) {
        return;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);

    fprintf(f, "\n");
    fclose(f);
}

uint16_t chuni_io_get_api_version(void)
{
    // 0x0102: adds chuni_io_led_init / chuni_io_led_set_colors (Chusan
    // tower LED support). Reporting 0x0101 here was silently accepted
    // for loading but chusanApp still demanded the LED exports below --
    // report the version that actually matches what this DLL implements.
    return 0x0102;
}

HRESULT chuni_io_jvs_init(void)
{
    if (!chuni_io_state_lock_initialized) {
        InitializeCriticalSection(&chuni_io_state_lock);
        chuni_io_state_lock_initialized = true;
    }

    chuni_io_log("chuni_io_jvs_init: ready");
    return S_OK;
}

void chuni_io_jvs_read_coin_counter(uint16_t *out)
{
    if (out == NULL) {
        return;
    }

    // No physical coin mech wired to the Pico in this build -- keep the
    // keyboard-vk fallback pattern from the reference so a debug coin
    // key still works if you want one later. For now this just reports
    // whatever count has accumulated (always 0 unless you wire a coin
    // input into the frame format).
    *out = chuni_io_coins;
}

void chuni_io_jvs_poll(uint8_t *opbtn, uint8_t *beams)
{
    if (!chuni_io_state_lock_initialized) {
        return;
    }

    EnterCriticalSection(&chuni_io_state_lock);

    if (chuni_io_latest_test_btn) {
        *opbtn |= 0x01; /* Test */
    }
    if (chuni_io_latest_service_btn) {
        *opbtn |= 0x02; /* Service */
    }

    for (int i = 0; i < 6; i++) {
        if (chuni_io_latest_air[i]) {
            *beams |= (1 << i);
        }
    }

    LeaveCriticalSection(&chuni_io_state_lock);
}

HRESULT chuni_io_slider_init(void)
{
    return S_OK;
}

void chuni_io_slider_start(chuni_io_slider_callback_t callback)
{
    if (chuni_io_slider_thread != NULL) {
        return;
    }

    chuni_io_slider_stop_flag = false;
    chuni_io_slider_thread = (HANDLE)_beginthreadex(
            NULL, 0, chuni_io_slider_thread_proc, (void *)callback, 0, NULL);
}

void chuni_io_slider_stop(void)
{
    if (chuni_io_slider_thread == NULL) {
        return;
    }

    chuni_io_slider_stop_flag = true;

    WaitForSingleObject(chuni_io_slider_thread, INFINITE);
    CloseHandle(chuni_io_slider_thread);
    chuni_io_slider_thread = NULL;
    chuni_io_slider_stop_flag = false;
}

void chuni_io_slider_set_leds(const uint8_t *rgb)
{
    // TODO: no LED-back-to-Pico path yet. Brokenithm's reference sends
    // an "\x63LED..." packet back over the same UDP socket; to mirror
    // that here you'd Serial.write() a distinct LED frame (e.g. prefix
    // byte 0x4C 'L' + 96 bytes RGB) from this function and have the
    // Pico firmware read it non-blockingly between packet sends. Left
    // as a stub so the DLL still loads and runs without it.
    (void)rgb;
}

// -----------------------------------------------------------------------
// Chusan-era LED board API (see chuniio.h for background). Chusan's
// loader requires these to exist even if you don't have LED hardware --
// leaving chuni_io_led_init unexported is what produces:
//   "Custom IO DLL does not provide function 'chuni_io_led_init'."
// and it will similarly complain about chuni_io_led_set_colors if that's
// missing too. Returning S_OK here just tells the game "LED board
// present and ready"; chuni_io_led_set_colors is then called
// periodically with the actual color data, which we currently discard
// (no LED hardware wired to the Pico yet -- same TODO as
// chuni_io_slider_set_leds above).
//
// NOTE ON SIGNATURE: the exact parameter shape of chuni_io_led_set_colors
// is not independently confirmed from source in this session -- only its
// existence and purpose ("Tower LEDs support") are confirmed from the
// segatools changelog. This mirrors the (board, rgb) shape used by the
// slider LED path in the same API generation. If the game logs a
// different missing-export or an access-violation/crash specifically
// when LEDs would update, the signature is the first thing to check
// against the actual chusanhook/chuni-dll.h struct for your build.
// -----------------------------------------------------------------------
HRESULT chuni_io_led_init(void)
{
    chuni_io_log("chuni_io_led_init: reporting LED board ready (no hardware wired yet)");
    return S_OK;
}

void chuni_io_led_set_colors(uint8_t board, uint8_t *rgb)
{
    // TODO: same as chuni_io_slider_set_leds -- no return path to the
    // Pico yet. board distinguishes which LED board is being addressed;
    // rgb is the color buffer (size per chuniio.h -- 198 bytes in the
    // slider-adjacent LED path, unconfirmed for this specific function).
    (void)board;
    (void)rgb;
}

// -----------------------------------------------------------------------
// Slider thread: owns the serial connection lifecycle end-to-end --
// auto-scan for the Pico's COM port, connect, stream frames into the
// game via callback, and auto-reconnect (with a fresh scan, in case the
// Pico enumerated on a different COM port after replug) if the link
// drops.
// -----------------------------------------------------------------------
static unsigned int __stdcall chuni_io_slider_thread_proc(void *ctx)
{
    chuni_io_slider_callback_t callback = (chuni_io_slider_callback_t)ctx;
    HANDLE hSerial = INVALID_HANDLE_VALUE;
    sl_frame_t frame;

    while (!chuni_io_slider_stop_flag) {
        if (hSerial == INVALID_HANDLE_VALUE) {
            int port = sl_find_pico_port();
            if (port < 0) {
                chuni_io_log("waiting for Pico (VID_%04X&PID_%04X)...", PICO_USB_VID, PICO_USB_PID);
                if (chuni_io_state_lock_initialized) {
                    EnterCriticalSection(&chuni_io_state_lock);
                    chuni_io_device_connected = false;
                    LeaveCriticalSection(&chuni_io_state_lock);
                }
                Sleep(1000);
                continue;
            }

            chuni_io_log("found Pico on COM%d, connecting...", port);
            hSerial = sl_open_port(port);
            if (hSerial == INVALID_HANDLE_VALUE) {
                chuni_io_log("failed to open COM%d, retrying...", port);
                Sleep(1000);
                continue;
            }

            chuni_io_log("connected to COM%d", port);
            if (chuni_io_state_lock_initialized) {
                EnterCriticalSection(&chuni_io_state_lock);
                chuni_io_device_connected = true;
                LeaveCriticalSection(&chuni_io_state_lock);
            }
        }

        if (!sl_read_frame(hSerial, &frame)) {
            // Could be a transient bad-checksum frame (harmless, just
            // retry) or a genuine disconnect. Distinguish by checking
            // whether the handle is still valid.
            DWORD errors;
            COMSTAT status;
            if (!ClearCommError(hSerial, &errors, &status)) {
                chuni_io_log("serial link lost, will rescan");
                CloseHandle(hSerial);
                hSerial = INVALID_HANDLE_VALUE;
                if (chuni_io_state_lock_initialized) {
                    EnterCriticalSection(&chuni_io_state_lock);
                    chuni_io_device_connected = false;
                    LeaveCriticalSection(&chuni_io_state_lock);
                }
            }
            continue;
        }

        if (chuni_io_state_lock_initialized) {
            EnterCriticalSection(&chuni_io_state_lock);
            memcpy(chuni_io_latest_air, frame.air, sizeof(chuni_io_latest_air));
            chuni_io_latest_test_btn = frame.test_btn;
            chuni_io_latest_service_btn = frame.service_btn;
            
            if (frame.coin_btn && !chuni_io_coin) {
                chuni_io_coins++;
            }
            chuni_io_coin = frame.coin_btn;
            LeaveCriticalSection(&chuni_io_state_lock);
        }

        uint8_t slider_pressure[32];
        for (int i = 0; i < 32; i++) {
            slider_pressure[i] = (frame.slider_bits[i / 8] & (1 << (i % 8))) ? 255 : 0;
        }

        callback(slider_pressure);
    }

    if (hSerial != INVALID_HANDLE_VALUE) {
        CloseHandle(hSerial);
    }

    return 0;
}