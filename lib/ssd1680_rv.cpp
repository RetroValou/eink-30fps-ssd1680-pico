#include "ssd1680_rv.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include <iostream>
#include <cstring>
#include <chrono>

#include "../var/cmd.h"
#include "../var/config.h"


SSD1680::SSD1680(spi_inst_t &spiInstance
            , int cs, int dc, int rst, int busy, int clk, int mosi
            , int height , int width, int freq) 
{
    spi = &spiInstance;
    cs_pin = cs;
    dc_pin = dc;
    rst_pin = rst;
    busy_pin = busy;

    screen_height = height;
    screen_width = width;

    buffer_size = (size_t)(screen_height*screen_width/8);
    
    lut_value = LUT();

    last_config_screen = 0x00;
    futur_config_screen = 0x00;
    grey_detected = nullptr;
    grey_compensation = false;

    initBufferScreen();

    initSPIScreen(freq, clk, mosi);
    resetScreen(); 

    configScreen();
    temperatureInit();

    cleanRam();
    configureRam();

    sendBufferImg();

    last_config_screen = 0xF7;
    command(CMD_ConfigUpdate, &last_config_screen, 1);
}

        
SSD1680::~SSD1680() {
    delete[] buffer_img_now;
    delete[] buffer_img_before;
    delete[] buffer_img_between;

    delete[] buffer_accumulated_charge;
    delete[] buffer_img_compensate_negative;
    delete[] buffer_img_compensate_positive;

    delete[] grey_detected;
}





/* -------------- Update Available -------------- */

void SSD1680::updateBlack(){
    moveBufferImg(true);
    sendBufferImg();
    command(CMD_LoadLut, lut_value.v_black, lut_value.size);
    uint8_t data_conf[1] { DATA_CompletUpdate };
    command(CMD_ConfigUpdate, data_conf, sizeof(data_conf));
    command(CMD_StartUpdate);
    moveBufferImg(false);
}

void SSD1680::updateClean(){
    moveBufferImg(true);
    sendBufferImg();
    command(CMD_LoadLut, lut_value.v_clean, lut_value.size);
    uint8_t data_conf[1] { DATA_OnlyUpdateScreen };
    command(CMD_ConfigUpdate, data_conf, sizeof(data_conf));
    command(CMD_StartUpdate);
    moveBufferImg(false);
}


void SSD1680::updateDiff(){

    //// Update Img ////
    sendBufferImg();

    command(CMD_LoadLut, lut_value.v_diff, lut_value.size);
    uint8_t data_conf[1] { DATA_OnlyUpdateScreen };
    command(CMD_ConfigUpdate, data_conf, sizeof(data_conf));
    command(CMD_StartUpdate);

    //// compensate screen ////
    updateAccumulateChargeScreen();
    generateCompensationBuffers();
    sendCompensateImg();

    command(CMD_LoadLut, lut_value.v_compensate, lut_value.size);
    command(CMD_ConfigUpdate, data_conf, sizeof(data_conf));
    command(CMD_StartUpdate);

    //// End ////
    moveBufferImg(false);
}


void SSD1680::updateAlreadyLut(){
    sendBufferImg();
    command(CMD_StartUpdate);
    moveBufferImg(false);
}


void SSD1680::endUpdate() { 
    if((last_config_screen & DATA_DisableAnalog) != DATA_DisableAnalog){ // last update has not deactivate analog
        disableAnalog(); 
    } 
}



/* -------------- Initialisation -------------- */

