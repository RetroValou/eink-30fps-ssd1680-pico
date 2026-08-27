// SPI connector used (spi1 or spi0 on PICO)
#define SPI_PORT    spi1 
#define CS_PIN      13
#define DC_PIN      12
#define RST_PIN     15
#define BUSY_PIN    14
#define MOSI_PIN    11
#define CLK_PIN     10

#define FREQUENCY_SPI 21*1000*1000 // 21MHz -> 20.83MHz (multiple of frequency to RP_PICO)

// Screen Size
#define HEIGHT      296
#define WIDTH       128  // MUST BE A MULTIPLE OF 8! Round up if necessary


#define START_PIN   18



#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "lib/ssd1680_rv.h"
#include "lib/videodecoder.h"
#include "video/video_data.h"





int main() {
    uint8_t image_buffer[HEIGHT * WIDTH / 8];

    stdio_init_all();  
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, true);

    gpio_init(START_PIN);
    gpio_set_dir(START_PIN, GPIO_IN);
    gpio_pull_down(START_PIN);


    SSD1680 screen = SSD1680(*SPI_PORT
                            , CS_PIN, DC_PIN, RST_PIN, BUSY_PIN, CLK_PIN, MOSI_PIN
                            , HEIGHT, WIDTH, FREQUENCY_SPI);

    while (!gpio_get(START_PIN)) { sleep_ms(50); }
    gpio_put(PICO_DEFAULT_LED_PIN, false);


    // Screen clearing phase
    for(int i = 0; i < 5; i++){ screen.updateClean(); }

    // Prepare screen by exercising the ink
    for(int i = 0; i < HEIGHT * WIDTH / 8; i++){ image_buffer[i] = 0x00; }
    screen.writeImg(image_buffer);
    screen.updateDiff();

    for(int i = 0; i < HEIGHT * WIDTH / 8; i++){ image_buffer[i] = 0xFF; }
    screen.writeImg(image_buffer);
    screen.updateDiff();


    VideoDecoder curr_video( video_data, sizeof(video_data), 
            VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FRAMES, VIDEO_FPS );

    if(VIDEO_GREY) { screen.startGreyCompensation(); }
    else { screen.stopGreyCompensation(); }

    uint8_t* frame_ptr = curr_video.get_next_frame();


    // Loop through video frames and update display
    while(true){

        if (gpio_get(START_PIN)) { break; }

        absolute_time_t start = get_absolute_time(); 
        
        frame_ptr = curr_video.get_next_frame();
        if (frame_ptr == nullptr) { break;  }

        screen.writeImg(frame_ptr);
        screen.updateDiff();

        absolute_time_t end = get_absolute_time();
        int64_t duration = absolute_time_diff_us(start, end);
        printf("____ Update : %.6f seconds ____\n", duration / 1000000.0);
        printf("____ FPS    : %.6f ____\n\n", 1000000.0 / duration);        
    }


    // Cleanup display and reset to USB bootloader
    for(int i = 0; i < HEIGHT * WIDTH / 8; i++){ image_buffer[i] = 0xFF; }

    screen.writeImg(image_buffer);
    for(int i = 0; i < 5; i++){ screen.updateClean(); }
    screen.writeImg(image_buffer);
    screen.endUpdate();

    sleep_ms(1000);

    reset_usb_boot(0, 0);
}