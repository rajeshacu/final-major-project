#include <SPI.h>
#include <SD.h>
#include <LoRa.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

// --- LoRa (VSPI) ---
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_CS    5
#define LORA_RST   14
#define LORA_DIO0  2

// --- SD Card (HSPI) ---
#define SD_CS    13
#define SD_SCK   25
#define SD_MISO  26
#define SD_MOSI  27

// --- Buzzer ---
#define BUZZER_PIN 21  // Piezo buzzer + -> GPIO21, - -> GND

SPIClass hspi(HSPI);
AsyncWebServer server(80);

bool alertActive = false;   // Track if alert is ongoing
unsigned long lastBuzzTime = 0;

String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  if (filename.endsWith(".css"))  return "text/css";
  if (filename.endsWith(".js"))   return "application/javascript";
  if (filename.endsWith(".json")) return "application/json";
  if (filename.endsWith(".txt"))  return "text/plain";
  return "application/octet-stream";
}

void startBuzzer() {
  tone(BUZZER_PIN, 1000);  // 1 kHz tone
}

void stopBuzzer() {
  noTone(BUZZER_PIN);
}

void setup() {
  Serial.begin(115200);

  // Buzzer setup
  pinMode(BUZZER_PIN, OUTPUT);
  stopBuzzer();

  // LoRa init
  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, LOW); delay(10);
  digitalWrite(LORA_RST, HIGH); delay(100);
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI);
  LoRa.setSPIFrequency(1E6);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed!");
    while (1);
  }
  Serial.println("LoRa OK");

  // SD init
  hspi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, hspi)) {
    Serial.println("SD init failed!");
    while (1);
  }
  Serial.println("SD OK");

  // Wi-Fi AP
  WiFi.softAP("LoRa_UI", "12345678");
  Serial.println(WiFi.softAPIP());

  // Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SD, "/index.html", "text/html");
  });

  server.on("/latest", HTTP_GET, [](AsyncWebServerRequest *request){
    if (SD.exists("/latest.txt")) {
      request->send(SD, "/latest.txt", "application/json");
    } else {
      request->send(404, "application/json", "{}");
    }
  });

  server.onNotFound([](AsyncWebServerRequest *request){
    String path = request->url();
    if (SD.exists(path)) {
      request->send(SD, path, getContentType(path));
    } else {
      request->send(404, "text/plain", "File Not Found");
    }
  });

  server.begin();
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    String data = "";
    while (LoRa.available()) {
      data += (char)LoRa.read();
    }
    Serial.println("Received: " + data);

    // --- Alert detection for "alert":1 or "alert":0 ---
    if (data.indexOf("\"alert\":1") != -1) {
      if (!alertActive) {
        Serial.println("🚨 ALERT detected! Starting alarm...");
        alertActive = true;
        startBuzzer();
      }
    } else if (data.indexOf("\"alert\":0") != -1) {
      if (alertActive) {
        Serial.println("✅ Alert cleared. Stopping alarm...");
        alertActive = false;
        stopBuzzer();
      }
    }

    // Append to log
    File logFile = SD.open("/data.txt", FILE_APPEND);
    if (logFile) {
      logFile.println(data);
      logFile.close();
    }

    // Overwrite latest
    File latestFile = SD.open("/latest.txt", FILE_WRITE);
    if (latestFile) {
      latestFile.print(data);
      latestFile.close();
    }
  }

  // Keep buzzer ON while alert is active
  if (alertActive) {
    if (millis() - lastBuzzTime > 1000) {  // refresh every 1 sec
      startBuzzer();
      lastBuzzTime = millis();
    }
  }
}
