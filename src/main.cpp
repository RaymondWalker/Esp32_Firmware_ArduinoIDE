#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include "config.h" 

WiFiClient client;

void setup_i2s() {
  // Configuration for the SPH0645 (SEL pin high -> Left Channel)
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // SPH0645 requires 32-bit slots
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,  // Aligns with your SEL pin pulled high
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = false
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = -1, 
    .data_in_num = I2S_SD_PIN
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
}

void setup() {
  Serial.begin(115200);
  
  // 1. WiFi Connection
  Serial.print("\nConnecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // 2. TCP Connection to Backend
  Serial.print("Connecting to Backend: ");
  Serial.print(HOST_IP);
  Serial.print(":");
  Serial.println(PORT);
  
  if (client.connect(HOST_IP, PORT)) {
    Serial.println("Connected to TCP Server. Sending Handshake...");
    delay(200); 
    
    // Format the handshake
    String handshakeData = "{\"api_key\": \"" + String(API_KEY) + "\", \"sensor_id\": \"" + String(SENSOR_ID) + "\", \"location\": \"" + String(LOCATION) + "\"}";
    
    client.println(handshakeData); 
    Serial.println("Sent Handshake: " + handshakeData);
    
    // Wait for server response
    long timeout = millis();
    bool auth_success = false;
    
    while (millis() - timeout < 5000) {
      if (client.available()) {
        String response = client.readStringUntil('\n'); 
        Serial.println("Server Response: " + response);
        
        // Checking for the exact word your server uses
        if (response.indexOf("authenticated") != -1) {
           auth_success = true;
           break;
        }
      }
    }
    
    if (auth_success) {
       Serial.println("Authentication and Registration Successful!");
    } else {
       Serial.println("Auth Timeout or Failed! Restarting...");
       delay(1000);
       ESP.restart();
    }
  } else {
    Serial.println("TCP Connection Failed! Restarting...");
    delay(2000);
    ESP.restart();
  }

  // 3. Start I2S
  setup_i2s();
}

void loop() {
  if (!client.connected()) {
    Serial.println("Server Disconnected. Restarting...");
    delay(1000);
    ESP.restart();
  }

  // 4. Read I2S and Stream over TCP
  size_t bytes_read;
  uint32_t buffer[DMA_BUF_LEN]; 
  esp_err_t result = i2s_read(I2S_PORT, &buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);

  if (result == ESP_OK && bytes_read > 0) {
    client.write((const uint8_t *)buffer, bytes_read);
    
    // --- DEBUG PRINT ---
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 2000) { // Print every 2 seconds
      Serial.print("Streaming active... Bytes sent in last chunk: ");
      Serial.println(bytes_read);
      lastPrint = millis();
    }
  }
}