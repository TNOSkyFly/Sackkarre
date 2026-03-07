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
        
        .vmax-container { display: flex; justify-content: center; align-items: center; gap: 8px; background: rgba(255, 255, 255, 0.05); padding: 8px 12px; border-radius: 25px; width: fit-content; margin: 0 auto 15px auto; border: 1px solid rgba(255,255,255,0.1); }
        .vmax-label { color: #ffcc00; font-size: 0.85rem; font-weight: bold; text-transform: uppercase; margin-right: 5px; }
        
        .btn-reset { 
            background: rgba(255,255,255,0.1); border: 1px solid rgba(255,255,255,0.2); color: #eee; 
            padding: 6px 12px; border-radius: 15px; font-size: 0.7rem; font-weight: bold;
            cursor: pointer; text-transform: uppercase; outline: none; transition: all 0.2s;
            -webkit-tap-highlight-color: transparent; touch-action: manipulation;
        }
        .btn-reset:active { transform: scale(0.95); }
        #btn-sim.active { background: #ff00ff; color: #fff; border-color: #ff00ff; box-shadow: 0 0 10px #ff00ff; }

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
        #lock-msg { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(150, 0, 0, 0.7); z-index: 1000; flex-direction: column; justify-content: center; align-items: center; color: white; backdrop-filter: blur(5px); }
    </style>
</head>
<body>
    <div id="lock-msg">
        <h1 style="font-size:3.5rem; margin:0; text-shadow: 0 0 20px #000;">Safety Lock</h1>
        <p style="font-size:1.2rem; font-weight: bold;">Fuß vom Gas!</p>
    </div>

    <div class="vmax-container">
        <span class="vmax-label">Top: <span id="vmax">0.0</span></span>
        <button class="btn-reset" onclick="resetVmax()">Reset</button>
        <button id="btn-sim" class="btn-reset" onclick="toggleSim()">Sim 37V</button>
        <button class="btn-reset" onclick="toggleFullScreen()">Full</button>
    </div>

    <div class="speed-box">
        <div id="speed">0</div>
        <div class="unit">KM/H</div>
    </div>

    <div class="diag-grid">
        <div class="diag-card"> 
            <span class="unit">SYSTEM TEMP</span>
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
                
                // Temp
                const tVal = document.getElementById('temp-val');
                tVal.innerHTML = d.t.toFixed(1) + "<small>°C</small>";
                tVal.style.color = d.t >= 80 ? "#ff4444" : "#00ffcc";
                document.body.classList.toggle('alarm', d.t >= 80);

                // Akku
                let volt = d.v;
                let perc = Math.round((volt - 32) * (100 / (41 - 32)));
                perc = Math.max(0, Math.min(100, perc));
                document.getElementById('bat-val').innerHTML = volt.toFixed(1);
                document.getElementById('bat-perc').innerHTML = perc;
                const bBar = document.getElementById('bat-bar');
                bBar.style.width = perc + "%";
                bBar.style.background = perc < 15 ? "#ff4444" : (perc < 40 ? "#ffcc00" : "#39ff14");

                // PWM
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
    </script>
</body>
</html>
)rawliteral";