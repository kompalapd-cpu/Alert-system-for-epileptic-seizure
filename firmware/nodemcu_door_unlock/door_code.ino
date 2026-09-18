/*
  Alert System for Epileptic Seizures — NodeMCU Door Unlock Controller
  Board: NodeMCU (ESP8266)

  Polls the "Door" field in Firebase Realtime Database and drives a
  servo motor to lock/unlock a door accordingly. The "Door" field is
  set to "Open" by the ESP-01 relay when a seizure is detected.

  IMPORTANT: your Firebase project's ".read" and ".write" rules must be
  set to "true" for the MCU to reach the database. See:
  https://github.com/Rupakpoddar/ESP8266Firebase

  BEFORE UPLOADING: replace the placeholder values below with your own
  WiFi credentials and Firebase project URL. Do NOT commit real
  credentials to a public repository — put the placeholders back before
  pushing any changes.
*/

#include <ESP8266Firebase.h>
#include <ESP8266WiFi.h>
#include <Servo.h>

// ---------------------------------------------------------------------
// Configuration — replace with your own values
// ---------------------------------------------------------------------
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define FIREBASE_REFERENCE_URL "https://YOUR_PROJECT-default-rtdb.firebaseio.com/"

const int SERVO_PIN = D5;
const int SERVO_ANGLE_OPEN  = 0;
const int SERVO_ANGLE_CLOSE = 180;
const unsigned long POLL_INTERVAL_MS = 2000;

// ---------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------
Servo doorServo;
Firebase firebase(FIREBASE_REFERENCE_URL);

// ---------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  doorServo.attach(SERVO_PIN);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  connectToWiFi();

  digitalWrite(LED_BUILTIN, HIGH);
}

// ---------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------
void loop() {
  String doorState = firebase.getString("Door");
  Serial.print("Door state: ");
  Serial.println(doorState);

  if (doorState == "Open") {
    doorServo.write(SERVO_ANGLE_OPEN);
    Serial.println("Door open");
  } else {
    doorServo.write(SERVO_ANGLE_CLOSE);
    Serial.println("Door closed");
  }

  delay(POLL_INTERVAL_MS);
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
