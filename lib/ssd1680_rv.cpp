#include "ssd1680_rv.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include <iostream>
#include <cstring>
#include <chrono>

#include "../var/cmd.h"
#include "../var/config.h"


// Constructeur
SSD1680::SSD1680(spi_inst_t &spiInstance
            , int cs, int dc, int rst, int busy, int clk, int mosi
            , int height , int width, int freq) 
        {
            spi = &spiInstance; // Stocke le pointeur SPI
            csPin = cs;
            dcPin = dc;
            rstPin = rst;
            busyPin = busy;

            screenHeight = height;
            screenWidth = width;

            bufferSize = (size_t)(screenHeight*screenWidth/8);
            
            lutValue = LUT();


            initBufferScreen();

            initSPIScreen(freq, clk, mosi);
            resetScreen(); 

            configScreen();
            temperature_init();

            cleanRam();
            configureRam();
            
            sendBufferImg();

            last_config_screen = 0xF7;
            command(CMD_ConfigUpdate, &last_config_screen, 1);
        }

// Destructeur
SSD1680::~SSD1680() {
    delete[] bufferImg_now;
    delete[] bufferImg_before;
    delete[] bufferImg_between;
}



void SSD1680::writeImg(const uint8_t* img){
    std::memcpy(bufferImg_now, img, bufferSize * sizeof(uint8_t));
    compensation_grey();
    grey_update();
}



/* -------------- Update Avalaible -------------- */


void SSD1680::updateBlack(){
    moveBufferImg(true);
    sendBufferImg();
    command(CMD_LoadLut, lutValue.v_black, lutValue.size);
    uint8_t data_conf[1] { DATA_CompletUpdate };
    command(CMD_ConfigUpdate, data_conf, sizeof(data_conf));
    command(CMD_StartUpdate);
    moveBufferImg(false);
}

void SSD1680::updateClean(){
    moveBufferImg(true);
    sendBufferImg();
    command(CMD_LoadLut, lutValue.v_clean, lutValue.size);
    uint8_t data_conf[1] { DATA_OnlyUpdateScreen };
    command(CMD_ConfigUpdate, data_conf, sizeof(data_conf));
    command(CMD_StartUpdate);
    moveBufferImg(false);
}


