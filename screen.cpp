#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "lib/ssd1680_rv.h"
#include "var/config.h"
#include "var/cmd.h"

#include "lib/videodecoder.h"
#include "video/video_data.h"


// Symboles exportés par le fichier de linkage (.ld) du Pico SDK
extern "C" char __bss_end__;
extern "C" char __StackLimit;

// Fonction de mesure de mémoire pour le RP2040
void print_memory_info() {
    struct mallinfo m = mallinfo();
    char *heap_end = (char*)sbrk(0);
    
    uint32_t heap_used = m.uordblks; // Mémoire allouée via new / malloc
    uint32_t free_ram  = (uint32_t)(&__StackLimit - heap_end); // RAM encore disponible entre Heap et Stack

    printf("[MEM] Heap utilisé : %u octets | RAM disponible : %u octets\n", heap_used, free_ram);
}



uint8_t imageBuffer[HEIGHT * WIDTH / 8];

int startPin = 18;
int upPin = 16;


int main() {

    stdio_init_all();  
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, true);

    gpio_init(startPin);
    gpio_set_dir(startPin, GPIO_IN);
    gpio_pull_down(startPin);

    gpio_init(upPin);
    gpio_set_dir(upPin, GPIO_IN);
    gpio_pull_down(upPin);

    SSD1680 screen = SSD1680(*SPI_PORT
                            , CS_PIN, DC_PIN, RST_PIN, BUSY_PIN, CLK_PIN, MOSI_PIN
                            , HEIGHT, WIDTH, FREQUENCY_SPI);

    print_memory_info();

    int index_temp = 3;
    while (!gpio_get(startPin)) { 
        if(gpio_get(upPin)){
            index_temp = (index_temp + 1) % 4;
            for(int i = 0; i < index_temp + 1; i++){
                gpio_put(PICO_DEFAULT_LED_PIN, false);
                sleep_ms(200); 
                gpio_put(PICO_DEFAULT_LED_PIN, true);
                sleep_ms(200); 
            }
        }
        sleep_ms(1); 
    }


    if(index_temp == 0){ screen.updateLutValue(0); }
    else if(index_temp == 1){ screen.updateLutValue(25); }
    else if(index_temp == 2){ screen.updateLutValue(33); }
    else if(index_temp == 3){ screen.updateLutValue(40); }

    gpio_put(PICO_DEFAULT_LED_PIN, false);


    // Cleaning
    for(int i = 0; i < 3; i++){ screen.updateClean(); }

    // Preaper screen by moving ink a small
    for(int i = 0; i < HEIGHT * WIDTH / 8; i++){ imageBuffer[i] = 0x00; }
    screen.writeImg(imageBuffer);
    screen.updateDiff();

    for(int i = 0; i < HEIGHT * WIDTH / 8; i++){ imageBuffer[i] = 0xFF; }
    screen.writeImg(imageBuffer);
    screen.updateDiff();


    VideoDecoder curr_video( video_data, sizeof(video_data), 
            VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FRAMES, VIDEO_FPS );

    if(VIDEO_GREY) { screen.start_greyCompensation(); }
    else { screen.stop_greyCompensation(); }

    uint8_t* frame_ptr = curr_video.get_next_frame();


    // Loop to video and update screen
    while(true){

        if (gpio_get(startPin)) { break; }

        absolute_time_t start = get_absolute_time(); 
        
        frame_ptr = curr_video.get_next_frame();
        if (frame_ptr == nullptr) { break;  }

        screen.writeImg(frame_ptr);
        screen.updateDiff();

        print_memory_info();
        absolute_time_t end = get_absolute_time();
        int64_t duration = absolute_time_diff_us(start, end);
        printf("____ Update : %.6f seconds ____\n", duration / 1000000.0);
        printf("____ FPS    : %.6f ____\n\n", 1000000.0 / duration);        
    }


    // End -> Cleaning and reset usb
    for(int i = 0; i < HEIGHT * WIDTH / 8; i++){ imageBuffer[i] = 0xFF; }

    screen.writeImg(imageBuffer);
    for(int i = 0; i < 3; i++){ screen.updateClean(); }
    screen.writeImg(imageBuffer);
    screen.end_update();

    sleep_ms(1000);

    reset_usb_boot(0, 0);
}