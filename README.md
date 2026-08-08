# Smart EV Battery Management System

### Controlled Emergency Operation & Safe Shutdown Mechanism

> **A dual-ECU embedded EV battery management system that combines real-time battery monitoring, CAN-based ECU communication, intelligent thermal protection, motor control, emergency shutdown, and MQTT-enabled vehicle telemetry.**

[![Platform](https://img.shields.io/badge/Platform-ESP32%20%7C%20STM32-blue)](https://www.espressif.com/)
[![Embedded](https://img.shields.io/badge/Embedded-C%20%7C%20C%2B%2B-red)](https://www.arduino.cc/)
[![Communication](https://img.shields.io/badge/Communication-CAN-green)](https://www.can-cia.org/)
[![IoT](https://img.shields.io/badge/IoT-MQTT%20%7C%20EMQX-purple)](https://www.emqx.com/)
[![Battery](https://img.shields.io/badge/Battery-3S%20LiPo-orange)]()
[![Domain](https://img.shields.io/badge/Domain-EV%20%7C%20BMS-black)]()

---

## 📌 Overview

The **Smart EV Battery Management System with Controlled Emergency Operation and Safe Shutdown Mechanism** is an embedded EV prototype designed to improve **battery safety, real-time monitoring, motor control, and fault response**.

The system uses a **dual-ECU architecture**, where each microcontroller is assigned a dedicated role:

- 🔋 **ESP32 ECU** – Battery sensor monitoring, data acquisition, SOC estimation, MQTT communication, and dashboard control
- ⚙️ **STM32 ECU** – Motor control, PWM generation, thermal protection, emergency shutdown, and encoder feedback processing

The two ECUs communicate through a **500 kbps CAN bus**, while the ESP32 communicates with an **EMQX MQTT broker** to provide live telemetry and remote control through the **NEXUS EV Dashboard**.

The system continuously monitors:

* 🔋 Battery voltage
* ⚡ Battery current
* 🌡️ Battery temperature
* 🏎️ Motor RPM
* 🔋 State of Charge
* 🚨 Fault conditions

The BMS can automatically respond to unsafe conditions through **PWM derating or complete motor shutdown**.

---

# 🎯 Problem

Small EV prototypes often use basic battery protection circuits that provide only simple voltage cutoff functionality.

Such systems may not provide:

* Real-time battery monitoring
* Motor-controller coordination
* Current monitoring
* Temperature-based motor control
* Remote telemetry
* Fault visualization
* Intelligent emergency response

This creates a gap between **battery protection and vehicle control**.

For example:

```text
Battery Overheating
        ↓
Basic Protection Circuit
        ↓
Battery Cutoff
````

The proposed system instead provides:

```text
Battery Condition
        ↓
Sensor Monitoring
        ↓
ESP32 ECU
        ↓
CAN
        ↓
STM32 ECU
        ↓
Controlled Motor Response
        ↓
Dashboard Alert
```

The goal is to make the battery management system an **active part of the vehicle control architecture** rather than an isolated protection circuit.

---

# 💡 Proposed Solution

The system integrates battery monitoring and motor control using two embedded ECUs.

### ESP32 ECU

The ESP32 acts as the **Battery Monitoring & Vehicle Communication ECU**.

It performs:

1. Battery voltage measurement
2. Battery current measurement
3. Temperature measurement
4. Motor RPM calculation
5. SOC estimation
6. MQTT communication
7. Dashboard command reception
8. CAN communication with STM32

### STM32 ECU

The STM32 acts as the **Real-Time Motor Control ECU**.

It performs:

1. CAN command reception
2. PWM generation
3. Motor direction control
4. Thermal derating
5. Emergency shutdown
6. TB6612FNG control
7. Motor control based on battery conditions

---

# 🏗️ System Architecture

```text
                         ┌─────────────────────────┐
                         │       3S LiPo Battery   │
                         │                         │
                         │   11.1V Nominal        │
                         │   12.6V Fully Charged  │
                         │   9.0V Cutoff          │
                         └────────────┬────────────┘
                                      │
                    ┌─────────────────┴─────────────────┐
                    │                                   │
                    ▼                                   ▼
          ┌────────────────────┐              ┌──────────────────┐
          │ Battery Sensors    │              │  Buck Converter  │
          │                    │              │                  │
          │ Voltage Divider    │              │   5V / 3.3V     │
          │ ACS712-30A         │              │   Logic Supply   │
          │ DS18B20            │              └────────┬─────────┘
          └──────────┬─────────┘                       │
                     │                                 │
                     ▼                                 ▼
          ┌──────────────────────────────┐
          │          ESP32 ECU            │
          │                              │
          │ • Voltage Monitoring         │
          │ • Current Monitoring         │
          │ • Temperature Monitoring     │
          │ • RPM Measurement            │
          │ • SOC Estimation             │
          │ • MQTT Communication         │
          │ • Dashboard Control          │
          │ • CAN Transmission           │
          └──────────────┬───────────────┘
                         │
                         │ CAN
                         │ 500 kbps
                         ▼
          ┌──────────────────────────────┐
          │          STM32 ECU            │
          │                              │
          │ • CAN Reception              │
          │ • Motor Control              │
          │ • PWM Generation             │
          │ • Thermal Protection         │
          │ • Emergency Shutdown         │
          │ • Encoder Processing         │
          └──────────────┬───────────────┘
                         │
                         ▼
                  ┌───────────────┐
                  │   TB6612FNG   │
                  │ Motor Driver  │
                  └───────┬───────┘
                          │
                          ▼
                  ┌───────────────┐
                  │ Encoder Motor │
                  └───────┬───────┘
                          │
                          │ Encoder Feedback
                          ▼
                       ESP32 ECU


             ESP32
                │
                │ MQTT
                ▼
          ┌─────────────┐
          │    EMQX     │
          │ MQTT Broker │
          └──────┬──────┘
                 │
                 │ WebSocket
                 ▼
       ┌──────────────────────┐
       │  NEXUS EV Dashboard  │
       │                      │
       │ • Battery Status     │
       │ • Voltage            │
       │ • Current            │
       │ • Temperature        │
       │ • RPM / Speed        │
       │ • Drive Modes        │
       │ • PWM Control        │
       │ • Direction          │
       │ • Emergency Alerts   │
       └──────────────────────┘
```

---

# 🧩 Hardware Components

## Battery & Power

| Component       | Specification           | Purpose                 |
| --------------- | ----------------------- | ----------------------- |
| 3S LiPo Battery | 11.1V, 35C              | Main energy source      |
| Buck Converter  | 9–15V input → 5V / 3.3V | Logic power supply      |
| Voltage Divider | 10kΩ / 2.2kΩ            | Battery voltage sensing |

---

## ESP32 ECU

| Component       | Purpose                                |
| --------------- | -------------------------------------- |
| ESP32           | Sensor acquisition + IoT communication |
| ACS712-30A      | Battery current measurement            |
| DS18B20         | Battery temperature measurement        |
| Voltage Divider | Pack voltage measurement               |
| MCP2515         | CAN communication                      |
| Encoder Input   | Motor RPM measurement                  |

---

## STM32 ECU

| Component         | Purpose                    |
| ----------------- | -------------------------- |
| STM32             | Real-time motor controller |
| MCP2515           | CAN communication          |
| TB6612FNG         | Motor driving              |
| Encoder Interface | Motor feedback             |
| PWM Timer         | Motor speed control        |

---

# 🔋 Battery Configuration

The prototype uses a **3S LiPo battery pack**.

| Parameter            |  Value |
| -------------------- | -----: |
| Configuration        |     3S |
| Nominal Voltage      | 11.1 V |
| Fully Charged        | 12.6 V |
| Minimum Voltage      |  9.0 V |
| Continuous Discharge |    35C |

The system monitors the aggregate battery voltage and uses software-defined voltage boundaries for protection.

---

# 📊 Battery Monitoring

Three primary battery parameters are monitored.

### Voltage

A resistor divider scales the battery voltage into the ESP32 ADC input range.

```text
Battery
   ↓
10kΩ / 2.2kΩ Divider
   ↓
ESP32 ADC
   ↓
Pack Voltage
```

### Current

The **ACS712-30A Hall-effect current sensor** measures the battery current.

```text
Battery
   ↓
ACS712
   ↓
Motor Driver
```

The sensor provides a ratiometric output centered around VCC/2.

### Temperature

The **DS18B20** is mounted directly on the battery cell surface.

```text
Battery Cell
     ↓
DS18B20
     ↓
1-Wire
     ↓
ESP32
```

---

# 🧠 Dual-ECU Architecture

One of the key design aspects of this project is the separation of monitoring and real-time motor control.

## ESP32 ECU — Monitoring & Connectivity

```text
Sensors
   ↓
ESP32
   ├── Battery Monitoring
   ├── SOC Estimation
   ├── RPM Calculation
   ├── MQTT
   ├── Dashboard
   └── CAN
```

The ESP32 handles connectivity-heavy tasks while collecting real-time battery data.

---

## STM32 ECU — Motor Control & Safety

```text
CAN
 ↓
STM32
 ├── PWM
 ├── Direction
 ├── Thermal Protection
 ├── Emergency Shutdown
 └── Motor Driver
```

The STM32 handles the real-time motor-control functions independently of the web dashboard.

### Design Philosophy

> **ESP32 → Monitoring + Connectivity**

> **STM32 → Deterministic Motor Control + Safety**

This architecture separates high-level communication from real-time actuation.

---

# 📡 CAN Communication

CAN provides communication between the ESP32 ECU and STM32 ECU.

### Configuration

```text
Protocol       → CAN
Bit Rate       → 500 kbps
Interface      → MCP2515
Physical Bus   → CAN_H / CAN_L
Termination    → 120Ω
```

The CAN frame carries motor-control information such as:

```text
PWM Duty Cycle
Motor Frequency
Motor Direction
```

The ESP32 transmits the required motor command and the STM32 decodes the frame to control the motor.

---

# 🌡️ Three-Zone Thermal Management

The thermal-management system is one of the main safety features of the project.

Instead of immediately shutting down the motor when temperature increases, the system first **derates the motor output**.

---

## 🟢 Zone 1 — Normal

```text
Temperature < 40°C
```

The commanded PWM is applied without modification.

```text
Dashboard PWM
      ↓
STM32
      ↓
Full Motor Output
```

---

## 🟡 Zone 2 — Warning

```text
40°C ≤ Temperature ≤ 55°C
```

The STM32 applies proportional PWM derating.

```text
Temperature Increase
        ↓
PWM Reduction
        ↓
Lower Motor Output
        ↓
Reduced Thermal Stress
```

The system therefore allows continued operation while reducing the thermal load.

---

## 🔴 Zone 3 — Emergency

```text
Temperature > 55°C
```

The motor is immediately disabled.

```text
Temperature > 55°C
        ↓
STM32 Emergency Logic
        ↓
TB6612FNG STBY = LOW
        ↓
Motor Shutdown
        ↓
Emergency CAN Frame
        ↓
ESP32
        ↓
MQTT
        ↓
NEXUS Dashboard
        ↓
🚨 OVERHEAT DETECTED
```

A manual reset is required before the motor can operate again.

---

# ⚠️ Battery Protection

The BMS provides protection against multiple abnormal conditions.

## Overvoltage

```text
Pack Voltage > 12.6V
        ↓
Voltage Fault
        ↓
Motor Protection
        ↓
Dashboard Alert
```

## Undervoltage

```text
Pack Voltage < 9.0V
        ↓
Undervoltage Fault
        ↓
Motor Protection
        ↓
Dashboard Alert
```

## Overcurrent

The ACS712 continuously monitors the main battery current.

A sustained overcurrent condition causes motor PWM reduction and can result in an emergency fault.

## Overtemperature

```text
< 40°C      → Full Power
40–55°C     → PWM Derating
> 55°C      → Emergency Shutdown
```

---

# 🔋 State of Charge Estimation

The project uses a combined **voltage lookup + coulomb counting** approach.

Approximate mapping:

| Pack Voltage |  SOC |
| -----------: | ---: |
|       12.6 V | 100% |
|       11.1 V |  50% |
|        9.6 V |  20% |
|        9.0 V |   0% |

During motor operation, the ESP32 integrates current over time to estimate the charge removed from the battery.

When the motor is idle, voltage-based correction is used to reduce accumulated coulomb-counting error.

---

# 🖥️ NEXUS EV Dashboard

The **NEXUS EV Dashboard** provides a browser-based interface for monitoring and controlling the vehicle.

### Dashboard Features

* 🔋 Battery percentage
* ⚡ Pack voltage
* 🔌 Battery current
* 🌡️ Temperature
* 🏎️ RPM
* 🚗 Speed
* 🎛️ PWM control
* 🔄 Motor direction
* 🚦 Drive modes
* 💡 Vehicle indicators
* 📈 Speed history
* 📡 MQTT communication log
* 🚨 Emergency alerts

---

# 🚗 Drive Modes

The dashboard provides four drive modes.

| Mode   | PWM Limit |
| ------ | --------: |
| ECO    | 100 / 255 |
| NORMAL | 180 / 255 |
| SPORT  | 220 / 255 |
| RACE   | 255 / 255 |

These modes provide different maximum motor-output levels.

---

# 📡 MQTT Communication

The ESP32 communicates with the EMQX broker using MQTT.

### Telemetry Topic

```text
vehicle/telemetry
```

Telemetry is published approximately every **500 ms**.

Example:

```json
{
  "voltage": 11.4,
  "current": 2.8,
  "temperature": 37.5,
  "rpm": 1240,
  "soc": 68
}
```

---

## Emergency Topic

```text
vehicle/emergency
```

Possible fault messages include:

```text
OVERHEAT
OVERCURRENT
UNDERVOLTAGE
```

---

## Control Topics

```text
vehicle/control/pwm
vehicle/control/direction
vehicle/control/frequency
vehicle/control/...
```

The dashboard publishes commands to EMQX.

The ESP32 receives them and sends the corresponding motor-control information to the STM32 through CAN.

---

# 🔄 Complete Control Flow

## Dashboard → Motor

```text
NEXUS Dashboard
       ↓
     MQTT
       ↓
     EMQX
       ↓
     ESP32
       ↓
      CAN
       ↓
     STM32
       ↓
     PWM
       ↓
   TB6612FNG
       ↓
     Motor
```

---

## Motor → Dashboard

```text
Motor
  ↓
Encoder
  ↓
ESP32
  ↓
RPM Calculation
  ↓
MQTT
  ↓
EMQX
  ↓
NEXUS Dashboard
```

---

# 🚨 Emergency Shutdown

The emergency shutdown system is designed to prevent continued motor operation during critical battery conditions.

```text
Battery Condition
       ↓
Sensor Detection
       ↓
ESP32
       ↓
CAN
       ↓
STM32
       ↓
Critical Fault?
       │
      YES
       ↓
Motor Shutdown
       ↓
Emergency CAN Frame
       ↓
ESP32
       ↓
MQTT
       ↓
Dashboard Alert
```

The STM32 controls the motor shutdown locally, reducing dependence on the cloud communication path for the actual safety action.

---

# 🔌 Power Architecture

The LiPo battery supplies two primary branches.

```text
                   3S LiPo Battery
                         │
              ┌──────────┴──────────┐
              │                     │
              ▼                     ▼
         ACS712 Branch         Buck Converter
              │                     │
              ▼                  5V / 3.3V
        TB6612FNG                   │
              │             ┌───────┴───────┐
              ▼             │               │
           Motor           ESP32          STM32
```

The voltage divider is connected across the battery terminals for voltage measurement.

CAN_H and CAN_L connect the two CAN interfaces with **120Ω termination resistors at the physical ends of the bus**.

---

# 💻 Software & Technologies

## Embedded

* C
* C++
* ESP32
* STM32
* ESP-IDF
* STM32 HAL
* ADC
* GPIO
* PWM
* Interrupts
* 1-Wire
* SPI

## Communication

* CAN
* MQTT
* WebSocket
* JSON

## IoT / Dashboard

* HTML5
* CSS3
* JavaScript
* Paho MQTT
* EMQX
* GitHub Pages

## Hardware

* ESP32
* STM32
* MCP2515
* ACS712-30A
* DS18B20
* TB6612FNG
* Encoder Motor
* Buck Converter

---

# 🧪 Testing

The prototype was evaluated under real operating conditions.

### Main Tests

| Test                       | Expected Result                    |
| -------------------------- | ---------------------------------- |
| Battery voltage monitoring | Correct voltage displayed          |
| Current monitoring         | Real-time current measurement      |
| Temperature monitoring     | Real-time battery temperature      |
| Motor control              | PWM-controlled operation           |
| Encoder feedback           | RPM measurement                    |
| CAN communication          | ESP32 ↔ STM32 communication        |
| MQTT telemetry             | Dashboard updates                  |
| Thermal derating           | PWM reduction                      |
| Emergency shutdown         | Motor immediately disabled         |
| Manual reset               | Motor remains disabled until reset |

---

# 📊 Prototype Results

The system achieved the following reported results:

| Parameter                 |                  Result |
| ------------------------- | ----------------------: |
| CAN Communication         |                500 kbps |
| Sensor Polling            |                  100 ms |
| MQTT Telemetry Interval   |                  500 ms |
| MQTT Round-Trip Latency   |                 ~210 ms |
| Emergency Dashboard Alert |                 ~350 ms |
| CAN Emergency Response    |                   <2 ms |
| MQTT Message Drops        | 0 during 10-minute test |
| Thermal Derating          |   Successfully verified |
| Emergency Shutdown        |   Successfully verified |

---

# 🌡️ Thermal Test

During a 60-second drive test:

```text
Initial Temperature
        ↓
       27°C
        ↓
Motor Operation
        ↓
       43°C
        ↓
Zone 2
        ↓
PWM Derating
        ↓
Reduced Motor Output
```

The motor temperature increased from **27°C to 43°C**, entering the warning zone.

The system responded by reducing motor output by approximately **15%**.

This demonstrated that the proportional thermal derating mechanism was functioning.

---

# 🚨 Emergency Test

A deliberate thermal stress test was performed with PWM held at:

```text
220 / 255
```

The battery surface temperature crossed the emergency threshold.

```text
Temperature > 55°C
        ↓
STM32 detects emergency
        ↓
Motor cutoff
        ↓
CAN Emergency Frame
        ↓
ESP32
        ↓
MQTT Emergency Message
        ↓
NEXUS Dashboard
        ↓
OVERHEAT DETECTED
```

The dashboard alert appeared within approximately **350 ms** of the threshold being crossed.

Five consecutive emergency trigger-and-reset cycles were successfully performed.

---

# 📈 Communication Performance

During testing:

```text
MQTT Test Duration
        ↓
10 minutes

Message Drops
        ↓
0

Average Round Trip
        ↓
210 ms
```

The measured latency was considered suitable for the gradual motor-speed control used in the prototype.

However, the MQTT path is not intended to replace a deterministic real-time control loop.

The actual motor protection remains locally implemented in the STM32 ECU.

---

# 👨‍💻 My Contribution

This was a **three-member team project**.

My primary contribution focused on **embedded hardware, firmware, ECU communication, and system integration**.

### I worked on:

* ESP32 sensor acquisition
* Battery voltage sensing
* ACS712 current sensing
* DS18B20 temperature sensing
* ESP32 CAN communication
* ESP32–STM32 ECU integration
* STM32 motor-control integration
* Encoder feedback
* TB6612FNG motor-driver integration
* Thermal protection logic
* Emergency shutdown
* Hardware integration
* Debugging and testing

### My main engineering responsibility

```text
Battery Sensors
      ↓
    ESP32 ECU
      ↓
  CAN Communication
      ↓
    STM32 ECU
      ↓
 Motor Control
      ↓
Encoder Feedback
      ↓
    ESP32 ECU
```

This gave me practical experience in:

**Embedded Hardware + Firmware + ECU Communication + Motor Control + EV Safety**

---

# 🧠 Key Engineering Learnings

Through this project, I gained practical experience in:

* Multi-ECU embedded architecture
* Sensor interfacing
* ADC-based voltage measurement
* Current sensing
* Digital temperature sensing
* CAN bus communication
* PWM motor control
* Encoder feedback
* Motor-driver integration
* Battery monitoring
* Thermal protection
* Fault handling
* Embedded debugging
* MQTT communication
* Hardware-software integration
* System-level testing

A major design learning was that **safety-critical motor decisions should remain local to the real-time ECU**, rather than depending on the cloud or dashboard.

---

# ⚠️ Limitations

The current prototype has several limitations.

### 1. Aggregate Battery Voltage

The current implementation monitors aggregate pack voltage rather than individual cell voltages.

Therefore, individual cell imbalance cannot be directly detected.

### 2. Public MQTT Broker

The prototype uses a public EMQX broker without production-grade security.

### 3. Surface Temperature

The DS18B20 measures the battery surface temperature rather than internal cell temperature.

### 4. SOC Accuracy

The voltage + coulomb-counting method can accumulate error over long operating cycles.

---

# 🔮 Future Development

Potential improvements include:

* [ ] Per-cell voltage monitoring
* [ ] Active cell balancing
* [ ] Passive cell balancing
* [ ] EKF-based SOC estimation
* [ ] SOH estimation
* [ ] TLS-secured MQTT
* [ ] Private MQTT broker
* [ ] OTA firmware updates
* [ ] Improved thermal modelling
* [ ] Internal battery temperature estimation
* [ ] Multi-motor support
* [ ] Production-level PCB implementation
* [ ] Automotive-grade protection architecture

---

# 🏭 Relevance to EV & Embedded Systems

This project provides hands-on exposure to concepts relevant to:

### EV Electronics

* Battery monitoring
* Motor control
* ECU architecture
* Thermal management
* Fault protection

### Embedded Systems

* ESP32
* STM32
* ADC
* PWM
* GPIO
* Interrupts
* SPI
* CAN

### Automotive Communication

* CAN bus
* ECU-to-ECU communication
* Distributed control architecture

### IoT

* MQTT
* EMQX
* WebSocket
* Real-time telemetry

---

# ⭐ Why This Project Is Different

Most basic EV prototypes separate battery monitoring from motor control.

This project creates a coordinated architecture:

```text
             ┌────────────────────┐
             │    Battery Pack    │
             └─────────┬──────────┘
                       ↓
              ┌─────────────────┐
              │    ESP32 ECU    │
              │                 │
              │ Monitoring      │
              │ SOC             │
              │ MQTT            │
              │ Dashboard       │
              └────────┬────────┘
                       │
                      CAN
                       │
                       ▼
              ┌─────────────────┐
              │    STM32 ECU    │
              │                 │
              │ Motor Control   │
              │ PWM             │
              │ Thermal Safety  │
              │ Shutdown        │
              └────────┬────────┘
                       ↓
                    Motor
```

### Key Engineering Concept

> **The ESP32 handles monitoring and connectivity, while the STM32 handles deterministic motor control and safety.**

This separation improves modularity and allows the real-time safety functions to remain independent of the IoT communication layer.

---

# 🚀 Getting Started

## 1. Clone the Repository

```bash
git clone https://github.com/chandru31lab/Smart-EV-BMS.git
cd Smart-EV-BMS
```

## 2. ESP32 Setup

Configure:

* Battery voltage sensor
* ACS712
* DS18B20
* Encoder
* MCP2515

Then upload the ESP32 firmware.

---

## 3. STM32 Setup

Configure:

* MCP2515 / CAN interface
* PWM timer
* TB6612FNG
* Encoder interface
* Thermal protection logic

Then flash the STM32 firmware.

---

## 4. MQTT Setup

Configure the EMQX broker and MQTT topics:

```text
vehicle/telemetry
vehicle/emergency
vehicle/control/...
```

---

## 5. Dashboard Setup

Open the NEXUS dashboard and configure the MQTT/WebSocket connection.

Dashboard:

**NEXUS EV Dashboard**

---

# 📚 Documentation

The complete academic documentation contains:

* Introduction
* Problem Statement
* Objectives
* Literature Survey
* System Architecture
* Hardware Design
* Communication Architecture
* BMS Functions
* Thermal Management
* SOC Estimation
* Dashboard Architecture
* Testing
* Results
* Limitations
* Future Work

The detailed project report is included separately from this README.

---

# 🎓 Academic Information

**Project:** 21ECP302L – Minor Project

**Degree:** B.Tech Electronics & Communication Engineering

**Institution:**
SRM Institute of Science and Technology
Kattankulathur, Tamil Nadu, India

**Academic Year:** 2025–2026

### Team

* **Ramachandru J**
* **Shibi S**
* **Parnapalli Anish**

### Project Guide

**Dr. T. Rajalakshmi**
Associate Professor
Department of Electronics & Communication Engineering
SRM Institute of Science and Technology

---

# 📌 Project Highlights

```text
Dual ECU Architecture
        ↓
ESP32 + STM32

Battery Monitoring
        ↓
Voltage + Current + Temperature

Automotive Communication
        ↓
CAN @ 500 kbps

Motor Control
        ↓
PWM + Encoder + TB6612FNG

Safety
        ↓
Thermal Derating + Emergency Shutdown

IoT
        ↓
MQTT + EMQX + Web Dashboard
```

---

# ⭐ Project Summary

> **A dual-ECU Smart EV Battery Management System that combines real-time battery sensing, CAN-based ECU communication, intelligent thermal derating, motor control, emergency shutdown, and MQTT-enabled vehicle telemetry into a single embedded EV platform.**

This project demonstrates practical experience across **embedded hardware, firmware, automotive communication, motor control, battery management, IoT, and system-level debugging**, making it particularly relevant to **EV, automotive electronics, embedded systems, and hardware engineering roles**.

---

# 👤 Author

## Ramachandru J

**B.Tech Electronics & Communication Engineering**

**Embedded Systems | EV Electronics | IoT | Hardware Design**

Interested in building reliable embedded hardware and intelligent electronic systems for:

* 🚗 Electric Vehicles
* 🔋 Battery Management Systems
* ⚙️ Embedded Systems
* 🔌 Automotive Electronics
* 📡 IoT Systems

---

## 🔗 Project Resources

* 📄 **Project Report:** `Documentation/Project_Report.pdf`
* 🌐 **NEXUS EV Dashboard:** `https://anishparnapalli.github.io/Nexsus-EV-Dashboard/`

---

## ⭐ If you find this project useful

Consider giving the repository a ⭐ and exploring the implementation.

```text
Built with:
ESP32 • STM32 • CAN • MQTT • EMQX • LiPo • PWM • Sensors
```


