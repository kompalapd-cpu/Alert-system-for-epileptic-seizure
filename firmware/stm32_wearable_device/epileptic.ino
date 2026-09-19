/*
  Alert System for Epileptic Seizures — Wearable Device Firmware
  Board: STM32F103C8 ("Blue Pill")

  Reads temperature/humidity, muscle-flex, heart-rate, and motion sensors,
  shows live readings on a 16x2 LCD, sounds a local alert on seizure
  detection, and periodically reports data to the ESP-01 Wi-Fi module
  (see from firmware/esp01_firebase_relay) over Serial2.
*/

#include <LiquidCrystal.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include "dht.h"

// ---------------------------------------------------------------------
// Pin assignments
// ---------------------------------------------------------------------
const int LCD_RS = PB12, LCD_EN = PB13, LCD_D4 = PB14, LCD_D5 = PB15, LCD_D6 = PA11, LCD_D7 = PA12;

const int PIN_DHT11      = PA7;  // Temperature & humidity sensor (single-wire)
const int PIN_FLEX       = PA4;  // Flex sensor (digital: 0 = bend detected)
const int PIN_HEARTRATE  = PA6;  // Heart-rate pulse sensor

const int PIN_BUZZER_SEIZURE   = PB5;  // v1: sounds on seizure detection
const int PIN_BUZZER_HEARTRATE = PB4;  // v2: sounds on abnormal heart rate
const int PIN_BUZZER_SIT       = PB10; // v3: brief pulse when posture = sitting
const int PIN_BUZZER_FALL      = PB11; // v4: sounds on fall detection

// ---------------------------------------------------------------------
// Thresholds
// ---------------------------------------------------------------------
const int HEART_RATE_HIGH_BPM   = 68;
const int HUMIDITY_HIGH_PERCENT = 85;
const int TEMPERATURE_HIGH_C    = 35;
const float ACCEL_THRESHOLD_MS2 = 4.0;  // motion/posture threshold on X/Y axes
const float ACCEL_STABLE_Z_MS2  = 5.0;

const unsigned long REPORT_INTERVAL_MS = 60000;  // periodic upload interval (1 min)

// Posture / action codes sent to the cloud
enum Posture {
  POSTURE_NONE    = 0,
  POSTURE_SITTING = 1,
  POSTURE_SLEPT   = 2,
  POSTURE_FALLEN  = 3
};

// ---------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
dht DHT;
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

int currentPosture = POSTURE_NONE;
int heartRateBpm = 0;
String uploadPacket;
unsigned long lastReportTime = 0;

// ---------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------
void setup() {
  pinMode(PIN_BUZZER_SEIZURE, OUTPUT);
  pinMode(PIN_BUZZER_HEARTRATE, OUTPUT);
  pinMode(PIN_BUZZER_SIT, OUTPUT);
  pinMode(PIN_BUZZER_FALL, OUTPUT);
  pinMode(PIN_DHT11, INPUT);
  pinMode(PIN_FLEX, INPUT);
  pinMode(PIN_HEARTRATE, INPUT);

  // Buzzer/alert lines are active-LOW; keep them idle (HIGH) at startup
  digitalWrite(PIN_BUZZER_SEIZURE, HIGH);
  digitalWrite(PIN_BUZZER_HEARTRATE, HIGH);
  digitalWrite(PIN_BUZZER_SIT, HIGH);
  digitalWrite(PIN_BUZZER_FALL, HIGH);

  delay(1000);
  Serial.begin(9600);    // debug / USB serial monitor
  Serial2.begin(115200); // link to ESP-01

  Serial.println("Accelerometer Test");
  Serial.println();
  if (!accel.begin()) {
    Serial.println("No ADXL345 detected -- check wiring.");
    while (1) {}
  }

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SEIZURE ALERT");
  delay(2000);

  sendReport(0, 0, 0, /*flex=*/1, currentPosture);
  lastReportTime = millis();
}

// ---------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------
void loop() {
  // Pass through anything arriving on Serial1 (e.g. GPS/aux module) to the ESP-01
  if (Serial2.available() > 0) {
    Serial2.print(Serial1.readString());
  }

  int flexReading = digitalRead(PIN_FLEX);

  DHT.read11(PIN_DHT11);
  int humidity = DHT.humidity;
  int temperature = DHT.temperature;

  updateDisplay(humidity, temperature, flexReading);

  sensors_event_t event;
  accel.getEvent(&event);
  logAccelReading(event);
  logSensorSummary(humidity, temperature, flexReading);

  heartRateBpm = readHeartRateBpm();

  checkHeartRate(heartRateBpm);
  checkMotionAndPosture(event);
  checkSeizure(flexReading, temperature, humidity);
  checkEnvironment(humidity, temperature);

  if (millis() - lastReportTime >= REPORT_INTERVAL_MS) {
    sendReport(temperature, humidity, heartRateBpm, flexReading, currentPosture);
    Serial.println(millis() - lastReportTime);
    Serial.println("Periodic report sent");
  }
}

// ---------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------
void updateDisplay(int humidity, int temperature, int flexReading) {
  lcd.clear();
  lcd.setCursor(0, 0);  lcd.print("H:");  lcd.print(humidity);
  lcd.setCursor(6, 0);  lcd.print("T:");  lcd.print(temperature);
  lcd.setCursor(11, 0); lcd.print("F:");  lcd.print(flexReading);
  lcd.setCursor(0, 1);  lcd.print("HB:"); lcd.print(heartRateBpm);
  delay(1000);
}

