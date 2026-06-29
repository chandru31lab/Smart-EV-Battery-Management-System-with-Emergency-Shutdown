# EV Instrument Cluster Dashboard

A premium, interactive IoT web dashboard designed for an Electric Vehicle (EV) instrument cluster. This dashboard visualizes real-time sensor data over MQTT and features a modern, high-end automotive aesthetic with glassmorphism, carbon-fiber grids, and dynamic CSS/Canvas animations.

## Features

- **Side-by-Side Gauges**: Full 270° circular Speedometer and Tachometer (RPM) built with the HTML5 Canvas API, featuring dynamic tick marks, neon gradients, glowing needles, and rotating decorative SVG rings.
- **Battery Management System (BMS) Panel**: Real-time visualization of Battery State of Charge (SOC), Voltage, Current, and Temperature, complete with color-coded warning indicators.
- **Motor Control Hub**: Interactive controls for motor speed (PWM duty cycle) and motor frequency.
- **Directional Control**: Integrated keyboard-based vehicle direction control (`↑` Forward, `↓` Backward, `Spacebar` Neutral).
- **MQTT Integration**: Real-time communication via WebSockets using `mqtt.js`. Configurable broker URL directly from the UI.
- **Demo Mode**: Built-in fallback simulation mode that generates realistic animated dashboard data if no MQTT connection is established or active.

## Technologies Used

- **HTML5 & Vanilla JavaScript**: No heavy frameworks.
- **CSS3**: Custom properties, Flexbox/Grid layouts, glassmorphism (`backdrop-filter`), and CSS keyframe animations.
- **Canvas API**: High-performance, pixel-perfect rendering of the analog gauges with HiDPI display support.
- **MQTT.js**: For WebSocket-based telemetry stream subscription and motor control publishing.

## Installation & Usage

1. **Clone the repository:**
   ```bash
   git clone https://github.com/chandru31lab/Instrument_Cluster.git
   cd Instrument_Cluster
   ```

2. **Open the Dashboard:**
   Simply open `index.html` in any modern web browser (Edge, Chrome, Firefox, Safari). To avoid CORS limitations with local files depending on your browser, you may want to serve the directory with a local HTTP server:
   ```bash
   # If using python
   python -m http.server 8000
   
   # Or using Node.js / npx
   npx serve .
   ```

3. **MQTT Broker Configuration:**
   - The dashboard connects to a WebSocket-enabled MQTT broker (Default: `ws://localhost:9001`).
   - If using a broker like Eclipse Mosquitto, ensure `protocol websockets` is enabled in your configuration.
   - You can update the broker URL dynamically using the configuration toggle ⚙️ in the top right corner of the dashboard.

## MQTT Topics Overview

**Subscribes to (Sensor Data):**
- `ev/speed` (Units: m/s, Max: 10)
- `ev/rpm` (Units: RPM, Max: 400)
- `ev/battery/soc` (0-100%)
- `ev/battery/voltage`
- `ev/battery/current`
- `ev/battery/temperature`

**Publishes to (Motor Commands):**
- `ev/motor/pwm` (PWM Duty Cycle %)
- `ev/motor/frequency` (Hz)
- `ev/motor/direction` ('forward', 'backward', 'neutral')

## Keyboard Controls

| Key | Action | Topic & Value Published |
|---|---|---|
| **`↑` Arrow Up** | Drive Forward | `ev/motor/direction` ➔ "forward" |
| **`↓` Arrow Down** | Drive Backward | `ev/motor/direction` ➔ "backward" |
| **`Spacebar`** | Neutral / Stop | `ev/motor/direction` ➔ "neutral" |

## Preview

The dashboard is fully responsive, breaking down into a vertical scrolling layout on smaller mobile viewports while preserving gauge definitions and interactivity.