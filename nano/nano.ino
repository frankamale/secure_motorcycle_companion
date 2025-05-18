#include <SoftwareSerial.h>

// Bluetooth Module
SoftwareSerial btSerial(3, 4); // RX, TX

// GSM Module
SoftwareSerial gsmSerial(10, 11); // RX, TX

// Pins
const uint8_t relayPin = 6;
const uint8_t btStatusLed = 5; // Corrected from btmeasuringLed
const uint8_t relayStatusLed = 7;

// ThingSpeak API Keys
const char* accidentApiKey = "DF511C8WUI2LXFMS"; // Accident channel
const char* monitoringApiKey = "Y5IDJ5FUZ1C7Q68S"; // Monitoring channel

// Emergency contact number
const char* phoneNumber = "+256776975662";

// Connection tracking
unsigned long lastHeartbeatTime = 0;
const unsigned long TIMEOUT = 2000; // 2 seconds
bool accidentTriggered = false;
const char* helmetID = "H-001";

// Monitoring interval (2 minutes)
const unsigned long MONITORING_INTERVAL = 120000;
unsigned long lastMonitoringTime = 0;

// GPS tracking
String lastLat = "0.0000";
String lastLng = "0.0000";

// GSM lock flag
volatile bool isSendingAccidentData = false;

void setup() {
  Serial.begin(9600);
  btSerial.begin(9600);

  pinMode(relayPin, OUTPUT);
  pinMode(btStatusLed, OUTPUT); // Corrected
  pinMode(relayStatusLed, OUTPUT);

  digitalWrite(relayPin, LOW);
  digitalWrite(btStatusLed, LOW); // Corrected
  digitalWrite(relayStatusLed, LOW);

  Serial.println(F("System Ready"));
}

void loop() {
  handleBluetoothCommunication();
  checkConnectionStatus();

  // Monitoring only if not sending accident data
  if (!isSendingAccidentData && millis() - lastMonitoringTime >= MONITORING_INTERVAL) {
    sendMonitoringData();
    lastMonitoringTime = millis();
  }
}

