const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <meta charset="UTF-8">
    <title>Sackkarre Dashboard</title>
    <link rel="stylesheet" href="/style.css">
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

    <script src="/script.js"></script>
</body>
</html>
)rawliteral";