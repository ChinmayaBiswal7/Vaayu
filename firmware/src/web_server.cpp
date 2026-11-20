// ============================================================
//  web_server.cpp — Built-in ESP32 SoftAP Web Dashboard & CSV Logger
// ============================================================
#include "web_server.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

static WebServer server(HTTP_PORT);
static ControlState* g_cs = nullptr;
static SensorData*   g_sd = nullptr;

// Test Logging Ring Buffer (up to 120 rows = 2 hours)
struct LogRow {
    uint32_t minute;
    float    roomTemp;
    float    roomHumid;
    float    bathHumid;
    float    acOutlet;
    uint8_t  fanLevel;
    bool     acOn;
    int      acSetpoint;
    float    ci;
    float    targetCi;
    char     reason[48];
};

static LogRow logRows[120];
static int    logCount = 0;
static bool   loggingActive = false;
static char   sessionLabel[32] = "Baseline";
static float  startKwh = 0.0f;
static float  endKwh = 0.0f;

// ── HTML Dashboard in PROGMEM ────────────────────────────────
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Vaayu Hub Live</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, monospace; }
    body { background: #0d1117; color: #c9d1d9; padding: 12px; }
    h1 { font-size: 1.3rem; color: #2dd4bf; margin-bottom: 4px; display: flex; justify-content: space-between; align-items: center; }
    .badge { font-size: 0.75rem; padding: 3px 8px; border-radius: 12px; background: #238636; color: #fff; }
    .badge-warn { background: #d29922; }
    .badge-alarm { background: #da3633; }
    .grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; margin: 12px 0; }
    .card { background: #161b22; border: 1px solid #30363d; border-radius: 8px; padding: 10px; }
    .card-title { font-size: 0.75rem; color: #8b949e; text-transform: uppercase; }
    .card-value { font-size: 1.4rem; font-weight: bold; margin-top: 4px; color: #f0f6fc; }
    .reason-box { background: #1f242c; border-left: 4px solid #2dd4bf; padding: 10px; border-radius: 4px; margin: 10px 0; font-size: 0.85rem; }
    .btn-row { display: flex; flex-wrap: wrap; gap: 6px; margin: 8px 0; }
    button { background: #21262d; border: 1px solid #363b42; color: #c9d1d9; padding: 8px 12px; border-radius: 6px; font-size: 0.85rem; cursor: pointer; flex: 1 1 30%; }
    button.active { background: #2dd4bf; color: #0d1117; font-weight: bold; border-color: #2dd4bf; }
    .log-box { margin-top: 15px; border-top: 1px solid #30363d; padding-top: 10px; }
    input { background: #0d1117; border: 1px solid #30363d; color: #fff; padding: 6px 8px; border-radius: 4px; width: 100px; }
  </style>
</head>
<body>
  <h1>VAAYU HUB <span id="mode-badge" class="badge">AUTO</span></h1>
  <div class="reason-box" id="reason-text">Initialising...</div>

  <div class="grid">
    <div class="card"><div class="card-title">Room Temp / RH</div><div class="card-value" id="room-trh">--°C / --%</div></div>
    <div class="card"><div class="card-title">Comfort Index</div><div class="card-value" id="ci-val">-- / Target --</div></div>
    <div class="card"><div class="card-title">Room Fan (L9110S)</div><div class="card-value" id="fan-val">Level 0 (0%)</div></div>
    <div class="card"><div class="card-title">Air Conditioner</div><div class="card-value" id="ac-val">OFF</div></div>
    <div class="card"><div class="card-title">Radar (LD2410C)</div><div class="card-value" id="radar-val">Searching...</div></div>
    <div class="card"><div class="card-title">Bathroom RH</div><div class="card-value" id="bath-val">--% (Exh OFF)</div></div>
  </div>

  <div class="card-title">Profile</div>
  <div class="btn-row">
    <button id="p-0" onclick="sendCmd('profile', 0)">Normal</button>
    <button id="p-1" onclick="sendCmd('profile', 1)">Sleep</button>
    <button id="p-2" onclick="sendCmd('profile', 2)">Humid</button>
    <button id="p-3" onclick="sendCmd('profile', 3)">Eco</button>
  </div>

  <div class="card-title" style="margin-top:10px;">Manual Controls</div>
  <div class="btn-row">
    <button onclick="sendCmd('fan', 0)">Fan 0</button>
    <button onclick="sendCmd('fan', 1)">Fan 1</button>
    <button onclick="sendCmd('fan', 2)">Fan 2</button>
    <button onclick="sendCmd('fan', 3)">Fan 3</button>
    <button onclick="sendCmd('fan', 4)">Fan 4</button>
    <button onclick="sendCmd('fan', 5)">Fan 5</button>
  </div>
  <div class="btn-row">
    <button onclick="sendCmd('ac_power', 1)">AC ON</button>
    <button onclick="sendCmd('ac_power', 0)">AC OFF</button>
    <button onclick="sendCmd('ac_temp', -1)">Temp -</button>
    <button onclick="sendCmd('ac_temp', 1)">Temp +</button>
  </div>
  <div class="btn-row">
    <button id="fast-btn" onclick="sendCmd('fast_test', 0)">Fast Test (1m=5s)</button>
    <button onclick="sendCmd('resume', 0)">Resume Auto</button>
  </div>

  <div class="log-box">
    <div class="card-title">Energy Test Logger</div>
    <div style="display:flex; gap:8px; align-items:center; margin-top:8px;">
      <input type="text" id="sess-label" value="Vaayu" placeholder="Label">
      <input type="number" id="start-kwh" step="0.01" placeholder="Start kWh">
      <input type="number" id="end-kwh" step="0.01" placeholder="End kWh">
    </div>
    <div class="btn-row" style="margin-top:8px;">
      <button id="log-btn" onclick="toggleLogging()">Start Session</button>
      <button onclick="location.href='/api/log/csv'">Download CSV</button>
    </div>
  </div>

  <script>
    function update() {
      fetch('/api/status').then(r => r.json()).then(d => {
        document.getElementById('reason-text').innerText = d.reason;
        document.getElementById('room-trh').innerText = d.roomTemp.toFixed(1) + '°C / ' + d.roomHumid.toFixed(0) + '%';
        document.getElementById('ci-val').innerText = d.ci.toFixed(1) + ' / ' + d.targetCi.toFixed(1) + '°';
        document.getElementById('fan-val').innerText = 'Level ' + d.fanLevel + ' (' + d.fanDutyPct + '%)';
        document.getElementById('ac-val').innerText = (d.acOn ? 'ON @ ' + d.acSetpoint + '°C' : 'OFF') + (d.monsoon ? ' (DRY)' : '');
        document.getElementById('radar-val').innerText = (d.presence ? 'Occupied (' + d.distance + 'cm)' : 'Empty (' + d.emptyMin + 'm)');
        document.getElementById('bath-val').innerText = d.bathHumid.toFixed(0) + '% (Exh ' + (d.exhaustOn ? 'ON' : 'OFF') + ')';
        
        let mb = document.getElementById('mode-badge');
        if (d.fastTest) { mb.className = 'badge badge-warn'; mb.innerText = 'FAST TEST (1m=5s)'; }
        else if (d.override) { mb.className = 'badge badge-warn'; mb.innerText = 'OVERRIDE PAUSE'; }
        else if (d.alarm) { mb.className = 'badge badge-alarm'; mb.innerText = 'FAULT ALARM'; }
        else { mb.className = 'badge'; mb.innerText = d.manual ? 'MANUAL' : 'AUTO'; }

        for (let i=0; i<4; i++) {
          let b = document.getElementById('p-' + i);
          if (b) b.className = (d.profile === i) ? 'active' : '';
        }
      }).catch(e => console.log(e));
    }
    setInterval(update, 1000);
    update();

    function sendCmd(act, val) {
      fetch('/api/cmd?act=' + act + '&val=' + val, { method: 'POST' }).then(() => update());
    }

    function toggleLogging() {
      let b = document.getElementById('log-btn');
      let lbl = document.getElementById('sess-label').value;
      let skwh = document.getElementById('start-kwh').value;
      let ekwh = document.getElementById('end-kwh').value;
      if (b.innerText === 'Start Session') {
        fetch('/api/log/start?label=' + encodeURIComponent(lbl) + '&kwh=' + skwh, { method: 'POST' });
        b.innerText = 'Stop Session';
        b.className = 'active';
      } else {
        fetch('/api/log/stop?kwh=' + ekwh, { method: 'POST' });
        b.innerText = 'Start Session';
        b.className = '';
      }
    }
  </script>
</body>
</html>
)rawliteral";

// ── Web Server Setup ─────────────────────────────────────────
void web_server_init(ControlState& cs, SensorData& sd) {
    g_cs = &cs;
    g_sd = &sd;

    // Start SoftAP
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(SOFTAP_SSID, SOFTAP_PASSWORD);
    Serial.printf("[web] SoftAP started: %s (connect to http://192.168.4.1)\n", SOFTAP_SSID);

    // Endpoints
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", INDEX_HTML);
    });

    server.on("/api/status", HTTP_GET, []() {
        if (!g_cs || !g_sd) { server.send(500, "text/plain", "Not ready"); return; }
        StaticJsonDocument<512> doc;
        doc["roomTemp"]   = g_sd->roomTemp;
        doc["roomHumid"]  = g_sd->roomHumidity;
        doc["bathHumid"]  = g_sd->bathHumidity;
        doc["acOutlet"]   = g_sd->acOutletTemp;
        doc["presence"]   = g_sd->presenceDetected;
        doc["distance"]   = g_sd->targetDistanceCm;
        doc["emptyMin"]   = g_cs->emptyMinutes;
        doc["fanLevel"]   = g_cs->fanLevel;
        doc["fanDutyPct"] = (int)(FAN_DUTIES[g_cs->fanLevel] * 100 / 255);
        doc["acOn"]       = g_cs->acOn;
        doc["acSetpoint"] = g_cs->acSetpoint;
        doc["monsoon"]    = g_cs->monsoonMode;
        doc["exhaustOn"]  = g_cs->exhaustOn;
        doc["ci"]         = g_cs->currentCI;
        doc["targetCi"]   = g_cs->targetCI;
        doc["profile"]    = (int)g_cs->profile;
        doc["manual"]     = g_cs->manualMode;
        doc["override"]   = g_cs->overrideActive;
        doc["alarm"]      = g_cs->alarmActive;
        doc["fastTest"]   = g_cs->fastTestMode;
        doc["reason"]     = g_cs->reason;

        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    });

    server.on("/api/cmd", HTTP_POST, []() {
        if (!g_cs) return;
        String act = server.arg("act");
        int val = server.arg("val").toInt();

        if (act == "profile") {
            control_set_profile(*g_cs, (ComfortProfile)val);
        } else if (act == "fan") {
            control_set_manual_fan(*g_cs, val);
        } else if (act == "ac_power") {
            control_set_manual_ac(*g_cs, val == 1, g_cs->acSetpoint);
        } else if (act == "ac_temp") {
            control_set_manual_ac(*g_cs, g_cs->acOn, g_cs->acSetpoint + val);
        } else if (act == "fast_test") {
            control_set_fast_test(*g_cs, !g_cs->fastTestMode);
        } else if (act == "resume") {
            g_cs->manualMode = false;
            g_cs->overrideActive = false;
            snprintf(g_cs->reason, sizeof(g_cs->reason), "Automation resumed from web dashboard");
        }
        server.send(200, "text/plain", "OK");
    });

    server.on("/api/log/start", HTTP_POST, []() {
        String lbl = server.arg("label");
        if (lbl.length() > 0) strncpy(sessionLabel, lbl.c_str(), sizeof(sessionLabel) - 1);
        startKwh = server.arg("kwh").toFloat();
        logCount = 0;
        loggingActive = true;
        server.send(200, "text/plain", "Started");
    });

    server.on("/api/log/stop", HTTP_POST, []() {
        endKwh = server.arg("kwh").toFloat();
        loggingActive = false;
        server.send(200, "text/plain", "Stopped");
    });

    server.on("/api/log/csv", HTTP_GET, []() {
        String csv = "Minute,Session,RoomTemp_C,RoomRH_pct,BathRH_pct,OutletTemp_C,FanLevel,AC_State,AC_Setpoint,CI_C,TargetCI_C,Reason\n";
        for (int i = 0; i < logCount; i++) {
            csv += String(logRows[i].minute) + "," +
                   String(sessionLabel) + "," +
                   String(logRows[i].roomTemp, 1) + "," +
                   String(logRows[i].roomHumid, 1) + "," +
                   String(logRows[i].bathHumid, 1) + "," +
                   String(logRows[i].acOutlet, 1) + "," +
                   String(logRows[i].fanLevel) + "," +
                   (logRows[i].acOn ? "ON" : "OFF") + "," +
                   String(logRows[i].acSetpoint) + "," +
                   String(logRows[i].ci, 1) + "," +
                   String(logRows[i].targetCi, 1) + ",\"" +
                   String(logRows[i].reason) + "\"\n";
        }
        server.sendHeader("Content-Disposition", "attachment; filename=\"vaayu_test_log.csv\"");
        server.send(200, "text/csv", csv);
    });

    server.begin();
    Serial.println("[web] HTTP server listening on port 80");
}

void web_server_handle() {
    server.handleClient();
}

void web_server_log_minute(const ControlState& cs, const SensorData& sd) {
    if (!loggingActive || logCount >= 120) return;
    LogRow& r = logRows[logCount];
    r.minute     = logCount + 1;
    r.roomTemp   = sd.roomTemp;
    r.roomHumid  = sd.roomHumidity;
    r.bathHumid  = sd.bathHumidity;
    r.acOutlet   = sd.acOutletTemp;
    r.fanLevel   = cs.fanLevel;
    r.acOn       = cs.acOn;
    r.acSetpoint = cs.acSetpoint;
    r.ci         = cs.currentCI;
    r.targetCi   = cs.targetCI;
    strncpy(r.reason, cs.reason, sizeof(r.reason) - 1);
    logCount++;
}
