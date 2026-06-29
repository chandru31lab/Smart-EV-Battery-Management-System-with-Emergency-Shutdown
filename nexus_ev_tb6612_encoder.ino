/*
 * ═══════════════════════════════════════════════════════════════
 *   NEXUS EV DASHBOARD — ESP32 Firmware
 *   TB6612FNG Motor Driver + Quadrature Encoder Feedback
 *   + Voltage Sensor + Current Sensor (ACS712) + DS18B20 Temp
 *
 *   ┌─────────────────────────────────────────────────────────┐
 *   │  WHAT THIS DOES                                         │
 *   │  • Reads quadrature encoder → computes RPM, Speed, Freq │
 *   │  • Reads voltage via ADC voltage divider                │
 *   │  • Reads current via ACS712 current sensor              │
 *   │  • Reads temperature via DS18B20 OneWire sensor         │
 *   │  • THERMAL PROTECTION: reduces PWM to 150 if T > 60°C  │
 *   │  • Publishes sensor data to MQTT every 500 ms           │
 *   │  • Subscribes to MQTT commands (dir, PWM, freq, mode)   │
 *   │  • Drives TB6612FNG motor driver accordingly            │
 *   │  • Serial Monitor commands for manual override / debug  │
 *   └─────────────────────────────────────────────────────────┘
 *
 *   HOW TO SEND VALUES FROM SERIAL MONITOR:
 *   ─────────────────────────────────────────────────────────────
 *   Open Serial Monitor at 115200 baud, line ending = Newline
 *
 *   spd:75.5     override published speed  (km/h)
 *   rpm:3200     override published RPM
 *   curr:18.4    override current  (A)
 *   volt:51.2    override voltage  (V)
 *   temp:45.0    override temperature (°C)
 *   freq:1200    override frequency (Hz)
 *   all:80,3500,20,52,40,900  → set all 6 at once
 *   encoder      switch back to live sensor readings
 *   reset        zero all overrides
 *   status       print all values
 *   help         print command list
 *
 *   BROKER  : jace13a6.ala.asia-southeast1.emqxsl.com
 *   PORT    : 8883 (TLS)
 *   USER    : anish
 *   PASS    : Suneetha@123
 *
 *   LIBRARIES (Tools → Manage Libraries):
 *     1. PubSubClient  — Nick O'Leary
 *     2. ArduinoJson   — Benoit Blanchon v6.x
 *     3. OneWire       — Paul Stoffregen
 *     4. DallasTemperature — Miles Burton
 *
 *   BOARD : ESP32 Dev Module  (Arduino Core v3.x)
 * ═══════════════════════════════════════════════════════════════
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ─────────────────────────────────────────────
//  WiFi
// ─────────────────────────────────────────────
const char* WIFI_SSID     = "SSID";
const char* WIFI_PASSWORD = "PASSWORD";

// ─────────────────────────────────────────────
//  EMQX Broker
// ─────────────────────────────────────────────
const char* MQTT_BROKER = "********************************";
const int   MQTT_PORT   = ****;
const char* MQTT_USER   = "**********";
const char* MQTT_PASS   = "******************";

// ─────────────────────────────────────────────
//  PUBLISH Topics  (ESP32 → Website)
// ─────────────────────────────────────────────
#define TOPIC_SPEED      "vehicle/speed"
#define TOPIC_RPM        "vehicle/rpm"
#define TOPIC_CURRENT    "vehicle/current"
#define TOPIC_VOLTAGE    "vehicle/voltage"
#define TOPIC_TEMP       "vehicle/temp"
#define TOPIC_FREQUENCY  "vehicle/frequency"

// ─────────────────────────────────────────────
//  SUBSCRIBE Topics  (Website → ESP32)
// ─────────────────────────────────────────────
#define TOPIC_COMMAND    "vehicle/command"
#define TOPIC_PWM        "vehicle/pwm"
#define TOPIC_FREQCTRL   "vehicle/freqctrl"
#define TOPIC_MODE_CMD   "vehicle/mode_cmd"
#define TOPIC_INDICATOR  "vehicle/indicator"
#define TOPIC_LED        "led/control"

// ─────────────────────────────────────────────
//  GPIO — TB6612FNG Motor Driver (Motor A)
// ─────────────────────────────────────────────
//  TB6612FNG truth table (Motor A):
//    AIN1  AIN2  PWMA   │  Mode
//    HIGH  LOW   PWM    │  CW  (Forward)
//    LOW   HIGH  PWM    │  CCW (Backward)
//    HIGH  HIGH   X     │  Short brake
//    LOW   LOW    X     │  Stop / Coast
//    STBY must be HIGH to enable the chip

#define PIN_AIN1    27   // Motor A direction bit 1  (in1)
#define PIN_AIN2    26   // Motor A direction bit 2  (in2)
#define PIN_PWMA    14   // Motor A PWM speed         (enA)
#define PIN_STBY     5   // TB6612FNG standby (HIGH = active)

// Motor B
#define PIN_BIN1    13   // Motor B direction bit 1  (in3)
#define PIN_BIN2    33   // Motor B direction bit 2  (in4)
#define PIN_PWMB    32   // Motor B PWM speed         (enB)

// ─────────────────────────────────────────────
//  GPIO — Quadrature Encoder
// ─────────────────────────────────────────────
#define ENCODER_A_PIN   18
#define ENCODER_B_PIN   19

// ─────────────────────────────────────────────
//  GPIO — Sensors
// ─────────────────────────────────────────────
#define PIN_VOLTAGE_SENSOR  34   // ADC1_CH0 — Voltage divider output
#define PIN_CURRENT_SENSOR  35   // ADC1_CH3 — ACS712 analog output
#define PIN_DS18B20          4   // OneWire data pin for DS18B20

// ─────────────────────────────────────────────
//  GPIO — Indicators / LED
// ─────────────────────────────────────────────
#define PIN_LED          2
#define PIN_LEFT_IND     39   // input-only GPIOs OK for output-only wiring;
#define PIN_RIGHT_IND    36   // change if you need true output (use 4,5,…)
#define PIN_HEADLIGHT    12
#define PIN_HAZARD       13

// ─────────────────────────────────────────────
//  PWM (Arduino Core v3.x)
// ─────────────────────────────────────────────
#define PWM_CARRIER_HZ  5000
#define PWM_RESOLUTION  8        // 8-bit → 0–255

// ─────────────────────────────────────────────
//  Encoder / Wheel Config
// ─────────────────────────────────────────────
const int   PPR              = 333;
const float WHEEL_DIAM_M     = 0.07f;
const float WHEEL_CIRC_M     = WHEEL_DIAM_M * 3.14159265f;
const float METERS_PER_COUNT = WHEEL_CIRC_M / (float)PPR;
// Speed in km/h = (m/s) * 3.6

// ─────────────────────────────────────────────
//  Voltage Sensor Config (Resistive Divider)
// ─────────────────────────────────────────────
//  R1 = 30kΩ (top), R2 = 7.5kΩ (bottom)
//  V_in = V_adc × (R1+R2)/R2 = V_adc × 5.0
//  ESP32 ADC 12-bit (0–4095), Vref ≈ 3.3V
const float VDIV_RATIO       = 5.0f;    // (R1+R2)/R2
const float ADC_VREF         = 3.3f;
const int   ADC_MAX          = 4095;

// ─────────────────────────────────────────────
//  Current Sensor Config (ACS712 – 30A module)
// ─────────────────────────────────────────────
//  ACS712-30A: sensitivity = 66 mV/A, offset = Vcc/2 ≈ 2.5V
//  If using 5V sensor → add a voltage divider (3.3V safe).
//  Adjust CURRENT_OFFSET if your quiescent voltage differs.
const float ACS712_SENSITIVITY = 0.066f;  // V per A
const float CURRENT_OFFSET     = 2.5f;    // quiescent voltage at 0A

// ─────────────────────────────────────────────
//  DS18B20 Temperature Config
// ─────────────────────────────────────────────
const float TEMP_OVERHEAT_THRESHOLD = 60.0f;  // °C — above this, reduce PWM
const int   TEMP_PROTECTION_PWM     = 150;    // PWM cap during overheat

// ─────────────────────────────────────────────
//  MQTT + TLS
// ─────────────────────────────────────────────
WiFiClientSecure wifiSecure;
PubSubClient     mqttClient(wifiSecure);

// ─────────────────────────────────────────────
//  DS18B20 OneWire bus + DallasTemperature
// ─────────────────────────────────────────────
OneWire           oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);

// ─────────────────────────────────────────────
//  Encoder volatile state
// ─────────────────────────────────────────────
volatile long          encoderCount = 0;
volatile unsigned long lastRisingA  = 0;
volatile unsigned long lastRisingB  = 0;
volatile unsigned long periodA_us   = 0;  // period of channel A pulse
volatile unsigned long periodB_us   = 0;
volatile long          phaseDiff_us = 0;
volatile bool          dirFwd       = true;

// ─────────────────────────────────────────────
//  Live sensor values (computed from encoder)
// ─────────────────────────────────────────────
float liveSpeed     = 0.0f;   // km/h
float liveRpm       = 0.0f;
float liveFrequency = 0.0f;   // Hz  (encoder pulse frequency)

// ─────────────────────────────────────────────
//  Live sensor values (from analog/digital sensors)
// ─────────────────────────────────────────────
float liveVoltage    = 0.0f;   // V  — from voltage divider ADC
float liveCurrent    = 0.0f;   // A  — from ACS712
float liveTemp       = 0.0f;   // °C — from DS18B20
bool  ds18b20Found   = false;  // true if sensor detected at boot
bool  overheating    = false;  // thermal protection flag

// ─────────────────────────────────────────────
//  Manual override values (Serial Monitor)
// ─────────────────────────────────────────────
bool  useEncoderData = true;   // false = use manual overrides
bool  useManualVoltage  = false; // true = use manual voltage override
bool  useManualCurrent  = false; // true = use manual current override
bool  useManualTemp     = false; // true = use manual temp override
float manSpeed       = 0.0f;
float manRpm         = 0.0f;
float manCurrent     = 0.0f;
float manVoltage     = 0.0f;
float manTemp        = 0.0f;
float manFrequency   = 0.0f;

// ─────────────────────────────────────────────
//  Website → ESP32 motor command state
// ─────────────────────────────────────────────
struct VehicleCommand {
  String direction = "STOP";
  int    pwm       = 0;       // 0–255
  int    freq      = 0;       // carrier freq requested (optional)
  String driveMode = "NORMAL";
};
VehicleCommand cmd;

struct Indicators {
  bool left  = false;
  bool right = false;
  bool head  = false;
  bool haz   = false;
};
Indicators ind;

bool ledState = false;

// ─────────────────────────────────────────────
//  Serial input buffer
// ─────────────────────────────────────────────
String serialBuffer = "";

// ─────────────────────────────────────────────
//  Timing
// ─────────────────────────────────────────────
unsigned long lastPublish      = 0;
unsigned long lastReconnect    = 0;
unsigned long lastEncoderCalc  = 0;
unsigned long lastSensorRead   = 0;
long          lastEncoderCount = 0;

const unsigned long PUBLISH_INTERVAL   = 500;
const unsigned long RECONNECT_INTERVAL = 5000;
const unsigned long ENCODER_CALC_MS    = 200;   // compute RPM/speed every 200 ms
const unsigned long SENSOR_READ_MS     = 500;   // read analog/temp sensors every 500ms

// ═══════════════════════════════════════════════════════════════
//  ENCODER ISRs
// ═══════════════════════════════════════════════════════════════

void IRAM_ATTR onRisingA() {
  unsigned long now = micros();
  if (lastRisingA > 0 && now > lastRisingA)
    periodA_us = now - lastRisingA;
  lastRisingA = now;

  // Quadrature decode: sample B at A's rising edge
  int bNow = digitalRead(ENCODER_B_PIN);
  if (bNow == LOW) {
    dirFwd = true;
    encoderCount++;
    if (lastRisingB > 0)
      phaseDiff_us = (long)(now - lastRisingB);
  } else {
    dirFwd = false;
    encoderCount--;
    if (lastRisingB > 0)
      phaseDiff_us = -(long)(now - lastRisingB);
  }
}

void IRAM_ATTR onRisingB() {
  unsigned long now = micros();
  if (lastRisingB > 0 && now > lastRisingB)
    periodB_us = now - lastRisingB;
  lastRisingB = now;
}

// ═══════════════════════════════════════════════════════════════
//  ENCODER COMPUTATION  (call every ENCODER_CALC_MS)
// ═══════════════════════════════════════════════════════════════

void computeEncoderMetrics() {
  noInterrupts();
  long          count = encoderCount;
  unsigned long pA    = periodA_us;
  interrupts();

  unsigned long now     = millis();
  long          delta   = count - lastEncoderCount;
  float         dtSec   = (now - lastEncoderCalc) / 1000.0f;
  lastEncoderCount      = count;
  lastEncoderCalc       = now;

  if (dtSec <= 0) return;

  // --- Frequency: pulses per second (channel A)
  liveFrequency = (pA > 0) ? (1e6f / (float)pA) : 0.0f;

  // --- RPM: (delta counts / PPR) / dt * 60
  float rps = (float)abs(delta) / (float)PPR / dtSec;
  liveRpm   = rps * 60.0f;

  // --- Speed: rps * circumference → m/s → km/h
  float mps    = rps * WHEEL_CIRC_M;
 liveSpeed    = mps * 3.6f; 
}

// ═══════════════════════════════════════════════════════════════
//  SENSOR READING FUNCTIONS
// ═══════════════════════════════════════════════════════════════

float readVoltageSensor() {
  int raw = analogRead(PIN_VOLTAGE_SENSOR);
  float vAdc = ((float)raw / (float)ADC_MAX) * ADC_VREF;
  return vAdc * VDIV_RATIO;
}

float readCurrentSensor() {
  // Average 10 samples for stability
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(PIN_CURRENT_SENSOR);
    delayMicroseconds(100);
  }
  float vAdc = ((float)sum / 10.0f / (float)ADC_MAX) * ADC_VREF;
  float current = (vAdc - CURRENT_OFFSET) / ACS712_SENSITIVITY;
  return abs(current);   // return absolute value
}

// DS18B20 non-blocking: request on one call, read on the next
bool     ds18b20Requested = false;
unsigned long ds18b20RequestTime = 0;
const unsigned long DS18B20_CONVERSION_MS = 750; // 12-bit conversion time

float readDS18B20() {
  if (!ds18b20Found) return -127.0f;

  if (!ds18b20Requested) {
    // Phase 1: start conversion (non-blocking)
    ds18b20.requestTemperatures();
    ds18b20Requested = true;
    ds18b20RequestTime = millis();
    return liveTemp;  // return last known value while converting
  }

  // Phase 2: read result once conversion is done
  if (millis() - ds18b20RequestTime >= DS18B20_CONVERSION_MS) {
    ds18b20Requested = false;
    float t = ds18b20.getTempCByIndex(0);
    if (t == DEVICE_DISCONNECTED_C) return liveTemp;
    return t;
  }

  return liveTemp;  // still converting, return last known
}

void readAllSensors() {
  liveVoltage = readVoltageSensor();
  liveCurrent = readCurrentSensor();
  float temp  = readDS18B20();
  if (temp > -126.0f) liveTemp = temp;  // update only if valid

  // ── Thermal Protection ─────────────────────────────────────
  if (liveTemp >= TEMP_OVERHEAT_THRESHOLD && !overheating) {
    overheating = true;
    Serial.println();
    Serial.println("╔══════════════════════════════════════════════════╗");
    Serial.println("║  ⚠  THERMAL PROTECTION ACTIVATED                ║");
    Serial.printf ("║  Temperature: %.1f°C  (threshold: %.1f°C)       ║\n",
                   liveTemp, TEMP_OVERHEAT_THRESHOLD);
    Serial.printf ("║  PWM capped to %d                                ║\n",
                   TEMP_PROTECTION_PWM);
    Serial.println("╚══════════════════════════════════════════════════╝");
    // Reduce PWM immediately
    if (cmd.pwm > TEMP_PROTECTION_PWM) {
      cmd.pwm = TEMP_PROTECTION_PWM;
      applyMotorCommand();
    }
  }
  else if (liveTemp < (TEMP_OVERHEAT_THRESHOLD - 5.0f) && overheating) {
    // 5°C hysteresis before clearing flag
    overheating = false;
    Serial.println();
    Serial.println("[THERMAL] Temperature normal — protection cleared");
  }

  // While overheating, continuously cap any PWM changes
  if (overheating && cmd.pwm > TEMP_PROTECTION_PWM) {
    cmd.pwm = TEMP_PROTECTION_PWM;
    applyMotorCommand();
  }
}

// ═══════════════════════════════════════════════════════════════
//  TB6612FNG MOTOR CONTROL
// ═══════════════════════════════════════════════════════════════

void motorForward(int pwmVal) {
  pwmVal = constrain(pwmVal, 0, 255);
  digitalWrite(PIN_STBY, HIGH);
  // Motor A forward
  digitalWrite(PIN_AIN1, HIGH);
  digitalWrite(PIN_AIN2, LOW);
  ledcWrite(PIN_PWMA, pwmVal);
  // Motor B forward (reversed polarity wiring)
  digitalWrite(PIN_BIN1, LOW);
  digitalWrite(PIN_BIN2, HIGH);
  ledcWrite(PIN_PWMB, pwmVal);
}

void motorBackward(int pwmVal) {
  pwmVal = constrain(pwmVal, 0, 255);
  digitalWrite(PIN_STBY, HIGH);
  // Motor A backward
  digitalWrite(PIN_AIN1, LOW);
  digitalWrite(PIN_AIN2, HIGH);
  ledcWrite(PIN_PWMA, pwmVal);
  // Motor B backward (reversed polarity wiring)
  digitalWrite(PIN_BIN1, HIGH);
  digitalWrite(PIN_BIN2, LOW);
  ledcWrite(PIN_PWMB, pwmVal);
}

void motorShortBrake() {
  // IN1=HIGH, IN2=HIGH → short brake (fast stop, holds motor)
  digitalWrite(PIN_AIN1, HIGH);
  digitalWrite(PIN_AIN2, HIGH);
  ledcWrite(PIN_PWMA, 255);
  digitalWrite(PIN_BIN1, HIGH);
  digitalWrite(PIN_BIN2, HIGH);
  ledcWrite(PIN_PWMB, 255);
}

void motorCoast() {
  // IN1=LOW, IN2=LOW → coast (free spin)
  digitalWrite(PIN_AIN1, LOW);
  digitalWrite(PIN_AIN2, LOW);
  ledcWrite(PIN_PWMA, 0);
  digitalWrite(PIN_BIN1, LOW);
  digitalWrite(PIN_BIN2, LOW);
  ledcWrite(PIN_PWMB, 0);
}

void motorStop() {
  // STBY LOW → all outputs Hi-Z  (lowest power)
  ledcWrite(PIN_PWMA, 0);
  ledcWrite(PIN_PWMB, 0);
  digitalWrite(PIN_AIN1, LOW);
  digitalWrite(PIN_AIN2, LOW);
  digitalWrite(PIN_BIN1, LOW);
  digitalWrite(PIN_BIN2, LOW);
  digitalWrite(PIN_STBY, LOW);
}

void applyMotorCommand() {
  if (cmd.direction == "FORWARD") {
    motorForward(cmd.pwm);
    Serial.printf("[MOTOR] FORWARD  PWM=%d\n", cmd.pwm);
  }
  else if (cmd.direction == "BACKWARD" || cmd.direction == "REVERSE") {
    motorBackward(cmd.pwm);
    Serial.printf("[MOTOR] BACKWARD  PWM=%d\n", cmd.pwm);
  }
  else if (cmd.direction == "BRAKE") {
    motorShortBrake();
    Serial.println("[MOTOR] SHORT BRAKE");
  }
  else {
    motorStop();
    Serial.println("[MOTOR] STOPPED (STBY)");
  }
}

void applyPwm(int v) {
  // Update PWM while keeping current direction
  cmd.pwm = constrain(v, 0, 255);
  if      (cmd.direction == "FORWARD")                          motorForward(cmd.pwm);
  else if (cmd.direction == "BACKWARD" || cmd.direction == "REVERSE") motorBackward(cmd.pwm);
  else                                                           ledcWrite(PIN_PWMA, cmd.pwm);
}

void applyFrequency(int freqHz) {
  if (freqHz > 0 && freqHz <= 50000) {
    ledcAttach(PIN_PWMA, freqHz, PWM_RESOLUTION);  // re-attach (Core v3.x safe)
    ledcWrite(PIN_PWMA, cmd.pwm);                  // restore duty after re-attach
    Serial.printf("[CARRIER FREQ] Set to %d Hz\n", freqHz);
  }
}

void applyModeSettings() {
  // Interpret drive modes and map to PWM/direction
  if (cmd.driveMode == "ECO") {
    cmd.pwm = min(cmd.pwm, 120);   // cap at ~47% duty
  } else if (cmd.driveMode == "SPORT") {
    // allow full range — no cap
  } else if (cmd.driveMode == "REGEN") {
    motorShortBrake();             // short brake = regenerative-like
    Serial.println("[MODE] REGEN — short brake active");
    return;
  }
  applyMotorCommand();
  Serial.printf("[MODE] %s  PWM=%d\n", cmd.driveMode.c_str(), cmd.pwm);
}

void applyIndicators() {
  // NOTE: GPIO 34/35 are INPUT-ONLY on most ESP32 modules.
  //       Change PIN_LEFT_IND / PIN_RIGHT_IND to safe output GPIOs
  //       (e.g. 4, 5, 21, 22) if you need real indicator outputs.
  digitalWrite(PIN_HEADLIGHT, ind.head ? HIGH : LOW);
  digitalWrite(PIN_HAZARD,    ind.haz  ? HIGH : LOW);
  // Uncomment after reassigning pins:
  // digitalWrite(PIN_LEFT_IND,  (ind.left  || ind.haz) ? HIGH : LOW);
  // digitalWrite(PIN_RIGHT_IND, (ind.right || ind.haz) ? HIGH : LOW);
}

// ═══════════════════════════════════════════════════════════════
//  PUBLISH sensor values to website
// ═══════════════════════════════════════════════════════════════

void publishSensorData() {
  if (!mqttClient.connected()) {
    Serial.println("[WARN] MQTT not connected — values NOT published");
    return;
  }

  // Decide which source to publish
  float pubSpeed = useEncoderData ? liveSpeed     : manSpeed;
  float pubRpm   = useEncoderData ? liveRpm       : manRpm;
  float pubFreq  = useEncoderData ? liveFrequency : manFrequency;
  float pubCurr  = useManualCurrent  ? manCurrent  : liveCurrent;
  float pubVolt  = useManualVoltage  ? manVoltage  : liveVoltage;
  float pubTemp  = useManualTemp     ? manTemp     : liveTemp;

  char buf[16];
  dtostrf(pubSpeed, 5, 1, buf); mqttClient.publish(TOPIC_SPEED,     buf);
  dtostrf(pubRpm,   6, 0, buf); mqttClient.publish(TOPIC_RPM,       buf);
  dtostrf(pubCurr,  5, 1, buf); mqttClient.publish(TOPIC_CURRENT,   buf);
  dtostrf(pubVolt,  5, 1, buf); mqttClient.publish(TOPIC_VOLTAGE,   buf);
  dtostrf(pubTemp,  5, 1, buf); mqttClient.publish(TOPIC_TEMP,      buf);
  dtostrf(pubFreq,  6, 1, buf); mqttClient.publish(TOPIC_FREQUENCY, buf);

  Serial.printf("[PUB|%s] SPD:%.2f km/h  RPM:%.0f  I:%.1fA  V:%.1fV  T:%.1f°C  F:%.1fHz%s\n",
                useEncoderData ? "ENC" : "MAN",
                pubSpeed, pubRpm, pubCurr, pubVolt, pubTemp, pubFreq,
                overheating ? "  ⚠OVERHEAT" : "");
}

// ═══════════════════════════════════════════════════════════════
//  MQTT CALLBACK — receives commands from website
// ═══════════════════════════════════════════════════════════════

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  String t(topic);

  Serial.println();
  Serial.println("┌─────────────────────────────────────────┐");
  Serial.println("│          COMMAND FROM WEBSITE            │");
  Serial.printf ("│  Topic : %-30s │\n", t.c_str());
  Serial.printf ("│  Data  : %-30s │\n", msg.c_str());
  Serial.println("└─────────────────────────────────────────┘");

  // ── vehicle/command — JSON { direction, pwm, freq } ─────────
  if (t == TOPIC_COMMAND) {
  StaticJsonDocument<256> doc;
  if (!deserializeJson(doc, msg)) {
    if (doc.containsKey("direction")) {
      cmd.direction = doc["direction"].as<String>();
      cmd.direction.toUpperCase();
    }
    if (doc.containsKey("pwm"))  cmd.pwm  = constrain((int)doc["pwm"],  0, 255);
    if (doc.containsKey("freq")) cmd.freq = constrain((int)doc["freq"], 0, 100000);

    applyMotorCommand();   // ← direction + PWM first

    if (cmd.freq > 0) {    // ← frequency change AFTER motor is running
      ledcAttach(PIN_PWMA, cmd.freq, PWM_RESOLUTION);  // re-attach at new freq
      ledcWrite(PIN_PWMA, cmd.pwm);                    // re-apply duty
      Serial.printf("[CARRIER FREQ] Set to %d Hz\n", cmd.freq);
    }
  }
}

  // ── vehicle/pwm — plain int 0–255 ───────────────────────────
  else if (t == TOPIC_PWM) {
    int v = constrain(msg.toInt(), 0, 255);
    applyPwm(v);
    Serial.printf("[PWM] Set to %d\n", v);
  }

  // ── vehicle/freqctrl — plain int Hz ─────────────────────────
  else if (t == TOPIC_FREQCTRL) {
    int v = constrain(msg.toInt(), 1, 100000);
    cmd.freq = v;
    applyFrequency(v);
  }

  // ── vehicle/mode_cmd — JSON { mode, pwm, freq } ─────────────
  else if (t == TOPIC_MODE_CMD) {
    StaticJsonDocument<256> doc;
    if (!deserializeJson(doc, msg)) {
      if (doc.containsKey("mode")) {
  cmd.driveMode = doc["mode"].as<String>();
  cmd.driveMode.toUpperCase();
}
      if (doc.containsKey("pwm"))  cmd.pwm  = constrain((int)doc["pwm"],  0, 255);
      if (doc.containsKey("freq")) cmd.freq = constrain((int)doc["freq"], 0, 100000);
      if (cmd.freq > 0) applyFrequency(cmd.freq);
      applyModeSettings();
    }
  }

  // ── vehicle/indicator — JSON { left, right, head, haz } ─────
  else if (t == TOPIC_INDICATOR) {
    StaticJsonDocument<128> doc;
    if (!deserializeJson(doc, msg)) {
      ind.left  = doc["left"]  | false;
      ind.right = doc["right"] | false;
      ind.head  = doc["head"]  | false;
      ind.haz   = doc["haz"]   | false;
      applyIndicators();
      Serial.printf("[IND] LEFT=%d  RIGHT=%d  HEAD=%d  HAZ=%d\n",
                    ind.left, ind.right, ind.head, ind.haz);
    }
  }

  // ── led/control — "1" / "0" ─────────────────────────────────
  else if (t == TOPIC_LED) {
    ledState = (msg == "1");
    digitalWrite(PIN_LED, ledState ? HIGH : LOW);
    Serial.printf("[LED] %s\n", ledState ? "ON" : "OFF");
  }

  Serial.println();
}

// ═══════════════════════════════════════════════════════════════
//  SERIAL COMMAND PARSER
// ═══════════════════════════════════════════════════════════════

void printHelp() {
  Serial.println();
  Serial.println("╔═══════════════════════════════════════════════════╗");
  Serial.println("║      NEXUS EV + TB6612FNG — Serial Commands       ║");
  Serial.println("╠═══════════════════════════════════════════════════╣");
  Serial.println("║  ENCODER MODE (default — live sensor readings)    ║");
  Serial.println("║    encoder   → use live encoder + sensor data     ║");
  Serial.println("╠═══════════════════════════════════════════════════╣");
  Serial.println("║  MANUAL OVERRIDES  (format  FIELD:VALUE)          ║");
  Serial.println("║    spd:VALUE    Speed   km/h   e.g. spd:80        ║");
  Serial.println("║    rpm:VALUE    RPM             e.g. rpm:3200      ║");
  Serial.println("║    curr:VALUE   Current A       e.g. curr:18      ║");
  Serial.println("║    volt:VALUE   Voltage V       e.g. volt:52      ║");
  Serial.println("║    temp:VALUE   Temp    °C      e.g. temp:45      ║");
  Serial.println("║    freq:VALUE   Freq    Hz      e.g. freq:900     ║");
  Serial.println("║    all:spd,rpm,curr,volt,temp,freq                ║");
  Serial.println("╠═══════════════════════════════════════════════════╣");
  Serial.println("║  MOTOR TEST COMMANDS (direct control)             ║");
  Serial.println("║    fwd:PWM    run forward  e.g. fwd:150           ║");
  Serial.println("║    bwd:PWM    run backward e.g. bwd:150           ║");
  Serial.println("║    brake      short brake (fast stop)             ║");
  Serial.println("║    stop       coast to stop (STBY low)            ║");
  Serial.println("╠═══════════════════════════════════════════════════╣");
  Serial.println("║  SENSORS                                          ║");
  Serial.println("║    Voltage : GPIO 36 (ADC) — divider R1/R2       ║");
  Serial.println("║    Current : GPIO 39 (ADC) — ACS712-30A          ║");
  Serial.println("║    Temp    : GPIO  4       — DS18B20 OneWire     ║");
  Serial.println("║    Thermal Protect: T>60°C → PWM capped to 150   ║");
  Serial.println("╠═══════════════════════════════════════════════════╣");
  Serial.println("║  UTILITIES                                        ║");
  Serial.println("║    status   print all values                      ║");
  Serial.println("║    reset    zero all manual overrides             ║");
  Serial.println("║    help     this menu                             ║");
  Serial.println("╚═══════════════════════════════════════════════════╝");
  Serial.println();
}

void printStatus() {
  noInterrupts();
  long cnt = encoderCount;
  interrupts();

  Serial.println();
  Serial.println("┌──────────────────────────────────────────────┐");
  Serial.println("│              NEXUS EV STATUS                  │");
  Serial.println("├──────────────────────────────────────────────┤");
  Serial.printf ("│  Data source  : %-28s│\n", useEncoderData ? "ENCODER (live)" : "MANUAL OVERRIDE");
  Serial.println("├──────────────────────────────────────────────┤");
  Serial.println("│  ENCODER LIVE VALUES                          │");
  Serial.printf ("│   Speed       : %8.2f  km/h               │\n", liveSpeed);
  Serial.printf ("│   RPM         : %8.2f                     │\n", liveRpm);
  Serial.printf ("│   Frequency   : %8.2f  Hz                 │\n", liveFrequency);
  Serial.printf ("│   Count       : %8ld                      │\n", cnt);
  Serial.println("├──────────────────────────────────────────────┤");
  Serial.println("│  SENSOR LIVE VALUES                           │");
  Serial.printf ("│   Voltage     : %8.2f  V   (GPIO 36 ADC)  │\n", liveVoltage);
  Serial.printf ("│   Current     : %8.2f  A   (GPIO 39 ACS)  │\n", liveCurrent);
  Serial.printf ("│   Temperature : %8.2f  °C  (DS18B20)      │\n", liveTemp);
  Serial.printf ("│   DS18B20     : %-28s│\n", ds18b20Found ? "DETECTED" : "NOT FOUND");
  Serial.printf ("│   Overheat    : %-28s│\n", overheating ? "⚠ YES — PWM CAPPED" : "NO");
  Serial.println("├──────────────────────────────────────────────┤");
  Serial.println("│  MANUAL OVERRIDES                             │");
  Serial.printf ("│   Speed       : %8.2f  km/h  [%s]      │\n", manSpeed,    useEncoderData    ? "OFF" : " ON");
  Serial.printf ("│   RPM         : %8.2f       [%s]      │\n", manRpm,      useEncoderData    ? "OFF" : " ON");
  Serial.printf ("│   Current     : %8.2f  A    [%s]      │\n", manCurrent,  useManualCurrent  ? " ON" : "OFF");
  Serial.printf ("│   Voltage     : %8.2f  V    [%s]      │\n", manVoltage,  useManualVoltage  ? " ON" : "OFF");
  Serial.printf ("│   Temp        : %8.2f  °C   [%s]      │\n", manTemp,     useManualTemp     ? " ON" : "OFF");
  Serial.printf ("│   Frequency   : %8.2f  Hz   [%s]      │\n", manFrequency,useEncoderData    ? "OFF" : " ON");
  Serial.println("├──────────────────────────────────────────────┤");
  Serial.println("│  MOTOR STATE                                  │");
  Serial.printf ("│   Direction   : %-28s│\n", cmd.direction.c_str());
  Serial.printf ("│   PWM         : %-28d│\n", cmd.pwm);
  Serial.printf ("│   Mode        : %-28s│\n", cmd.driveMode.c_str());
  Serial.println("├──────────────────────────────────────────────┤");
  Serial.printf ("│  MQTT : %-36s│\n", mqttClient.connected() ? "CONNECTED" : "DISCONNECTED");
  Serial.printf ("│  WiFi : %-36s│\n", WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED");
  Serial.println("└──────────────────────────────────────────────┘");
  Serial.println();
}

void parseSerialInput(String input) {
  input.trim();

  if (input.length() == 0) return;

  String lower = input;
  lower.toLowerCase();

  // ── Utility commands ──────────────────────────────────────
  if (lower == "help")    { printHelp();   return; }
  if (lower == "status")  { printStatus(); return; }

  if (lower == "encoder") {
    useEncoderData = true;
    useManualVoltage = false;
    useManualCurrent = false;
    useManualTemp    = false;
    Serial.println("[MODE] Switched to LIVE sensor data (encoder + voltage + current + DS18B20)");
    return;
  }

  if (lower == "reset") {
    manSpeed = manRpm = manCurrent = manVoltage = manTemp = manFrequency = 0.0f;
    useManualVoltage = false;
    useManualCurrent = false;
    useManualTemp    = false;
    Serial.println("[RESET] All manual overrides zeroed — using live sensors");
    return;
  }

  if (lower == "brake") {
    motorShortBrake();
    cmd.direction = "BRAKE";
    Serial.println("[MOTOR] BRAKE command from Serial");
    return;
  }

  if (lower == "stop") {
    motorStop();
    cmd.direction = "STOP";
    cmd.pwm = 0;
    Serial.println("[MOTOR] STOP command from Serial");
    return;
  }

  // ── FIELD:VALUE ───────────────────────────────────────────
  int colonPos = input.indexOf(':');
  if (colonPos == -1) {
    Serial.println("[ERROR] Unknown command — type 'help'");
    return;
  }

  String field = lower.substring(0, colonPos);
  String valStr = input.substring(colonPos + 1);
  float  value  = valStr.toFloat();
  field.trim();

  // ── Motor test commands ───────────────────────────────────
  if (field == "fwd") {
    int pwm = constrain((int)value, 0, 255);
    cmd.direction = "FORWARD"; cmd.pwm = pwm;
    motorForward(pwm);
    Serial.printf("[MOTOR] FORWARD PWM=%d (Serial test)\n", pwm);
    return;
  }
  if (field == "bwd") {
    int pwm = constrain((int)value, 0, 255);
    cmd.direction = "BACKWARD"; cmd.pwm = pwm;
    motorBackward(pwm);
    Serial.printf("[MOTOR] BACKWARD PWM=%d (Serial test)\n", pwm);
    return;
  }

  // ── all:spd,rpm,curr,volt,temp,freq ──────────────────────
  if (field == "all") {
    float vals[6] = {0,0,0,0,0,0};
    int idx = 0, start = 0;
    for (int i = 0; i <= (int)valStr.length() && idx < 6; i++) {
      if (i == (int)valStr.length() || valStr[i] == ',') {
        vals[idx++] = valStr.substring(start, i).toFloat();
        start = i + 1;
      }
    }
    if (idx < 6) { Serial.println("[ERROR] all: needs 6 values"); return; }
    manSpeed=vals[0]; manRpm=vals[1]; manCurrent=vals[2];
    manVoltage=vals[3]; manTemp=vals[4]; manFrequency=vals[5];
    useEncoderData   = false;
    useManualVoltage = true;
    useManualCurrent = true;
    useManualTemp    = true;
    Serial.printf("[ALL] spd=%.1f rpm=%.0f curr=%.1f volt=%.1f temp=%.1f freq=%.1f\n",
                  manSpeed, manRpm, manCurrent, manVoltage, manTemp, manFrequency);
    publishSensorData();
    return;
  }

  // ── Single field overrides ────────────────────────────────
  bool known = true;
  if      (field=="spd"  || field=="speed")     { manSpeed=max(0.0f,value); useEncoderData=false; Serial.printf("[SET] Speed=%.2f km/h (manual)\n",    manSpeed); }
  else if (field=="rpm")                         { manRpm=max(0.0f,value);   useEncoderData=false; Serial.printf("[SET] RPM=%.0f (manual)\n",            manRpm); }
  else if (field=="curr" || field=="current")    { manCurrent=max(0.0f,value); useManualCurrent=true; Serial.printf("[SET] Current=%.2f A (manual, live=%.2fA)\n", manCurrent, liveCurrent); }
  else if (field=="volt" || field=="voltage")    { manVoltage=max(0.0f,value); useManualVoltage=true; Serial.printf("[SET] Voltage=%.2f V (manual, live=%.2fV)\n", manVoltage, liveVoltage); }
  else if (field=="temp")                        { manTemp=value; useManualTemp=true; Serial.printf("[SET] Temp=%.2f °C (manual, live=%.2f°C)\n", manTemp, liveTemp); }
  else if (field=="freq" || field=="frequency")  { manFrequency=max(0.0f,value); useEncoderData=false; Serial.printf("[SET] Freq=%.2f Hz (manual)\n",  manFrequency); }
  else { known = false; Serial.printf("[ERROR] Unknown field '%s' — type 'help'\n", field.c_str()); }

  if (known) publishSensorData();
}

// ═══════════════════════════════════════════════════════════════
//  WiFi CONNECT
// ═══════════════════════════════════════════════════════════════

void connectWiFi() {
  Serial.println("\n══════════════════════════════════════");
  Serial.println("  NEXUS EV — Connecting to WiFi...");
  Serial.println("══════════════════════════════════════");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
    if (++attempts > 40) { Serial.println("\n[WiFi] Timeout — restarting"); delay(1000); ESP.restart(); }
  }
  Serial.printf("\n[WiFi] Connected!  IP=%s  RSSI=%d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

// ═══════════════════════════════════════════════════════════════
//  MQTT CONNECT
// ═══════════════════════════════════════════════════════════════

void connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  Serial.print("[MQTT] Connecting...");

  String clientId = "nexus_esp32_";
  clientId += String((uint32_t)esp_random(), HEX);

  if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
    Serial.println(" Connected!");
    mqttClient.subscribe(TOPIC_COMMAND);
    mqttClient.subscribe(TOPIC_PWM);
    mqttClient.subscribe(TOPIC_FREQCTRL);
    mqttClient.subscribe(TOPIC_MODE_CMD);
    mqttClient.subscribe(TOPIC_INDICATOR);
    mqttClient.subscribe(TOPIC_LED);
    Serial.println("[MQTT] Subscribed to all vehicle/* and led/control topics");
  } else {
    int s = mqttClient.state();
    Serial.printf(" Failed, state=%d", s);
    if (s==-4) Serial.print(" (TIMEOUT)");
    if (s==-2) Serial.print(" (BAD_CREDENTIALS)");
    if (s== 5) Serial.print(" (UNAUTHORIZED)");
    Serial.println(" — retry in 5s");
  }
}

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("╔══════════════════════════════════════════════════╗");
  Serial.println("║   NEXUS EV — TB6612FNG + Encoder + Sensors      ║");
  Serial.println("║   Voltage | Current | DS18B20 | Thermal Protect ║");
  Serial.println("║   Arduino Core v3.x  |  Type: help              ║");
  Serial.println("╚══════════════════════════════════════════════════╝");

  // ── Encoder ────────────────────────────────────────────────
  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), onRisingA, RISING);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B_PIN), onRisingB, RISING);

  // ── Analog Sensor Pins ─────────────────────────────────────
  pinMode(PIN_VOLTAGE_SENSOR, INPUT);
  pinMode(PIN_CURRENT_SENSOR, INPUT);
  analogSetAttenuation(ADC_11db);  // Full range 0–3.3V for ADC

  // ── DS18B20 Temperature Sensor ─────────────────────────────
  ds18b20.begin();
  int deviceCount = ds18b20.getDeviceCount();
  if (deviceCount > 0) {
    ds18b20Found = true;
    ds18b20.setResolution(12);        // 12-bit = 0.0625°C resolution
    ds18b20.setWaitForConversion(false); // non-blocking reads
    Serial.printf("[DS18B20] Found %d sensor(s) on GPIO %d\n", deviceCount, PIN_DS18B20);
  } else {
    Serial.println("[DS18B20] ⚠ No sensor found — temperature will read 0°C");
    Serial.println("          Check wiring: DATA→GPIO4, 4.7kΩ pull-up to 3.3V");
  }

  // ── TB6612FNG ─────────────────────────────────────────────
  pinMode(PIN_AIN1, OUTPUT);
  pinMode(PIN_AIN2, OUTPUT);
  pinMode(PIN_STBY, OUTPUT);
  pinMode(PIN_BIN1, OUTPUT);
  pinMode(PIN_BIN2, OUTPUT);

  // Optional indicator / headlight pins
  pinMode(PIN_LED,       OUTPUT);
  pinMode(PIN_HEADLIGHT, OUTPUT);
  pinMode(PIN_HAZARD,    OUTPUT);

  // Safe initial state — STBY low, all outputs low
  digitalWrite(PIN_AIN1, LOW);
  digitalWrite(PIN_AIN2, LOW);
  digitalWrite(PIN_BIN1, LOW);
  digitalWrite(PIN_BIN2, LOW);
  digitalWrite(PIN_STBY, LOW);
  digitalWrite(PIN_LED,       LOW);
  digitalWrite(PIN_HEADLIGHT, LOW);
  digitalWrite(PIN_HAZARD,    LOW);

  // ── PWM (Core v3.x ledcAttach) ────────────────────────────
  ledcAttach(PIN_PWMA, PWM_CARRIER_HZ, PWM_RESOLUTION);
  ledcAttach(PIN_PWMB, PWM_CARRIER_HZ, PWM_RESOLUTION);
  ledcWrite(PIN_PWMA, 0);
  ledcWrite(PIN_PWMB, 0);

  // ── Network ────────────────────────────────────────────────
  connectWiFi();
  wifiSecure.setInsecure();   // skip TLS cert verification (EMQX cloud)
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(30);
  connectMQTT();

  lastEncoderCalc  = millis();
  lastSensorRead   = millis();
  lastEncoderCount = 0;

  // ── Initial sensor read ────────────────────────────────────
  readAllSensors();
  Serial.printf("[SENSORS] Initial read — V:%.2fV  I:%.2fA  T:%.1f°C\n",
                liveVoltage, liveCurrent, liveTemp);

  printHelp();
  Serial.println("[READY] Live sensors publishing every 500 ms");
  Serial.println("        Voltage(GPIO36) + Current(GPIO39) + DS18B20(GPIO4)");
  Serial.println("        Thermal protection active: T>60°C → PWM=150");
  Serial.println();
}

// ═══════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════

void loop() {
  unsigned long now = millis();

  // ── WiFi watchdog ─────────────────────────────────────────
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Lost — reconnecting...");
    motorStop();   // safe the motor before attempting reconnect
    connectWiFi();
    return;
  }

  // ── MQTT reconnect (non-blocking) ─────────────────────────
  if (!mqttClient.connected()) {
    if (now - lastReconnect >= RECONNECT_INTERVAL) {
      lastReconnect = now;
      connectMQTT();
    }
    // Don't return here — keep computing encoder metrics even while
    // MQTT is down so the data is ready the moment it reconnects.
  }

  mqttClient.loop();

  // ── Compute encoder metrics every 200 ms ─────────────────
  if (now - lastEncoderCalc >= ENCODER_CALC_MS) {
    computeEncoderMetrics();
  }

  // ── Read voltage, current, DS18B20 every 500 ms ───────────
  if (now - lastSensorRead >= SENSOR_READ_MS) {
    lastSensorRead = now;
    readAllSensors();
  }

  // ── Read Serial input ─────────────────────────────────────
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialBuffer.length() > 0) {
        Serial.print("> "); Serial.println(serialBuffer);
        parseSerialInput(serialBuffer);
        serialBuffer = "";
      }
    } else {
      serialBuffer += c;
    }
  }

  // ── Periodic publish every 500 ms ─────────────────────────
  if (now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;
    publishSensorData();
  }
}

/*
 * ═══════════════════════════════════════════════════════════════
 *   WIRING REFERENCE — TB6612FNG
 * ═══════════════════════════════════════════════════════════════
 *
 *   ESP32 GPIO  │ TB6612FNG Pin │ Purpose
 *   ────────────┼───────────────┼──────────────────────────
 *   GPIO 27     │ AIN1 (in1)    │ Motor A direction bit 1
 *   GPIO 26     │ AIN2 (in2)    │ Motor A direction bit 2
 *   GPIO 14     │ PWMA (enA)    │ Motor A PWM speed
 *   GPIO  5     │ STBY          │ Standby (HIGH = enabled)
 *   GPIO 25     │ BIN1 (in3)    │ Motor B direction bit 1
 *   GPIO 33     │ BIN2 (in4)    │ Motor B direction bit 2
 *   GPIO 32     │ PWMB (enB)    │ Motor B PWM speed
 *   3.3V        │ VCC (logic)   │ Logic supply
 *   GND         │ GND           │ Common ground
 *   Motor Vcc   │ VM            │ Motor supply (2.5–13.5 V)
 *
 *   TB6612FNG Truth Table (Motor A):
 *   AIN1  AIN2  PWMA   │ Mode
 *   HIGH  LOW   PWM    │ Forward (CW)
 *   LOW   HIGH  PWM    │ Backward (CCW)
 *   HIGH  HIGH  HIGH   │ Short brake
 *   LOW   LOW   -      │ Coast / stop
 *   STBY = LOW  →  all outputs Hi-Z regardless of IN pins
 *
 * ═══════════════════════════════════════════════════════════════
 *   WIRING REFERENCE — Quadrature Encoder
 * ═══════════════════════════════════════════════════════════════
 *
 *   Encoder Pin │ ESP32 GPIO
 *   ────────────┼──────────
 *   Channel A   │ GPIO 18
 *   Channel B   │ GPIO 19
 *   VCC         │ 3.3V
 *   GND         │ GND
 *
 * ═══════════════════════════════════════════════════════════════
 *   WIRING REFERENCE — Voltage Sensor (Resistive Divider)
 * ═══════════════════════════════════════════════════════════════
 *
 *   Battery V+ ──┤ R1=30kΩ ├──┬── GPIO 36 (ADC input)
 *                             │
 *                          R2=7.5kΩ
 *                             │
 *   Battery GND ──────────────┘
 *
 *   V_battery = V_adc × (R1+R2)/R2 = V_adc × 5.0
 *   Max measurable voltage: 3.3V × 5.0 = 16.5V
 *   Change R1/R2 ratio for higher voltages (e.g. 48V pack)
 *
 * ═══════════════════════════════════════════════════════════════
 *   WIRING REFERENCE — Current Sensor (ACS712-30A)
 * ═══════════════════════════════════════════════════════════════
 *
 *   ACS712 Pin  │ Connection
 *   ────────────┼──────────────────
 *   VCC         │ 5V
 *   GND         │ GND
 *   OUT         │ GPIO 39 (ADC input)*
 *   IP+         │ Load positive
 *   IP-         │ Load negative
 *
 *   * If ACS712 outputs 0–5V, add a voltage divider to
 *     scale to 0–3.3V before connecting to ESP32 ADC.
 *   Quiescent output at 0A ≈ Vcc/2 = 2.5V
 *   Sensitivity: 66 mV/A (30A module)
 *
 * ═══════════════════════════════════════════════════════════════
 *   WIRING REFERENCE — DS18B20 Temperature Sensor
 * ═══════════════════════════════════════════════════════════════
 *
 *   DS18B20 Pin │ Connection
 *   ────────────┼──────────────────
 *   VDD (red)   │ 3.3V
 *   GND (black) │ GND
 *   DATA (yellow)│ GPIO 4
 *                │ + 4.7kΩ pull-up resistor to 3.3V
 *
 *   ⚠ THERMAL PROTECTION:
 *   When DS18B20 reads ≥ 60°C → PWM is capped to 150
 *   Clears when temperature drops below 55°C (5°C hysteresis)
 *
 * ═══════════════════════════════════════════════════════════════
 *   MQTT DATA FLOW
 * ═══════════════════════════════════════════════════════════════
 *
 *   ESP32 → Website (published every 500 ms):
 *     vehicle/speed      → km/h  (from encoder)
 *     vehicle/rpm        → RPM   (from encoder)
 *     vehicle/frequency  → Hz    (encoder pulse freq)
 *     vehicle/current    → A     (from ACS712 sensor)
 *     vehicle/voltage    → V     (from ADC voltage divider)
 *     vehicle/temp       → °C    (from DS18B20)
 *
 *   Website → ESP32 (subscribed):
 *     vehicle/command    → JSON { direction, pwm, freq }
 *     vehicle/pwm        → int  0–255
 *     vehicle/freqctrl   → int  Hz  (PWM carrier freq)
 *     vehicle/mode_cmd   → JSON { mode, pwm, freq }
 *     vehicle/indicator  → JSON { left, right, head, haz }
 *     led/control        → "1" / "0"
 *
 * ═══════════════════════════════════════════════════════════════
 */
