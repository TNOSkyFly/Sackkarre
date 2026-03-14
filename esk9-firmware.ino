#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "driver/mcpwm.h"

// =======================
// Header-Dateien
// =======================
#include "style.h"   // Enthält DASHBOARD_CSS
#include "script.h"  // Enthält DASHBOARD_JS
#include "index.h"   // Enthält DASHBOARD_HTML

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

bool simuliereAkku = false; // Standardmäßig aus

int R11 = 1830;         
int R12 = 330;
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
    if (simuliereAkku) return 37.5;

    int rawADC = analogRead(UADC_PIN); 
    if (rawADC < 10) return 0.0; // Keine Spannung vorhanden

    // Spannung am ESP32 Pin (0 - 3.3V)
    float pinVoltage = rawADC * (3.3 / 4095.0); 

    float UZ = 20; 
    float dividerRatio = (float(R11 + R12) / R12); // (56k + 10k) / 10k = 6.6
    
    float currentVolt = (pinVoltage * dividerRatio) + UZ;

    // Glättung (EMA Filter)
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
    
    // --- Webserver Endpunkte ---
    server.on("/", [](){ 
        server.send_P(200, "text/html", DASHBOARD_HTML); 
    });

    server.on("/style.css", [](){ 
        server.send_P(200, "text/css", DASHBOARD_CSS); 
    });

    server.on("/script.js", [](){ 
        server.send_P(200, "application/javascript", DASHBOARD_JS); 
    });

    server.on("/status", [](){
        server.send(200, "application/json", "{\"s\":" + String((int)curState) + ",\"p\":" + String(currentPWM) + ",\"t\":" + String(aktuelleTemperatur) + ",\"v\":" + String(UBat) + "}");
    });

    server.on("/toggleSim", [](){
        simuliereAkku = !simuliereAkku;
        server.send(200, "text/plain", simuliereAkku ? "SIM_AN" : "SIM_AUS");
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