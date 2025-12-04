/* 
 * OV7670 USB Camera for RP2040
 * Based on TinyUSB Video example
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tusb.h"
#include "usb_descriptors.h"

#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "ov7670.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

// Frame buffer for camera
static uint8_t frame_buffer[FRAME_WIDTH * FRAME_HEIGHT * 2];  // YUY2 format (2 bytes per pixel)

// Camera configuration
static struct ov2640_config camera_config = {
    .sccb = i2c0,
    .pin_sioc = 21,           // I2C0 SCL
    .pin_siod = 4,            // I2C0 SDA
    .pin_resetb = 17,         // Camera reset
    .pin_xclk = 3,            // Master clock for camera
    .pin_vsync = 16,          // Vertical sync
    .pin_y2_pio_base = 6,     // D0-D7, PCLK, HREF base pin
    .pio = pio0,
    .pio_sm = 0,
    .dma_channel = 0,
    .image_buf = frame_buffer,
    .image_buf_size = sizeof(frame_buffer)
};

/* Blink pattern:
 * - 250 ms : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);
void video_task(void);

/*------------- MAIN -------------*/
int main(void)
{
    board_init();
    
    // Initialize TinyUSB
    tusb_init();

    stdio_init_all();
    sleep_ms(2000);  // Give time for USB serial to initialize

    printf("\n\nOV7670 USB Camera\n");
    printf("Initializing camera...\n");

    // Initialize the camera
    ov2640_init(&camera_config);
    
    // Read camera ID to verify I2C communication
    uint8_t pid = ov2640_reg_read(&camera_config, 0x0A);  // Product ID
    uint8_t ver = ov2640_reg_read(&camera_config, 0x0B);  // Version
    printf("Camera ID: PID=0x%02X, VER=0x%02X (should be 0x76, 0x73)\n", pid, ver);
    
    printf("Camera initialized successfully!\n");
    printf("Resolution: %dx%d\n", FRAME_WIDTH, FRAME_HEIGHT);
    printf("Frame buffer size: %d bytes\n", sizeof(frame_buffer));
    
    // Fill buffer with test pattern to verify USB streaming works
    for(int i = 0; i < sizeof(frame_buffer); i += 4) {
        frame_buffer[i] = 0x80;      // Y
        frame_buffer[i+1] = 0x80;    // U
        frame_buffer[i+2] = 0x80;    // Y
        frame_buffer[i+3] = 0x80;    // V (gray color)
    }
    printf("Test pattern loaded\n");

    while (1)
    {
        tud_task(); // TinyUSB device task
        led_blinking_task();
        video_task();
    }

    return 0;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
    blink_interval_ms = BLINK_MOUNTED;
    printf("Device mounted\n");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    blink_interval_ms = BLINK_NOT_MOUNTED;
    printf("Device unmounted\n");
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    blink_interval_ms = BLINK_SUSPENDED;
    printf("Device suspended\n");
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    blink_interval_ms = BLINK_MOUNTED;
    printf("Device resumed\n");
}

//--------------------------------------------------------------------+
// USB Video
//--------------------------------------------------------------------+

static unsigned frame_num = 0;
static unsigned tx_busy = 0;
static unsigned interval_ms = 1000 / FRAME_RATE;

void video_task(void)
{
    static unsigned start_ms = 0;
    static unsigned already_sent = 0;

    if (!tud_video_n_streaming(0, 0)) {
        already_sent  = 0;
        frame_num     = 0;
        tx_busy       = 0;
        return;
    }

    if (!already_sent) {
        already_sent = 1;
        tx_busy = 1;
        start_ms = board_millis();
        
        // Capture a frame from the camera
        printf("Capturing frame %u...\n", frame_num);
        ov2640_capture_frame(&camera_config);
        printf("Frame captured (%d bytes)\n", sizeof(frame_buffer));
        
        tud_video_n_frame_xfer(0, 0, (void*)frame_buffer, sizeof(frame_buffer));
        return;
    }

    unsigned cur = board_millis();
    if (cur - start_ms < interval_ms) return; // not enough time
    if (tx_busy) return;
    
    tx_busy = 1;
    start_ms += interval_ms;

    // Capture next frame
    printf("Capturing frame %u...\n", ++frame_num);
    ov2640_capture_frame(&camera_config);
    printf("Frame captured\n");
    
    tud_video_n_frame_xfer(0, 0, (void*)frame_buffer, sizeof(frame_buffer));
}

void tud_video_frame_xfer_complete_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx)
{
    (void)ctl_idx; (void)stm_idx;
    tx_busy = 0;
    /* flip buffer */
    //already_sent = 0;
}

int tud_video_commit_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx,
                        video_probe_and_commit_control_t const *parameters)
{
    (void)ctl_idx; (void)stm_idx;
    /* convert unit to ms from 100 ns */
    interval_ms = parameters->dwFrameInterval / 10000;
    printf("Video commit: interval = %u ms\n", interval_ms);
    return VIDEO_ERROR_NONE;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
    static uint32_t start_ms = 0;
    static bool led_state = false;

    // Blink every interval ms
    if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
    start_ms += blink_interval_ms;

    board_led_write(led_state);
    led_state = 1 - led_state; // toggle
}
