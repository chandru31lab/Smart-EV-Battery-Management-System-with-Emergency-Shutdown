Absolutely. For a **recruiter-friendly GitHub README**, I would position this project around **embedded systems, EV electronics, ECU architecture, CAN communication, battery safety, motor control, and hardware integration** rather than making it read like an academic report.

Below is a complete README you can directly use in the repository. It is based on the full project report you uploaded. 

````markdown
# ⚡ Smart EV Battery Management System

### Controlled Emergency Operation & Safe Shutdown Mechanism

> An embedded EV battery management and motor-control system using dual ECUs, CAN communication, real-time sensing, thermal protection, and an IoT-enabled vehicle dashboard.

---

## 🚗 Project Overview

The **Smart EV Battery Management System** is an embedded system developed for a small electric vehicle prototype to improve **battery safety, real-time monitoring, motor control, and fault handling**.

The system uses a **dual-ECU architecture**:

- **ESP32 ECU** – Battery sensor monitoring, data acquisition, MQTT communication, and dashboard control
- **STM32 ECU** – Motor control, thermal protection, PWM generation, and encoder feedback processing

The two ECUs communicate through a **500 kbps CAN bus**, providing a reliable communication link between the battery monitoring and motor-control subsystems.

A web-based **NEXUS EV Dashboard** provides real-time battery telemetry, motor information, drive-mode selection, manual control, and emergency alerts through MQTT and WebSocket communication.

The system was designed around a **3S LiPo battery pack** and implements protection against:

- 🌡️ Over-temperature
- ⚡ Overcurrent
- 🔋 Overvoltage
- 🔻 Undervoltage
- 🚨 Emergency motor shutdown
- 🔋 Low battery condition

---

## 🎯 Why We Built It

Many low-cost EV prototypes rely mainly on basic battery protection circuits that can disconnect the battery during unsafe conditions but provide limited communication, diagnostics, or coordinated motor control.

This project addresses that limitation by creating an integrated architecture where:

```text
Battery → Sensors → ESP32 ECU → CAN → STM32 ECU → Motor
                     ↓                    ↓
                   MQTT                Protection
                     ↓
              NEXUS Dashboard
````

The system therefore combines:

**Battery Monitoring + Embedded Control + CAN + Motor Control + IoT + Safety Logic**

---

# 🧠 System Architecture

```text
                         ┌─────────────────────────┐
                         │      3S LiPo Battery    │
                         │     11.1V Nominal       │
                         │     12.6V Fully Charged │
                         └────────────┬────────────┘
                                      │
                  ┌───────────────────┴──────────────────┐
                  │                                      │
                  ▼                                      ▼
        ┌───────────────────┐                  ┌──────────────────┐
        │ Battery Sensors   │                  │  Buck Converter  │
        │                   │                  │                  │
        │ Voltage Divider   │                  │  5V / 3.3V Rails │
        │ ACS712 Current    │                  └────────┬─────────┘
        │ DS18B20 Temp      │                           │
        └─────────┬─────────┘                           │
                  │                                     │
                  ▼                                     │
        ┌─────────────────────────┐                     │
        │       ESP32 ECU         │◄────────────────────┘
        │                         │
        │ • Sensor Monitoring     │
        │ • SOC Estimation        │
        │ • MQTT Communication    │
        │ • Dashboard Control     │
        │ • CAN Transmission      │
        └────────────┬────────────┘
                     │
                  CAN BUS
                  500 kbps
                     │
                     ▼
        ┌─────────────────────────┐
        │       STM32 ECU         │
        │                         │
        │ • CAN Reception         │
        │ • Motor Control         │
        │ • PWM Generation        │
        │ • Thermal Protection    │
        │ • Emergency Shutdown    │
        │ • Encoder Processing    │
        └────────────┬────────────┘
                     │
                     ▼
              ┌───────────────┐
              │ TB6612FNG H-Bridge │
              └───────┬───────┘
                      │
                      ▼
              ┌───────────────┐
              │ Encoder Motor │
              └───────┬───────┘
                      │
                  Encoder
                      │
                      ▼
                  ESP32 ECU


             ESP32 ↔ MQTT ↔ EMQX
                         │
                         ▼
               ┌──────────────────┐
               │ NEXUS EV Dashboard│
               │                  │
               │ • Battery Status │
               │ • Current        │
               │ • Temperature    │
               │ • RPM / Speed    │
               │ • Drive Modes    │
               │ • PWM Control    │
               │ • Direction      │
               │ • Fault Alerts   │
               └──────────────────┘
