#ifndef SSD1680_RV
#define SSD1680_RV

#include <cstdint>
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "../var/lut_value.h"


class SSD1680 {

    private:
        spi_inst_t *spi;
        int cs_pin;
        int dc_pin;
        int rst_pin;
        int busy_pin;

        int screen_height;
        int screen_width;

        LUT lut_value;

        uint8_t* buffer_img_now;
        uint8_t* buffer_img_before;
        uint8_t* buffer_img_between;
        size_t buffer_size;

        int8_t* buffer_accumulated_charge;
        uint8_t* buffer_img_compensate_negative;
        uint8_t* buffer_img_compensate_positive;


        uint8_t last_config_screen;
        uint8_t futur_config_screen;

        bool grey_compensation = false; /* try to compensate black color change after grey, can make video glitch on black white*/
        uint8_t* grey_detected;

        
    public:
        // Constructeur
        SSD1680(spi_inst_t &spiInstance
                    , int cs, int dc, int rst, int busy, int clk, int mosi
                    , int height , int width
                    , int freq = 21 * 1000 * 1000);

        ~SSD1680(); // Destructeur
        
        void writeImg(const uint8_t* img);


        /* -------------- Update Avalaible -------------- */
 
        void updateBlack();

        void updateClean();
        
        void updateDiff();

        void updateAlreadyLut();

        void endUpdate();


        /* -------------- Grey -------------- */
        void startGreyCompensation();
        void stopGreyCompensation();


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

        void moveBufferImg(bool copy_on_new = true);

        void sendBufferImg();

        void sendCompensateImg();


        /* -------------- ControlScreen  -------------- */

        bool manageAnalog(uint8_t *cmd_, const uint8_t *data );

        void enableAnalog();
        void disableAnalog();

        void temperatureInit();


        /* -------------- CompensateScreen  -------------- */

        void updateAccumulateChargeScreen();
        void generateCompensationBuffers();

        void greyUpdate();
        void compensationGrey();

};


#endif