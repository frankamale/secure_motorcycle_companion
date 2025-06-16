# 🛡️ Secure Motorcycle Companion

**Secure Motorcycle Companion** is an embedded systems and IoT-based safety solution designed to enhance the safety and accountability of motorcycle riders. This smart system ensures that a motorcycle can only be started when the rider is wearing their helmet and automatically alerts emergency contacts in case of an accident.

## 🔧 Project Overview

This project integrates hardware, software, and cloud technologies to deliver a seamless safety experience. The solution features:

- **Smart Helmet-Controlled Ignition System**
- **Real-Time Accident Detection & Notification**
- **GPS Location Tracking**
- **Web Dashboard for Administrator Oversight**

## 🎯 Core Features

### ✅ Helmet-Dependent Ignition Control
- Motorcycle ignition is electronically locked unless the helmet is worn.
- Uses  Bluetooth for helmet detection.

### 🚨 Accident Detection & Alerts
- Tilt sensors and accelerometers detect sudden impact or abnormal angles.
- Upon detection, the system sends:
  - A real-time **SMS notification**
  - **GPS coordinates** to a registered emergency contact

### 🌐 Administrator Web Dashboard
- Displays real-time data from devices
- Allows registration and management of users and their helmets
- Provides logs of alerts, ride history, and helmet status

## 🔩 Technologies Used

### 🖥️ Embedded Systems
- **Microcontrollers**: Arduino nano, ESP 32
- **Sensors**: Tilt sensor, Accelerometer, touch sensor
- **Communication Modules**: GSM, GPS, Bluetooth
- **Power**: Rechargeable lithium-ion battery
- **Actuation**: Relay module for ignition control

### 🌐 Web Dashboard
- **Frontend**: Next js and Tailwind css
- **Backend**: Node.js
- **Database**: MongoDB
- **Cloud Messaging**: GSM module for SMS alerts

## 🚀 Setup Instructions

### Hardware
1. Assemble helmet with sensors and communication modules.
2. Connect microcontroller to relay and motorcycle ignition circuit.
3. Program using Arduino IDE with appropriate firmware.

### Software (Dashboard)
```bash
git clone https://github.com/frankamale/secure-motorcycle-companion.git

