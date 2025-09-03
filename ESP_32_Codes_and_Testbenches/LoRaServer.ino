 /*
  LoRa Multi-Sensor Temperature and Humidity Monitor - Controller
  lora-temp-humid-control.ino
  Central Controller for LoRa Temperature and Humidity Monitor
  Uses ESP32, RFM95W LoRa & SSD1306 I2C OLED Display
  Displays Temperature and Humidity readings from remote sensors
  Requires LoRa Library by Sandeep Mistry - https://github.com/sandeepmistry/arduino-LoRa
  Requires Adafruit GFX and SSD1306 libraries
  

*/

// Include required libraries
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Define the pins used by the LoRa module
const int csPin = 5;      // LoRa radio chip select
const int resetPin = 14;  // LoRa radio reset
const int irqPin = 2;     // Must be a hardware interrupt pin

// Source and sensorAddress1 addresses
byte localAddress = 0x01;    // Address of this device (Controller = 0x01)
byte sensorAddress1 = 0xAA;  // Address of Sensor 1
//byte sensorAddress2 = 0xBB;  // Address of Sensor 2

// OLED parameters
#define SCREEN_WIDTH 128     // OLED display width, in pixels
#define SCREEN_HEIGHT 64     // OLED display height, in pixels
#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  // Change if required

// Define display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Remote temperature and humidity variables
// Data variable
String remoteData1 = "TT.tt:HH.hh";
String remoteData2 = "TT.tt:HH.hh";

// Sensor 1
String remoteTemp1;
String remoteHumid1;
// Sensor 2
String remoteTemp2;
String remoteHumid2;

// Remote sensor time variables
unsigned long currentActive1 = millis();
unsigned long currentActive2 = millis();
const long checkInterval = 12500;  // 12.5 second sensor check interval

// Outgoing Message counter
byte msgCount = 0;

// FUNCTION newDisplay() - Refresh the display with new data
void newDisplay(String temp1, String humid1, String temp2, String humid2, int displayOrder) {

  // Print display header
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("REMOTE TEMP & HUMID");

  // If displayOrder = 1 then reverse display order

  if (displayOrder == 1) {

    // Remote Sensor 2 is first

    display.setTextSize(2);
    display.setCursor(0, 16);
    display.print("T2: ");
    display.setCursor(38, 16);
    display.print(temp2);
    display.print("C");

    display.setCursor(0, 34);
    display.print("H2: ");
    display.setCursor(38, 34);
    display.print(humid2);
    display.print("%");

    // Node 1 in smaller font

    display.setTextSize(1);

    display.setCursor(0, 55);
    display.print("T1: ");
    display.setCursor(18, 55);
    display.print(temp1);
    display.print("C");

    display.setCursor(60, 55);
    display.print("H1: ");
    display.setCursor(78, 55);
    display.print(humid1);
    display.print("%");

  } else {

    // Remote Sensor 1 1 is first

    display.setTextSize(2);
    display.setCursor(0, 16);
    display.print("T1: ");
    display.setCursor(38, 16);
    display.print(temp1);
    display.print("C");

    display.setCursor(0, 34);
    display.print("H1: ");
    display.setCursor(38, 34);
    display.print(humid1);
    display.print("%");

    // Node 2 in smaller font

    display.setTextSize(1);

    display.setCursor(0, 55);
    display.print("T2: ");
    display.setCursor(18, 55);
    display.print(temp2);
    display.print("C");

    display.setCursor(60, 55);
    display.print("H2: ");
    display.setCursor(78, 55);
    display.print(humid2);
    display.print("%");
  }

  display.display();
}

// FUNCTION getValue() - Extract value from delimited string
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