```

---

# 🔩 Hardware Architecture

## 1. ESP32 ECU

The ESP32 acts as the **battery monitoring and vehicle communication ECU**.

### Responsibilities

* Read battery voltage
* Measure battery current
* Measure battery temperature
* Calculate motor RPM
* Estimate State of Charge
* Communicate with the STM32 through CAN
* Publish telemetry through MQTT
* Receive dashboard commands
* Relay control commands to the STM32

---

## 2. STM32 ECU

The STM32 acts as the **real-time motor-control ECU**.

### Responsibilities

* Receive commands through CAN
* Generate motor PWM
* Control motor direction
* Apply thermal derating
* Detect emergency temperature conditions
* Disable motor during critical faults
* Interface with the TB6612FNG motor driver

This separation allows the system to divide responsibilities between:

```text
ESP32 → Monitoring + Connectivity
STM32 → Deterministic Motor Control + Safety
```

---

# 📡 Communication Architecture

The project uses two communication layers.

### CAN Bus

Used for communication between the two ECUs.

**Configuration:**

* Protocol: CAN
* Bit rate: 500 kbps
* Interface: MCP2515
* Physical signals: CAN_H / CAN_L
* Termination: 120 Ω at each end

CAN was selected because it provides differential signalling and is well suited for communication in electrically noisy motor-control environments.

### MQTT

Used for IoT telemetry and dashboard communication.

```text
ESP32
  │
  │ MQTT
  ▼
EMQX Broker
  │
  │ WebSocket
  ▼
NEXUS EV Dashboard
```

---

# 📊 Sensors & Instrumentation

| Component       | Specification      | Purpose                     |
| --------------- | ------------------ | --------------------------- |
| Voltage Divider | 10 kΩ / 2.2 kΩ     | Battery voltage measurement |
| ACS712-30A      | ±30 A, 66 mV/A     | Battery current measurement |
| DS18B20         | 12-bit, 1-Wire     | Battery surface temperature |
| Encoder Motor   | Quadrature encoder | Motor speed / RPM feedback  |

The ESP32 polls the sensor data at approximately **100 ms intervals**.

---

# 🔋 Battery Configuration

The prototype uses a:

**3S LiPo Battery Pack**

| Parameter                   |  Value |
| --------------------------- | -----: |
| Configuration               |     3S |
| Nominal Voltage             | 11.1 V |
| Fully Charged               | 12.6 V |
| Minimum Voltage             |  9.0 V |
| Continuous Discharge Rating |    35C |

The voltage boundaries are monitored continuously to prevent unsafe operating conditions.

---

# 🌡️ Three-Zone Thermal Management

One of the key features of the project is the **three-zone thermal protection strategy**.

Instead of immediately shutting down the motor when temperature rises, the system progressively reduces motor power.

### 🟢 Zone 1 — Normal

**Temperature < 40°C**

```text
Commanded PWM
      ↓
Motor operates normally
```

The full commanded PWM is allowed.

---

### 🟡 Zone 2 — Warning / Derating

**40°C ≤ Temperature ≤ 55°C**

The STM32 progressively reduces the motor PWM based on temperature.

```text
Higher Temperature
        ↓
Higher PWM Reduction
        ↓
Lower Motor Output
        ↓
Reduced Battery Heating
```

This allows the vehicle to continue operating while reducing thermal stress.

---

### 🔴 Zone 3 — Emergency

**Temperature > 55°C**

The STM32 immediately disables the motor by pulling the **TB6612FNG STBY pin LOW**.

```text
Temperature > 55°C
        ↓
Emergency Condition
        ↓
STM32 disables motor
        ↓
CAN emergency frame
        ↓
ESP32 publishes fault
        ↓
Dashboard displays alert
```

The motor remains disabled until a **manual reset** is issued.

---

# ⚠️ Battery Protection

The BMS implements multiple protection mechanisms.

### Overvoltage

```text
Pack Voltage > 12.6V
        ↓
Fault
        ↓
Motor Disabled
```

### Undervoltage

```text
Pack Voltage < 9.0V
        ↓
Fault
        ↓
