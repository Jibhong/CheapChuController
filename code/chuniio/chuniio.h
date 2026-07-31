#ifndef CHUNIIO_H_INCLUDED
#define CHUNIIO_H_INCLUDED

#include <windows.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*chuni_io_slider_callback_t)(const uint8_t *pressure);

__declspec(dllexport) uint16_t chuni_io_get_api_version(void);

__declspec(dllexport) HRESULT chuni_io_jvs_init(void);
__declspec(dllexport) void chuni_io_jvs_read_coin_counter(uint16_t *out);
__declspec(dllexport) void chuni_io_jvs_poll(uint8_t *opbtn, uint8_t *beams);

__declspec(dllexport) HRESULT chuni_io_slider_init(void);
__declspec(dllexport) void chuni_io_slider_start(chuni_io_slider_callback_t callback);
__declspec(dllexport) void chuni_io_slider_stop(void);
__declspec(dllexport) void chuni_io_slider_set_leds(const uint8_t *rgb);

// -----------------------------------------------------------------------
// Chusan-era LED board API. Confirmed from segatools changelog (API
// bumped to 0x102): "Added chuni_io_led_init and chuni_io_led_set_colors
// (Tower LEDs support)". Without chuni_io_led_init exported, the game
// refuses to start with:
//   "Custom IO DLL does not provide function 'chuni_io_led_init'."
// and once that's added, it then requires chuni_io_led_set_colors too.
// -----------------------------------------------------------------------
__declspec(dllexport) HRESULT chuni_io_led_init(void);
__declspec(dllexport) void chuni_io_led_set_colors(uint8_t board, uint8_t *rgb);

#ifdef __cplusplus
}
#endif

#endif // CHUNIIO_H_INCLUDED