// FUNCTION onReceive() - Receive call-back function
void onReceive(int packetSize) {
  if (packetSize == 0) return;  // if there's no packet, return

  // read packet header bytes:
  int recipient = LoRa.read();        // recipient address
  byte sender = LoRa.read();          // sender address
  byte incomingMsgId = LoRa.read();   // incoming msg ID
  byte incomingLength = LoRa.read();  // incoming msg length

  String incoming = "";  // payload of packet

  while (LoRa.available()) {        // can't use readString() in callback, so
    incoming += (char)LoRa.read();  // add bytes one by one
  }

  if (incomingLength != incoming.length()) {  // check length for error
    Serial.println("error: message length does not match length");
    return;  // skip rest of function
  }

  // if the recipient isn't this device
  if (recipient != localAddress) {
    Serial.println("This message is not for me.");
    return;  // skip rest of function
  }

  // Determine sender, then update data variables and time stamps
  if (sender == sensorAddress1) {
    //Remote Sensor 1
    remoteData1 = incoming;
    currentActive1 = millis();
  } /*else if (sender == sensorAddress2) {
    //Remote Sensor 2
    remoteData2 = incoming;
    currentActive2 = millis();
  }*/
}

// FUNCTION sendMessage() - Send LoRa Packet
void sendMessage(String outgoing, byte target) {
  LoRa.beginPacket();             // start packet
  LoRa.write(target);             // add sensorAddress1 address
  LoRa.write(localAddress);       // add sender address
  LoRa.write(msgCount);           // add message ID
  LoRa.write(outgoing.length());  // add payload length
  LoRa.print(outgoing);           // add payload
  LoRa.endPacket();               // finish packet and send it
  msgCount++;                     // increment message ID
}

// FUNCTION  getValues() - get the temperature and humidity values from the data variables
void getValues() {
  // Check to see if sensors have reported in recently
  // Get current timestamp value
  unsigned long currentMillis = millis();

  // See if we have exceeded the check interval time limit
  // Sensor 1
  if (currentMillis - currentActive1 <= checkInterval) {
    // Data is good, extract temp ahd humid
    remoteTemp1 = getValue(remoteData1, ':', 0);   // Remote 1 Temperature
    remoteHumid1 = getValue(remoteData1, ':', 1);  // Remote 1 Humidity
  } else {
    remoteTemp1 = "??.??";
    remoteHumid1 = "??.??";
  }
  // Sensor 2
  if (currentMillis - currentActive2 <= checkInterval) {
    // Data is good, extract temp ahd humid
    remoteTemp2 = getValue(remoteData2, ':', 0);   // Remote 1 Temperature
    remoteHumid2 = getValue(remoteData2, ':', 1);  // Remote 1 Humidity
  } else {
    remoteTemp2 = "??.??";
    remoteHumid2 = "??.??";
  }
}

void setup() {
  Serial.begin(9600);
  while (!Serial)
    ;

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  // Clear the display buffer
  display.clearDisplay();
  display.display();

  // Refresh OLED
  newDisplay("XX.XX", "XX.XX", "XX.XX", "XX.XX", 0);

  // Setup LoRa module
  LoRa.setPins(csPin, resetPin, irqPin);

  Serial.println("LoRa Receiver Test");

  // Start LoRa module at local frequency
  // 433E6 for Asia
  // 866E6 for Europe
  // 915E6 for North America

  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1)
      ;
  }

  LoRa.onReceive(onReceive);
  LoRa.receive();
  Serial.println("LoRa init succeeded.");
}

void loop() {

  // Get latest data values
  getValues();

  Serial.print("Temp 1: ");
  Serial.print(remoteTemp1);
  Serial.print(" - Humid 1: ");
  Serial.println(remoteHumid1);

  Serial.print("Temp 2: ");
  Serial.print(remoteTemp2);
  Serial.print(" - Humid 2: ");
  Serial.println(remoteHumid2);

  // Update OLED
  newDisplay(remoteTemp1, remoteHumid1, remoteTemp2, remoteHumid2, 0);

  // Delay 3 seconds to hold display
  delay(3000);

  // Send message to remote 1
  String outMsg1 = "";
  outMsg1 = outMsg1 + msgCount;
  sendMessage(outMsg1, sensorAddress1);

  // Place LoRa back into Receive Mode
  LoRa.receive();

  // Refresh the data values
  getValues();

  // Update OLED - reverse display
  newDisplay(remoteTemp1, remoteHumid1, remoteTemp2, remoteHumid2, 1);

  // Delay 3 seconds to hold display
  delay(3000);

  // Send message to remote 2
  String outMsg2 = "";
  outMsg2 = outMsg2 + msgCount;
  //sendMessage(outMsg2, sensorAddress2);

  // Place LoRa back into Receive Mode
  LoRa.receive();
}