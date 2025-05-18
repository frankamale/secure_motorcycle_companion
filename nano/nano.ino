#include <SoftwareSerial.h>

// Bluetooth Module
SoftwareSerial btSerial(3, 4); // RX, TX

// GSM Module
SoftwareSerial gsmSerial(10, 11); // RX, TX

// Pins
const uint8_t relayPin = 6;
const uint8_t btStatusLed = 5;
const uint8_t relayStatusLed = 7;

// ThingSpeak API Key
const char* apiKey = "DF511C8WUI2LXFMS";

// Emergency contact number
const char* phoneNumber = "‪+256776975662‬";

// Connection tracking
unsigned long lastHeartbeatTime = 0;
const unsigned long TIMEOUT = 2000; // 2 seconds
bool accidentTriggered = false;

void setup() {
  Serial.begin(9600);
  btSerial.begin(9600);

  pinMode(relayPin, OUTPUT);
  pinMode(btStatusLed, OUTPUT);
  pinMode(relayStatusLed, OUTPUT);

  digitalWrite(relayPin, LOW);
  digitalWrite(btStatusLed, LOW);
  digitalWrite(relayStatusLed, LOW);

  Serial.println(F("System Ready"));
}

void loop() {
  handleBluetoothCommunication();
  checkConnectionStatus();
}

void handleBluetoothCommunication() {
  if (btSerial.available()) {
    String received = btSerial.readStringUntil('\n');
    received.trim();

    if (received.equals("CONNECTED")) {
      lastHeartbeatTime = millis();
      digitalWrite(btStatusLed, HIGH);
      Serial.println(F("Bluetooth Connected"));

    } else if (received.startsWith("ACCIDENT,")) {
      Serial.println(F("ALERT: Accident Detected!"));

      int firstComma = received.indexOf(',');
      int secondComma = received.indexOf(',', firstComma + 1);
      int thirdComma = received.indexOf(',', secondComma + 1);

      if (firstComma != -1 && secondComma != -1 && thirdComma != -1) {
        String lat = received.substring(firstComma + 1, secondComma);
        String lng = received.substring(secondComma + 1, thirdComma);
        String speed = received.substring(thirdComma + 1);

        lat.trim();
        lng.trim();
        speed.trim();

        Serial.print(F("Lat: "));
        Serial.println(lat);
        Serial.print(F("Lng: "));
        Serial.println(lng);
        Serial.print(F("Speed: "));
        Serial.println(speed);

        String gpsData = lat + "," + lng + "," + speed;
        sendToThingSpeak(gpsData);
      } else {
        Serial.println(F("ERROR: Invalid ACCIDENT data format."));
      }

    } else if (received.startsWith("GPS:")) {
      String gpsData = received.substring(4);
      Serial.print(F("GPS Data Received: "));
      Serial.println(gpsData);

      if (accidentTriggered) {
        sendToThingSpeak(gpsData);  
        accidentTriggered = false; 
      }

    } else if (received.equals("ON")) {
      digitalWrite(relayPin, HIGH);
      digitalWrite(relayStatusLed, HIGH);
      Serial.println(F("Relay ON"));

    } else if (received.equals("OFF")) {
      digitalWrite(relayPin, LOW);
      digitalWrite(relayStatusLed, LOW);
      Serial.println(F("Relay OFF"));
    }
  }
}

void checkConnectionStatus() {
  if (millis() - lastHeartbeatTime > TIMEOUT) {
    digitalWrite(btStatusLed, LOW);
  }
}

bool sendCommand(const char* command, const char* expected, unsigned long timeout) {
  gsmSerial.println(command);
  Serial.println(command);
  
  unsigned long start = millis();
  String response = "";
  
  while (millis() - start < timeout) {
    while (gsmSerial.available()) {
      char c = gsmSerial.read();
      response += c;
      Serial.print(c);
      if (response.indexOf(expected) != -1) return true;
    }
  }
  
  Serial.print(F("Timeout or Unexpected Response: "));
  Serial.println(command);
  return false;
}

void flushGSM() {
  while (gsmSerial.available()) {
    Serial.write(gsmSerial.read());
  }
}