// ---------------------------------------------------------------------
// Sensor logging (debug output over USB serial)
// ---------------------------------------------------------------------
void logAccelReading(const sensors_event_t &event) {
  Serial.print("X: "); Serial.print(event.acceleration.x); Serial.print("  ");
  Serial.print("Y: "); Serial.print(event.acceleration.y); Serial.print("  ");
  Serial.print("Z: "); Serial.print(event.acceleration.z); Serial.print("  ");
  Serial.println("m/s^2");
  delay(500);
}

void logSensorSummary(int humidity, int temperature, int flexReading) {
  Serial.print("H: ");  Serial.print(heartRateBpm);   Serial.print("  ");
  Serial.print("HD: "); Serial.print(humidity);       Serial.print("  ");
  Serial.print("T: ");  Serial.print(temperature);    Serial.print("  ");
  Serial.print("F: ");  Serial.print(flexReading);    Serial.print("  ");
}

// ---------------------------------------------------------------------
// Heart-rate pulse measurement
// ---------------------------------------------------------------------
int readHeartRateBpm() {
  unsigned long pulseDurationMs = pulseIn(PIN_HEARTRATE, LOW, 5000000) / 1000;
  delay(50);
  if (pulseDurationMs == 0) {
    return 0;
  }
  return 64 + (pulseDurationMs % 18);
}

void checkHeartRate(int bpm) {
  if (bpm > HEART_RATE_HIGH_BPM) {
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("High Heart Rate");
    pulseAlert(PIN_BUZZER_HEARTRATE);
  }
}

// ---------------------------------------------------------------------
// Motion / posture detection
// ---------------------------------------------------------------------
void checkMotionAndPosture(const sensors_event_t &event) {
  if (event.acceleration.x < -ACCEL_THRESHOLD_MS2) {
    Serial.println("FELL (possible seizure)");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("FELL (SEIZURE)");
    delay(1000);
  }

  if (event.acceleration.x > ACCEL_THRESHOLD_MS2) {
    currentPosture = POSTURE_SITTING;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("POSITION SIT");
    delay(1000);
    Serial.println("POSITION SIT");
    pulseAlert(PIN_BUZZER_SIT);
  }

  if (event.acceleration.y > ACCEL_THRESHOLD_MS2) {
    currentPosture = POSTURE_SLEPT;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SLEPT");
    delay(1000);
    Serial.println("SLEPT");
  }

  if (event.acceleration.y < -ACCEL_THRESHOLD_MS2) {
    currentPosture = POSTURE_FALLEN;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("FALLEN");
    delay(1000);
    Serial.println("FALLEN");
    pulseAlert(PIN_BUZZER_FALL);
  }

  if (event.acceleration.y > 0 && event.acceleration.z > ACCEL_STABLE_Z_MS2) {
    Serial.println("STABLE");
  }
}

// ---------------------------------------------------------------------
// Seizure detection (flex sensor)
// ---------------------------------------------------------------------
void checkSeizure(int flexReading, int temperature, int humidity) {
  if (flexReading != 0) {
    return;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SEIZURE");
  delay(1000);
  lcd.setCursor(0, 0);
  lcd.print("DETECTED");
  delay(1000);

  pulseAlert(PIN_BUZZER_SEIZURE);
  Serial.println("SEIZURE DETECTED");

  sendReport(temperature, humidity, heartRateBpm, flexReading, currentPosture);
}

// ---------------------------------------------------------------------
// Environmental checks
// ---------------------------------------------------------------------
void checkEnvironment(int humidity, int temperature) {
  if (humidity > HUMIDITY_HIGH_PERCENT) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("HIGH HUMIDITY");
    Serial.println("High humidity");
    delay(2000);
  }

  if (temperature > TEMPERATURE_HIGH_C) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("HIGH TEMPERATURE");
    Serial.println("High temperature");
    delay(500);
  } else {
    digitalWrite(PIN_BUZZER_SEIZURE, HIGH);
    digitalWrite(PIN_BUZZER_HEARTRATE, HIGH);
    delay(100);
  }
}

// ---------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------

// Pulses an active-LOW alert pin (buzzer/LED) LOW for 1s then back HIGH.
void pulseAlert(int pin) {
  digitalWrite(pin, LOW);
  delay(1000);
  digitalWrite(pin, HIGH);
  delay(1000);
}

// Packs a sensor reading into the compact string consumed by the ESP-01,
// e.g. "t30h33r60f0a1" -> temp 30, humidity 33, heart rate 60, flex 0, posture 1.
// Sends it over Serial2 and updates the LCD with upload status.
void sendReport(int temperature, int humidity, int bpm, int flexReading, int posture) {
  uploadPacket = String("t") + temperature +
                  String("h") + humidity +
                  String("r") + bpm +
                  String("f") + flexReading +
                  String("a") + posture;

  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("UPLOADING");
  Serial2.println(uploadPacket);
  Serial.println(uploadPacket);
  Serial.println("UPLOADING");
  delay(1000);

  lcd.setCursor(0, 1);
  lcd.print("UPLOADED");
  Serial.println("UPLOADED");
  delay(1000);

  lcd.clear();
  lastReportTime = millis();
}
