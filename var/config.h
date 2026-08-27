#ifndef config
#define config

// RAM size of SSD1680
#define MAX_HEIGHT_RAM  296
#define MAX_WIDTH_RAM   172


// Compensation value
#define THRESHOLD_POS       +17  // Black
#define THRESHOLD_NEG       -22  // White
#define CHANGE_TO_BLACK     +127 
#define CHANGE_TO_WHITE     -127
#define KEEP_BLACK          +1
#define KEEP_WHITE          -1
#define LIMIT_GREY_MIN      -26
#define LIMIT_GREY_MAX      +26
#define COMP_PULSE_CHARGE_POS   +10
#define COMP_PULSE_CHARGE_NEG   -10


// Screen voltage configuration
#define CONF_BOOSTER_DRIVING_STRENGTH   0x07 // Strong
#define CONF_BOOSTER_MIN_OFF_TIME       0x05 // 3.2
#define CONF_BOOSTER_DURATION           0x00 // 10ms per phase

#define CONF_VCOM   0x64    // -2.5v
#define CONF_VGATE  0x15    // 19v
#define CONF_VSH1   0x41    // 15v
#define CONF_VSH2   0x9E    // 4v
#define CONF_VSL    0x28    // -12.5v


#endif
