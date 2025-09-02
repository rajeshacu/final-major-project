#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_BMP085.h>
#include <TinyGPS++.h>

// LoRa Pins
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 2

// Button
#define BUTTON_PIN 27

// GPS Pins
#define RXD2 16
#define TXD2 17
#define GPS_BAUD 9600

// Battery monitoring configuration (improved from reference code)
const int BATTERY_PIN = 34;      // ADC1 pin
const float VREF = 3.3;
const float R1 = 150000.0;
const float R2 = 100000.0;
const float SCALE = (R1 + R2) / R2;     // 2.5

// Calibration (adjust based on your actual measurements)
const float CAL_SCALE  = 1.0097f;       // gain correction ~+0.97%
const float CAL_OFFSET = 0.00f;         // offset correction

// Sensors
Adafruit_BMP085 bmp;
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

// Battery percentage lookup table
float pctFromVoltage(float v) {
  if (v <= 6.00) return 0;
  if (v >= 8.40) return 100;
  
  struct P { float v, p; };
  static const P T[] = {
    {8.40,100},{8.20,90},{8.00,80},{7.80,70},{7.60,60},{7.40,50},
    {7.20,40},{7.00,30},{6.80,20},{6.60,10},{6.40,5},{6.00,0}
  };
  
  for (int i = 0; i < (int)(sizeof(T)/sizeof(T[0]))-1; ++i) {
    if (v >= T[i+1].v) {
      float t = (v - T[i+1].v) / (T[i].v - T[i+1].v);
      return T[i+1].p + t * (T[i].p - T[i+1].p);
    }
  }
  return 0;
}

// Stable ADC reading with averaging
uint16_t readAdcRaw(int samples = 32) {
  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db);  // ~0–3.5V
  
  uint32_t acc = 0;
  for (int i = 0; i < samples; ++i) { 
    acc += analogRead(BATTERY_PIN); 
    delayMicroseconds(200); 
  }
  return (uint16_t)(acc / samples);
}

// Battery estimation using improved method
int getBatteryPercentage() {
  uint16_t adcRaw = readAdcRaw(32);                 // average for stability
  float vAdc  = (adcRaw / 4095.0f) * VREF;         // pin voltage
  float vBatU = vAdc * SCALE;                      // uncalibrated pack volts
  float vBat  = vBatU * CAL_SCALE + CAL_OFFSET;    // calibrated pack volts
  float pct   = pctFromVoltage(vBat);
  
  if (pct < 0) pct = 0; 
  if (pct > 100) pct = 100;
  
  return (int)pct;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  
  if (!bmp.begin()) {
    Serial.println("Could not find BMP180 sensor!");
    while (1);
  }
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  Serial.println("P2 Node Ready");
  delay(2500); // Offset send by 2.5 seconds to avoid collision
}

void loop() {
  while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());
  float temperature = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0;
  float altitude = bmp.readAltitude();
  int alertFlag = (digitalRead(BUTTON_PIN) == LOW) ? 1 : 0;
  int battery = getBatteryPercentage();
  String jsonData = "{";
  jsonData += "\"id\":\"P2\",";
  jsonData += "\"temperature\":" + String(temperature, 1) + ",";
  jsonData += "\"pressure\":" + String(pressure, 1) + ",";
  jsonData += "\"altitude\":" + String(altitude, 1) + ",";
  if (gps.location.isValid()) {
    jsonData += "\"latitude\":" + String(gps.location.lat(), 6) + ",";
    jsonData += "\"longitude\":" + String(gps.location.lng(), 6) + ",";
  } else {
    jsonData += "\"latitude\":0.000000,";
    jsonData += "\"longitude\":0.000000,";
  }
  jsonData += "\"battery\":" + String(battery) + ",";
  jsonData += "\"alert\":" + String(alertFlag);
  jsonData += "}";
  LoRa.beginPacket();
  LoRa.print(jsonData);
  LoRa.endPacket();
  Serial.println("Sent P2: " + jsonData);
  delay(5000); // Send every 5 sec
}