void SSD1680::updateDiff(){

    //// Update Img ////
    sendBufferImg();

    command(CMD_LoadLut, lutValue.v_diff, lutValue.size);
    uint8_t data_conf[1] { DATA_OnlyUpdateScreen };
    command(CMD_ConfigUpdate, data_conf, sizeof(data_conf));
    command(CMD_StartUpdate);

    //// compensate screen ////
    updateAnalyseScreen();
    generateCompensationBuffers();
    sendCompensateImg();

    command(CMD_LoadLut, lutValue.v_compensate, lutValue.size);
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

void SSD1680::updateLutValue(int temperature) {
    lutValue.updateLut(temperature);
}



/* -------------- Initialisation -------------- */

void SSD1680::initBufferScreen()
{
    //// Img Buffer ////
    bufferImg_now = new uint8_t[bufferSize];
    bufferImg_before = new uint8_t[bufferSize];
    bufferImg_between = new uint8_t[bufferSize];

    for (size_t i = 0; i < bufferSize; ++i) {
        bufferImg_now[i] = 0xFF; 
        bufferImg_before[i] = 0xFF; 
        bufferImg_between[i] = 0xFF; 
    }


    //// Compensate Buffer ////
    bufferImg_compensate_negatif = new uint8_t[bufferSize];
    bufferImg_compensate_positif = new uint8_t[bufferSize];

    for (size_t i = 0; i < bufferSize; ++i) {
        bufferImg_compensate_positif[i] = 0x00; 
        bufferImg_compensate_negatif[i] = 0x00; 
    }

    size_t bufferSize_analyse = (size_t)screenHeight*screenWidth;
    analyse_screen_img = new int8_t[bufferSize_analyse];
    for (size_t i = 0; i < bufferSize_analyse; ++i) {
        analyse_screen_img[i] = 0; 
    }
}


void SSD1680::initSPIScreen(int freq, int clk, int mosi) {

    gpio_set_function(clk, GPIO_FUNC_SPI);
    gpio_set_function(mosi, GPIO_FUNC_SPI);

    spi_init(spi, freq);
    spi_set_format(spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);  // Configuration SPI : 8 bits, CPOL=0, CPHA=0
    spi_set_slave(spi, false);  

    gpio_init(csPin);
    gpio_set_dir(csPin, GPIO_OUT);
    gpio_put(csPin, 1);  

    gpio_init(dcPin);
    gpio_set_dir(dcPin, GPIO_OUT);

    gpio_init(rstPin);
    gpio_set_dir(rstPin, GPIO_OUT);

    gpio_init(busyPin);
    gpio_set_dir(busyPin, GPIO_IN);
}



void SSD1680::resetScreen(){
    gpio_put(rstPin, 0);  
    sleep_ms(200);
    gpio_put(rstPin, 1);  
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
    disableAnalog();
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
    gpio_put(csPin, 0);
    sendCommand(CMD_WriteRamRED);
    for (int i = 0; i < total_bytes; i += sizeof(chunk)) {
        int bytes_to_send = (total_bytes - i < sizeof(chunk)) ? (total_bytes - i) : sizeof(chunk);
        sendData(chunk, bytes_to_send);
    }
    gpio_put(csPin, 1);

    // Send for RAM BW
    waitScreenReady();
    gpio_put(csPin, 0);
    sendCommand(CMD_WriteRamBW);
    for (int i = 0; i < total_bytes; i += sizeof(chunk)) {
        int bytes_to_send = (total_bytes - i < sizeof(chunk)) ? (total_bytes - i) : sizeof(chunk);
        sendData(chunk, bytes_to_send);
    }
    gpio_put(csPin, 1);
}


void SSD1680::configureRam(){

    // Size of ram we use
    uint8_t xSize[2]{ 0x00, static_cast<uint8_t>(screenWidth/8-1)};
    command(CMD_RamXSize, xSize, sizeof(xSize));
    int y_decalage = MAX_HEIGHT_RAM - screenHeight;
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
    while (gpio_get(busyPin)) { tight_loop_contents(); }
}


void SSD1680::sendCommand(uint8_t cmd_){ // Envoyer une commande SPI
    gpio_put(dcPin, 0);  
    spi_write_blocking(spi, &cmd_, 1);
}

void SSD1680::sendData(const uint8_t* data, int data_size){ // Envoyer une donnée SPI
    gpio_put(dcPin, 1); 
    spi_write_blocking(spi, data, data_size);
}

void SSD1680::command(uint8_t cmd_, const uint8_t *data, int data_size){
    gestionAnalog(&cmd_, data);
    // wait screen finish action (in more case : Update)
    waitScreenReady();
    gpio_put(csPin, 0);  
    sendCommand(cmd_);
    if(data_size > 0) { sendData(data, data_size); }
    gpio_put(csPin, 1);  
}

        
/* -------------- Buffer images  -------------- */
void SSD1680::moveBufferImg(bool copyOnNew){
    /* Exchange pos buffer */
    uint8_t* tmp_buffer = bufferImg_before;
    bufferImg_before = bufferImg_between;
    bufferImg_between = bufferImg_now;
    bufferImg_now = tmp_buffer; 
    if(copyOnNew){ /* Copy old buffer on new buffer */
        std::memcpy(bufferImg_now, bufferImg_before, bufferSize * sizeof(uint8_t));
        std::memcpy(bufferImg_between, bufferImg_before, bufferSize * sizeof(uint8_t));
    }
}

void SSD1680::sendBufferImg(){
    command(CMD_WriteRamRED, bufferImg_before, bufferSize);
    command(CMD_WriteRamBW, bufferImg_now, bufferSize);
}


void SSD1680::sendCompensateImg(){
    command(CMD_WriteRamRED, bufferImg_compensate_negatif, bufferSize);
    command(CMD_WriteRamBW , bufferImg_compensate_positif, bufferSize);
}



/* -------------- ControlScreen  -------------- */

bool SSD1680::gestionAnalog(uint8_t *cmd_, const uint8_t *data ){
    /* gestion of analog if command are config screen or update */
    if(CMD_StartUpdate == *cmd_){

        if ( ((last_config_screen & DATA_DisableAnalog) == DATA_DisableAnalog) && // last update has desactivate analog
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
    uint8_t data_temp[1]{ DATA_EnableAnalog };
    command(CMD_ConfigUpdate, data_temp, 1);
    command(CMD_StartUpdate);
    printf("###ENABLE ANALOGUE###");
}

void SSD1680::disableAnalog(){
    uint8_t data_temp[1]{ DATA_DisableAnalog };
    command(CMD_ConfigUpdate, data_temp, 1);
    command(CMD_StartUpdate);
    printf("###ENABLE ANALOGUE###");
}

void SSD1680::temperature_init(){
    /* 
        Ressemble a un BUG. 
        Si on change la valeur ou si on remet le capteur interne c'est impossible a reproduire ce "bug" de vitesse
        casse peut etre le moment ou l'écran recherche une LUT ?
    */
    uint8_t data_temp[1] {0x00};
    command(CMD_TemperatureSensorControl, data_temp, sizeof(data_temp)); // Read exterior sensor (no exist, insert fake value for speed)
}



/* -------------- CompensateScreen  -------------- */

void SSD1680::updateAnalyseScreen()
{
    int8_t* pAnalyse = analyse_screen_img;

    for (size_t i = 0; i < bufferSize; ++i) {
        uint8_t now = bufferImg_now[i];
        uint8_t diff = now ^ (bufferImg_before[i]);

        for (int b = 7; b >= 0; --b) {
            uint8_t mask = (1 << b);
            int16_t q = *pAnalyse;

            // 1. Décharge naturelle RC (relaxation ionique)
            q = q - (q >> 4);

            // 2. Bilan de charge selon l'état
            if(grey_compensation && (grey_detected[i] & mask)){ // grey
                if(q < LIMIT_GREY_MIN){ q = LIMIT_GREY_MIN; }
                else if(q > LIMIT_GREY_MAX){ q = LIMIT_GREY_MAX; }
                q -= 1;
            }
            else if (diff & mask) { // Transition
                if (now & mask) { // 0 -> 1 : Passage au BLANC (LUT 1 / VSL / Négatif)
                    q += CHANGE_TO_WHITE; //-= 127; //12;
                } else { // 1 -> 0 : Passage au NOIR (LUT 2 / VSH1 / Positif) 
                    q += CHANGE_TO_BLACK; //+= 127; //12;
                }
            } else { // Maintien d'état
                if (now & mask) { // 1 -> 1 : Resté BLANC (LUT 3)
                    q -= 1;
                } else { // 0 -> 0 : Resté NOIR (LUT 0)
                    q += 1;
                }
            }

            // Saturation dans la plage [-127, 127]
            if (q > 127) q = 127;
            if (q < -127) q = -127;

            *pAnalyse++ = (int8_t)q;
        }
    }
}



void SSD1680::generateCompensationBuffers()
{
    int8_t* pAnalyse = analyse_screen_img;

    // Impulsion de la LUT de compensation (1 tick G0 = 2 unités brutes = 8 en échelle x4)
    const int8_t COMP_PULSE_CHARGE = 8; 

    for (size_t i = 0; i < bufferSize; ++i) {
        uint8_t byteNeg = 0;
        uint8_t bytePos = 0;

        for (int b = 7; b >= 0; --b) {
            int16_t val = *pAnalyse;

            if (val >= THRESHOLD_POS) {
                // Pixel trop NOIR (charge positive résiduelle)
                // -> On applique VSL (tension négative) pour blanchir légèrement
                byteNeg |= (1 << b);
                
                // Soustraction physique de l'impulsion appliquée :
                val += COMP_PULSE_CHARGE_NEG; 
                
                // (Option alternative : val = 0; si ton impulsion annule 100% de la charge)
            } 
            else if (val <= THRESHOLD_NEG) {
                // Pixel trop BLANC (charge négative résiduelle)
                // -> On applique VSH1 (tension positive) pour noircir légèrement
                bytePos |= (1 << b);
                
                // Ajout physique de l'impulsion appliquée :
                val += COMP_PULSE_CHARGE_POS;
            }

            // Saturation de sécurité
            if (val > 127) val = 127;
            if (val < -127) val = -127;

            *pAnalyse++ = (int8_t)val;
        }

        bufferImg_compensate_negatif[i] = byteNeg;
        bufferImg_compensate_positif[i] = bytePos;
    }
}




/* -------------- Grey Compensation  -------------- */

void SSD1680::start_greyCompensation(){ 
    if(grey_compensation) { return; }
    grey_compensation = true; 
    grey_detected = new uint8_t[bufferSize];

    grey_update(); 
}


void SSD1680::stop_greyCompensation(){ 
    if(!grey_compensation) { return; }
    delete[] grey_detected;
    grey_detected = nullptr;

    grey_compensation = false; 
}


// Used for try to compensate change between grey and black img
// can provoc glich visual on black white image

void SSD1680::grey_update(){
    // Detection of grey color
    if(grey_compensation){
        for (size_t i = 0; i < bufferSize; ++i) {
            grey_detected[i] = (bufferImg_before[i] ^ bufferImg_between[i]) & (bufferImg_between[i] ^ bufferImg_now[i]); /* need to patern 0-1-0 or 1-0-1 */
        }
    }
}

void SSD1680::compensation_grey(){
    // application of compensation
    if(grey_compensation){
        for (size_t i = 0; i < bufferSize; ++i) {
            /* if not grey -> Not change */
            /* if grey -> add 2 frame of update if update to black, else not change */
            bufferImg_between[i] = bufferImg_between[i] | (grey_detected[i] & ~(bufferImg_now[i]));
        }
    }
}
