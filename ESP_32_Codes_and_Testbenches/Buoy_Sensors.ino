#include <TinyGPSPlus.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_BME280.h>

// #define DHTPIN 5          // Digital pin connected to the DHT sensor
// #define DHTTYPE DHT22     // DHT 22 (AM2302)
#define SEALEVELPRESSURE_HPA (1013.25)

#define gpsSerial Serial2

// Wi-Fi credentials (replace with your own)
const char* ssid = "YOUR_WIFI_SSID";      // Replace with your Wi-Fi SSID
const char* password = "YOUR_WIFI_PASSWORD"; // Replace with your Wi-Fi password

// Google Apps Script endpoint
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbwypjROyPUOAWA0xhI4WKgfsl1FT1ZKfBQPvmkdK08UTvc0nIZ-d1YqoJmV5V1SrUNN/exec";

// DHT sensor object
// DHT dht(DHTPIN, DHTTYPE);

// BME280 sensor object
Adafruit_BME280 bme; // I2C

// Variables for DHT sensor
float Humd = 0.0;
float Temp = 0.0;
String Status_Read_Sensor = "";

// Variables for turbidity sensor
int turbidityPin = 15;    // Use a valid ADC pin (ESP32: 32–39 recommended)
int rawValue = 0;
float voltage = 0.0;
float turbidityNTU = 0.0;

// Variables for battery measurement
int batteryPin = 34;      // ADC pin for battery voltage divider
float batteryVoltage = 0.0;

// GPS object
TinyGPSPlus gps;

// Variables for data collection
float gps_latitude = 37.7749;
float gps_longitude = -122.4194;
float battery_percentage = 85.5;
float water_turbidity = 15.3;
float water_temperature = 24.8;
float humidity = 72.0;
float air_pressure = 1013.2;
String detected_object_class = "marine_debris";

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  dht.begin();
  
  // Initialize BME280
  if (!bme.begin(0x76)) {   // Try 0x76 or 0x77 depending on your module
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");

  Serial.println("Waiting for GPS fix and satellites...");
}

void loop() {
  readGPS();
  readDHT22();
  readTurbidity();
  readBattery();
  readBME280();
  transferSensorData();
  sendToGoogleSheet();
  delay(1000);
}

void readGPS() {
  while (gpsSerial.available() > 0) {
    if (gps.encode(gpsSerial.read())) {
      displayLocationInfo();
    }
  }

  if (millis() > 5000 && gps.charsProcessed() < 10) {
    Serial.println(F("No GPS detected: check wiring."));
    while (true);
  }
}

void readTurbidity() {
  rawValue = analogRead(turbidityPin);       // ADC: 0–4095
  voltage = rawValue * (3.3 / 4095.0);       // ESP32 ADC → 0–3.3V
  turbidityNTU = (voltage / 3.3) * 3000.0;   // Maps 0–3.3 V to 0–3000 NTU

  Serial.print("Turbidity: ");
  Serial.print(turbidityNTU, 1);
  Serial.println(" NTU");
}

void readBattery() {
  int batteryRaw = analogRead(batteryPin);    // ADC: 0–4095
  batteryVoltage = batteryRaw * (3.3 / 4095.0); // Convert to voltage (0–3.3V)
  // Map voltage to percentage (0V = 0%, 2V = 100%)
  battery_percentage = constrain((batteryVoltage / 2.0) * 100.0, 0.0, 100.0);

  Serial.print("Battery Voltage: ");
  Serial.print(batteryVoltage, 2);
  Serial.println(" V");
  Serial.print("Battery Percentage: ");
  Serial.print(battery_percentage, 1);
  Serial.println(" %");
}

void readBME280() {
  // Read pressure from BME280
  air_pressure = bme.readPressure() / 100.0F; // Convert Pa to hPa

  Serial.print("Air Pressure: ");
  Serial.print(air_pressure);
  Serial.println(" hPa");
  water_temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  Serial.print("BME280 Temperature: ");
  Serial.print(water_temperature);
  Serial.println(" °C");
  Serial.print("BME280 Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");
  
}

