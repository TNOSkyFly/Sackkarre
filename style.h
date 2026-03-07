const char DASHBOARD_CSS[] PROGMEM = R"rawliteral(
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
#lock-msg { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(150, 0, 0, 0.5); z-index: 1000; flex-direction: column; justify-content: center; align-items: center; color: white; backdrop-filter: blur(5px); }
)rawliteral";