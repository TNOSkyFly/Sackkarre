#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "driver/mcpwm.h"

// =======================
// Konfiguration & Pins
// =======================
#define PIN_HIN 18
#define PIN_LIN 19
#define ZUENDSCHALTER_PIN 5
#define GASPEDAL_PIN 33
#define SDA_PIN 21
#define SCL_PIN 22
#define TMP112_ADDR 0x48

WebServer server(80);

enum MotorState { MOTOR_AUS = 0, ZUENDUNG_AN = 1, MOTOR_EIN = 2, SICHERHEITS_SPERRE = 3 };
MotorState curState = MOTOR_AUS;

// --- Regelungsparameter ---
int targetPWM = 0;   
int currentPWM = 0;  
const int MAX_PWM_LIMIT = 216; 
const int START_PWM_VALUE = 30; 
const float RAMP_STEP = 1.0;   

// --- Zeitsteuerung & Sensorik ---
unsigned long RESTART_DELAY_MS = 500; 
unsigned long lastStopMillis = 0; 
unsigned long lastTempMillis = 0;
float aktuelleTemperatur = 0.0;

int readings[15]; 
int readIndex = 0;
long total = 0;

// =======================
// HTML Dashboard
// =======================
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="UTF-8">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
    <meta name="mobile-web-app-capable" content="yes">
    <style>
        body { 
            font-family: 'Segoe UI', sans-serif; background: #000; color: #eee; 
            margin: 0; padding: 15px; text-align: center; 
            transition: background 0.5s; overflow-x: hidden;
            -webkit-user-select: none; -ms-user-select: none; user-select: none; 
        }
        body.alarm { background: #440000; }
        
        .vmax-container { display: flex; justify-content: center; align-items: center; gap: 10px; background: rgba(255, 255, 255, 0.05); padding: 5px 15px; border-radius: 20px; width: fit-content; margin: 0 auto 15px auto; }
        .vmax-label { color: #ffcc00; font-size: 0.9rem; font-weight: bold; text-transform: uppercase; }
        
        .btn-reset { 
            background: #222; border: 1px solid #444; color: #eee; padding: 2px 10px; border-radius: 12px; font-size: 0.7rem; 
            cursor: pointer; text-transform: uppercase; -webkit-tap-highlight-color: transparent; outline: none; 
        }
        .btn-reset:active { background: #ff4444; color: #fff; }

        .speed-box { margin: 10px 0 40px 0; padding: 20px 0; }
        #speed { font-size: 6.5rem; font-weight: 900; color: #39ff14; line-height: 1; text-shadow: 0 0 25px rgba(57,255,20,0.4); font-family: monospace; }
        .unit { color: #eee; font-size: 1rem; font-weight: bold; letter-spacing: 3px; text-transform: uppercase; }
        
        .diag-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin: 20px 0; }
        .diag-card { background: #111; border: 1px solid #222; padding: 12px; border-radius: 15px; }
        .diag-val { font-size: 1.6rem; font-weight: bold; display: block; margin: 5px 0; font-family: monospace; color: #fff; }
        
        /* FIX: Einheiten (°C und %) groß und weiß */
        .diag-val small { 
            font-size: 1.2rem !important; 
            color: #fff !important; 
            margin-left: 4px; 
            font-weight: bold;
        }
        
        .led-box { display: flex; justify-content: center; margin-top: 8px; }
        .led { width: 20px; height: 20px; border-radius: 50%; background: #222; transition: all 0.1s; box-shadow: inset 0 0 5px #000; border: 1px solid #333; }
        .led-hin.active { background: #00ffcc; box-shadow: 0 0 20px #00ffcc, inset 0 0 5px #fff; }
        .led-lin.active { background: #ff4444; box-shadow: 0 0 20px #ff4444, inset 0 0 5px #fff; }

        .bar-wrap { width: 100%; height: 12px; background: #1a1a1a; border-radius: 6px; margin: 10px 0; overflow: hidden; border: 1px solid #222; }
        #p-bar { height: 100%; width: 0%; background: linear-gradient(90deg, #00aa88, #39ff14); transition: width 0.1s; }
        
        .footer { border-top: 1px solid #222; padding-top: 15px; margin-top: 25px; font-weight: bold; }
        .state-0 { color: #ff4444; } .state-1 { color: #ffcc00; } .state-2 { color: #00ffcc; }
        #lock-msg { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(255, 0, 0, 0.95); z-index: 1000; flex-direction: column; justify-content: center; align-items: center; color: white; }
    </style>
</head>
<body>
    <div id="lock-msg">
        <h1 style="font-size:3rem; margin:0;">FU&szlig; VOM GAS!</h1>
        <p style="font-size:1.2rem;">SICHERHEITS-SPERRE AKTIV</p>
    </div>

    <div class="vmax-container">
        <span class="vmax-label">Top: <span id="vmax">0.0</span> km/h</span>
        <button class="btn-reset" onclick="vmax=0">Reset</button>
        <button class="btn-reset" onclick="toggleFullScreen()">Fullscreen</button>
    </div>

    <div class="speed-box">
        <div id="speed">0</div>
        <div class="unit">KM/H</div>
    </div>

    <div class="diag-card" style="margin-bottom:20px;"> 
        <span class="unit">SYSTEM TEMP</span>
        <div class="diag-val">
            <span id="temp" style="color:#00ffcc">--</span><small>°C</small>
        </div>
    </div>

    <div class="diag-grid">
        <div class="diag-card">
            <span class="unit">HIN (G18)</span>
            <span id="v18" class="diag-val">0</span>
            <div class="led-box"><div id="l18" class="led led-hin"></div></div>
        </div>
        <div class="diag-card">
            <span class="unit">LIN (G19)</span>
            <span id="v19" class="diag-val">--</span>
            <div class="led-box"><div id="l19" class="led led-lin"></div></div>
        </div>
    </div>

    <div class="diag-card">
        <span class="unit">POWER</span>
        <div class="diag-val"><span id="p-txt">0</span><small>%</small></div>
        <div class="bar-wrap"><div id="p-bar"></div></div>
    </div>

    <div class="footer">STATUS: <span id="st-txt">---</span></div>

    <script>
        let vmax = 0; let lastAlarm = 0;
        if ("geolocation" in navigator) {
            navigator.geolocation.watchPosition(p => {
                let s = (p.coords.speed * 3.6); if (s < 0 || isNaN(s)) s = 0;
                document.getElementById('speed').innerHTML = Math.floor(s);
                if (s > vmax) { vmax = s; document.getElementById('vmax').innerHTML = vmax.toFixed(1); }
            }, null, {enableHighAccuracy:true});
        }
        setInterval(() => {
            fetch('/status').then(r => r.json()).then(d => {
                const states = ["AUS", "BEREIT", "AKTIV", "SPERRE"];
                document.getElementById('st-txt').innerHTML = states[d.s];
                document.getElementById('st-txt').className = "state-" + d.s;
                document.getElementById('lock-msg').style.display = (d.s == 3) ? "flex" : "none";

                const isHot = d.t >= 80;
                document.body.classList.toggle('alarm', isHot);
                document.getElementById('temp').innerHTML = d.t.toFixed(1);
                document.getElementById('temp').style.color = isHot ? "#ff4444" : "#00ffcc";

                if (isHot && Date.now() - lastAlarm > 2000) { 
                    if ("vibrate" in navigator) navigator.vibrate([400, 100, 400]); 
                    lastAlarm = Date.now(); 
                }

                let p_perc = Math.round((d.p / 255) * 100);
                document.getElementById('p-txt').innerHTML = p_perc;
                document.getElementById('p-bar').style.width = p_perc + "%";
                document.getElementById('v18').innerHTML = d.p;
                document.getElementById('l18').classList.toggle('active', d.p > 0);
                document.getElementById('v19').innerHTML = (d.p == 0) ? "HIGH" : "LOW";
                document.getElementById('l19').classList.toggle('active', d.p == 0);
            });
        }, 200);
        
        function toggleFullScreen() {
            if (!document.fullscreenElement) {
                document.documentElement.requestFullscreen().catch(err => {
                    alert(`Fehler: ${err.message}`);
                });
            } else {
                document.exitFullscreen();
            }
        }
    </script>
</body>
</html>
)rawliteral";

// =======================
// Steuerung & Logik (ESP32)
// =======================

void read_temp_sensor() {
    Wire.beginTransmission(TMP112_ADDR); Wire.write(0x00);
    if (Wire.endTransmission() != 0) return;
    Wire.requestFrom(TMP112_ADDR, 2);
    if (Wire.available() >= 2) {
        int16_t val = (Wire.read() << 4) | (Wire.read() >> 4);
        if (val > 0x7FF) val |= 0xF000;
        aktuelleTemperatur = val * 0.0625;
    }
}

bool get_gas(int &pwm) {
    total = total - readings[readIndex];
    readings[readIndex] = analogRead(GASPEDAL_PIN);
    total = total + readings[readIndex];
    readIndex = (readIndex + 1) % 15;
    int avg = (int)(total / 15);
    if (avg < 750) { pwm = 0; return false; }
    pwm = map(avg, 750, 3183, START_PWM_VALUE, MAX_PWM_LIMIT);
    pwm = constrain(pwm, START_PWM_VALUE, MAX_PWM_LIMIT);
    return true;
}

void setup() {
    Serial.begin(115200); Wire.begin(SDA_PIN, SCL_PIN);
    WiFi.softAP("Sackkarre", "12345678");
    pinMode(ZUENDSCHALTER_PIN, INPUT_PULLUP);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, PIN_HIN);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, PIN_LIN);
    mcpwm_config_t pc; pc.frequency = 5000; pc.cmpr_a = 0;
    pc.counter_mode = MCPWM_UP_COUNTER; pc.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pc);
    mcpwm_deadtime_enable(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, 80, 80);
    server.on("/", [](){ server.send_P(200, "text/html", DASHBOARD_HTML); });
    server.on("/status", [](){
        server.send(200, "application/json", "{\"s\":" + String((int)curState) + ",\"p\":" + String(currentPWM) + ",\"t\":" + String(aktuelleTemperatur) + "}");
    });
    server.begin();
}

void loop() {
    server.handleClient();
    if (millis() - lastTempMillis > 1000) { read_temp_sensor(); lastTempMillis = millis(); }

    int pIn = 0;
    bool active = get_gas(pIn);
    bool sw = !digitalRead(ZUENDSCHALTER_PIN);
    int currentLimit = (aktuelleTemperatur >= 90.0) ? 0 : (aktuelleTemperatur >= 80.0 ? MAX_PWM_LIMIT / 2 : MAX_PWM_LIMIT);

    switch (curState) {
        case MOTOR_AUS:
            targetPWM = 0; currentPWM = 0;
            if (sw) {
                if (active) curState = SICHERHEITS_SPERRE;
                else if (aktuelleTemperatur < 80.0) curState = ZUENDUNG_AN;
            }
            break;

        case SICHERHEITS_SPERRE:
            targetPWM = 0; currentPWM = 0;
            if (!sw) curState = MOTOR_AUS;
            if (!active) curState = ZUENDUNG_AN;
            break;

        case ZUENDUNG_AN:
            targetPWM = 0; currentPWM = 0;
            if (!sw) curState = MOTOR_AUS;
            else if (active && aktuelleTemperatur < 90.0) {
                if (millis() - lastStopMillis >= RESTART_DELAY_MS) {
                    curState = MOTOR_EIN; currentPWM = START_PWM_VALUE;
                }
            }
            break;

        case MOTOR_EIN:
            if (!sw || aktuelleTemperatur >= 90.0) { 
                targetPWM = 0; currentPWM = 0; 
                curState = (aktuelleTemperatur >= 90.0) ? ZUENDUNG_AN : MOTOR_AUS; 
                lastStopMillis = millis(); 
            }
            else if (!active) {
                targetPWM = 0;
                if (currentPWM <= START_PWM_VALUE) { curState = ZUENDUNG_AN; currentPWM = 0; lastStopMillis = millis(); }
            } else { targetPWM = min(pIn, currentLimit); }
            break;
    }

    if (currentPWM < targetPWM) {
        currentPWM += RAMP_STEP; if (currentPWM > targetPWM) currentPWM = targetPWM;
    } else if (currentPWM > targetPWM) {
        currentPWM -= (RAMP_STEP * 3.0); if (currentPWM < targetPWM) currentPWM = targetPWM;
        if (currentPWM < (START_PWM_VALUE - 5) && targetPWM == 0) currentPWM = 0;
    }
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, (currentPWM / 255.0) * 100.0);
    delay(10);
}