void SSD1680::initBufferScreen()
{
    //// Img Buffer ////
    buffer_img_now = new uint8_t[buffer_size];
    buffer_img_before = new uint8_t[buffer_size];
    buffer_img_between = new uint8_t[buffer_size];

    for (size_t i = 0; i < buffer_size; ++i) {
        buffer_img_now[i] = 0xFF; 
        buffer_img_before[i] = 0xFF; 
        buffer_img_between[i] = 0xFF; 
    }


    //// Compensate Buffer ////
    buffer_img_compensate_negative = new uint8_t[buffer_size];
    buffer_img_compensate_positive = new uint8_t[buffer_size];

    for (size_t i = 0; i < buffer_size; ++i) {
        buffer_img_compensate_positive[i] = 0x00; 
        buffer_img_compensate_negative[i] = 0x00; 
    }

    size_t buffer_size_analyse = (size_t)screen_height*screen_width;
    buffer_accumulated_charge = new int8_t[buffer_size_analyse];
    for (size_t i = 0; i < buffer_size_analyse; ++i) {
        buffer_accumulated_charge[i] = 0; 
    }
}


void SSD1680::initSPIScreen(int freq, int clk, int mosi) {

    gpio_set_function(clk, GPIO_FUNC_SPI);
    gpio_set_function(mosi, GPIO_FUNC_SPI);

    spi_init(spi, freq);
    spi_set_format(spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);  // Configuration SPI : 8 bits, CPOL=0, CPHA=0
    spi_set_slave(spi, false);  

    gpio_init(cs_pin);
    gpio_set_dir(cs_pin, GPIO_OUT);
    gpio_put(cs_pin, 1);  

    gpio_init(dc_pin);
    gpio_set_dir(dc_pin, GPIO_OUT);

    gpio_init(rst_pin);
    gpio_set_dir(rst_pin, GPIO_OUT);

    gpio_init(busy_pin);
    gpio_set_dir(busy_pin, GPIO_IN);
}



void SSD1680::resetScreen(){
    gpio_put(rst_pin, 0);  
    sleep_ms(200);
    gpio_put(rst_pin, 1);  
    sleep_ms(200);
}


void SSD1680::configScreen(){
    uint8_t data_DOC[3] {(MAX_HEIGHT_RAM-1)&0xFF, ((MAX_HEIGHT_RAM-1) >> 8) & 0xFF, 0x01};
    command(CMD_DriverOuputControl, data_DOC, sizeof(data_DOC)); // Driver output control (default 0x270100) (here 0x270101)
    uint8_t data_Wave[1] {0xC0};//{0x80};
    command(CMD_BorderWaveform, data_Wave, sizeof(data_Wave)); // BorderWavefrom (default 0xC0)
    uint8_t data_DUC[2] {0x00, 0x80};
    command(CMD_UpdateRamControl, data_DUC, sizeof(data_DUC)); // Display update control

    uint8_t data_VCom[1] { CONF_VCOM };
    uint8_t data_VGate[1] { CONF_VGATE };
    uint8_t data_VSource[3] { CONF_VSH1, CONF_VSH2, CONF_VSL };
    command(CMD_VoltVCom, data_VCom, sizeof(data_VCom));
    command(CMD_VoltGate, data_VGate, sizeof(data_VGate));
    command(CMD_VoltSource, data_VSource, sizeof(data_VSource));

    uint8_t conf_bss = 0xF0 | (CONF_BOOSTER_DRIVING_STRENGTH<<4) | CONF_BOOSTER_MIN_OFF_TIME;
    uint8_t data_BSS[4] { conf_bss, conf_bss, conf_bss, CONF_BOOSTER_DURATION };
    command(CMD_BoosterSoftStart, data_BSS, sizeof(data_BSS));
    uint8_t data_DEM[1] {0x03};
    command(CMD_DataEntryMode, data_DEM, sizeof(data_DEM)); // Address mode: top to bottom, left to right
    sleep_ms(100);
}


