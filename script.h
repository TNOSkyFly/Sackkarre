const char DASHBOARD_JS[] PROGMEM = R"rawliteral(
let vmax = 0; 
let speedHistory = [0, 0, 0, 0, 0, 0, 0, 0];

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
        let perc = Math.round((volt - 32) * (100 / (41 - 32)));
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
    if (!document.fullscreenElement) document.documentElement.requestFullscreen();
    else document.exitFullscreen();
}
)rawliteral";