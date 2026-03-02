#include <WiFi.h>
#include <WebServer.h>
#include "driver/mcpwm.h"

// =======================
// Konfiguration & Pins
// =======================
#define PIN_HIN 18
#define PIN_LIN 19
#define ZUENDSCHALTER_PIN 5
#define GASPEDAL_PIN 33

WebServer server(80);

enum MotorState { MOTOR_AUS = 0, ZUENDUNG_AN = 1, MOTOR_EIN = 2 };
MotorState curState = MOTOR_AUS;

// --- Regelungsparameter ---
int targetPWM = 0;   
int currentPWM = 0;  
const int MAX_PWM_LIMIT = 216; 
const int START_PWM_VALUE = 30; 
const float RAMP_STEP = 1.0;   

// --- NEU: Sperrzeit-Variable (in Millisekunden) ---
// Setze diesen Wert auf 0, um die Sperre komplett zu deaktivieren.
unsigned long RESTART_DELAY_MS = 500; 
unsigned long lastStopMillis = 0; // Merker, wann der Motor auf 0 ging

int readings[15]; 
int readIndex = 0;
long total = 0;

// =======================
// HTML Dashboard (Final)
// =======================
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="UTF-8">
    <style>
        body { font-family: sans-serif; background: #000; color: #eee; margin: 0; padding: 15px; text-align: center; }
        .vmax-label { color: #ffcc00; font-size: 1rem; margin-bottom: 10px; }
        .speed-box { margin: 10px 0; padding: 20px 0; }
        #speed { font-size: 6rem; font-weight: bold; color: #39ff14; line-height: 1; text-shadow: 0 0 20px rgba(57,255,20,0.3); }
        .unit { color: #555; font-size: 1.2rem; letter-spacing: 3px; }
        .diag-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin: 20px 0; }
        .diag-card { background: #111; border: 1px solid #222; padding: 10px; border-radius: 10px; }
        .diag-val { font-size: 1.5rem; font-weight: bold; display: block; margin: 5px 0; }
        .led { width: 100px; height: 10px; border-radius: 5px; display: inline-block; background: #333; margin-top: 5px; }
        .led-hin { background: #00ffcc; box-shadow: 0 0 8px #00ffcc; }
        .led-lin { background: #ff4444; box-shadow: 0 0 8px #ff4444; }
        .bar-wrap { width: 100%; height: 8px; background: #222; border-radius: 4px; margin: 10px 0; overflow: hidden; }
        #p-bar { height: 100%; width: 0%; background: #00ffcc; transition: width 0.1s; }
        .footer { border-top: 1px solid #222; padding-top: 15px; margin-top: 20px; font-weight: bold; }
        .state-0 { color: #ff4444; } .state-1 { color: #ffcc00; } .state-2 { color: #00ffcc; }
        button { background: #222; border: 1px solid #444; color: #888; padding: 5px 10px; border-radius: 4px; font-size: 0.8rem; }
    </style>
</head>
<body>
    <div class="vmax-label">VMAX: <span id="vmax">0.0</span> km/h <button onclick="vmax=0">RESET</button></div>
    <div class="speed-box"><div id="speed">0</div><div class="unit">KM/H</div></div>
    <div class="diag-grid">
        <div class="diag-card"><span class="unit">G18 (HIN)</span><span id="v18" class="diag-val">0</span><div id="l18" class="led"></div></div>
        <div class="diag-card"><span class="unit">G19 (LIN)</span><span id="v19" class="diag-val">LOW</span><div id="l19" class="led"></div></div>
    </div>
    <div class="diag-card" style="width: auto;"><span class="unit">Gesamtleistung</span><div class="diag-val"><span id="p-txt">0</span>%</div><div class="bar-wrap"><div id="p-bar"></div></div></div>
    <div class="footer">STATUS: <span id="st-txt" class="state-0">OFF</span></div>
    <script>
        let vmax = 0;
        if ("geolocation" in navigator) {
            navigator.geolocation.watchPosition(p => {
                let s = (p.coords.speed * 3.6);
                if (s < 0 || isNaN(s)) s = 0;
                document.getElementById('speed').innerHTML = Math.floor(s);
                if (s > vmax) vmax = s;
                document.getElementById('vmax').innerHTML = vmax.toFixed(1);
            }, null, {enableHighAccuracy:true});
        }
        setInterval(() => {
            fetch('/status').then(r => r.json()).then(d => {
                const states = ["AUS", "BEREIT", "AKTIV"];
                document.getElementById('st-txt').innerHTML = states[d.s];
                document.getElementById('st-txt').className = "state-" + d.s;
                let p_perc = Math.round((d.p / 255) * 100);
                document.getElementById('p-txt').innerHTML = p_perc;
                document.getElementById('p-bar').style.width = p_perc + "%";
                document.getElementById('v18').innerHTML = d.p;
                document.getElementById('l18').className = d.p > 0 ? "led led-hin" : "led";
                let linActive = (d.p == 0);
                document.getElementById('v19').innerHTML = linActive ? "HIGH" : "LOW";
                document.getElementById('l19').className = linActive ? "led led-lin" : "led";
            });
        }, 200);
    </script>
</body>
</html>
)rawliteral";

// =======================
// Steuerung & Logik
// =======================

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
    Serial.begin(115200);
    WiFi.softAP("ESP32-MOTOR", "12345678");
    pinMode(ZUENDSCHALTER_PIN, INPUT_PULLUP);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, PIN_HIN);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, PIN_LIN);
    mcpwm_config_t pc;
    pc.frequency = 5000; pc.cmpr_a = 0;
    pc.counter_mode = MCPWM_UP_COUNTER; pc.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pc);
    mcpwm_deadtime_enable(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, 80, 80);
    server.on("/", [](){ server.send_P(200, "text/html", DASHBOARD_HTML); });
    server.on("/status", [](){
        String j = "{\"s\":" + String((int)curState) + ",\"p\":" + String(currentPWM) + "}";
        server.send(200, "application/json", j);
    });
    server.begin();
}

void loop() {
    server.handleClient();
    int pIn = 0;
    bool active = get_gas(pIn);
    bool sw = !digitalRead(ZUENDSCHALTER_PIN);

    switch (curState) {
        case MOTOR_AUS:
            targetPWM = 0; currentPWM = 0;
            if (sw) curState = ZUENDUNG_AN;
            break;

        case ZUENDUNG_AN:
            targetPWM = 0; currentPWM = 0;
            if (!sw) curState = MOTOR_AUS;
            else if (active) { 
                // Prüfen, ob die Sperrzeit abgelaufen ist
                if (millis() - lastStopMillis >= RESTART_DELAY_MS) {
                    curState = MOTOR_EIN; 
                    currentPWM = START_PWM_VALUE; 
                }
            }
            break;

        case MOTOR_EIN:
            if (!sw) { curState = MOTOR_AUS; targetPWM = 0; }
            else if (!active) { 
                targetPWM = 0;
                if (currentPWM <= START_PWM_VALUE) { 
                    curState = ZUENDUNG_AN; 
                    currentPWM = 0;
                    lastStopMillis = millis(); // Zeitpunkt des Stopps merken
                }
            } else { targetPWM = pIn; }
            break;
    }

    // Rampe berechnen
    if (currentPWM < targetPWM) {
        currentPWM += RAMP_STEP;
        if (currentPWM > targetPWM) currentPWM = targetPWM;
    } else if (currentPWM > targetPWM) {
        currentPWM -= (RAMP_STEP * 3.0);
        if (currentPWM < targetPWM) currentPWM = targetPWM;
        if (currentPWM < (START_PWM_VALUE - 5) && targetPWM == 0) currentPWM = 0;
    }
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, (currentPWM / 255.0) * 100.0);
    delay(10); 
}