

// remove this, placed here to get rid of 
// squigglies under uint
#include <stdint.h>
// 
// 

#ifndef SECRETS_H
#define SECRETS_H
// WIFI CONFIGURATION
#define WIFI_SSID     ""
#define WIFI_PASSWORD ""
// SOCKET CONFIGURATION
const char* HOST_IP =  "";
const uint16_t PORT =  0;
// AUTHENTICATION CONFIGURATION
#define SENSOR_ID   "sensor_001"
#define API_KEY     "key123"
#define LOCATION    "LAPTOP"
// I2S PIN DEFINITIONS
#define I2S_WS_PIN  15
#define I2S_SD_PIN  31
#define I2S_SCK_PIN 14
// I2S CONFIGURATION
#define I2S_PORT    I2S_NUM_0 
#define SAMPLE_RATE 16000
#define DMA_BUF_LEN 512 //Samples per buffer
#endif
//