void handleBluetoothCommunication() {
  if (btSerial.available()) {
    String received = btSerial.readStringUntil('\n');
    received.trim();

    Serial.print(F("Received Bluetooth: "));
    Serial.println(received);

    if (received.equals("CONNECTED")) {
      lastHeartbeatTime = millis();
      digitalWrite(btStatusLed, HIGH); // Corrected
      Serial.println(F("Bluetooth Connected"));

    } else if (received.startsWith("ACCIDENT,")) {
      Serial.println(F("ALERT: Accident Detected!"));
      isSendingAccidentData = true;

      int firstComma = received.indexOf(',');
      int secondComma = received.indexOf(',', firstComma + 1);
      int thirdComma = received.indexOf(',', secondComma + 1);
      int fourthComma = received.indexOf(',', thirdComma + 1);

      if (firstComma != -1 && secondComma != -1 && thirdComma != -1) {
        String lat = received.substring(firstComma + 1, secondComma);
        String lng = received.substring(secondComma + 1, thirdComma);
        String speed = received.substring(thirdComma + 1, fourthComma != -1 ? fourthComma : received.length());
        String severity = fourthComma != -1 ? received.substring(fourthComma + 1) : "unknown";

        lat.trim();
        lng.trim();
        speed.trim();
        severity.trim();

        Serial.print(F("Lat: "));
        Serial.println(lat);
        Serial.print(F("Lng: "));
        Serial.println(lng);
        Serial.print(F("Speed: "));
        Serial.println(speed);
        Serial.print(F("Severity: "));
        Serial.println(severity);

        String gpsData = lat + "," + lng + "," + speed + "," + severity;
        sendToThingSpeak(gpsData);
        isSendingAccidentData = false;
      } else {
        Serial.println(F("ERROR: Invalid ACCIDENT data format."));
        isSendingAccidentData = false;
      }

    } else if (received.startsWith("GPS:")) {
      String gpsData = received.substring(4);
      Serial.print(F("GPS Data Received: "));
      Serial.println(gpsData);

      // Parse GPS data (lat,lng)
      int comma = gpsData.indexOf(',');
      if (comma != -1) {
        lastLat = gpsData.substring(0, comma);
        lastLng = gpsData.substring(comma + 1);
        lastLat.trim();
        lastLng.trim();
        Serial.print(F("Updated lastLat: "));
        Serial.println(lastLat);
        Serial.print(F("Updated lastLng: "));
        Serial.println(lastLng);
      }

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
    digitalWrite(btStatusLed, LOW); // Corrected
  }
}

bool sendCommand(const char* command, const char* expected, unsigned long timeout) {
  flushGSM();
  gsmSerial.println(command);
  Serial.println(command);

  unsigned long start = millis();
  String response = "";

  while (millis() - start < timeout) {
    while (gsmSerial.available()) {
      char c = gsmSerial.read();
      response += c;
      Serial.print(c);
      if (response.indexOf(expected) != -1) {
        flushGSM();
        return true;
      }
    }
  }

  Serial.print(F("Timeout or Unexpected Response: "));
  Serial.println(response);
  flushGSM();
  return false;
}

void flushGSM() {
  while (gsmSerial.available()) {
    gsmSerial.read();
  }
}

void stopGSM() {
  Serial.println(F("Stopping GSM serial communication..."));
  gsmSerial.end();
  Serial.println(F("GSM serial stopped."));
}

void sendSMSLocation(const String& lat, const String& lng) {
  Serial.println(F("Pausing Bluetooth for SMS..."));
  btSerial.end();
  gsmSerial.begin(9600);
  Serial.println(F("Bluetooth paused, GSM started for SMS."));

  // Reset GSM module
  sendCommand("ATZ", "OK", 2000); // Soft reset
  delay(2000); // Allow module to stabilize

  Serial.print(F("Sending SMS to "));
  Serial.println(phoneNumber);

  // Initialize GSM
  if (!sendCommand("AT", "OK", 2000)) {
    Serial.println(F("GSM not responding."));
    stopGSM();
    restartBluetooth();
    return;
  }

  // Set SMS text mode
  if (!sendCommand("AT+CMGF=1", "OK", 2000)) {
    Serial.println(F("Failed to set SMS mode."));
    stopGSM();
    restartBluetooth();
    return;
  }

  // Start SMS
  flushGSM();
  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(phoneNumber);
  gsmSerial.println("\"");
  Serial.print(F("AT+CMGS=\""));
  Serial.print(phoneNumber);
  Serial.println(F("\""));
  delay(2000); // Increased from 1000

  // Wait for prompt
  if (!waitForResponse(">", 10000)) { // Increased from 5000
    Serial.println(F("Failed to get SMS prompt."));
    stopGSM();
    restartBluetooth();
    return;
  }

  // Send SMS content
  gsmSerial.print("Alert!!\nAn Accident has been Detected!\nLocation: https://maps.google.com/?q=");
  gsmSerial.print(lat);
  gsmSerial.print(",");
  gsmSerial.println(lng);
  Serial.print(F("SMS Content: Alert!!\nAn Accident has been Detected!\nLocation: https://maps.google.com/?q="));
  Serial.print(lat);
  Serial.print(",");
  Serial.println(lng);

  // Send Ctrl+Z
  gsmSerial.write(26);
  Serial.println(F("Sent Ctrl+Z"));

  // Wait for SMS confirmation
  if (!waitForResponse("OK", 10000)) {
    Serial.println(F("Failed to send SMS."));
    stopGSM();
    restartBluetooth();
    return;
  }

  Serial.println(F("Message sent."));
  delay(2000);
  stopGSM();
  restartBluetooth();
}

bool waitForResponse(String expected, unsigned long timeout) {
  unsigned long start = millis();
  String response = "";

  while (millis() - start < timeout) {
    while (gsmSerial.available()) {
      char c = gsmSerial.read();
      response += c;
      Serial.print(c);
      if (response.indexOf(expected) != -1) {
        Serial.print(F("Received expected response: "));
        Serial.println(response);
        flushGSM();
        return true;
      }
    }
  }

  Serial.print(F("WaitForResponse Timeout: "));
  Serial.println(response);
  flushGSM();
  return false;
}

void sendToThingSpeak(const String& gpsData) {
  Serial.println(F("Pausing Bluetooth for accident data..."));
  btSerial.end();
  gsmSerial.begin(9600);
  Serial.println(F("Bluetooth paused, GSM started for accident data."));

  // Parse GPS data
  int firstComma = gpsData.indexOf(',');
  int secondComma = gpsData.indexOf(',', firstComma + 1);
  int thirdComma = gpsData.indexOf(',', secondComma + 1);

  if (firstComma == -1 || secondComma == -1 || thirdComma == -1) {
    Serial.println(F("Invalid GPS data format"));
    stopGSM();
    restartBluetooth();
    sendSMSLocation(gpsData.substring(0, firstComma), gpsData.substring(firstComma + 1, secondComma));
    return;
  }

  String lat = gpsData.substring(0, firstComma);
  String lng = gpsData.substring(firstComma + 1, secondComma);
  String speedRaw = gpsData.substring(secondComma + 1, thirdComma);
  String severity = gpsData.substring(thirdComma + 1);

  lat.trim();
  lng.trim();
  speedRaw.trim();
  severity.trim();

  int spaceIndex = speedRaw.indexOf(' ');
  String speed = (spaceIndex != -1) ? speedRaw.substring(0, spaceIndex) : speedRaw;

  Serial.println(F("Initializing GSM for Accident Data..."));

  if (!sendCommand("AT+CSQ", "+CSQ", 2000)) {
    Serial.println(F("Failed to get signal quality"));
    stopGSM();
    restartBluetooth();
    sendSMSLocation(lat, lng);
    return;
  }

  const int MAX_RETRIES = 3;
  int retryCount = 0;
  bool sentOK = false;

  while (retryCount < MAX_RETRIES && !sentOK) {
    retryCount++;
    Serial.print(F("Attempt "));
    Serial.print(retryCount);
    Serial.print(F(" of "));
    Serial.println(MAX_RETRIES);

    if (!sendCommand("AT", "OK", 2000)) continue;
    if (!sendCommand("AT+CPIN?", "READY", 5000)) continue;
    if (!sendCommand("AT+CREG?", "0,1", 5000)) continue;
    if (!sendCommand("AT+CGATT?", "1", 5000)) continue;
    if (!sendCommand("AT+CIPSHUT", "SHUT OK", 5000)) continue;
    if (!sendCommand("AT+CIPMUX=0", "OK", 2000)) continue;
    if (!sendCommand("AT+CSTT=\"web.mtn.co.ug\"", "OK", 3000)) continue;
    if (!bringUpConnection()) continue;

    gsmSerial.println(F("AT+CIFSR"));
    delay(2000);
    flushGSM();

    if (!sendCommand("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",\"80\"", "CONNECT OK", 10000)) continue;
    delay(3000);

    String getRequest = String(F("GET /update?api_key=")) + accidentApiKey +
                        String(F("&field1=")) + lat +
                        String(F("&field2=")) + lng +
                        String(F("&field3=")) + speed +
                        String(F("&field4=")) + helmetID +
                        String(F("&field5=")) + severity +
                        String(F(" HTTP/1.1\r\nHost: api.thingspeak.com\r\nConnection: close\r\nAccept: text/plain\r\n\r\n"));

    Serial.print(F("Accident GET Request: "));
    Serial.println(getRequest);

    String cipSend = String(F("AT+CIPSEND=")) + String(getRequest.length());
    if (!sendCommand(cipSend.c_str(), ">", 5000)) continue;

    flushGSM(); // Clear buffer before sending
    gsmSerial.print(getRequest);
    delay(2000);
    gsmSerial.write(26); // Ctrl+Z

    unsigned long start = millis();
    String fullResponse = "";
    bool sendOkReceived = false;

    while (millis() - start < 20000 && !sentOK) { // Increased to 20s
      if (gsmSerial.available()) {
        String response = "";
        while (gsmSerial.available()) { // Read all available data
          char c = gsmSerial.read();
          response += c;
        }
        fullResponse += response;
        Serial.print(F("ThingSpeak Accident Response: "));
        Serial.println(response);

        // Check for successful POST
        if (response.indexOf("HTTP/1.1 200 OK") != -1) {
          // Parse response body for entry ID
          int bodyStart = fullResponse.indexOf("\r\n\r\n");
          if (bodyStart != -1) {
            String body = fullResponse.substring(bodyStart + 4);
            body.trim();
            if (body.length() > 0 && body != "0") {
              sentOK = true;
              Serial.print(F("Data posted successfully. Entry ID: "));
              Serial.println(body);
            } else if (body == "0") {
              Serial.println(F("Data not posted (ThingSpeak returned 0)."));
            }
          }
        } else if (response.indexOf("SEND OK") != -1) {
          sendOkReceived = true; // Mark SEND OK but wait for HTTP response
        } else if (response.indexOf("429") != -1) {
          Serial.println(F("Rate limit exceeded!"));
          break; // No retry on rate limit
        } else if (response.indexOf("400") != -1) {
          Serial.println(F("Bad request! Check API key."));
          break; // No retry on bad request
        } else if (response.indexOf("CLOSED") != -1 && sendOkReceived) {
          // Fallback: Assume success if SEND OK and connection closed
          sentOK = true;
          Serial.println(F("Assuming data posted (SEND OK and CLOSED received)."));
        }
      }
    }

    delay(2000);
    sendCommand("AT+CIPSHUT", "SHUT OK", 5000);

    if (sentOK) {
      break; // Exit retry loop on success
    }
  }

  if (!sentOK) {
    Serial.println(F("Failed to send accident data after "));
    Serial.print(MAX_RETRIES);
    Serial.println(F(" attempts."));
  } else {
    Serial.println(F("Accident data sent successfully."));
  }

  // Always send SMS regardless of ThingSpeak success
  Serial.println(F("Now sending SMS..."));
  sendSMSLocation(lat, lng);
  delay(2000);
  flushGSM();

  stopGSM();
  restartBluetooth();
}

void sendMonitoringData() {
  Serial.println(F("Pausing Bluetooth for monitoring data..."));
  btSerial.end();
  gsmSerial.begin(9600);
  Serial.println(F("Bluetooth paused, GSM started for monitoring data."));

  Serial.println(F("Initializing GSM for Monitoring Data..."));

  if (!sendCommand("AT+CSQ", "+CSQ", 3000)) {
    Serial.println(F("Failed to get signal quality"));
    stopGSM();
    restartBluetooth();
    return;
  }

  const int MAX_RETRIES = 3;
  int retryCount = 0;
  bool sentOK = false;

  while (retryCount < MAX_RETRIES && !sentOK) {
    retryCount++;
    Serial.print(F("Attempt "));
    Serial.print(retryCount);
    Serial.print(F(" of "));
    Serial.println(MAX_RETRIES);

    stopGSM();
    gsmSerial.begin(9600);
    delay(2000);

    if (!sendCommand("AT", "OK", 3000)) continue;
    if (!sendCommand("AT+CPIN?", "READY", 5000)) continue;
    if (!sendCommand("AT+CREG?", "0,1", 5000)) continue;
    if (!sendCommand("AT+CGATT=1", "OK", 10000)) continue;
    if (!sendCommand("AT+CIPSHUT", "SHUT OK", 5000)) continue;
    if (!sendCommand("AT+CIPMUX=0", "OK", 3000)) continue;
    if (!sendCommand("AT+C dxSTT=\"web.mtn.co.ug\"", "OK", 5000)) continue;
    if (!bringUpConnection()) continue;

    gsmSerial.println(F("AT+CIFSR"));
    delay(3000);
    flushGSM();

    if (!sendCommand("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",\"80\"", "CONNECT OK", 15000)) continue;
    delay(3000);

    String battery = "85";
    String temperature = "25";
    String status = "active";
    String signalStrength = "-70";

    Serial.print(F("Monitoring lat: "));
    Serial.println(lastLat);
    Serial.print(F("Monitoring lng: "));
    Serial.println(lastLng);
    Serial.print(F("Monitoring battery: "));
    Serial.println(battery);
    Serial.print(F("Monitoring temperature: "));
    Serial.println(temperature);
    Serial.print(F("Monitoring status: "));
    Serial.println(status);
    Serial.print(F("Monitoring signal strength: "));
    Serial.println(signalStrength);
    Serial.print(F("Monitoring Helmet ID: "));
    Serial.println(helmetID);

    String getRequest = String(F("GET /update?api_key=")) + monitoringApiKey +
                        String(F("&field1=")) + lastLat +
                        String(F("&field2=")) + lastLng +
                        String(F("&field3=")) + battery +
                        String(F("&field4=")) + helmetID +
                        String(F("&field5=")) + temperature +
                        String(F("&field6=")) + status +
                        String(F("&field7=")) + signalStrength +
                        String(F(" HTTP/1.1\r\nHost: api.thingspeak.com\r\nConnection: close\r\n\r\n"));

    Serial.print(F("Monitoring GET Request: "));
    Serial.println(getRequest);

    String cipSend = String(F("AT+CIPSEND=")) + String(getRequest.length());
    if (!sendCommand(cipSend.c_str(), ">", 5000)) continue;

    gsmSerial.print(getRequest);
    delay(3000);
    gsmSerial.write(26);

    unsigned long start = millis();
    String fullResponse = "";

    while (millis() - start < 15000 && !sentOK) {
      if (gsmSerial.available()) {
        String response = gsmSerial.readString();
        fullResponse += response;
        Serial.print(F("ThingSpeak Monitoring Response: "));
        Serial.println(response);
        if (response.indexOf("SEND OK") != -1 || response.indexOf("HTTP/1.1 200") != -1) {
          sentOK = true;
        }
        if (response.indexOf("429") != -1) {
          Serial.println(F("Rate limit exceeded!"));
          break;
        }
        if (response.indexOf("400") != -1) {
          Serial.println(F("Bad request! Check API key."));
          break;
        }
      }
    }

    delay(3000);
    sendCommand("AT+CIPSHUT", "SHUT OK", 5000);
  }

  if (!sentOK) {
    Serial.println(F("Failed to send monitoring data after "));
    Serial.print(MAX_RETRIES);
    Serial.println(F(" attempts."));
  } else {
    Serial.println(F("Monitoring data sent successfully."));
  }

  stopGSM();
  restartBluetooth();
}

bool bringUpConnection() {
  for (int i = 0; i < 3; i++) {
    Serial.print(F("Attempt "));
    Serial.print(i + 1);
    Serial.println(F(" to bring up wireless connection (AT+CIICR)..."));
    flushGSM();
    if (sendCommand("AT+CIICR", "OK", 10000)) {
      Serial.println(F("AT+CIICR succeeded."));
      return true;
    }
    delay(3000);
  }
  Serial.println(F("Failed to bring up wireless connection after retries."));
  return false;
}

void restartBluetooth() {
  Serial.println(F("Resuming Bluetooth connection..."));
  btSerial.begin(9600);
  btSerial.println("Ready again");
  Serial.println(F("Bluetooth serial resumed."));
}
