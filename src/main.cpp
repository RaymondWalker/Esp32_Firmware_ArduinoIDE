#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include "secrets.h"


int status = WL_IDLE_STATUS;

void setup() {
    Serial.begin(115200);
    delay(1000); 

    Serial.print("Connecting to: ");
    Serial.println(WIFI_SSID);

    // Start the connection process
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Poll WiFi.status() until it returns WL_CONNECTED
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}


void loop() {
  // put your main code here, to run repeatedly:

}
