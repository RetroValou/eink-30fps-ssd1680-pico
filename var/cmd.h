#ifndef CMD
#define CMD

/* Config eInk */
#define CMD_DriverOuputControl  0x01
#define CMD_BorderWaveform      0x3C
#define CMD_BoosterSoftStart    0x0C
#define CMD_VoltVCom            0x2C
#define CMD_VoltGate            0x03
#define CMD_VoltSource          0x04

/* Config memory */
#define CMD_DataEntryMode       0x11
#define CMD_UpdateRamControl    0x21

/* Write Ram */
#define CMD_WriteRamBW          0x24
#define CMD_WriteRamRED         0x26
#define CMD_RamXSize            0x44
#define CMD_RamYSize            0x45
#define CMD_RamXStart           0x4E
#define CMD_RamYStart           0x4F

/* Screen action */
#define CMD_LoadLut             0x32
#define CMD_ConfigUpdate        0x22
#define CMD_StartUpdate         0x20


/* Other */
#define CMD_TemperatureSensorControl   0x18


#define DATA_DisableAnalog      0x03
#define DATA_EnableAnalog       0xC0

#define DATA_OnlyUpdateScreen   0x04
#define DATA_CompletUpdate      0xC7


#endif