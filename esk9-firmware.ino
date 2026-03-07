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
#define UADC_PIN 32                 // Spannung Batterie (geteilt)
#define TMP112_ADDR 0x48

WebServer server(80);

enum MotorState { MOTOR_AUS = 0, ZUENDUNG_AN = 1, MOTOR_EIN = 2, SICHERHEITS_SPERRE = 3 };
MotorState curState = MOTOR_AUS;

// --- Regelungsparameter ---
int targetPWM = 0;   
int currentPWM = 0;  
int minPoti = 4095;       
int maxPoti = 0;          
int lastLoggedMin = 4095; 
int lastLoggedMax = 0;

int R11 = 56000;        
int R12 = 10000;
int UZ = 20;            // Spannung Zehnerdiode
float UBat = 0.0;

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
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <meta charset="UTF-8">
    <style>
        body { 
            font-family: 'Segoe UI', sans-serif; background: #000; color: #eee; 
            margin: 0; padding: 15px; text-align: center; 
            transition: background 0.5s; overflow-x: hidden;
            -webkit-user-select: none; user-select: none; 
        }
        body.alarm { background: #440000; }
        
        .vmax-container { display: flex; justify-content: center; align-items: center; gap: 10px; background: rgba(255, 255, 255, 0.05); padding: 8px 15px; border-radius: 25px; width: fit-content; margin: 0 auto 15px auto; border: 1px solid rgba(255,255,255,0.1); }
        .vmax-label { color: #ffcc00; font-size: 0.9rem; font-weight: bold; text-transform: uppercase; }
        
        .btn-reset { 
            background: rgba(255,255,255,0.1); border: 1px solid rgba(255,255,255,0.2); color: #eee; 
            padding: 6px 16px; border-radius: 15px; font-size: 0.75rem; font-weight: bold;
            cursor: pointer; text-transform: uppercase; outline: none; transition: all 0.2s;
            -webkit-tap-highlight-color: transparent; touch-action: manipulation;
        }
        .btn-reset:active { background: #39ff14; color: #000; transform: scale(0.95); }

        .speed-box { margin: 10px 0 30px 0; padding: 15px 0; }
        #speed { font-size: 7rem; font-weight: 900; color: #39ff14; line-height: 1; text-shadow: 0 0 30px rgba(57,255,20,0.5); font-family: monospace; }
        .unit { color: #888; font-size: 0.9rem; font-weight: bold; letter-spacing: 3px; text-transform: uppercase; }
        
        .diag-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin: 20px 0; }
        .diag-card { background: #0a0a0a; border: 1px solid #222; padding: 12px; border-radius: 18px; box-shadow: 0 4px 15px rgba(0,0,0,0.5); }
        .diag-val { font-size: 1.6rem; font-weight: bold; display: block; margin: 5px 0; font-family: monospace; color: #fff; }
        
        .state-0 { color: #ff4444 !important; }
        .state-1 { color: #ffcc00 !important; }
        .state-2 { color: #00ffcc !important; }
        .state-3 { color: #ffffff; text-shadow: 0 0 10px red; }
        
        .led-box { display: flex; justify-content: center; margin-top: 8px; }
        .led { width: 20px; height: 20px; border-radius: 50%; background: #111; border: 2px solid #333; transition: all 0.1s; }
        .led-hin.active { background: #00ffcc; box-shadow: 0 0 15px #00ffcc; border-color: #fff; }
        .led-lin.active { background: #ff4444; box-shadow: 0 0 15px #ff4444; border-color: #fff; }

        .bar-wrap { width: 100%; height: 10px; background: #111; border-radius: 5px; margin: 10px 0; overflow: hidden; border: 1px solid #333; }
        #p-bar { height: 100%; width: 0%; background: linear-gradient(90deg, #00aa88, #39ff14); transition: width 0.1s; }
        
        .footer { border-top: 1px solid #222; padding-top: 20px; margin-top: 25px; font-weight: bold; letter-spacing: 1px; }
        #lock-msg { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(150, 0, 0, 0.9); z-index: 1000; flex-direction: column; justify-content: center; align-items: center; color: white; backdrop-filter: blur(5px); }
    </style>
</head>
<body>
    <div id="lock-msg">
        <h1 style="font-size:3.5rem; margin:0; text-shadow: 0 0 20px #000;">Sicherheitssperre</h1>
        <p style="font-size:1.2rem; font-weight: bold;">Fuß vom Gas!</p>
    </div>

    <div class="vmax-container">
        <span class="vmax-label">Top: <span id="vmax">0.0</span></span>
        <button class="btn-reset" onclick="resetVmax()">Reset</button>
        <button class="btn-reset" onclick="toggleFullScreen()">Full</button>
    </div>

    <div class="speed-box">
        <div id="speed">0</div>
        <div class="unit">KM/H</div>
    </div>

    <div class="diag-grid">
        <div class="diag-card"> 
            <span class="unit">TEMP</span>
            <div class="diag-val" id="temp-val">--<small>°C</small></div>
        </div>
        <div class="diag-card"> 
            <span class="unit">AKKU 36V</span>
            <div class="diag-val"><span id="bat-val">--</span><small>V</small></div>
            <div class="bar-wrap"><div id="bat-bar" style="height:100%; width:0%; background: #00ffcc; transition: width 0.8s;"></div></div>
            <div style="font-size: 0.75rem;"><span id="bat-perc">0</span>%</div>
        </div>
    </div>

    <div class="diag-grid">
        <div class="diag-card">
            <span class="unit">MOSFET 1</span>
            <span id="v18" class="diag-val">0</span>
            <div class="led-box"><div id="l18" class="led led-hin"></div></div>
        </div>
        <div class="diag-card">
            <span class="unit">MOSFET 2</span>
            <span id="v19" class="diag-val">--</span>
            <div class="led-box"><div id="l19" class="led led-lin"></div></div>
        </div>
    </div>

    <div class="diag-card">
        <span class="unit">MOTOR POWER</span>
        <div class="diag-val"><span id="p-txt">0</span><small>%</small></div>
        <div class="bar-wrap"><div id="p-bar"></div></div>
    </div>

    <div class="footer">STATUS: <span id="st-txt">VERBINDUNG...</span></div>

    <script>
        let vmax = 0; 
        let speedHistory = [0, 0, 0, 0, 0, 0, 0, 0];

        function resetVmax() {
            vmax = 0;
            speedHistory.fill(0);
            document.getElementById('vmax').innerHTML = "0.0";
            if ("vibrate" in navigator) navigator.vibrate(40);
        }

        if ("geolocation" in navigator) {
            navigator.geolocation.watchPosition(p => {
                let s = (p.coords.speed * 3.6) || 0;
                speedHistory.push(s);
                speedHistory.shift();
                let avg = speedHistory.reduce((a,b) => a+b) / 8;
                let display = avg < 0.5 ? 0 : Math.round(avg);
                document.getElementById('speed').innerHTML = display;
                if (avg > vmax) { vmax = avg; document.getElementById('vmax').innerHTML = vmax.toFixed(1); }
            }, null, { enableHighAccuracy: true });
        }

        setInterval(() => {
            fetch('/status').then(r => r.json()).then(d => {
                const states = ["ZÜNDUNG AUS", "BEREIT", "AKTIV", "SPERRE"];
                document.getElementById('st-txt').innerHTML = states[d.s];
                document.getElementById('st-txt').className = "state-" + d.s;
                document.getElementById('lock-msg').style.display = (d.s == 3) ? "flex" : "none";
                
                // Temp
                const tVal = document.getElementById('temp-val');
                tVal.innerHTML = d.t.toFixed(1) + "<small>°C</small>";
                tVal.style.color = d.t >= 80 ? "#ff4444" : "#00ffcc";

                // Akku 36V Logik
                let volt = d.v;
                let perc = Math.round((volt - 32) * (100 / (41 - 32)));
                perc = Math.max(0, Math.min(100, perc));
                
                const bVal = document.getElementById('bat-val');
                const bBar = document.getElementById('bat-bar');
                bVal.innerHTML = volt.toFixed(1);
                document.getElementById('bat-perc').innerHTML = perc;
                bBar.style.width = perc + "%";
                
                if(perc < 15) bBar.style.background = "#ff4444";
                else if(perc < 40) bBar.style.background = "#ffcc00";
                else bBar.style.background = "#39ff14";

                // PWM
                let p_perc = Math.round((d.p / 255) * 100);
                document.getElementById('p-txt').innerHTML = p_perc;
                document.getElementById('p-bar').style.width = p_perc + "%";
                document.getElementById('v18').innerHTML = d.p;
                document.getElementById('l18').classList.toggle('active', d.p > 0);
                document.getElementById('v19').innerHTML = (d.p == 0) ? "HIGH" : "LOW";
                document.getElementById('l19').classList.toggle('active', d.p == 0);
            }).catch(e => { document.getElementById('st-txt').innerHTML = "OFFLINE"; });
        }, 200);

        function toggleFullScreen() {
            if (!document.fullscreenElement) document.documentElement.requestFullscreen();
            else document.exitFullscreen();
        }
    </script>
</body>
</html>
)rawliteral";

// =======================
// Steuerung & Logik
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

float read_battery_voltage() {
    int rawADC = analogRead(UADC_PIN); 
    float pinVoltage = rawADC * (3.3 / 4095.0);
    float currentVolt = (pinVoltage * (float(R11 + R12) / R12)) + UZ;
    if (rawADC < 10) return 0.0;
    static float smoothedVolt = 0.0;
    if (smoothedVolt < 1.0) smoothedVolt = currentVolt;
    smoothedVolt = (smoothedVolt * 0.95) + (currentVolt * 0.05);
    return smoothedVolt;
}

bool get_gas(int &pwm) {
    int rawValue = analogRead(GASPEDAL_PIN);
    if (rawValue < minPoti) minPoti = rawValue;
    if (rawValue > maxPoti) maxPoti = rawValue;
    if (rawValue < 50 || rawValue > 4050) { pwm = 0; return false; }
    total = total - readings[readIndex];
    readings[readIndex] = rawValue;
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
    mcpwm_deadtime_enable(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, 10, 10);
    
    server.on("/", [](){ server.send_P(200, "text/html", DASHBOARD_HTML); });
    server.on("/status", [](){
        server.send(200, "application/json", "{\"s\":" + String((int)curState) + ",\"p\":" + String(currentPWM) + ",\"t\":" + String(aktuelleTemperatur) + ",\"v\":" + String(UBat) + "}");
    });
    server.begin();
}

void loop() {
    server.handleClient();
    if (millis() - lastTempMillis > 1000) { 
        read_temp_sensor();
        UBat = read_battery_voltage();
        lastTempMillis = millis(); 
    }

    int pIn = 0;
    bool active = get_gas(pIn);
    bool sw = !digitalRead(ZUENDSCHALTER_PIN);
    bool lowBat = (UBat < 31.5 && UBat > 5.0); 
    int currentLimit = (aktuelleTemperatur >= 90.0) ? 0 : (aktuelleTemperatur >= 80.0 ? MAX_PWM_LIMIT / 2 : MAX_PWM_LIMIT);

    switch (curState) {
        case MOTOR_AUS:
            targetPWM = 0; currentPWM = 0;
            if (sw) {
                if (active) curState = SICHERHEITS_SPERRE;
                else if (aktuelleTemperatur < 80.0 && !lowBat) curState = ZUENDUNG_AN;
            }
            break;

        case SICHERHEITS_SPERRE:
            targetPWM = 0; currentPWM = 0;
            if (!sw) curState = MOTOR_AUS;
            if (!active) curState = ZUENDUNG_AN;
            break;

        case ZUENDUNG_AN:
            targetPWM = 0; currentPWM = 0;
            if (!sw || lowBat) curState = MOTOR_AUS;
            else if (active && aktuelleTemperatur < 90.0) {
                if (millis() - lastStopMillis >= RESTART_DELAY_MS) {
                    curState = MOTOR_EIN; currentPWM = START_PWM_VALUE;
                }
            }
            break;

        case MOTOR_EIN:
            if (!sw || aktuelleTemperatur >= 90.0 || lowBat) { 
                targetPWM = 0; currentPWM = 0; 
                curState = (lowBat || aktuelleTemperatur >= 90.0) ? SICHERHEITS_SPERRE : MOTOR_AUS; 
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