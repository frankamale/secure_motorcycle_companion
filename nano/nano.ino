#include <SoftwareSerial.h>

// Pin Definitions
const int relayPin = 6;          // Relay control pin
const int btStatusLed = 5;       // Bluetooth status LED
const int relayStatusLed = 7;    // Relay status LED
const int btStatePin = 12;       // HC-05 STATE pin (not used in this version)

// Bluetooth Serial
SoftwareSerial mySerial(2, 3);   // RX=2, TX=3

// Connection Tracking
unsigned long lastHeartbeatTime = 0;
const unsigned long timeout = 2000; // 2-second connection timeout

void setup() {
  initializeSystem();
}

void loop() {
  handleBluetoothCommunication();
  checkConnectionStatus();
 
}

void initializeSystem() {
  Serial.begin(9600);
  mySerial.begin(9600);

  // Set pin modes
  pinMode(relayPin, OUTPUT);
  pinMode(btStatusLed, OUTPUT);
  pinMode(relayStatusLed, OUTPUT);
  pinMode(btStatePin, INPUT); // Not used but configured

  // Set initial states
  digitalWrite(btStatusLed, LOW);
  digitalWrite(relayStatusLed, LOW);
  digitalWrite(relayPin, LOW);

  Serial.println("System Ready");
}

// Function to handle Bluetooth communication
void handleBluetoothCommunication() {
  if (mySerial.available()) {
    String received = mySerial.readStringUntil('\n');
    received.trim();

    if (received == "CONNECTED") {
      handleBluetoothConnected();
    } else {
      processBluetoothCommand(received);
    }
  }
}

// Function to handle Bluetooth connection status
void handleBluetoothConnected() {
  lastHeartbeatTime = millis();
  digitalWrite(btStatusLed, HIGH); // LED on when connected
  Serial.println("Bluetooth Connected");
}

// Function to process received Bluetooth commands
void processBluetoothCommand(String command) {
  Serial.print("Received: ");
  Serial.println(command);

  if (command == "ON") {
    controlRelay(true);
  } else if (command == "OFF") {
    controlRelay(false);
  }
}

// Function to control the relay and status LED
void controlRelay(bool state) {
  digitalWrite(relayPin, state ? HIGH : LOW);
  digitalWrite(relayStatusLed, state ? HIGH : LOW);
  Serial.println(state ? "Relay ON" : "Relay OFF");
}

// Function to check connection status and handle timeout
void checkConnectionStatus() {
  if (millis() - lastHeartbeatTime > timeout) {
    digitalWrite(btStatusLed, LOW); // LED off when disconnected
  }
}
