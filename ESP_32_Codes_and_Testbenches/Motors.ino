// === Include Required for Newer ESP32 Core ===
#include <Arduino.h>

// === Pin Definitions ===
const int AIN1 = 26;
const int AIN2 = 25;
const int BIN1 = 27;
const int BIN2 = 14;
const int PWMA = 15;
const int PWMB = 13;

// === PWM Configuration ===
const int freq = 1000;          // 1 kHz
const int resolution = 8;       // 8-bit resolution: 0–255
const int pwmChannelA = 0;      // PWM channel for motor A
const int pwmChannelB = 1;      // PWM channel for motor B

void setup() {
  Serial.begin(115200);
  Serial.println("TB6612FNG Dual Motor Test (Updated for new ESP32 core)");

  // Direction pin setup
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

  // PWM setup for motor A
  ledcSetup(pwmChannelA, freq, resolution);
  ledcAttachPin(PWMA, pwmChannelA);

  // PWM setup for motor B
  ledcSetup(pwmChannelB, freq, resolution);
  ledcAttachPin(PWMB, pwmChannelB);
}

void loop() {
  Serial.println("Motors Forward");

  // Motor A forward
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  ledcWrite(pwmChannelA, 200);

  // Motor B forward
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  ledcWrite(pwmChannelB, 200);

  delay(2000);

  Serial.println("Motors Stop");
  ledcWrite(pwmChannelA, 0);
  ledcWrite(pwmChannelB, 0);
  delay(1000);

  Serial.println("Motors Backward");

  // Motor A backward
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  ledcWrite(pwmChannelA, 200);

  // Motor B backward
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  ledcWrite(pwmChannelB, 200);

  delay(2000);

  Serial.println("Motors Stop");
  ledcWrite(pwmChannelA, 0);
  ledcWrite(pwmChannelB, 0);
  delay(2000);
}
