#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

// Pin Definitions
const int touchPin1 = 5;
const int touchPin2 = 2;
const int tiltPin = 14;

// System State
bool systemState = false;
unsigned long offTimer = 0;
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000; // 5 seconds between reconnect attempts

// HC-05 MAC Address
uint8_t hc05_addr[6] = {0x00, 0x24, 0x01, 0x00, 0x06, 0xC1};

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(touchPin1, INPUT);
  pinMode(touchPin2, INPUT);
  pinMode(tiltPin, INPUT);
  
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
  
  delay(100); // Small delay to prevent excessive processing
}

void initBluetooth() {
  SerialBT.begin("ESP32_Master", true); // Bluetooth in Master Mode
  SerialBT.setPin("1234", 4); // Passcode for connecting to the Bluetooth
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
  if (millis() - lastTiltPrint >= 1000) { // Only print every second
    lastTiltPrint = millis();
    if (digitalRead(tiltPin)) {
      Serial.println("TILTED");
    } else {
      Serial.println("NOT TILTED");
    }
  }
}

void sendHeartbeat() {
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat >= 1000) { // Send every second
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
  } else {
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