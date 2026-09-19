/*
  Alert System for Epileptic Seizures — ESP-01 Firebase Relay
  Board: ESP8266 (ESP-01)

*/

#include <ESP8266Firebase.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ---------------------------------------------------------------------
// Configuration 
// ---------------------------------------------------------------------
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define FIREBASE_REFERENCE_URL "https://YOUR_PROJECT-default-rtdb.firebaseio.com/"

const double TIMEZONE_OFFSET_HOURS = 5.5;  // adjust to your local timezone

// ---------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
Firebase firebase(FIREBASE_REFERENCE_URL);

enum Posture {
  POSTURE_NONE    = 0,
  POSTURE_SITTING = 1,
  POSTURE_SLEPT   = 2,
  POSTURE_FALLEN  = 3
};

struct SensorReading {
  int temperature;
  int humidity;
  int heartRate;
  int flex;
  int posture;
};

// ---------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  connectToWiFi();

  timeClient.begin();
  timeClient.setTimeOffset(TIMEZONE_OFFSET_HOURS * 3600);
  delay(2000);

  firebase.setString("Door", "Close");  // door starts closed
}

// ---------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------
void loop() {
  timeClient.update();
  String timestamp = buildTimestamp();

  if (Serial.available() > 0) {
    String packet = Serial.readString();
    SensorReading reading = parsePacket(packet);

    if (reading.flex == 0) {
      firebase.setString("Door", "Open");  // seizure detected -> unlock door
    }

    uploadReading(reading, timestamp);
  }
}

// ---------------------------------------------------------------------
// WiFi connection
// ---------------------------------------------------------------------
void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(1000);

  Serial.println();
  Serial.print("Connecting to: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("-");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
}

// ---------------------------------------------------------------------
// Timestamp helper
// ---------------------------------------------------------------------
String buildTimestamp() {
  time_t epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime((time_t *)&epochTime);

  String date = String(ptm->tm_mday) + "/" +
                String(ptm->tm_mon + 1) + "/" +
                String(ptm->tm_year + 1900);

  return date + " " + timeClient.getFormattedTime();
}

// ---------------------------------------------------------------------
// Packet parsing
// ---------------------------------------------------------------------
// Parses the compact packet sent by the STM32, e.g. "t30h33r60f0a1"
// -> temperature 30, humidity 33, heart rate 60, flex 0, posture 1.
SensorReading parsePacket(const String &data) {
  SensorReading reading;
  reading.temperature = data.substring(data.indexOf('t') + 1, data.indexOf('h')).toInt();
  reading.humidity     = data.substring(data.indexOf('h') + 1, data.indexOf('r')).toInt();
  reading.heartRate     = data.substring(data.indexOf('r') + 1, data.indexOf('f')).toInt();
  reading.flex          = data.substring(data.indexOf('f') + 1, data.indexOf('a')).toInt();
  reading.posture       = data.substring(data.indexOf('a') + 1).toInt();
  return reading;
}

// ---------------------------------------------------------------------
// Firebase upload
// ---------------------------------------------------------------------
void uploadReading(const SensorReading &reading, const String &timestamp) {
  firebase.pushString("Data/TimeStamp", timestamp);
  firebase.pushInt("Data/Temperature", reading.temperature);
  firebase.pushInt("Data/Humidity", reading.humidity);
  firebase.pushInt("Data/HeartRate", reading.heartRate);
  firebase.pushInt("Data/Flex", reading.flex);
  firebase.pushString("Data/Position", postureLabel(reading.posture));
}

String postureLabel(int posture) {
  switch (posture) {
    case POSTURE_SITTING: return "SITTING";
    case POSTURE_SLEPT:   return "SLEEPING";
    case POSTURE_FALLEN:  return "FELL";
    default:              return "INITIATED";
  }
}
