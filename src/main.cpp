#include <Arduino.h>                                                                                                                            
#include <WiFi.h>                                                                                                                               
#include <driver/i2s.h>                                                                                                                         
#include <HTTPClient.h>                                                                                                                         
#include "config.h"                                                                                                                             
                                                                                                                                                
WiFiClient client;                                                                                                                              
                                                          
static volatile uint32_t g_bytes_since_hb = 0;                                                                                                  
static SemaphoreHandle_t g_bytes_mutex    = nullptr;
                                                                                                                                                
// ─── Battery ──────────────────────────────────────────────────────────────                                                                   
// Returns battery percentage. Replace the body with ADC logic when ready.
float get_battery_percent() {                                                                                                                   
    return 10.0f;                                                                                                                              
}

// example implementation using ADC (uncomment and adjust as needed):
//  float get_battery_percent() {                                                                                                                   
//       // Example: 4.2V max, 3.0V min, voltage divider halves the voltage
//       const float V_MAX = 4.2f, V_MIN = 3.0f;                                                                                                     
//       int raw = analogRead(BATTERY_ADC_PIN);          // 0–4095
//       float voltage = (raw / 4095.0f) * 3.3f * 2.0f; // ×2 for divider
//       float pct = (voltage - V_MIN) / (V_MAX - V_MIN) * 100.0f;                                                                                   
//       return constrain(pct, 0.0f, 100.0f);    
//   }    

                                                                                                                                                
// ─── I2S setup ────────────────────────────────────────────────────────────
void setup_i2s() {                                                                                                                              
    const i2s_config_t i2s_config = {                     
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),                                                                    
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,                                                                                      
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,                                                                                      
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,                                                                                           
        .dma_buf_count        = 8,                                                                                                              
        .dma_buf_len          = DMA_BUF_LEN,                                                                                                    
        .use_apll             = false,                                                                                                          
    };                                                                                                                                          
    const i2s_pin_config_t pin_config = {                                                                                                       
        .bck_io_num   = I2S_SCK_PIN,                                                                                                            
        .ws_io_num    = I2S_WS_PIN,                                                                                                             
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = I2S_SD_PIN,                                                                                                             
    };                                                                                                                                          
    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);                                                                                                         
}                                                                                                                                               
  
// ─── Heartbeat task (Core 0) ──────────────────────────────────────────────                                                                   
void heartbeat_task(void*) {                              
    TickType_t last_wake = xTaskGetTickCount();                                                                                                 
                                                                                                                                                
    while (true) {                          
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));                                                                      
                                                          
        if (WiFi.status() != WL_CONNECTED) continue;                                                                                            
                                        
        uint32_t bytes_snap = 0;                                                                                                                
        xSemaphoreTake(g_bytes_mutex, portMAX_DELAY);                                                                                           
        bytes_snap       = g_bytes_since_hb;
        g_bytes_since_hb = 0;                                                                                                                   
        xSemaphoreGive(g_bytes_mutex);                    
                                                                                                                                                
        float bandwidth_kbps = (bytes_snap * 8.0f) / HEARTBEAT_INTERVAL_MS;
        float rssi           = (float)WiFi.RSSI();                                                                                              
        float battery        = get_battery_percent();     
                                                                                                                                                
        String url = String("http://") + HOST_IP + ":" + HTTP_PORT
                    + "/api/device-metrics/heartbeat";                                                                                           
                                                                                                                                                
        HTTPClient http;                                                                                                                        
        http.begin(url);                                                                                                                        
        http.addHeader("Content-Type", "application/json");                                                                                     
        http.addHeader("X-API-Key",    API_KEY);                                                                                                
        http.addHeader("X-Sensor-ID",  SENSOR_ID);
        http.addHeader("X-Location",   LOCATION);                                                                                               
        http.setTimeout(5000);                            
                                                                                                                                                
        String body = String("{")                                                                                                               
            + "\"battery_percent\":"     + String(battery, 1)                                                                                   
            + ",\"signal_strength_dbm\":" + String(rssi, 1)                                                                                     
            + ",\"bandwidth_kbps\":"      + String(bandwidth_kbps, 2)                                                                           
            + ",\"firmware_version\":\""   + FIRMWARE_VERSION + "\""
            + "}";                                                                                                                              
                                                          
        int code = http.POST(body);                                                                                                             
        if (code > 0) {                                   
            Serial.printf("[HB] OK HTTP %d | Bat %.0f%% | RSSI %.0f dBm | BW %.1f kbps\n",                                                      
                          code, battery, rssi, bandwidth_kbps);                                                                                 
        } else {                        
            Serial.printf("[HB] Failed: %s\n", http.errorToString(code).c_str());                                                               
        }                                                                                                                                       
        http.end();                     
    }                                                                                                                                           
}                                                                                                                                               
  
