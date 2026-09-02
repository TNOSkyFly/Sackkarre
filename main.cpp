#include <Arduino.h>

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "driver/mcpwm.h"

// =======================
// Header-Dateien
// =======================
#include "style.h"
#include "script.h"
#include "index.h"

// =======================
// Konfiguration & Pins
// =======================
#define PIN_HIN 19
#define PIN_LIN 18
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
bool akkuOverride = false;  // Schaltet die Akku-Sperre manuell aus

int R11 = 1830;         
int R12 = 330;
int UZ = 20;            // Spannung Zenerdiode
float UBat = 0.0;

const int MAX_PWM_LIMIT = 216; 
const int START_PWM_VALUE = 22;         //von 30 auf 22 reduziert. Rechnung: (1/10000Hz) * (22/255) = 8,6us Pulsdauer
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
    float dividerRatio = (float(R11 + R12) / R12); 
    
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
    mcpwm_config_t pc; 
    pc.frequency = 10000;                       //von 5000 auf 10000 erhöht
    pc.cmpr_a = 0;
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

    server.on("/toggleOverride", [](){
        akkuOverride = !akkuOverride;
        server.send(200, "text/plain", akkuOverride ? "OVERRIDE_AN" : "OVERRIDE_AUS");
    });

    // Web-App Manifest für App-Installation und Icon-Darstellung
    server.on("/manifest.json", [](){
        String json = "{"
          "\"name\":\"Sackkarre Dashboard\","
          "\"short_name\":\"Sackkarre\","
          "\"start_url\":\"/\","
          "\"display\":\"standalone\","
          "\"orientation\":\"any\","
          "\"background_color\":\"#000000\","
          "\"theme_color\":\"#000000\","
          "\"icons\":["
            "{"
              "\"src\":\"data:image/jpeg;base64,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\","
              "\"sizes\":\"96x96 192x192 512x512\","
              "\"type\":\"image/png\","
              "\"purpose\":\"any maskable\""
            "}"
          "]"
        "}";

        server.send(200, "application/json", json);
    });

    // Webserver starten
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
    bool rawLowBat = (UBat < 31.5 && UBat > 5.0); 
    bool lowBat = rawLowBat && !akkuOverride;

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