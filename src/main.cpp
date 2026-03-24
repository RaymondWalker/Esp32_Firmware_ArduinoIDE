#include <Arduino.h>

// for connection
#include <WiFi.h>

// for socket comms
#include <WiFiClient.h>

#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include "config.h"

void sendAuth();

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
  WiFiClient client;

  Serial.println("Connecting to server");

  sendAuth();

}

void sendAuth() {
  WiFiClient client;

  if (client.connect(HOST_IP, PORT)) {
    String authJson = "{";
    authJson += "\"sensor_id\":\"" + String(SENSOR_ID) + "\",";
    authJson += "\"api_key\":\""   + String(API_KEY)   + "\",";
    authJson += "\"location\":\""  + String(LOCATION)  + "\"";
    authJson += "}";

    client.println(authJson);
    Serial.println("Sent: " + authJson);


    unsigned long startMillis = millis();
    bool received = false;


    while (millis() - startMillis < 5000) { 
      if (client.available()) {
        String line = client.readStringUntil('\n');
        Serial.println("Server says: " + line);
        received = true;
        break; 
      }
    }

    if (!received) {
      Serial.println("Error: Response timeout.");
    }

    client.stop(); 
  } else {
    Serial.println("Connection failed.");
  }
}