Motor Disabled / PWM Reduced
```

### Overcurrent

The system monitors current using the ACS712-30A sensor.

A sustained overcurrent condition causes the STM32 to reduce motor output and can ultimately trigger an emergency condition.

### Over-temperature

```text
T < 40°C      → Normal
40–55°C       → PWM Derating
T > 55°C      → Emergency Shutdown
```

---

# 🔋 State of Charge Estimation

The system uses a combined:

**Voltage Lookup + Coulomb Counting**

approach.

Approximate voltage-to-SOC mapping:

| Pack Voltage | Approx. SOC |
| -----------: | ----------: |
|       12.6 V |        100% |
|       11.1 V |         50% |
|        9.6 V |         20% |
|        9.0 V |          0% |

During operation, the ESP32 uses current measurements from the ACS712 to estimate charge consumed.

The system also uses battery voltage during idle periods to correct accumulated coulomb-counting error.

---

# 🖥️ NEXUS EV Dashboard

The project includes a browser-based vehicle dashboard for real-time monitoring and control.

### Dashboard Features

* 🔋 Battery percentage
* ⚡ Pack voltage
* 🔌 Current
* 🌡️ Temperature
* 🏎️ Motor RPM
* 🚗 Speed
* 🎛️ PWM control
* 🔄 Motor direction
* 🚦 Drive modes
* 💡 Vehicle indicators
* 📈 Speed history
* 📡 MQTT communication log
* 🚨 Emergency alerts

### Drive Modes

| Mode   | PWM Limit |
| ------ | --------: |
| ECO    | 100 / 255 |
| NORMAL | 180 / 255 |
| SPORT  | 220 / 255 |
| RACE   | 255 / 255 |

---

# 📡 MQTT Topic Architecture

The system uses a structured MQTT topic hierarchy.

### Telemetry

```text
vehicle/telemetry
```

ESP32 publishes JSON telemetry data approximately every **500 ms**.

Example structure:

```json
{
  "voltage": 11.4,
  "current": 2.8,
  "temperature": 37.5,
  "rpm": 1240,
  "soc": 68
}
```

### Emergency

```text
vehicle/emergency
```

Used for fault notifications such as:

```text
OVERHEAT
OVERCURRENT
UNDERVOLTAGE
```

### Control

```text
vehicle/control/pwm
vehicle/control/direction
vehicle/control/frequency
vehicle/control/...
```

The dashboard publishes commands, which are received by the ESP32 and forwarded to the STM32 through CAN.

---

# 🔄 Complete Data Flow

### Battery → Dashboard

```text
Battery
   ↓
Voltage / Current / Temperature Sensors
   ↓
ESP32
   ↓
MQTT
   ↓
EMQX Broker
   ↓
WebSocket
   ↓
NEXUS Dashboard
```

### Dashboard → Motor

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

### Motor Feedback

```text
Motor
  ↓
Quadrature Encoder
  ↓
ESP32
  ↓
RPM Calculation
  ↓
MQTT
  ↓
Dashboard
```

---

# 🛡️ Emergency Shutdown Flow

```text
Battery Temperature
        ↓
    DS18B20
        ↓
      ESP32
        ↓
      CAN
        ↓
      STM32
        ↓
 Temperature > 55°C ?
        │
       YES
        ↓
TB6612FNG STBY = LOW
        ↓
   Motor Shutdown
        ↓
 Emergency CAN Frame
        ↓
      ESP32
        ↓
 vehicle/emergency
        ↓
  NEXUS Dashboard
        ↓
🚨 OVERHEAT DETECTED
```

The shutdown logic prevents automatic motor restart after a critical thermal event.

---

# 🧰 Hardware Components

| Component        | Role                            |
| ---------------- | ------------------------------- |
| ESP32            | Sensor monitoring, MQTT and CAN |
| STM32            | Motor control and protection    |
| MCP2515 × 2      | CAN interface                   |
| 3S LiPo Battery  | Main energy source              |
| ACS712-30A       | Current measurement             |
| DS18B20          | Temperature measurement         |
| Voltage Divider  | Battery voltage measurement     |
| TB6612FNG        | Dual H-bridge motor driver      |
| DC Encoder Motor | Vehicle drive motor             |
| Buck Converter   | 5V / 3.3V power regulation      |

---

# 💻 Software & Technologies

### Embedded

* C / C++
* ESP32
* STM32
* ESP-IDF / Embedded firmware
* STM32 HAL
* PWM
* GPIO
* ADC
* Interrupts
* 1-Wire

### Communication

* CAN
* SPI
* MQTT
* WebSocket
* JSON

### IoT / Dashboard

* HTML5
* CSS3
* JavaScript
* Paho MQTT
* EMQX
* GitHub Pages

### Hardware

* MCP2515
* ACS712
* DS18B20
* TB6612FNG
* DC Encoder Motor
* Buck Converter

---

# 🧪 Testing & Results

The prototype was tested under real operating conditions.

### Key Results

| Parameter                 |                  Result |
| ------------------------- | ----------------------: |
| CAN Speed                 |                500 kbps |
| Sensor Polling            |                  100 ms |
| MQTT Telemetry            |                  500 ms |
| MQTT Round-Trip Latency   |                 ~210 ms |
| Emergency Dashboard Alert |                 ~350 ms |
| CAN Emergency Response    |                   <2 ms |
| MQTT Message Drops        | 0 during 10-minute test |
| Thermal Derating          |   Successfully verified |
| Emergency Shutdown        |   Successfully verified |

---

## 🌡️ Thermal Test

During a 60-second drive test:

```text
Motor Temperature
27°C → 43°C
```

The system entered the warning zone and automatically reduced motor output.

A **15% PWM reduction** was observed during the thermal event.

---

## 🚨 Emergency Test

During deliberate thermal stress testing:

```text
PWM = 220 / 255
       ↓