void SSD1680::cleanRam(){
    // Clean RAM before for not have pixel if screen is bigger to config
    // Value of SSD1680 are 296 x 172
    uint8_t xSize[2]{ 0x00, static_cast<uint8_t>(MAX_WIDTH_RAM/8-1)};
    command(CMD_RamXSize, xSize, sizeof(xSize));
    uint8_t ySize[4] {0x00, 0x00 
        , static_cast<uint8_t>(MAX_HEIGHT_RAM & 0xFF)
        , static_cast<uint8_t>((MAX_HEIGHT_RAM >> 8) & 0xFF) 
    };

    command(CMD_RamYSize, ySize, sizeof(ySize));
    uint8_t dataX_start[1] {0x00};
    // Position who we start
    command(CMD_RamXStart, dataX_start, sizeof(dataX_start));
    uint8_t dataY_start[2] {0x00, 0x00};
    command(CMD_RamYStart, dataY_start, sizeof(dataY_start));

    // Clean RAM by send small block of data
    uint8_t chunk[64];
    std::memset(chunk, 0xFF, sizeof(chunk));

    int total_bytes = (MAX_HEIGHT_RAM * MAX_WIDTH_RAM) / 8;

    // Send for RAM RED
    waitScreenReady();
    gpio_put(cs_pin, 0);
    sendCommand(CMD_WriteRamRED);
    for (int i = 0; i < total_bytes; i += sizeof(chunk)) {
        int bytes_to_send = (total_bytes - i < sizeof(chunk)) ? (total_bytes - i) : sizeof(chunk);
        sendData(chunk, bytes_to_send);
    }
    gpio_put(cs_pin, 1);

    // Send for RAM BW
    waitScreenReady();
    gpio_put(cs_pin, 0);
    sendCommand(CMD_WriteRamBW);
    for (int i = 0; i < total_bytes; i += sizeof(chunk)) {
        int bytes_to_send = (total_bytes - i < sizeof(chunk)) ? (total_bytes - i) : sizeof(chunk);
        sendData(chunk, bytes_to_send);
    }
    gpio_put(cs_pin, 1);
}


void SSD1680::configureRam(){

    // Size of ram we use
    uint8_t xSize[2]{ 0x00, static_cast<uint8_t>(screen_width/8-1)};
    command(CMD_RamXSize, xSize, sizeof(xSize));
    int y_decalage = MAX_HEIGHT_RAM - screen_height;
    uint8_t ySize[4] {
        static_cast<uint8_t>(y_decalage & 0xFF), 
        static_cast<uint8_t>((y_decalage >> 8) & 0xFF), 
        static_cast<uint8_t>(MAX_HEIGHT_RAM & 0xFF), 
        static_cast<uint8_t>((MAX_HEIGHT_RAM >> 8) & 0xFF) 
    };
    
    command(CMD_RamYSize, ySize, sizeof(ySize));
    uint8_t dataX_start[1] {0x00};
    // Position who we start
    command(CMD_RamXStart, dataX_start, sizeof(dataX_start));
    uint8_t dataY_start[2] {
        static_cast<uint8_t>(y_decalage & 0xFF), 
        static_cast<uint8_t>((y_decalage >> 8) & 0xFF)
    };
    command(CMD_RamYStart, dataY_start, sizeof(dataY_start));
}



/* -------------- Function Base -------------- */

void SSD1680::waitScreenReady() {
    while (gpio_get(busy_pin)) { tight_loop_contents(); }
}

void SSD1680::sendCommand(uint8_t cmd_){
    gpio_put(dc_pin, 0);  
    spi_write_blocking(spi, &cmd_, 1);
}

void SSD1680::sendData(const uint8_t* data, int data_size){
    gpio_put(dc_pin, 1); 
    spi_write_blocking(spi, data, data_size);
}

void SSD1680::command(uint8_t cmd_, const uint8_t *data, int data_size){
    manageAnalog(&cmd_, data);
    // wait screen finish action (in more case : Update)
    waitScreenReady();
    gpio_put(cs_pin, 0);  
    sendCommand(cmd_);
    if(data_size > 0) { sendData(data, data_size); }
    gpio_put(cs_pin, 1);  
}

        
/* -------------- Buffer images  -------------- */

