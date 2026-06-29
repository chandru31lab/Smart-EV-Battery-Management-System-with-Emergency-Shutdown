/* ═══════════════════════════════════════════════════════════
   EV INSTRUMENT CLUSTER — DASHBOARD LOGIC
   MQTT + Canvas Gauges + Motor Control + Keyboard Direction
   ═══════════════════════════════════════════════════════════ */

(() => {
  'use strict';

  // ─── CONFIGURATION ─── (matches ESP32 firmware)
  const CONFIG = {
    mqtt: {
      // EMQX Cloud broker — WebSocket Secure (port 8084)
      defaultUrl: 'wss://jace13a6.ala.asia-southeast1.emqxsl.com:8084/mqtt',
      username: 'anish',
      password: 'Suneetha@123',
      reconnectPeriod: 3000,
      // Topics MUST match ESP32 firmware exactly
      topics: {
        // ── Subscribe (ESP32 → Website) ──
        speed:       'vehicle/speed',
        rpm:         'vehicle/rpm',
        voltage:     'vehicle/voltage',
        current:     'vehicle/current',
        temperature: 'vehicle/temp',
        frequency:   'vehicle/frequency',
        // ── Publish (Website → ESP32) ──
        command:     'vehicle/command',      // JSON { direction, pwm, freq }
        pwm:         'vehicle/pwm',          // int 0–255
        freqctrl:    'vehicle/freqctrl',     // int Hz
        modeCmd:     'vehicle/mode_cmd',     // JSON { mode, pwm, freq }
        indicator:   'vehicle/indicator',    // JSON { left, right, head, haz }
        led:         'led/control',          // "1" / "0"
      },
    },
    gauges: {
      speed: { max: 10, redZone: 8 },
      rpm: { max: 400, redZone: 350 },
    },
    battery: {
      maxVoltage: 16.5,   // Max from voltage divider (3.3V × 5.0)
      maxCurrent: 30,     // ACS712-30A range
      maxTemp: 80,
      warningTemp: 45,
      criticalTemp: 60,   // Matches DS18B20 thermal protection threshold
    },
  };

  // ─── STATE ───
  const state = {
    speed: 0,
    targetSpeed: 0,
    rpm: 0,
    targetRpm: 0,
    soc: 0,
    targetSoc: 0,
    voltage: 0,
    current: 0,
    temperature: 0,
    pwm: 0,
    frequency: 1000,
    direction: 'neutral', // 'forward', 'backward', 'neutral'
    mqttConnected: false,
  };

  let mqttClient = null;

  // ─── DOM ELEMENTS ───
  const $ = (sel) => document.querySelector(sel);
  const DOM = {
    statusDot: $('#status-dot'),
    statusText: $('#status-text'),
    clock: $('#clock'),
    // Battery
    batteryLevel: $('#battery-level'),
    socValue: $('#soc-value'),
    voltageValue: $('#voltage-value'),
    currentValue: $('#current-value'),
    tempValue: $('#temp-value'),
    statTemp: $('#stat-temperature'),
    // Gauges
    speedCanvas: $('#speedometer-canvas'),
    rpmCanvas: $('#rpm-canvas'),
    speedDigital: $('#speed-digital-value'),
    rpmDigital: $('#rpm-digital-value'),
    // Direction
    dirForward: $('#dir-forward'),
    dirBackward: $('#dir-backward'),
    dirStatus: $('#dir-status'),
    // Motor control
    pwmSlider: $('#pwm-slider'),
    pwmDisplay: $('#pwm-display'),
    pwmFill: $('#pwm-fill'),
    freqSlider: $('#freq-slider'),
    freqDisplay: $('#freq-display'),
    freqFill: $('#freq-fill'),
    directionDisplay: $('#direction-display'),
    btnForward: $('#btn-forward'),
    btnBackward: $('#btn-backward'),
    btnStop: $('#btn-stop'),
    // MQTT Config
    mqttHost: $('#mqtt-host'),
    mqttConfigToggle: $('#mqtt-config-toggle'),
    mqttConfigBody: $('#mqtt-config-body'),
    btnReconnect: $('#btn-reconnect'),
  };

  // ═══════════════════ CLOCK ═══════════════════
  function updateClock() {
    const now = new Date();
    DOM.clock.textContent = now.toLocaleTimeString('en-US', {
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
      hour12: false,
    });
  }
  setInterval(updateClock, 1000);
  updateClock();

  // ═══════════════════ MQTT ═══════════════════
  function connectMQTT() {
    const url = DOM.mqttHost.value || CONFIG.mqtt.defaultUrl;
    setMQTTStatus('connecting');

    if (mqttClient) {
      mqttClient.end(true);
    }

    try {
      mqttClient = mqtt.connect(url, {
        reconnectPeriod: CONFIG.mqtt.reconnectPeriod,
        connectTimeout: 10000,
        username: CONFIG.mqtt.username,
        password: CONFIG.mqtt.password,
        clientId: 'nexus_web_' + Math.random().toString(16).substr(2, 8),
        protocolVersion: 4,
        clean: true,
        rejectUnauthorized: false,
        path: '/mqtt',
      });

      mqttClient.on('connect', () => {
        state.mqttConnected = true;
        setMQTTStatus('connected');
        console.log('[MQTT] Connected to', url);
        // Subscribe to all sensor topics from ESP32
        const subs = [
          CONFIG.mqtt.topics.speed,
          CONFIG.mqtt.topics.rpm,
          CONFIG.mqtt.topics.voltage,
          CONFIG.mqtt.topics.current,
          CONFIG.mqtt.topics.temperature,
          CONFIG.mqtt.topics.frequency,
        ];
        mqttClient.subscribe(subs, { qos: 0 }, (err) => {
          if (!err) console.log('[MQTT] Subscribed to:', subs);
          else console.error('[MQTT] Subscribe error:', err);
        });
      });

      mqttClient.on('message', (topic, message) => {
        const value = parseFloat(message.toString().trim());
        if (isNaN(value)) return;
        lastMqttReceive = Date.now();

        switch (topic) {
          case CONFIG.mqtt.topics.speed:
            state.targetSpeed = Math.min(value, CONFIG.gauges.speed.max);
            break;
          case CONFIG.mqtt.topics.rpm:
            state.targetRpm = Math.min(value, CONFIG.gauges.rpm.max);
            break;
          case CONFIG.mqtt.topics.voltage:
            state.voltage = value;
            // Calculate SOC from voltage (simple linear mapping)
            // Adjust min/max voltage for your battery pack
            const minV = 3.0, maxV = CONFIG.battery.maxVoltage;
            state.targetSoc = Math.max(0, Math.min(100, ((value - minV) / (maxV - minV)) * 100));
            updateBatteryVoltage();
            break;
          case CONFIG.mqtt.topics.current:
            state.current = value;
            updateBatteryCurrent();
            break;
          case CONFIG.mqtt.topics.temperature:
            state.temperature = value;
            updateBatteryTemp();
            break;
          case CONFIG.mqtt.topics.frequency:
            // Display frequency from encoder (read-only)
            break;
        }
      });

      mqttClient.on('close', () => {
        state.mqttConnected = false;
        setMQTTStatus('disconnected');
      });

      mqttClient.on('error', (err) => {
        console.error('[MQTT] Error:', err.message);
        state.mqttConnected = false;
        setMQTTStatus('disconnected');
      });

      mqttClient.on('reconnect', () => {
        setMQTTStatus('connecting');
      });
    } catch (err) {
      console.error('[MQTT] Connection error:', err);
      setMQTTStatus('disconnected');
    }
  }

  function publishMQTT(topic, value) {
    if (mqttClient && state.mqttConnected) {
      mqttClient.publish(topic, String(value), { qos: 0 });
    }
  }

  function setMQTTStatus(s) {
    DOM.statusDot.className = 'status-dot ' + s;
    const labels = {
      connecting: 'Connecting…',
      connected: 'Connected',
      disconnected: 'Disconnected',
    };
    DOM.statusText.textContent = labels[s] || s;
  }

  // ═══════════════════ BATTERY UPDATES ═══════════════════
  function updateBatterySOC() {
    const soc = state.soc;
    // Level bar height
    DOM.batteryLevel.style.height = soc + '%';

    // Color classes
    DOM.batteryLevel.classList.remove('low', 'medium');
    DOM.socValue.classList.remove('low', 'medium');
    if (soc <= 20) {
      DOM.batteryLevel.classList.add('low');
      DOM.socValue.classList.add('low');
    } else if (soc <= 40) {
      DOM.batteryLevel.classList.add('medium');
      DOM.socValue.classList.add('medium');
    }

    DOM.socValue.textContent = Math.round(soc);
  }

  function updateBatteryVoltage() {
    DOM.voltageValue.textContent = state.voltage.toFixed(1);
  }

  function updateBatteryCurrent() {
    DOM.currentValue.textContent = state.current.toFixed(1);
  }

  function updateBatteryTemp() {
    DOM.tempValue.textContent = Math.round(state.temperature);
    DOM.statTemp.classList.remove('warning', 'critical');
    if (state.temperature >= CONFIG.battery.criticalTemp) {
      DOM.statTemp.classList.add('critical');
    } else if (state.temperature >= CONFIG.battery.warningTemp) {
      DOM.statTemp.classList.add('warning');
    }
  }

  // ═══════════════════ SHARED GAUGE DRAWING UTILITIES ═══════════════════
  function drawOuterDecoRings(ctx, cx, cy, radius) {
    // Decorative outer dotted ring
    const dots = 60;
    for (let i = 0; i < dots; i++) {
      const angle = (i / dots) * Math.PI * 2;
      const r = radius + 18;
      const x = cx + Math.cos(angle) * r;
      const y = cy + Math.sin(angle) * r;
      ctx.beginPath();
      ctx.arc(x, y, i % 5 === 0 ? 1.5 : 0.6, 0, Math.PI * 2);
      ctx.fillStyle = i % 5 === 0 ? 'rgba(255,255,255,0.12)' : 'rgba(255,255,255,0.05)';
      ctx.fill();
    }

    // Inner shadow ring
    const innerGrad = ctx.createRadialGradient(cx, cy, radius * 0.3, cx, cy, radius * 0.85);
    innerGrad.addColorStop(0, 'rgba(0,0,0,0.25)');
    innerGrad.addColorStop(1, 'transparent');
    ctx.beginPath();
    ctx.arc(cx, cy, radius * 0.85, 0, Math.PI * 2);
    ctx.fillStyle = innerGrad;
    ctx.fill();
  }

  function drawNeedle(ctx, cx, cy, angle, len, tailLen, glowColor) {
    ctx.save();
    ctx.translate(cx, cy);
    ctx.rotate(angle);

    // Needle shadow
    ctx.beginPath();
    ctx.moveTo(-tailLen, 2);
    ctx.lineTo(len, 2);
    ctx.strokeStyle = 'rgba(0,0,0,0.3)';
    ctx.lineWidth = 3;
    ctx.stroke();

    // Needle body (tapered)
    ctx.beginPath();
    ctx.moveTo(-tailLen, 0);
    ctx.lineTo(len - 8, -1.5);
    ctx.lineTo(len, 0);
    ctx.lineTo(len - 8, 1.5);
    ctx.lineTo(-tailLen, 0);
    const needleGrad = ctx.createLinearGradient(-tailLen, 0, len, 0);
    needleGrad.addColorStop(0, 'rgba(255,255,255,0.05)');
    needleGrad.addColorStop(0.4, 'rgba(255,255,255,0.6)');
    needleGrad.addColorStop(1, '#ffffff');
    ctx.fillStyle = needleGrad;
    ctx.fill();

    // Needle tip glow
    ctx.beginPath();
    ctx.arc(len, 0, 4, 0, Math.PI * 2);
    ctx.fillStyle = '#ffffff';
    ctx.shadowColor = glowColor;
    ctx.shadowBlur = 20;
    ctx.fill();
    ctx.shadowBlur = 0;

    ctx.restore();

    // Center cap
    const capGrad = ctx.createRadialGradient(cx, cy, 0, cx, cy, 16);
    capGrad.addColorStop(0, '#2a2a40');
    capGrad.addColorStop(0.7, '#1a1a2a');
    capGrad.addColorStop(1, '#0f0f18');
    ctx.beginPath();
    ctx.arc(cx, cy, 14, 0, Math.PI * 2);
    ctx.fillStyle = capGrad;
    ctx.fill();
    ctx.beginPath();
    ctx.arc(cx, cy, 14, 0, Math.PI * 2);
    ctx.strokeStyle = 'rgba(255,255,255,0.08)';
    ctx.lineWidth = 1;
    ctx.stroke();
    // Center dot
    ctx.beginPath();
    ctx.arc(cx, cy, 3, 0, Math.PI * 2);
    ctx.fillStyle = glowColor;
    ctx.shadowColor = glowColor;
    ctx.shadowBlur = 8;
    ctx.fill();
    ctx.shadowBlur = 0;
  }

  // ═══════════════════ SPEEDOMETER GAUGE (Canvas) ═══════════════════
  function drawSpeedometer(ctx, w, h, speed) {
    const cx = w / 2;
    const cy = h / 2;
    const radius = Math.min(w, h) / 2 - 32;

    ctx.clearRect(0, 0, w, h);

    const startAngle = (3 * Math.PI) / 4;
    const endAngle = (9 * Math.PI) / 4;   // 270° sweep
    const totalSweep = endAngle - startAngle;

    // ── Decorative outer elements ──
    drawOuterDecoRings(ctx, cx, cy, radius);

    // ── Outer glow ring ──
    ctx.beginPath();
    ctx.arc(cx, cy, radius + 8, startAngle, endAngle, false);
    ctx.strokeStyle = 'rgba(0, 240, 255, 0.04)';
    ctx.lineWidth = 18;
    ctx.stroke();

    // ── Background track ──
    ctx.beginPath();
    ctx.arc(cx, cy, radius, startAngle, endAngle, false);
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.04)';
    ctx.lineWidth = 14;
    ctx.lineCap = 'round';
    ctx.stroke();

    // ── Background segments (subtle tick ring) ──
    for (let i = 0; i <= 48; i++) {
      const t = i / 48;
      const angle = startAngle + t * totalSweep;
      const x1 = cx + Math.cos(angle) * (radius - 1);
      const y1 = cy + Math.sin(angle) * (radius - 1);
      const x2 = cx + Math.cos(angle) * (radius + 1);
      const y2 = cy + Math.sin(angle) * (radius + 1);
      ctx.beginPath();
      ctx.moveTo(x1, y1);
      ctx.lineTo(x2, y2);
      ctx.strokeStyle = 'rgba(255,255,255,0.03)';
      ctx.lineWidth = 1;
      ctx.stroke();
    }

    // ── Active arc ──
    const speedRatio = Math.min(speed / CONFIG.gauges.speed.max, 1);
    const activeEnd = startAngle + speedRatio * totalSweep;

    const gradient = ctx.createConicGradient(startAngle, cx, cy);
    gradient.addColorStop(0, '#00f0ff');
    gradient.addColorStop(0.5, '#8b5cf6');
    gradient.addColorStop(0.85, '#ff3d5a');
    gradient.addColorStop(1, '#ff3d5a');

    ctx.beginPath();
    ctx.arc(cx, cy, radius, startAngle, activeEnd, false);
    ctx.strokeStyle = gradient;
    ctx.lineWidth = 14;
    ctx.lineCap = 'round';
    ctx.stroke();

    // ── Active arc glow (two layers) ──
    ctx.beginPath();
    ctx.arc(cx, cy, radius, startAngle, activeEnd, false);
    ctx.strokeStyle = 'rgba(0, 240, 255, 0.1)';
    ctx.lineWidth = 28;
    ctx.lineCap = 'round';
    ctx.stroke();

    ctx.beginPath();
    ctx.arc(cx, cy, radius, Math.max(startAngle, activeEnd - 0.3), activeEnd, false);
    ctx.strokeStyle = 'rgba(0, 240, 255, 0.2)';
    ctx.lineWidth = 20;
    ctx.lineCap = 'round';
    ctx.stroke();

    // ── Tick marks ──
    const majorTicks = 11; // 0, 1.0, 2.0, ... 10.0
    for (let i = 0; i < majorTicks; i++) {
      const t = i / (majorTicks - 1);
      const angle = startAngle + t * totalSweep;
      const val = (t * CONFIG.gauges.speed.max).toFixed(1);

      const innerR = radius - 24;
      const outerR = radius - 8;

      const x1 = cx + Math.cos(angle) * innerR;
      const y1 = cy + Math.sin(angle) * innerR;
      const x2 = cx + Math.cos(angle) * outerR;
      const y2 = cy + Math.sin(angle) * outerR;

      ctx.beginPath();
      ctx.moveTo(x1, y1);
      ctx.lineTo(x2, y2);
      ctx.strokeStyle =
        val >= CONFIG.gauges.speed.redZone
          ? 'rgba(255, 61, 90, 0.8)'
          : 'rgba(255, 255, 255, 0.3)';
      ctx.lineWidth = 2.5;
      ctx.stroke();

      // Labels
      const labelR = radius - 38;
      const lx = cx + Math.cos(angle) * labelR;
      const ly = cy + Math.sin(angle) * labelR;
      ctx.font = "700 11px 'Rajdhani', sans-serif";
      ctx.fillStyle =
        val >= CONFIG.gauges.speed.redZone
          ? 'rgba(255, 61, 90, 0.9)'
          : 'rgba(255,255,255,0.45)';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText(Number.isInteger(Number(val)) ? val : val, lx, ly);

      // Minor ticks
      if (i < majorTicks - 1) {
        for (let j = 1; j < 5; j++) {
          const mt = (i + j / 5) / (majorTicks - 1);
          const mAngle = startAngle + mt * totalSweep;
          const mx1 = cx + Math.cos(mAngle) * (radius - 16);
          const my1 = cy + Math.sin(mAngle) * (radius - 16);
          const mx2 = cx + Math.cos(mAngle) * (radius - 8);
          const my2 = cy + Math.sin(mAngle) * (radius - 8);
          ctx.beginPath();
          ctx.moveTo(mx1, my1);
          ctx.lineTo(mx2, my2);
          ctx.strokeStyle = 'rgba(255,255,255,0.1)';
          ctx.lineWidth = 1;
          ctx.stroke();
        }
      }
    }

    // ── Needle ──
    const needleAngle = startAngle + speedRatio * totalSweep;
    drawNeedle(ctx, cx, cy, needleAngle, radius - 48, 22, '#00f0ff');
  }

  // ═══════════════════ RPM GAUGE (Canvas — Full Circle 270°) ═══════════════════
  function drawRPMGauge(ctx, w, h, rpmVal) {
    const cx = w / 2;
    const cy = h / 2;
    const radius = Math.min(w, h) / 2 - 32;

    ctx.clearRect(0, 0, w, h);

    const startAngle = (3 * Math.PI) / 4;
    const endAngle = (9 * Math.PI) / 4;   // 270° sweep (same as speedometer)
    const totalSweep = endAngle - startAngle;

    // ── Decorative outer elements ──
    drawOuterDecoRings(ctx, cx, cy, radius);

    // ── Outer glow ring ──
    ctx.beginPath();
    ctx.arc(cx, cy, radius + 8, startAngle, endAngle, false);
    ctx.strokeStyle = 'rgba(255, 107, 43, 0.04)';
    ctx.lineWidth = 18;
    ctx.stroke();

    // ── Background track ──
    ctx.beginPath();
    ctx.arc(cx, cy, radius, startAngle, endAngle, false);
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.04)';
    ctx.lineWidth = 14;
    ctx.lineCap = 'round';
    ctx.stroke();

    // ── Background segments ──
    for (let i = 0; i <= 48; i++) {
      const t = i / 48;
      const angle = startAngle + t * totalSweep;
      const x1 = cx + Math.cos(angle) * (radius - 1);
      const y1 = cy + Math.sin(angle) * (radius - 1);
      const x2 = cx + Math.cos(angle) * (radius + 1);
      const y2 = cy + Math.sin(angle) * (radius + 1);
      ctx.beginPath();
      ctx.moveTo(x1, y1);
      ctx.lineTo(x2, y2);
      ctx.strokeStyle = 'rgba(255,255,255,0.03)';
      ctx.lineWidth = 1;
      ctx.stroke();
    }

    // ── Active arc ──
    const rpmRatio = Math.min(rpmVal / CONFIG.gauges.rpm.max, 1);
    const activeEnd = startAngle + rpmRatio * totalSweep;

    const gradient = ctx.createConicGradient(startAngle, cx, cy);
    gradient.addColorStop(0, '#ff6b2b');
    gradient.addColorStop(0.5, '#ffd600');
    gradient.addColorStop(0.85, '#ff3d5a');
    gradient.addColorStop(1, '#ff3d5a');

    ctx.beginPath();
    ctx.arc(cx, cy, radius, startAngle, activeEnd, false);
    ctx.strokeStyle = gradient;
    ctx.lineWidth = 14;
    ctx.lineCap = 'round';
    ctx.stroke();

    // ── Active arc glow ──
    ctx.beginPath();
    ctx.arc(cx, cy, radius, startAngle, activeEnd, false);
    ctx.strokeStyle = 'rgba(255, 107, 43, 0.1)';
    ctx.lineWidth = 28;
    ctx.lineCap = 'round';
    ctx.stroke();

    ctx.beginPath();
    ctx.arc(cx, cy, radius, Math.max(startAngle, activeEnd - 0.3), activeEnd, false);
    ctx.strokeStyle = 'rgba(255, 107, 43, 0.2)';
    ctx.lineWidth = 20;
    ctx.lineCap = 'round';
    ctx.stroke();

    // ── Tick marks ──
    const majorTicks = 5; // 0, 100, 200, 300, 400
    for (let i = 0; i < majorTicks; i++) {
      const t = i / (majorTicks - 1);
      const angle = startAngle + t * totalSweep;
      const val = Math.round(t * CONFIG.gauges.rpm.max);

      const innerR = radius - 24;
      const outerR = radius - 8;

      const x1 = cx + Math.cos(angle) * innerR;
      const y1 = cy + Math.sin(angle) * innerR;
      const x2 = cx + Math.cos(angle) * outerR;
      const y2 = cy + Math.sin(angle) * outerR;

      ctx.beginPath();
      ctx.moveTo(x1, y1);
      ctx.lineTo(x2, y2);
      ctx.strokeStyle =
        val >= CONFIG.gauges.rpm.redZone
          ? 'rgba(255, 61, 90, 0.8)'
          : 'rgba(255, 255, 255, 0.3)';
      ctx.lineWidth = 2.5;
      ctx.stroke();

      const labelR = radius - 38;
      const lx = cx + Math.cos(angle) * labelR;
      const ly = cy + Math.sin(angle) * labelR;
      ctx.font = "700 11px 'Rajdhani', sans-serif";
      ctx.fillStyle =
        val >= CONFIG.gauges.rpm.redZone
          ? 'rgba(255, 61, 90, 0.9)'
          : 'rgba(255,255,255,0.45)';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText(val, lx, ly);

      // Minor ticks
      if (i < majorTicks - 1) {
        for (let j = 1; j < 4; j++) {
          const mt = (i + j / 4) / (majorTicks - 1);
          const mAngle = startAngle + mt * totalSweep;
          const mx1 = cx + Math.cos(mAngle) * (radius - 16);
          const my1 = cy + Math.sin(mAngle) * (radius - 16);
          const mx2 = cx + Math.cos(mAngle) * (radius - 8);
          const my2 = cy + Math.sin(mAngle) * (radius - 8);
          ctx.beginPath();
          ctx.moveTo(mx1, my1);
          ctx.lineTo(mx2, my2);
          ctx.strokeStyle = 'rgba(255,255,255,0.1)';
          ctx.lineWidth = 1;
          ctx.stroke();
        }
      }
    }

    // ── Needle ──
    const needleAngle = startAngle + rpmRatio * totalSweep;
    drawNeedle(ctx, cx, cy, needleAngle, radius - 48, 22, '#ff6b2b');
  }

  // ═══════════════════ ANIMATION LOOP ═══════════════════
  function lerp(current, target, factor) {
    return current + (target - current) * factor;
  }

  function animationLoop() {
    // Smooth interpolation — higher factor = faster response
    state.speed = lerp(state.speed, state.targetSpeed, 0.25);
    state.rpm = lerp(state.rpm, state.targetRpm, 0.25);
    state.soc = lerp(state.soc, state.targetSoc, 0.15);

    // Update digital readouts
    DOM.speedDigital.textContent = Math.round(state.speed);
    DOM.rpmDigital.textContent = Math.round(state.rpm);

    // Draw gauges
    const sCtx = DOM.speedCanvas.getContext('2d');
    const rCtx = DOM.rpmCanvas.getContext('2d');

    // Handle HiDPI
    drawHiDPI(DOM.speedCanvas, sCtx, (ctx, w, h) => {
      drawSpeedometer(ctx, w, h, state.speed);
    });

    drawHiDPI(DOM.rpmCanvas, rCtx, (ctx, w, h) => {
      drawRPMGauge(ctx, w, h, state.rpm);
    });

    // Update battery SOC
    updateBatterySOC();

    requestAnimationFrame(animationLoop);
  }

  // HiDPI canvas helper
  const canvasStates = new WeakMap();
  function drawHiDPI(canvas, ctx, drawFn) {
    const dpr = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    const w = rect.width;
    const h = rect.height;

    // Only resize if needed
    const prev = canvasStates.get(canvas);
    if (!prev || prev.w !== w || prev.h !== h || prev.dpr !== dpr) {
      canvas.width = w * dpr;
      canvas.height = h * dpr;
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      canvasStates.set(canvas, { w, h, dpr });
    }

    drawFn(ctx, w, h);
  }

  // ═══════════════════ MOTOR CONTROLS ═══════════════════
  function updatePWMDisplay() {
    const pct = parseInt(DOM.pwmSlider.value);     // slider is 0–100 %
    const pwm255 = Math.round((pct / 100) * 255);  // ESP32 expects 0–255
    state.pwm = pwm255;
    DOM.pwmDisplay.innerHTML = pct + '<small>%</small>';
    DOM.pwmFill.style.width = pct + '%';
    publishMQTT(CONFIG.mqtt.topics.pwm, pwm255);
  }

  function updateFreqDisplay() {
    const val = parseInt(DOM.freqSlider.value);
    state.frequency = val;
    const display = val >= 1000 ? (val / 1000).toFixed(1) + 'k' : val;
    DOM.freqDisplay.innerHTML = display + '<small>Hz</small>';
    const min = parseInt(DOM.freqSlider.min);
    const max = parseInt(DOM.freqSlider.max);
    const pct = ((val - min) / (max - min)) * 100;
    DOM.freqFill.style.width = pct + '%';
    publishMQTT(CONFIG.mqtt.topics.freqctrl, val);
  }

  function setDirection(dir) {
    state.direction = dir;

    // Update center indicator
    DOM.dirForward.classList.toggle('active', dir === 'forward');
    DOM.dirBackward.classList.toggle('active', dir === 'backward');
    DOM.dirStatus.textContent = dir.toUpperCase();
    DOM.dirStatus.className = 'dir-status ' + (dir === 'neutral' ? '' : dir);

    // Update motor panel buttons
    DOM.btnForward.classList.toggle('active', dir === 'forward');
    DOM.btnBackward.classList.toggle('active', dir === 'backward');
    DOM.btnStop.classList.toggle('active', dir === 'neutral');

    // Update display
    DOM.directionDisplay.textContent = dir.toUpperCase();

    // Publish as JSON to vehicle/command (matches ESP32 callback)
    const dirMap = { forward: 'FORWARD', backward: 'BACKWARD', neutral: 'STOP' };
    const cmdJson = JSON.stringify({
      direction: dirMap[dir] || 'STOP',
      pwm: state.pwm,
      freq: state.frequency,
    });
    publishMQTT(CONFIG.mqtt.topics.command, cmdJson);
  }

  // ═══════════════════ EVENT LISTENERS ═══════════════════
  function setupListeners() {
    // PWM Slider
    DOM.pwmSlider.addEventListener('input', updatePWMDisplay);

    // Frequency Slider
    DOM.freqSlider.addEventListener('input', updateFreqDisplay);

    // Direction buttons
    DOM.btnForward.addEventListener('click', () => setDirection('forward'));
    DOM.btnBackward.addEventListener('click', () => setDirection('backward'));
    DOM.btnStop.addEventListener('click', () => setDirection('neutral'));

    // Center direction indicators (clickable)
    DOM.dirForward.addEventListener('click', () => setDirection('forward'));
    DOM.dirBackward.addEventListener('click', () => setDirection('backward'));

    // ── KEYBOARD CONTROLS ──
    document.addEventListener('keydown', (e) => {
      // Don't interfere with input fields
      if (e.target.tagName === 'INPUT') return;

      switch (e.key) {
        case 'ArrowUp':
          e.preventDefault();
          setDirection('forward');
          break;
        case 'ArrowDown':
          e.preventDefault();
          setDirection('backward');
          break;
        case ' ':
          e.preventDefault();
          setDirection('neutral');
          break;
      }
    });

    // MQTT config toggle
    DOM.mqttConfigToggle.addEventListener('click', () => {
      DOM.mqttConfigToggle.classList.toggle('open');
      DOM.mqttConfigBody.classList.toggle('open');
    });

    // Reconnect
    DOM.btnReconnect.addEventListener('click', connectMQTT);

    // Initialize displays
    updatePWMDisplay();
    updateFreqDisplay();
    setDirection('neutral');
  }

  // ═══════════════════ SIMULATION (for demo) ═══════════════════
  // Simulates sensor data when no MQTT messages arrive
  let simInterval = null;
  let lastMqttReceive = 0;    // updated in the message handler

  function startSimulation() {
    simInterval = setInterval(() => {
      // If connected and receiving data, skip simulation
      if (state.mqttConnected && Date.now() - lastMqttReceive < 5000) return;

      // Demo mode: gentle animations matching sensor ranges
      const t = Date.now() / 1000;
      state.targetSpeed = 4 + Math.sin(t * 0.3) * 2 + Math.sin(t * 0.7) * 1;
      state.targetRpm = 150 + Math.sin(t * 0.4) * 80 + Math.sin(t * 0.9) * 40;
      state.targetSoc = 72 + Math.sin(t * 0.05) * 8;
      state.voltage = 8 + Math.sin(t * 0.2) * 3;       // ~5–11V range
      state.current = 5 + Math.sin(t * 0.35) * 3;      // ~2–8A range
      state.temperature = 35 + Math.sin(t * 0.15) * 8;  // ~27–43°C

      updateBatteryVoltage();
      updateBatteryCurrent();
      updateBatteryTemp();
    }, 100);
  }

  // ═══════════════════ INIT ═══════════════════
  function init() {
    setupListeners();
    connectMQTT();
    animationLoop();

    // Start demo simulation after a short delay
    setTimeout(startSimulation, 2000);
  }

  // Wait for DOM
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