void sendSMSLocation(const String& lat, const String& lng) {
  Serial.print(F("Sending SMS to "));
  Serial.println(phoneNumber);

  gsmSerial.println("AT");
  delay(1000);
  while (gsmSerial.available()) Serial.write(gsmSerial.read());

  gsmSerial.println("AT+CMGF=1");
  delay(1000);
  while (gsmSerial.available()) Serial.write(gsmSerial.read());

  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(phoneNumber);
  gsmSerial.println("\"");
  delay(1000); // Wait for '>' prompt

  gsmSerial.print("Alert!!\nAn Accident has been Detected!\nLocation: https://maps.google.com/?q=");
  gsmSerial.print(lat);
  gsmSerial.print(",");
  gsmSerial.println(lng);

  gsmSerial.write(26); // Ctrl+Z
  delay(5000); // Wait for message to send

  Serial.println("Message sent.");
}

bool waitForResponse(String expected, unsigned long timeout) {
  unsigned long start = millis();
  String response = "";

  while (millis() - start < timeout) {
    while (gsmSerial.available()) {
      char c = gsmSerial.read();
      response += c;
      Serial.write(c);
      if (response.indexOf(expected) != -1) return true;
    }
  }

  return false;
}

void sendToThingSpeak(const String& gpsData) {
  gsmSerial.begin(9600);
  Serial.println(F("Initializing GSM..."));

  if (!sendCommand("AT", "OK", 2000)) return;
  if (!sendCommand("AT+CPIN?", "READY", 5000)) return;
  if (!sendCommand("AT+CREG?", "0,1", 5000)) return;
  if (!sendCommand("AT+CGATT?", "1", 5000)) return;
  if (!sendCommand("AT+CIPSHUT", "SHUT OK", 5000)) return;
  if (!sendCommand("AT+CIPMUX=0", "OK", 2000)) return;
  if (!sendCommand("AT+CSTT=\"internet\"", "OK", 3000)) return;
  if (!sendCommand("AT+CIICR", "OK", 5000)) return;

  gsmSerial.println(F("AT+CIFSR"));
  delay(2000);
  flushGSM();

  if (!sendCommand("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",\"80\"", "CONNECT OK", 10000)) return;
  delay(3000);

  int firstComma = gpsData.indexOf(',');
  int secondComma = gpsData.indexOf(',', firstComma + 1);

  if (firstComma == -1 || secondComma == -1) {
    Serial.println(F("Invalid GPS data format"));
    return;
  }

  String lat = gpsData.substring(0, firstComma);
  String lng = gpsData.substring(firstComma + 1, secondComma);
  String speedRaw = gpsData.substring(secondComma + 1);
  
  lat.trim();
  lng.trim();
  speedRaw.trim();

  int spaceIndex = speedRaw.indexOf(' ');
  String speed = (spaceIndex != -1) ? speedRaw.substring(0, spaceIndex) : speedRaw;

  Serial.print(F("Parsed lat: "));
  Serial.println(lat);
  Serial.print(F("Parsed lng: "));
  Serial.println(lng);
  Serial.print(F("Parsed speed: "));
  Serial.println(speed);

  if (!sendCommand("AT+CIPSEND", ">", 5000)) return;

  gsmSerial.print(F("GET /update?api_key="));
  gsmSerial.print(apiKey);
  gsmSerial.print(F("&field1="));
  gsmSerial.print(lat);
  gsmSerial.print(F("&field2="));
  gsmSerial.print(lng);
  gsmSerial.print(F("&field3="));
  gsmSerial.print(speed);
  gsmSerial.println(F(" HTTP/1.1\r\nHost: api.thingspeak.com\r\nConnection: close\r\n\r\n"));
  
  delay(2000);
  gsmSerial.write(26); // Ctrl+Z

  unsigned long start = millis();
  bool sentOK = false;
  
  while (millis() - start < 10000 && !sentOK) {
    if (gsmSerial.available()) {
      String response = gsmSerial.readString();
      Serial.print(response);
      if (response.indexOf("SEND OK") != -1) sentOK = true;
    }
  }

  if (!sentOK) {
    Serial.println(F("Failed to send data!"));
    return;
  }

  delay(2000);
  sendCommand("AT+CIPSHUT", "SHUT OK", 5000);
  Serial.println(F("Data sent"));

  sendSMSLocation(lat, lng);
  delay(1000);

  restartBluetooth();  // Software-only reinitialization
}

// Software-only Bluetooth restart
void restartBluetooth() {
  Serial.println(F("Reinitializing Bluetooth connection (software only)..."));
  btSerial.end();
  delay(1000);
  btSerial.begin(9600);
  btSerial.println("Ready again");
  Serial.println(F("Bluetooth serial restarted and notified."));
}
