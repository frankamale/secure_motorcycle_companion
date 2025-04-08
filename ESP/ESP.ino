#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

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

// Accelerometer Configuration
const int numSamples = 20;
const int sampleDelay = 5;
const float sensitivity = 0.3;
const float zeroG = 1.5;
const float voltageRef = 3.3;
const float thresholdG = 3;
const float jerkThreshold = 20;

// Accelerometer Variables
unsigned long lastSampleTime = 0;
int sampleCount = 0;
long xSum = 0, ySum = 0, zSum = 0;
float xAccel = 0, yAccel = 0, zAccel = 0;
float magnitude = 0, prevMagnitude = 0;
unsigned long prevAccelTime = 0;

// HC-05 MAC Address
uint8_t hc05_addr[6] = {0x00, 0x24, 0x01, 0x00, 0x06, 0xC1};

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(touchPin1, INPUT);
  pinMode(touchPin2, INPUT);
  pinMode(tiltPin, INPUT);

  // Configure ADC for accelerometer
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Initialize Bluetooth
  initBluetooth();
}

void loop() {
  // Maintain Bluetooth connection
  if (!SerialBT.connected() && millis() - lastReconnectAttempt >= RECONNECT_INTERVAL) {
    attemptReconnect();
  }

  // Main system functions
  if (SerialBT.connected()) {
    checkTiltSensor();
    sendHeartbeat();
    handleTouchSensors();
    receiveBluetoothMessages();
  }

  // Handle accelerometer data
  handleAccelerometer();
}

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

  if (!systemState) {
    if (sensor1 && sensor2) {
      if (SerialBT.connected()) {
        SerialBT.println("ON");
      }
      Serial.println("Sent: ON");
      systemState = true;
      offTimer = 0;
    }
  } 
  else {
    if (sensor1 || sensor2) {
      offTimer = 0;
    } else {
      if (offTimer == 0) {
        offTimer = millis();
      }
      if (millis() - offTimer >= 5000) {
        if (SerialBT.connected()) {
          SerialBT.println("OFF");
        }
        Serial.println("Sent: OFF");
        systemState = false;
        offTimer = 0;
      }
    }
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

void handleAccelerometer() {
  // Collect samples
  if (millis() - lastSampleTime >= sampleDelay && sampleCount < numSamples) {
    xSum += analogRead(xPin);
    ySum += analogRead(yPin);
    zSum += analogRead(zPin);
    sampleCount++;
    lastSampleTime = millis();
  }

  // Process when all samples collected
  if (sampleCount >= numSamples) {
    // Calculate averages
    float xAvg = xSum / (float)numSamples;
    float yAvg = ySum / (float)numSamples;
    float zAvg = zSum / (float)numSamples;

    // Convert to g-forces
    xAccel = ((xAvg / 4095.0 * voltageRef) - zeroG) / sensitivity;
    yAccel = ((yAvg / 4095.0 * voltageRef) - zeroG) / sensitivity;
    zAccel = ((zAvg / 4095.0 * voltageRef) - zeroG) / sensitivity;

    // Calculate magnitude
    magnitude = sqrt(xAccel*xAccel + yAccel*yAccel + zAccel*zAccel);

    // Calculate jerk
    unsigned long currentTime = millis();
    float deltaTime = (currentTime - prevAccelTime) / 1000.0;
    float jerk = (magnitude - prevMagnitude) / deltaTime;

    // Crash detection
    if (magnitude > thresholdG && jerk > jerkThreshold) {
      if (SerialBT.connected()) {
        SerialBT.println("CRASH");
      }
      Serial.println(">> CRASH DETECTED <<");
    }

    // Serial output
    Serial.print("Accel: ");
    Serial.print(xAccel, 2); Serial.print("g, ");
    Serial.print(yAccel, 2); Serial.print("g, ");
    Serial.print(zAccel, 2); Serial.print("g, Mag: ");
    Serial.print(magnitude, 2); Serial.print("g, Jerk: ");
    Serial.println(jerk, 2);

    // Reset variables
    xSum = ySum = zSum = 0;
    sampleCount = 0;
    prevMagnitude = magnitude;
    prevAccelTime = currentTime;
  }
}