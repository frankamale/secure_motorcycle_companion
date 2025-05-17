#include <HardwareSerial.h>
#include <TinyGPS++.h>
#include "BluetoothSerial.h"

// GPS Setup
HardwareSerial gpsSerial(1);
TinyGPSPlus gps;

// Bluetooth Setup
BluetoothSerial SerialBT;
uint8_t hc05_addr[6] = {0x00, 0x24, 0x01, 0x00, 0x06, 0xC1};

// Pin Definitions
const int touchPin1 = 5;
const int touchPin2 = 2;
const int tiltPin = 14;

// Accelerometer Pins
const int xPin = 32;
const int yPin = 33;
const int zPin = 34;

// System State
bool systemState = false;
unsigned long offTimer = 0;
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000;

// Accelerometer Config
const int numSamples = 20;
const int sampleDelay = 5;
const float sensitivity = 0.3;
const float zeroG = 1.5;
const float voltageRef = 3.3;
const float thresholdG = 1.2; 
const float jerkThreshold = 3.0;
// Accelerometer Variables
unsigned long lastSampleTime = 0;
int sampleCount = 0;
long xSum = 0, ySum = 0, zSum = 0;
float xAccel = 0, yAccel = 0, zAccel = 0;
float magnitude = 0, prevMagnitude = 0;
unsigned long prevAccelTime = 0;

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println("System Initializing...");

  // Pin Modes
  pinMode(touchPin1, INPUT);
  pinMode(touchPin2, INPUT);
  pinMode(tiltPin, INPUT);

  // ADC setup
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Bluetooth setup
  initBluetooth();
}

void loop() {
  // Maintain Bluetooth connection
  if (!SerialBT.connected() && millis() - lastReconnectAttempt >= RECONNECT_INTERVAL) {
    attemptReconnect();
  }

  if (SerialBT.connected()) {
    checkTiltSensor();
    sendHeartbeat();
    handleTouchSensors();
    receiveBluetoothMessages();
  }

  handleAccelerometer();
  handleGPS(); 
}

// --- BLUETOOTH FUNCTIONS ---

void initBluetooth() {
  SerialBT.begin("ESP32_Master", true);
  SerialBT.setPin("1234", 4);
  Serial.println("Initializing Bluetooth...");
  attemptReconnect();
}

void attemptReconnect() {
  Serial.println("Attempting to connect to HC-05...");
  lastReconnectAttempt = millis();
  
  if (SerialBT.connect(hc05_addr)) {
    Serial.println("Connected to HC-05!");
  } else {
    Serial.println("Connection failed. Will retry...");
  }
}

void checkTiltSensor() {
  static unsigned long lastTiltPrint = 0;
  if (millis() - lastTiltPrint >= 1000) {
    lastTiltPrint = millis();
    Serial.println(digitalRead(tiltPin) ? "TILTED" : "NOT TILTED");
  }
}

void sendHeartbeat() {
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat >= 1000) {
    lastHeartbeat = millis();
    SerialBT.println("CONNECTED");
  }
}

void handleTouchSensors() {
  bool sensor1 = digitalRead(touchPin1);
  bool sensor2 = digitalRead(touchPin2);

  if (sensor1 && sensor2 && !systemState) {
    SerialBT.println("ON");
    Serial.println("Sent: ON");
    systemState = true;
    offTimer = 0;
  } else if (!sensor1 && !sensor2 && systemState) {
    if (offTimer == 0) {
      offTimer = millis();
    } else if (millis() - offTimer >= 5000) {
      SerialBT.println("OFF");
      Serial.println("Sent: OFF");
      systemState = false;
      offTimer = 0;
    }
  } else if (sensor1 || sensor2) {
    offTimer = 0;
  }
}

void receiveBluetoothMessages() {
  if (SerialBT.available()) {
    String received = SerialBT.readStringUntil('\n');
    received.trim();
    Serial.print("Received: ");
    Serial.println(received);
  }
}

// --- ACCELEROMETER FUNCTION ---

void handleAccelerometer() {
  if (millis() - lastSampleTime >= sampleDelay && sampleCount < numSamples) {
    xSum += analogRead(xPin);
    ySum += analogRead(yPin);
    zSum += analogRead(zPin);
    sampleCount++;
    lastSampleTime = millis();
  }

  if (sampleCount >= numSamples) {
    float xAvg = xSum / (float)numSamples;
    float yAvg = ySum / (float)numSamples;
    float zAvg = zSum / (float)numSamples;

    xAccel = ((xAvg / 4095.0 * voltageRef) - zeroG) / sensitivity;
    yAccel = ((yAvg / 4095.0 * voltageRef) - zeroG) / sensitivity;
    zAccel = ((zAvg / 4095.0 * voltageRef) - zeroG) / sensitivity;

    magnitude = sqrt(xAccel * xAccel + yAccel * yAccel + zAccel * zAccel);

    unsigned long currentTime = millis();
    float deltaTime = (currentTime - prevAccelTime) / 1000.0;
    float jerk = (magnitude - prevMagnitude) / deltaTime;

    if (magnitude > thresholdG && jerk > jerkThreshold) {
      Serial.println(">> CRASH DETECTED <<");

      // Compose accident message
      String accidentMsg = "ACCIDENT";

      if (gps.location.isValid()) {
        accidentMsg += ",";
        accidentMsg += String(gps.location.lat(), 6);
        accidentMsg += ",";
        accidentMsg += String(gps.location.lng(), 6);
        accidentMsg += ",";
        accidentMsg += String(gps.speed.kmph(), 2);
        accidentMsg += " km/h";
      }

      // Send over Bluetooth
      if (SerialBT.connected()) {
        SerialBT.println(accidentMsg);
      }

      // Debug print
      Serial.println(accidentMsg);
    }

    Serial.print("Accel: ");
    Serial.print(xAccel, 2); Serial.print("g, ");
    Serial.print(yAccel, 2); Serial.print("g, ");
    Serial.print(zAccel, 2); Serial.print("g, Mag: ");
    Serial.print(magnitude, 2); Serial.print("g, Jerk: ");
    Serial.println(jerk, 2);

    xSum = ySum = zSum = 0;
    sampleCount = 0;
    prevMagnitude = magnitude;
    prevAccelTime = currentTime;
  }
}


// --- GPS HANDLER ---

void handleGPS() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());

    if (gps.location.isUpdated()) {
      Serial.print("Lat: ");
      Serial.println(gps.location.lat(), 6);
      Serial.print("Lng: ");
      Serial.println(gps.location.lng(), 6);
      Serial.print("Speed: ");
      Serial.print(gps.speed.kmph());
      Serial.println(" km/h");

      // Optionally send GPS data via Bluetooth
     if (SerialBT.connected()) {
        SerialBT.print("GPS:");
        SerialBT.print(gps.location.lat(), 6);
        SerialBT.print(",");
        SerialBT.print(gps.location.lng(), 6);
        SerialBT.print(",");
        SerialBT.print(gps.speed.kmph(), 2);
        SerialBT.println(" km/h");
}

    }
  }
}