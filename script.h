#pragma once
#include <Arduino.h>

const char DASHBOARD_JS[] PROGMEM = R"rawliteral(
let vmax = 0; 
let speedHistory = [0, 0, 0, 0, 0, 0, 0, 0];
let confirmCount = 0;

function checkLowBatteryOverride() {
    confirmCount = 0;
    askOverridePermission();
}

function askOverridePermission() {
    const messages = [
        "WARNUNG: Der Akku ist kritisch entladen (< 31.5V)!\n\nEin Tiefentladen kann den Akku dauerhaft beschädigen.\n\nBist du sicher, dass du fortfahren willst? (1/3)",
        "ZWEITE WARNUNG:\n\nDer Motor kann unter Last abrupt abschalten. Nutzung auf eigene Gefahr!\n\nWirklich übersteuern? (2/3)",
        "LETZTE BESTÄTIGUNG:\n\nHiermit wird die Akku-Schutzschaltung DEAKTIVIERT.\n\n'Ich weiß, was ich tue' aktivieren? (3/3)"
    ];

    if (confirm(messages[confirmCount])) {
        confirmCount++;
        if (confirmCount < 3) {
            askOverridePermission(); // Nächste Sicherheitsstufe abfragen
        } else {
            // Alle 3 Bestätigungen erhalten -> ESP32-Signal senden
            fetch('/toggleOverride')
                .then(r => r.text())
                .then(res => {
                    alert("NOTMODUS AKTIVIERT: Akku-Sperre wurde temporär überbrückt!");
                });
        }
    } else {
        alert("Abgebrochen. Der Akku-Schutz bleibt aktiv.");
    }
}

function resetVmax() {
    vmax = 0;
    speedHistory.fill(0);
    document.getElementById('vmax').innerHTML = "0.0";
}

function toggleSim() {
    fetch('/toggleSim').then(r => r.text()).then(res => {
        document.getElementById('btn-sim').classList.toggle('active', res === "SIM_AN");
    });
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
        // --- Akku-Notfall-Button Sichtbarkeit prüfen ---
        const ovrBtn = document.getElementById('override-btn');
        if (ovrBtn) {
            if (d.v < 31.5 && d.v > 5.0) {
                ovrBtn.style.display = 'block';
            } else {
                ovrBtn.style.display = 'none';
            }
        }

        const states = ["ZÜNDUNG AUS", "BEREIT", "AKTIV", "SAFE LOCK"];
        const stTxt = document.getElementById('st-txt');
        stTxt.innerHTML = states[d.s];
        stTxt.className = "state-" + d.s;
        
        document.getElementById('lock-msg').style.display = (d.s == 3) ? "flex" : "none";
        
        const tVal = document.getElementById('temp-val');
        tVal.innerHTML = d.t.toFixed(1) + "<small>°C</small>";
        tVal.style.color = d.t >= 80 ? "#ff4444" : "#00ffcc";
        document.body.classList.toggle('alarm', d.t >= 80);

        let volt = d.v;
        let perc = Math.round((volt - 32) * (100 / (42 - 32)));
        perc = Math.max(0, Math.min(100, perc));
        document.getElementById('bat-val').innerHTML = volt.toFixed(1);
        document.getElementById('bat-perc').innerHTML = perc;
        const bBar = document.getElementById('bat-bar');
        bBar.style.width = perc + "%";
        bBar.style.background = perc < 15 ? "#ff4444" : (perc < 40 ? "#ffcc00" : "#39ff14");

        let p_perc = Math.round((d.p / 255) * 100);
        document.getElementById('p-txt').innerHTML = p_perc;
        document.getElementById('p-bar').style.width = p_perc + "%";
        document.getElementById('v18').innerHTML = d.p;
        document.getElementById('l18').classList.toggle('active', d.p > 0);
        document.getElementById('v19').innerHTML = (d.p == 0) ? "HIGH" : "LOW";
        document.getElementById('l19').classList.toggle('active', d.p == 0);
    }).catch(() => {
        document.getElementById('st-txt').innerHTML = "OFFLINE";
    });
}, 250);

function toggleFullScreen() {
    let doc = window.document;
    let docEl = doc.documentElement;

    let requestFS = docEl.requestFullscreen || docEl.mozRequestPrefix || docEl.webkitRequestFullScreen || docEl.msRequestFullscreen;
    let cancelFS = doc.exitFullscreen || doc.mozCancelFullScreen || doc.webkitExitFullscreen || doc.msExitFullscreen;

    if (!doc.fullscreenElement && !doc.mozFullScreenElement && !doc.webkitFullscreenElement && !doc.msFullscreenElement) {
        if (requestFS) {
            requestFS.call(docEl).catch(err => {
                window.scrollTo(0, 1);
            });
        }
    } else {
        if (cancelFS) {
            cancelFS.call(doc);
        }
    }
}

// Bei der allerersten Berührung (Klick oder Touch) automatisch Vollbild aktivieren
document.addEventListener("click", toggleFullScreen, { once: true });
document.addEventListener("touchstart", toggleFullScreen, { once: true });

)rawliteral";