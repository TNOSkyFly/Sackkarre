const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <meta charset="UTF-8">
    <title>Sackkarre Dashboard</title>
    <link rel="stylesheet" href="/style.css">

    <!-- Verlinkung auf das externe Manifest -->
    <link rel="manifest" href="/manifest.json">

    <!-- App Icons (Sauberes PNG als Base64) -->
    <link rel="icon" type="image/png" href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGAAAABgCAYAAADimH4VAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAA0SURBVHhe3cExAQAAAMKg9U9tCj8gAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD4AN28AAXB3A28AAAAASUVORK5CYII=">
    <link rel="apple-touch-icon" href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGAAAABgCAYAAADimH4VAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAA0SURBVHhe3cExAQAAAMKg9U9tCj8gAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD4AN28AAXB3A28AAAAASUVORK5CYII=">

    <meta name="mobile-web-app-capable" content="yes">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
    <meta name="theme-color" content="#000000">
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
    <button id="override-btn" onclick="checkLowBatteryOverride()" style="display:none; width:100%; background:#ff4444; color:#fff; font-weight:bold; padding:12px; margin:15px 0; border:none; border-radius:8px; font-size:0.9rem; text-transform:uppercase; letter-spacing:1px; box-shadow: 0 0 10px rgba(255, 68, 68, 0.5);">
        Akku leer! Notfall-Override (3x Bestätigen)
    </button>
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