void transferSensorData() {
  // Update variables with available sensor data
  if (gps.location.isValid()) {
    gps_latitude = gps.location.lat();
    gps_longitude = gps.location.lng();
  }
  water_turbidity = turbidityNTU;
  water_temperature = Temp;
  humidity = Humd;
  // air_pressure is updated in readBME280()
  // detected_object_class remains as default value since no sensor is available

  // Print all collected data
  Serial.println(F("-------------------------------------"));
  Serial.println("Collected Sensor Data:");
  Serial.print("GPS Latitude: ");
  Serial.println(gps_latitude, 6);
  Serial.print("GPS Longitude: ");
  Serial.println(gps_longitude, 6);
  Serial.print("Battery Percentage: ");
  Serial.println(battery_percentage);
  Serial.print("Water Turbidity: ");
  Serial.println(water_turbidity, 1);
  Serial.print("Water Temperature: ");
  Serial.println(water_temperature);
  Serial.print("Humidity: ");
  Serial.println(humidity);
  Serial.print("Air Pressure: ");
  Serial.println(air_pressure);
  Serial.print("Detected Object Class: ");
  Serial.println(detected_object_class);
  Serial.println(F("-------------------------------------"));
}

void sendToGoogleSheet() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(googleScriptURL);
    http.addHeader("Content-Type", "application/json");

    // Create JSON payload
    String jsonPayload = "{\"gps_latitude\":";
    jsonPayload += gps_latitude;
    jsonPayload += ",\"gps_longitude\":";
    jsonPayload += gps_longitude;
    jsonPayload += ",\"battery_percentage\":";
    jsonPayload += battery_percentage;
    jsonPayload += ",\"water_turbidity\":";
    jsonPayload += water_turbidity;
    jsonPayload += ",\"water_temperature\":";
    jsonPayload += water_temperature;
    jsonPayload += ",\"humidity\":";
    jsonPayload += humidity;
    jsonPayload += ",\"air_pressure\":";
    jsonPayload += air_pressure;
    jsonPayload += ",\"detected_object_class\":\"";
    jsonPayload += detected_object_class;
    jsonPayload += "\"}";

    // Send POST request
    int httpResponseCode = http.POST(jsonPayload);

    // Check response
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      Serial.print("Response: ");
      Serial.println(response);
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("Wi-Fi disconnected, cannot send data to Google Sheet");
  }
}

void displayLocationInfo() {
  Serial.println(F("-------------------------------------"));
  Serial.println("\nLocation Info:");

  Serial.print("Latitude:  ");
  Serial.print(gps.location.lat(), 6);
  Serial.print(" ");
  Serial.println(gps.location.rawLat().negative ? "S" : "N");

  Serial.print("Longitude: ");
  Serial.print(gps.location.lng(), 6);
  Serial.print(" ");
  Serial.println(gps.location.rawLng().negative ? "W" : "E");

  Serial.print("Fix Quality: ");
  Serial.println(gps.location.isValid() ? "Valid" : "Invalid");

  Serial.print("Satellites: ");
  Serial.println(gps.satellites.value());

  Serial.print("Altitude:   ");
  Serial.print(gps.altitude.meters());
  Serial.println(" m");

  Serial.print("Speed:      ");
  Serial.print(gps.speed.kmph());
  Serial.println(" km/h");

  Serial.print("Course:     ");
  Serial.print(gps.course.deg());
  Serial.println("°");

  Serial.print("Date:       ");
  if (gps.date.isValid()) {
    Serial.printf("%02d/%02d/%04d\n", gps.date.day(), gps.date.month(), gps.date.year());
  } else {
    Serial.println("Invalid");
  }

  Serial.print("Time (UTC): ");
  if (gps.time.isValid()) {
    Serial.printf("%02d:%02d:%02d\n", gps.time.hour(), gps.time.minute(), gps.time.second());
  } else {
    Serial.println("Invalid");
  }

  Serial.println(F("-------------------------------------"));
}