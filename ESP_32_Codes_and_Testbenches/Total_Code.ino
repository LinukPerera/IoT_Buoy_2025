#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Firebase_ESP_Client.h>

#define SEALEVELPRESSURE_HPA (1013.25)

#define gpsSerial Serial2

// Wi-Fi credentials
const char* ssid = "iPhone";      
const char* password = "ABCD1234"; 

// Google Apps Script endpoint
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbzQj0_5O48qYbKG3WBVxqhCprundAdcuO7raUaCgN6xMCH7ld4IuDR8g-r8jKVBMWYN/exec";

// Firebase credentials
#define API_KEY "AIzaSyD-NqvDSI9KnNk2s4U2mrR1JKU33amXcIA"
#define DATABASE_URL "https://buoy2-cbc1f-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "shalukarukshan150@gmail.com"
#define USER_PASSWORD "Vishwa@1234"

// BME280 sensor object
Adafruit_BME280 bme; // I2C

// Firebase components
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variables for turbidity sensor
int turbidityPin = 32;    
int rawValue = 0;
float voltage = 0.0;
float turbidityNTU = 0.0;

// Variables for battery measurement
int batteryPin = 34;      // ADC pin for battery voltage divider
float batteryVoltage = 0.0;

// Motor control pins
const int AIN1 = 26;
const int AIN2 = 25;
const int BIN1 = 27;
const int BIN2 = 14;
const int PWMA = 15;
const int PWMB = 13;

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

// Timer variables for sending data and motor control
unsigned long lastSendTime = 0;
unsigned long lastMotorTime = 0;
const unsigned long sendInterval = 10000; // 10 seconds
const unsigned long motorInterval = 8000; // 8 seconds for motor cycle

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  
  // Initialize BME280
  if (!bme.begin(0x76)) {   
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  // Initialize motor control pins
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(PWMB, OUTPUT);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");

  // Initialize Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Waiting for GPS fix and satellites...");
}

void loop() {
  readGPS();
  readTurbidity();
  readBattery();
  readBME280();
  transferSensorData();
  
  // Send data every 10 seconds
  unsigned long currentTime = millis();
  if (currentTime - lastSendTime >= sendInterval && Firebase.ready()) {
    lastSendTime = currentTime;
    sendToGoogleSheet();
    sendToFirebase();
  }

  // Run motor control cycle every 8 seconds
  if (currentTime - lastMotorTime >= motorInterval) {
    lastMotorTime = currentTime;
    runMotorCycle();
  }
  
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
  rawValue = analogRead(batteryPin);         // ADC: 0–4095
  voltage = rawValue * (3.3 / 4095.0);      // Convert to voltage (measured at ADC)
  batteryVoltage = voltage / 0.248;         // Reverse the voltage divider
  // Map battery voltage (3.0V to 4.2V) to percentage (0% to 100%)
  battery_percentage = constrain(((batteryVoltage - 3.0) / (4.2 - 3.0)) * 100.0, 0.0, 100.0);

  Serial.print("Battery Voltage: ");
  Serial.print(batteryVoltage, 2);
  Serial.println(" V");
  Serial.print("Battery Percentage: ");
  Serial.print(battery_percentage, 1);
  Serial.println(" %");
}

void readBME280() {
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

String getTimestamp() {
  if (gps.date.isValid() && gps.time.isValid()) {
    char timestamp[30];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02dT%02d:%02d:%02d+05:30",
             gps.date.year(), gps.date.month(), gps.date.day(),
             gps.time.hour(), gps.time.minute(), gps.time.second());
    return String(timestamp);
  } else {
    // Fallback to system time (approximate)
    unsigned long epoch = millis() / 1000;
    int seconds = epoch % 60;
    epoch /= 60;
    int minutes = epoch % 60;
    epoch /= 60;
    int hours = (epoch % 24) + 5; // Adjust for +05:30
    int days = epoch / 24;
    int year = 2025;
    int month = 9;
    int day = 3 + days;
    char timestamp[30];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02dT%02d:%02d:%02d+05:30",
             year, month, day, hours, minutes, seconds);
    return String(timestamp);
  }
}

void transferSensorData() {
  if (gps.location.isValid()) {
    gps_latitude = gps.location.lat();
    gps_longitude = gps.location.lng();
  }
  water_turbidity = turbidityNTU;
  water_temperature = bme.readTemperature();
  humidity = bme.readHumidity();

  Serial.println(F("-------------------------------------"));
  Serial.println("Collected Sensor Data:");
  Serial.print("GPS Latitude: ");
  Serial.println(gps_latitude, 6);
  Serial.print("GPS Longitude: ");
  Serial.println(gps_longitude, 6);
  Serial.print("Battery Percentage: ");
  Serial.println(battery_percentage, 1);
  Serial.print("Water Turbidity: ");
  Serial.println(water_turbidity, 1);
  Serial.print("Water Temperature: ");
  Serial.println(water_temperature, 1);
  Serial.print("Humidity: ");
  Serial.println(humidity, 1);
  Serial.print("Air Pressure: ");
  Serial.println(air_pressure, 1);
  Serial.print("Detected Object Class: ");
  Serial.println(detected_object_class);
  Serial.println(F("-------------------------------------"));
}

void sendToGoogleSheet() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(googleScriptURL) + "?data=" 
                 + String(gps_latitude, 6) + "," 
                 + String(gps_longitude, 6) + "," 
                 + String(water_temperature, 1) + "," 
                 + String(humidity, 1) + "," 
                 + String(air_pressure, 1) + "," 
                 + String(water_turbidity, 1) + "," 
                 + String(battery_percentage, 1) + "," 
                 + detected_object_class + "," 
                 + getTimestamp();

    http.begin(url);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("Google Sheets HTTP Response code: ");
      Serial.println(httpResponseCode);
      Serial.println(http.getString());
    } else {
      Serial.print("Error sending to Google Sheets: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("Wi-Fi disconnected, cannot send data to Google Sheet");
  }
}

void sendToFirebase() {
  if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
    FirebaseJson json;
    json.set("gps/lat", gps_latitude);
    json.set("gps/lng", gps_longitude);
    json.set("battery", battery_percentage);
    json.set("turbidity", water_turbidity);
    json.set("temperature", water_temperature);
    json.set("humidity", humidity);
    json.set("pressure", air_pressure);
    json.set("objectClass", detected_object_class);
    json.set("timestamp", getTimestamp());

    if (Firebase.RTDB.setJSON(&fbdo, "/buoy_data", &json)) {
      Serial.println("Firebase: Data sent successfully");
    } else {
      Serial.print("Firebase: Failed to send data, ");
      Serial.println(fbdo.errorReason());
    }
  } else {
    Serial.println("Wi-Fi disconnected or Firebase not ready, cannot send data to Firebase");
  }
}

void motorForward(int speed) {
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMA, speed);
  analogWrite(PWMB, speed);
  Serial.print("Motors Forward at speed: ");
  Serial.println(speed);
}

void motorBackward(int speed) {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  analogWrite(PWMA, speed);
  analogWrite(PWMB, speed);
  Serial.print("Motors Backward at speed: ");
  Serial.println(speed);
}

void motorStop() {
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
  Serial.println("Motors Stop");
}

void runMotorCycle() {
  Serial.println("Starting Motor Cycle");
  motorForward(200);
  delay(2000);
  motorStop();
  delay(1000);
  motorBackward(200);
  delay(2000);
  motorStop();
  delay(1000);
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