Temperature > 55°C
       ↓
Motor Shutdown
       ↓
Emergency Alert
```

The dashboard displayed the **OVERHEAT DETECTED** alert and the motor remained disabled until manual reset.

Five consecutive trigger-and-reset cycles were successfully tested.

---

# 👨‍💻 My Contribution

This was a **three-member team project**.

My primary contribution focused on **embedded hardware and firmware integration**.

### I worked on:

* ESP32-based sensor acquisition
* Voltage sensing interface
* Current sensing using ACS712
* Temperature sensing using DS18B20
* ESP32 CAN communication
* ESP32–STM32 ECU integration
* STM32 motor-control integration
* Encoder feedback interface
* Motor driver integration
* Thermal protection logic
* Emergency shutdown mechanism
* Hardware integration and debugging
* System-level testing

### My main engineering focus

```text
Sensors
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
```

This project gave me practical experience in **embedded hardware design, ECU architecture, sensor interfacing, CAN communication, motor control, debugging, and EV safety systems**.

---

# 📈 What I Learned

Through this project, I gained hands-on experience with:

* Designing distributed embedded systems
* Working with multiple microcontrollers
* ECU-to-ECU communication
* CAN bus implementation
* Sensor interfacing
* Motor driver integration
* PWM-based motor control
* Encoder feedback
* Battery monitoring
* Thermal protection
* Fault handling
* Embedded debugging
* MQTT-based IoT systems
* Hardware-software integration
* System-level testing

A major learning from the project was the importance of **separating monitoring, communication, and real-time control responsibilities** across embedded nodes.

---

# ⚠️ Current Limitations

The current prototype has several limitations:

### 1. Aggregate Battery Voltage

The system monitors pack voltage rather than individual cell voltages.

Therefore, cell imbalance cannot be directly detected.

### 2. Public MQTT Broker

The prototype uses a public EMQX broker without production-level security.

This architecture is suitable for demonstration and laboratory testing but requires security improvements for real-world deployment.

### 3. Surface Temperature Measurement

The DS18B20 measures cell surface temperature rather than internal cell temperature.

### 4. SOC Accuracy

The voltage + coulomb-counting approach can accumulate estimation error over long operating cycles.

---

# 🚀 Future Improvements

Planned improvements include:

* [ ] Individual cell voltage monitoring
* [ ] Active/passive cell balancing
* [ ] EKF-based SOC estimation
* [ ] SOH estimation
* [ ] Secure MQTT using TLS
* [ ] Private MQTT broker
* [ ] OTA firmware updates
* [ ] Improved internal battery temperature estimation
* [ ] Multi-motor support
* [ ] Improved thermal modelling
* [ ] Production-level PCB implementation
* [ ] Automotive-grade protection architecture

---

# 🎓 Academic Information

**Project:** 21ECP302L – Project

**Degree:** B.Tech Electronics & Communication Engineering

**Institution:**
SRM Institute of Science and Technology
Kattankulathur, Tamil Nadu, India

**Academic Year:** 2025–2026

### Team

* **Ramachandru J**
* **Shibi S**
* **Parnapalli Anish**

---

# ⭐ Key Takeaways for Recruiters

This project demonstrates practical experience in:

**Embedded Systems**

* ESP32
* STM32
* ADC
* GPIO
* PWM
* Interrupts

**Automotive / EV Electronics**

* ECU architecture
* Battery monitoring
* Motor control
* Fault protection
* Encoder feedback

**Communication**

* CAN
* SPI
* MQTT
* WebSocket

**Hardware Integration**

* Current sensing
* Voltage sensing
* Temperature sensing
* Motor driver
* Power regulation

**Software**

* Embedded C/C++
* MQTT
* JSON
* JavaScript
* Real-time dashboard

---

## 🔗 Project Resources

* 📄 [Project Report](./minor-final-report.pdf)
* 🌐 [NEXUS EV Dashboard](https://anishparnapalli.github.io/Nexsus-EV-Dashboard/)

---

## 📌 Project Summary

> **A dual-ECU Smart EV Battery Management System that combines real-time battery sensing, CAN-based ECU communication, intelligent thermal derating, motor control, emergency shutdown, and MQTT-enabled vehicle telemetry into a single embedded EV platform.**

---

## 👤 Author

### Ramachandru J

**B.Tech Electronics & Communication Engineering | Embedded Systems | EV Electronics | IoT**

Interested in building reliable embedded hardware and intelligent electronic systems for **automotive, EV, industrial, and IoT applications**.

```seconds.
```