// ─── WiFi + TCP connection helpers ────────────────────────────────────────                                                                   
void connect_wifi() {                                     
    Serial.printf("\nConnecting to WiFi: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);                                                                                                       
    while (WiFi.status() != WL_CONNECTED) { 
        delay(500);                                                                                                                             
        Serial.print(".");                                
    }                                                                                                                                           
    Serial.printf("\nWiFi Connected! IP: %s  RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());                                                                              
}                                                         
                                                                                                                                                
void connect_tcp() {
    Serial.printf("Connecting to backend %s:%d\n", HOST_IP, PORT);                                                                              
    if (!client.connect(HOST_IP, PORT)) {                 
        Serial.println("TCP connection failed. Restarting...");                                                                                 
        delay(2000);                                      
        ESP.restart();                      
    }                                                                                                                                           
  
    delay(200);                                                                                                                                 
    String handshake = String("{\"sensor_id\":\"") + SENSOR_ID
                      + "\",\"api_key\":\""  + API_KEY                                                                                           
                      + "\",\"location\":\"" + LOCATION + "\"}";
    client.println(handshake);          
    Serial.println("Handshake sent: " + handshake);                                                                                             
                                                                                                                                                
    bool auth_ok = false;                                                                                                                       
    unsigned long t = millis();                                                                                                                 
    while (millis() - t < 5000) {                                                                                                               
        if (client.available()) {           
            String resp = client.readStringUntil('\n');                                                                                         
            Serial.println("Server: " + resp);            
            if (resp.indexOf("authenticated") != -1) { auth_ok = true; break; }                                                                 
        }                                   
    }                                                                                                                                           
    if (!auth_ok) {                                                                                                                             
        Serial.println("Auth failed. Restarting...");                                                                                           
        delay(1000);                                                                                                                            
        ESP.restart();                                                                                                                          
    }                                                     
    Serial.println("Authenticated!");                                                                                                           
}
                                                                                                                                                
// ─── Setup ────────────────────────────────────────────────────────────────
void setup() {                          
    Serial.begin(115200);
    connect_wifi();                                                                                                                             
    connect_tcp();
    setup_i2s();                                                                                                                                
                                                          
    g_bytes_mutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(heartbeat_task, "heartbeat", 4096, nullptr, 1, nullptr, 0);
}
                                                                                                                                                
// ─── Loop (Core 1) — audio streaming ──────────────────────────────────────
void loop() {                                                                                                                                   
    if (!client.connected()) {                            
        Serial.println("Disconnected. Restarting...");                                                                                          
        delay(1000);
        ESP.restart();                                                                                                                          
    }                                                     

    int32_t raw32[DMA_BUF_LEN];                                                                                                                 
    size_t bytes_read = 0;
    esp_err_t err = i2s_read(I2S_PORT, raw32, sizeof(raw32), &bytes_read, portMAX_DELAY);                                                       
    if (err != ESP_OK || bytes_read == 0) return;         
                                        
    size_t n_samples = bytes_read / 4;      
    int16_t pcm16[DMA_BUF_LEN];                                                                                                                 
    for (size_t i = 0; i < n_samples; i++) {
        pcm16[i] = (int16_t)(raw32[i] >> 16);                                                                                                   
    }                                                     
    size_t pcm_bytes = n_samples * 2;                                                                                                           
                                                          
    client.write((const uint8_t*)pcm16, pcm_bytes);                                                                                             
                                        
    xSemaphoreTake(g_bytes_mutex, portMAX_DELAY);                                                                                               
    g_bytes_since_hb += pcm_bytes;                                                                                                              
    xSemaphoreGive(g_bytes_mutex);
                                                                                                                                                
    static unsigned long lastPrint = 0;                   
    if (millis() - lastPrint > 2000) {
        Serial.printf("Streaming... %u bytes/chunk (16-bit PCM)\n", pcm_bytes);                                                                 
        lastPrint = millis();               
    }                                                                                                                                           
}