#include <WiFi.h>
#include <HTTPClient.h>

// Wi-Fi credentials
const char* ssid = ".";
const char* password = "098765432";

// Google Apps Script endpoint
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbzQj0_5O48qYbKG3WBVxqhCprundAdcuO7raUaCgN6xMCH7ld4IuDR8g-r8jKVBMWYN/exec";

void setup() {
  Serial.begin(115200);
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");
  
  Serial.println("Setup complete. Starting to send dummy data...");
}

void loop() {
  sendToGoogleSheet();
  delay(10000); // Wait 10 seconds between sends
}

void sendToGoogleSheet() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Dummy values
    float latitude    = 37.7749;
    float longitude   = -122.4194;
    float temperature = 24.8;
    float humidity    = 72.0;
    float pressure    = 1013.2;
    float turbidity   = 15.3;

    // Build query string (matches Apps Script expectations)
    String url = String(googleScriptURL) + "?data=" 
                 + String(latitude, 6) + "," 
                 + String(longitude, 6) + "," 
                 + String(temperature, 2) + "," 
                 + String(humidity, 2) + "," 
                 + String(pressure, 2) + "," 
                 + String(turbidity, 2);

    http.begin(url);

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("Google Sheets HTTP Response code: ");
      Serial.println(httpResponseCode);
      Serial.println(http.getString()); // Show server response
    } else {
      Serial.print("Error sending to Google Sheets: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("Wi-Fi disconnected");
  }
}