/*
 * WEATHER STATION SLAVE - DHT11 via HC-05 Bluetooth
 * Compatible with AMAN OS Weather App
 * 
 * Hardware:
 * - Arduino Uno (Slave)
 * - HC-05 Bluetooth Module (configured as SLAVE)
 * - DHT11 Temperature & Humidity Sensor
 * - Soil Moisture Sensor (optional - if not present, sends 50%)
 * 
 * Connections:
 * DHT11:
 *   VCC → 5V
 *   DATA → Pin 2
 *   GND → GND
 * 
 * Soil Moisture (optional):
 *   VCC → 5V
 *   GND → GND
 *   A0 → Analog Out
 * 
 * HC-05:
 *   VCC → 5V
 *   GND → GND
 *   TX → Pin 10
 *   RX → Pin 11 (through voltage divider or 2 diodes)
 * 
 * Protocol:
 * - Sends data every 3 seconds automatically
 * - Format: "PLANT,<soil>%,<temp>C,<humidity>%\n"
 * - Example: "PLANT,65%,23C,45%\n"
 */

#include <DHT.h>
#include <SoftwareSerial.h>

// Pin Definitions
#define DHTPIN 2          // DHT11 data pin
#define DHTTYPE DHT11     // DHT sensor type
#define BT_RX 10          // Connect to HC-05 TX
#define BT_TX 11          // Connect to HC-05 RX (through voltage divider!)
#define SOIL_PIN A0       // Soil moisture sensor (optional)

// Initialize sensors and communication
DHT dht(DHTPIN, DHTTYPE);
SoftwareSerial BTSerial(BT_RX, BT_TX);

// Sensor data
float currentTemperature = 0.0;
float currentHumidity = 0.0;
int currentSoil = 50;  // Default if no sensor
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 3000;  // Send every 3 seconds

// Statistics
unsigned long sendCount = 0;
unsigned long errorCount = 0;
bool hasSoilSensor = false;  // Set to true if you have soil sensor connected

void setup() {
  // Initialize Serial for debugging
  Serial.begin(9600);
  Serial.println(F("========================================"));
  Serial.println(F("  Weather Station Slave - AMAN OS"));
  Serial.println(F("========================================"));
  
  // Initialize Bluetooth communication
  BTSerial.begin(9600);
  Serial.println(F("[BT] Bluetooth initialized at 9600 baud"));
  
  // Initialize DHT sensor
  dht.begin();
  Serial.println(F("[DHT] Sensor initialized"));
  
  // Check if soil sensor is connected (optional)
  if (hasSoilSensor) {
    pinMode(SOIL_PIN, INPUT);
    Serial.println(F("[SOIL] Sensor enabled on A0"));
  } else {
    Serial.println(F("[SOIL] No sensor - using default 50%"));
  }
  
  // Wait for sensor to stabilize
  Serial.println(F("[INIT] Waiting for sensors to stabilize..."));
  delay(2000);
  
  // Initial sensor read
  readSensors();
  
  Serial.println(F("[READY] Weather station ready!"));
  Serial.println(F("        Sending data every 3 seconds"));
  Serial.println(F("========================================"));
  Serial.println();
}

void loop() {
  // Send data every 3 seconds
  if (millis() - lastSend >= SEND_INTERVAL) {
    readSensors();
    sendData();
    lastSend = millis();
    sendCount++;
  }
  
  // Status heartbeat every 10 seconds
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 10000) {
    Serial.print(F("[STATUS] Sends: "));
    Serial.print(sendCount);
    Serial.print(F(" | Errors: "));
    Serial.print(errorCount);
    Serial.print(F(" | Temp: "));
    Serial.print(currentTemperature, 1);
    Serial.print(F("°C | Hum: "));
    Serial.print(currentHumidity, 1);
    Serial.print(F("% | Soil: "));
    Serial.print(currentSoil);
    Serial.println(F("%"));
    lastHeartbeat = millis();
  }
}

void readSensors() {
  // Read DHT11
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  // Check if DHT readings failed
  if (isnan(h) || isnan(t)) {
    errorCount++;
    Serial.println(F("[ERROR] Failed to read DHT sensor!"));
    
    // Keep previous values or use defaults
    if (currentTemperature == 0.0) {
      currentTemperature = 22.0;  // Default temp
      currentHumidity = 50.0;     // Default humidity
    }
  } else {
    currentTemperature = t;
    currentHumidity = h;
    
    Serial.print(F("[DHT] Temp: "));
    Serial.print(currentTemperature, 1);
    Serial.print(F("°C | Humidity: "));
    Serial.print(currentHumidity, 1);
    Serial.println(F("%"));
  }
  
  // Read soil moisture if sensor is present
  if (hasSoilSensor) {
    int rawSoil = analogRead(SOIL_PIN);
    // Convert to percentage (adjust these values based on your sensor)
    // Typical: 0-300 = wet, 300-700 = normal, 700-1023 = dry
    currentSoil = map(rawSoil, 1023, 0, 0, 100);  // Inverted: high reading = dry
    currentSoil = constrain(currentSoil, 0, 100);
    
    Serial.print(F("[SOIL] Raw: "));
    Serial.print(rawSoil);
    Serial.print(F(" | Moisture: "));
    Serial.print(currentSoil);
    Serial.println(F("%"));
  }
  // If no sensor, currentSoil stays at default 50%
}

void sendData() {
  // Build the message string
  // Format: "PLANT,<soil>%,<temp>C,<humidity>%\n"
  String message = "PLANT,";
  message += String(currentSoil);
  message += "%,";
  message += String((int)currentTemperature);
  message += "C,";
  message += String((int)currentHumidity);
  message += "%\n";
  
  // Send via Bluetooth
  BTSerial.print(message);
  
  // Debug output
  Serial.print(F("[TX] Sent: "));
  Serial.print(message);
}

/*
 * CONFIGURATION NOTES:
 * 
 * 1. To enable soil moisture sensor:
 *    - Set hasSoilSensor = true (line 48)
 *    - Connect sensor to A0
 *    - Adjust map() values in readSensors() based on your sensor
 * 
 * 2. Soil Moisture Calibration:
 *    - Put sensor in water → note reading (should be low, like 200-300)
 *    - Put sensor in dry air → note reading (should be high, like 800-1023)
 *    - Adjust map(rawSoil, DRY, WET, 0, 100) accordingly
 * 
 * 3. Send Interval:
 *    - Default: 3000ms (3 seconds)
 *    - Adjust SEND_INTERVAL to change update frequency
 *    - Don't go below 2000ms (DHT11 limitation)
 * 
 * TROUBLESHOOTING:
 * 
 * 1. Master shows "Waiting for data":
 *    - Check Bluetooth pairing (HC-05 LED should blink slowly when paired)
 *    - Verify baud rates match (9600 on both)
 *    - Check TX/RX connections (TX→RX, RX→TX with voltage divider)
 * 
 * 2. "Failed to read DHT sensor!":
 *    - Check wiring (VCC→5V, DATA→Pin2, GND→GND)
 *    - Try adding 10kΩ pull-up resistor from DATA to VCC
 *    - Replace DHT11 if consistently failing
 * 
 * 3. Wrong temperature/humidity:
 *    - DHT11 accuracy: ±2°C, ±5% humidity
 *    - Keep away from heat sources
 *    - Allow 2 seconds between readings
 * 
 * 4. Wrong soil readings:
 *    - Calibrate using steps above
 *    - Some sensors are inverted (high=dry vs low=dry)
 *    - Adjust map() function accordingly
 */