void SSD1680::writeImg(const uint8_t* img){
    std::memcpy(buffer_img_now, img, buffer_size * sizeof(uint8_t));
    compensationGrey();
    greyUpdate();
}


void SSD1680::moveBufferImg(bool copy_on_new){
    /* Exchange pos buffer */
    uint8_t* tmp_buffer = buffer_img_before;
    buffer_img_before = buffer_img_between;
    buffer_img_between = buffer_img_now;
    buffer_img_now = tmp_buffer; 
    if(copy_on_new){ /* Copy old buffer on new buffer */
        std::memcpy(buffer_img_now, buffer_img_before, buffer_size * sizeof(uint8_t));
        std::memcpy(buffer_img_between, buffer_img_before, buffer_size * sizeof(uint8_t));
    }
}

void SSD1680::sendBufferImg(){
    command(CMD_WriteRamRED, buffer_img_before, buffer_size);
    command(CMD_WriteRamBW, buffer_img_now, buffer_size);
}

void SSD1680::sendCompensateImg(){
    command(CMD_WriteRamRED, buffer_img_compensate_negative, buffer_size);
    command(CMD_WriteRamBW , buffer_img_compensate_positive, buffer_size);
}



/* -------------- ControlScreen  -------------- */

bool SSD1680::manageAnalog(uint8_t *cmd_, const uint8_t *data ){
    /* gestion of analog if command are config screen or update */
    if(CMD_StartUpdate == *cmd_){

        if ( ((last_config_screen & DATA_DisableAnalog) == DATA_DisableAnalog) && // last update has deactivate analog
            ((futur_config_screen & DATA_EnableAnalog) != DATA_EnableAnalog) ) // new update need to activate analog )
        {    
            enableAnalog();
            return true;
        }
        last_config_screen = futur_config_screen;
    }
    else if(CMD_ConfigUpdate == *cmd_ && data != nullptr){
        futur_config_screen = data[0]; // stock futur config for update
    }
    return false;
}

void SSD1680::enableAnalog(){
    /* TODO: Add guard to prevent re-enabling analog power if already on (can be causes screen freeze) */
    // Turn on high-voltage circuitry to allow pixel driving
    // Must stay enabled during rapid & continuous updates to maintain speed
    uint8_t data_temp[1]{ DATA_EnableAnalog };
    command(CMD_ConfigUpdate, data_temp, 1);
    command(CMD_StartUpdate);
}

void SSD1680::disableAnalog(){
    /* TODO: Add guard to prevent re-disabling analog power if already off (causes always screen freeze) */
    // Cut high-voltage power to save energy once display update is done
    uint8_t data_temp[1]{ DATA_DisableAnalog };
    command(CMD_ConfigUpdate, data_temp, 1);
    command(CMD_StartUpdate);
}


void SSD1680::temperatureInit(){
    /* Looks like a controller bug:
        - Sending 0x00 (undocumented value) skyrockets the update speed.
        - Seems to completely disable thermal compensation.
        - Breaks standard updates using the default LUT.
        - ONLY works during boot initialization.
        - If switched back to internal/external sensor, impossible to reproduce without a hard reset.
    */
    uint8_t data_temp[1] {0x00};
    command(CMD_TemperatureSensorControl, data_temp, sizeof(data_temp)); // Configure temperature control to a value not exist
}




/* -------------- CompensateScreen  -------------- */

