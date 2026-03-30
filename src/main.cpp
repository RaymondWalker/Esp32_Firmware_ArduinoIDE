#include <Arduino.h>

// for connection
#include <WiFi.h>

// // for socket comms
// #include <WiFiClient.h>


#include <WebSocketsClient.h>
#include <driver/i2s.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include "config.h"



// Global WebSocket instance
WebSocketsClient webSocket;

// Flag to track if we are connected to the socket server
bool isConnected = false;

// --- WebSocket Event Handler ---
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WSc] Disconnected from server!");
      isConnected = false;
      break;
      
    case WStype_CONNECTED: {
      Serial.printf("[WSc] Connected to url: %s\n", payload);
      isConnected = true;

      // The moment we connect, send the JSON authentication payload
      String authJson = "{";
      authJson += "\"sensor_id\":\"" + String(SENSOR_ID) + "\",";
      authJson += "\"api_key\":\""   + String(API_KEY)   + "\",";
      authJson += "\"location\":\""  + String(LOCATION)  + "\"";
      authJson += "}";

      // Send as a text frame
      webSocket.sendTXT(authJson);
      Serial.println("Sent Auth: " + authJson);
      break;
    }
    
    case WStype_TEXT:
      // This triggers when the server sends a text message back
      Serial.printf("[WSc] Server says: %s\n", payload);
      // If your server sends an "Auth Success" or "Auth Failed" message, 
      // you would parse it here.
      break;
      
    case WStype_BIN:
    case WStype_PING:
    case WStype_PONG:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }
}

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD_PIN
  };
  
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);
}

void setup() {
  Serial.begin(115200);
  delay(1000); 

  // 1. Connect to WiFi
  Serial.print("Connecting to: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // 2. Setup I2S Microphone
  setupI2S();

  // 3. Setup WebSocket
  Serial.println("Connecting to WebSocket server...");
  // Connect to the host. The "/" is the URI path.
  webSocket.begin(HOST_IP, PORT, "/"); 
  
  // Assign our event handler function
  webSocket.onEvent(webSocketEvent);
  
  // If the connection drops, try to reconnect every 5 seconds
  webSocket.setReconnectInterval(5000); 
}

void loop() {
  // Keep the WebSocket client running and checking for events
  webSocket.loop();

  // Only read I2S and send data if the socket is actually open
  if (isConnected) {
    int32_t raw_samples[DMA_BUF_LEN];
    size_t bytes_read = 0;

    // Read data from I2S
    esp_err_t result = i2s_read(I2S_PORT, &raw_samples, sizeof(raw_samples), &bytes_read, portMAX_DELAY);

    if (result == ESP_OK && bytes_read > 0) {
      // Blast the raw byte buffer directly to the server as a binary frame
      webSocket.sendBIN((uint8_t*)raw_samples, bytes_read);
    }
  }
}
//