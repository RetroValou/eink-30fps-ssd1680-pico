#ifndef SSD1680_RV
#define SSD1680_RV

#include <cstdint>
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "../var/lut_value.h"


class SSD1680 {

    private:
        spi_inst_t *spi;
        dma_channel_config dma;
        int dma_channel;
        int csPin;
        int dcPin;
        int rstPin;
        int busyPin;

        int screenHeight;
        int screenWidth;

        LUT lutValue;

        uint8_t* bufferImg_now;
        uint8_t* bufferImg_before;
        uint8_t* bufferImg_between;
        size_t bufferSize;

        int8_t* analyse_screen_img;
        uint8_t* bufferImg_compensate_negatif;
        uint8_t* bufferImg_compensate_positif;


        uint8_t last_config_screen;
        uint8_t futur_config_screen;

        bool grey_compensation = false; /* try to compensate black color change after grey, can make video glitch on black white*/
        uint8_t* grey_detected;

        
    public:
        // Constructeur
        SSD1680(spi_inst_t &spiInstance
                    , int cs, int dc, int rst, int busy, int clk, int mosi
                    , int height , int width
                    , int freq = 20 * 1000 * 1000);

        ~SSD1680(); // Destructeur
        
        void writeImg(const uint8_t* img);


        /* -------------- Update Avalaible -------------- */
 
        void updateBlack();

        void updateClean();
        
        void updateDiff();

        void updateAlreadyLut();

        void updateLutValue(int temperature);

        void end_update() { disableAnalog(); }


        /* -------------- Grey -------------- */
        void start_greyCompensation();
        void stop_greyCompensation();


    private:

        /* -------------- Initialisation -------------- */

        void initBufferScreen();

        void initSPIScreen(int freq, int clk, int mosi);

        void resetScreen();

        void configScreen();

        void cleanRam();

        void configureRam();


        /* -------------- Function Base -------------- */

        void waitScreenReady();
        
        void sendCommand(uint8_t cmd_);

        void sendData(const uint8_t* data, int data_size);

        void command(uint8_t cmd_, const uint8_t *data = nullptr, int data_size = 0);

        
        /* -------------- Buffer images  -------------- */

        void moveBufferImg(bool copyOnNew = true);

        void sendBufferImg();

        void sendCompensateImg();


        /* -------------- ControlScreen  -------------- */

        bool gestionAnalog(uint8_t *cmd_, const uint8_t *data );

        void enableAnalog();
        void disableAnalog();

        void temperature_init();


        /* -------------- CompensateScreen  -------------- */

        void updateAnalyseScreen();
        void generateCompensationBuffers();

        void grey_update();
        void compensation_grey();

};


#endif