void SSD1680::updateAccumulateChargeScreen()
{
    int8_t* p_charge = buffer_accumulated_charge;

    for (size_t i = 0; i < buffer_size; ++i) {
        uint8_t now = buffer_img_now[i];
        uint8_t diff = now ^ (buffer_img_before[i]);

        for (int b = 7; b >= 0; --b) {
            uint8_t mask = (1 << b);
            int16_t curr_charge = *p_charge;

            // Natural loss of positive charge (COM is negative in current configuration)
            curr_charge = curr_charge - (curr_charge >> 4);

            // Charge balance based on pixel state
            if(grey_compensation && (grey_detected[i] & mask)){ // grey
                if(curr_charge < LIMIT_GREY_MIN) { curr_charge = LIMIT_GREY_MIN; }
                else if(curr_charge > LIMIT_GREY_MAX) { curr_charge = LIMIT_GREY_MAX; }
                curr_charge -= 1;
            }
            else if (diff & mask) { // Pixel transition
                if (now & mask) { // 0 -> 1 : Move to White (LUT line 2)
                    curr_charge += CHANGE_TO_WHITE;
                } else { // 1 -> 0 : Move to Black (LUT line 3)
                    curr_charge += CHANGE_TO_BLACK;
                }
            } else { // Keep current color
                if (now & mask) { // 1 -> 1 : Keep White (LUT line 4)
                    curr_charge += KEEP_WHITE;
                } else { // 0 -> 0 : Keep Black (LUT line 1)
                    curr_charge += KEEP_BLACK;
                }
            }

            // Clamp values to fit into int8_t [-127, 127]
            if (curr_charge > 127) { curr_charge = 127; }
            if (curr_charge < -127) { curr_charge = -127; }

            *p_charge++ = (int8_t)curr_charge;
        }
    }
}



void SSD1680::generateCompensationBuffers()
{
    int8_t* p_charge = buffer_accumulated_charge;

    for (size_t i = 0; i < buffer_size; ++i) {
        uint8_t byte_neg = 0;
        uint8_t byte_pos = 0;

        for (int b = 7; b >= 0; --b) {
            int16_t curr_charge = *p_charge;

            if (curr_charge >= THRESHOLD_POS) { // Pixel too black (too positiv electricity)
                byte_neg |= (1 << b); // Apply negativ electricity                
                curr_charge += COMP_PULSE_CHARGE_NEG; // add charge of negativ electricity in analyse
            } 
            else if (curr_charge <= THRESHOLD_NEG) { // Pixel too white (too negativ electricity)
                byte_pos |= (1 << b); // Apply positiv electricity                   
                curr_charge += COMP_PULSE_CHARGE_POS; // add charge of positiv electricity in analyse
            }

            // Clamp values to fit into int8_t [-127, 127]
            if (curr_charge > 127) { curr_charge = 127; }
            if (curr_charge < -127) { curr_charge = -127; }

            *p_charge++ = (int8_t)curr_charge;
        }

        buffer_img_compensate_negative[i] = byte_neg;
        buffer_img_compensate_positive[i] = byte_pos;
    }
}




/* -------------- Grey Compensation  -------------- */

void SSD1680::startGreyCompensation(){ 
    if(grey_compensation) { return; }
    grey_compensation = true; 
    grey_detected = new uint8_t[buffer_size];

    greyUpdate(); 
}


void SSD1680::stopGreyCompensation(){ 
    if(!grey_compensation) { return; }
    delete[] grey_detected;
    grey_detected = nullptr;

    grey_compensation = false; 
}



void SSD1680::greyUpdate(){
    // Detection of grey color
    if(grey_compensation){
        for (size_t i = 0; i < buffer_size; ++i) {
            /* grey = need to patern 0-1-0 or 1-0-1 */
            grey_detected[i] = (buffer_img_before[i] ^ buffer_img_between[i]) & (buffer_img_between[i] ^ buffer_img_now[i]);
        }
    }
}

void SSD1680::compensationGrey(){
    // Used for try to compensate change between grey and black img
    // can provoc glich visual on black white image
    if(grey_compensation){
        for (size_t i = 0; i < buffer_size; ++i) {
            /* if not grey -> Not change */
            /* if grey -> add 2 frame of update if update to black, else not change */
            buffer_img_between[i] = buffer_img_between[i] | (grey_detected[i] & ~(buffer_img_now[i]));
        }
    }
}
