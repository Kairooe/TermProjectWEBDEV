#include "wifi_manager.h"

// ── Constructor ────────────────────────────────────────────────────────────────
// Store display reference, zero state, configure button pins.
WiFiManager::WiFiManager(U8G2 &display)
    : _display(display),
      _server(80),
      _connected(false),
      _pairingMode(false),
      _holdStart(0),
      _holdActive(false)
{
    memset(_generatedPin, 0, sizeof(_generatedPin));
    pinMode(WM_PIN_BTN_C, INPUT_PULLUP);
    pinMode(WM_PIN_BTN_D, INPUT_PULLUP);
}

// ── generatePin ───────────────────────────────────────────────────────────────
// Produces a fresh 8-digit zero-padded numeric PIN from the hardware RNG.
// Result is stored in _generatedPin and lives only in RAM — never written to NVS.
void WiFiManager::generatePin() {
    uint32_t raw    = esp_random();
    uint32_t pinNum = raw % 100000000UL;
    snprintf(_generatedPin, sizeof(_generatedPin), "%08lu", (unsigned long)pinNum);
}

// ── eraseCredentials ──────────────────────────────────────────────────────────
// Wipes all keys in the "wifi_cfg" NVS namespace.
// Must be called before every entry into pairing mode.
void WiFiManager::eraseCredentials() {
    _prefs.begin(WM_NVS_NAMESPACE, false);
    _prefs.clear();
    _prefs.end();
}

// ── saveCredentials ───────────────────────────────────────────────────────────
// Persists a complete credential set to NVS after a successful /connect.
void WiFiManager::saveCredentials(const String &ssid,
                                  const String &password,
                                  const String &identity,
                                  const String &authType) {
    _prefs.begin(WM_NVS_NAMESPACE, false);
    _prefs.putString(WM_NVS_KEY_SSID,  ssid);
    _prefs.putString(WM_NVS_KEY_PASS,  password);
    _prefs.putString(WM_NVS_KEY_IDENT, identity);
    _prefs.putString(WM_NVS_KEY_AUTH,  authType);
    _prefs.end();
}

// ── loadCredentials ───────────────────────────────────────────────────────────
// Reads saved credentials from NVS into the _saved* members.
// Returns false if no SSID has been stored yet (first boot / after erase).
bool WiFiManager::loadCredentials() {
    _prefs.begin(WM_NVS_NAMESPACE, true);   // read-only
    _savedSSID     = _prefs.getString(WM_NVS_KEY_SSID,  "");
    _savedPassword = _prefs.getString(WM_NVS_KEY_PASS,  "");
    _savedIdentity = _prefs.getString(WM_NVS_KEY_IDENT, "");
    _savedAuthType = _prefs.getString(WM_NVS_KEY_AUTH,  "personal");
    _prefs.end();
    return _savedSSID.length() > 0;
}

// ── bothButtonsHeld ───────────────────────────────────────────────────────────
// Instantaneous read — returns true only when both C and D are LOW right now.
bool WiFiManager::bothButtonsHeld() const {
    return digitalRead(WM_PIN_BTN_C) == LOW &&
           digitalRead(WM_PIN_BTN_D) == LOW;
}

// ── attemptConnect ────────────────────────────────────────────────────────────
// Dispatches to the correct connection path then polls until WL_CONNECTED
// or WM_CONNECT_TIMEOUT_MS elapses.
// Note: this call is intentionally blocking — it is only invoked from begin()
// (setup context) or the /connect POST handler (async task), never from loop().
bool WiFiManager::attemptConnect(const String &ssid,
                                 const String &password,
                                 const String &identity,
                                 const String &authType) {
    if (authType == "enterprise") {
        return connectEnterprise(ssid, identity, password);
    }

    WiFi.mode(WIFI_STA);

    if (authType == "open") {
        // Open network — no password argument
        WiFi.begin(ssid.c_str());
    } else {
        // WPA / WPA2 Personal
        WiFi.begin(ssid.c_str(), password.c_str());
    }

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < WM_CONNECT_TIMEOUT_MS) {
        delay(500);
    }

    return WiFi.status() == WL_CONNECTED;
}

// ── connectEnterprise ─────────────────────────────────────────────────────────
// WPA2-Enterprise (PEAP / MSCHAPv2) connection using the esp_wpa2 API.
// identity is used for both the outer identity and the username.
bool WiFiManager::connectEnterprise(const String &ssid,
                                    const String &identity,
                                    const String &password) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);

    // The ESP32 WPA2 API requires non-const uint8_t* — cast away constness.
    esp_wifi_sta_wpa2_ent_set_identity(
        (uint8_t *)identity.c_str(), (int)identity.length());
    esp_wifi_sta_wpa2_ent_set_username(
        (uint8_t *)identity.c_str(), (int)identity.length());
    esp_wifi_sta_wpa2_ent_set_password(
        (uint8_t *)password.c_str(), (int)password.length());
    esp_wifi_sta_wpa2_ent_enable();

    WiFi.begin(ssid.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < WM_CONNECT_TIMEOUT_MS) {
        delay(500);
    }

    return WiFi.status() == WL_CONNECTED;
}

// ── oledStatus ────────────────────────────────────────────────────────────────
// Clears the display and renders up to three status lines with the 6x10 font.
// Empty strings are skipped. Y positions are fixed at 16, 32, 48.
void WiFiManager::oledStatus(const String &line1,
                              const String &line2,
                              const String &line3) {
    _display.clearBuffer();
    _display.setFont(u8g2_font_6x10_tf);
    if (line1.length()) _display.drawStr(0, 16, line1.c_str());
    if (line2.length()) _display.drawStr(0, 32, line2.c_str());
    if (line3.length()) _display.drawStr(0, 48, line3.c_str());
    _display.sendBuffer();
}

// ── oledPairingScreen ─────────────────────────────────────────────────────────
// Full pairing screen: AP name, large PIN, and browser IP.
// The PIN is rendered with the larger 9x15 font so it is easy to read.
void WiFiManager::oledPairingScreen() {
    _display.clearBuffer();

    // Line 1 — header
    _display.setFont(u8g2_font_6x10_tf);
    _display.drawStr(0, 10, "WiFi Setup Mode");

    // Line 2 — network name
    _display.drawStr(0, 22, "Join: TriviaGame-Setup");

    // Line 3 — PIN in large font
    _display.setFont(u8g2_font_9x15_tf);
    _display.drawStr(0, 40, _generatedPin);

    // Line 4 — browser address
    _display.setFont(u8g2_font_6x10_tf);
    _display.drawStr(0, 56, "Then: 192.168.4.1");

    _display.sendBuffer();
}

// ── Captive portal HTML (PROGMEM) ─────────────────────────────────────────────
static const char PORTAL_HTML[] PROGMEM = R"html(<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TriviaGame Setup</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#1a1a2e;color:#e0e0e0;font-family:sans-serif;padding:20px}
h1{color:#e94560;text-align:center;margin-bottom:20px;font-size:20px}
.card{background:#16213e;border-radius:12px;padding:20px;max-width:420px;margin:0 auto}
label{display:block;margin-top:14px;font-size:12px;color:#9a9ab0;font-weight:700;text-transform:uppercase;letter-spacing:.5px}
input,select{width:100%;padding:10px;margin-top:5px;background:#0f3460;color:#e0e0e0;border:1px solid #2a4a7f;border-radius:6px;font-size:15px;outline:none}
input:focus,select:focus{border-color:#e94560}
.btn{width:100%;padding:12px;margin-top:14px;border:none;border-radius:6px;font-size:15px;font-weight:700;cursor:pointer}
#connectBtn{background:#e94560;color:#fff}
#connectBtn:disabled{background:#6a2030;cursor:default}
#scanBtn{background:#0f3460;color:#ccc;border:1px solid #2a4a7f;font-size:13px;padding:9px}
#scanList{display:none;margin-top:6px}
.spin{display:none;text-align:center;color:#9a9ab0;margin-top:8px;font-size:13px}
.note{background:#0a2040;border-left:3px solid #e94560;padding:10px;margin-top:12px;font-size:13px;line-height:1.5;border-radius:3px}
#result{margin-top:16px;padding:12px;border-radius:6px;display:none;font-size:14px;text-align:center;line-height:1.6}
.ok{background:#0d2e0d;color:#90ee90;border:1px solid #2a6a2a}
.err{background:#2e0d0d;color:#ff9090;border:1px solid #6a2a2a}
</style></head>
<body><div class="card">
<h1>&#127760; TriviaGame WiFi Setup</h1>
<form id="frm">
<label>Network Name (SSID)</label>
<input type="text" id="ssid" name="ssid" placeholder="e.g. MyHomeWiFi" required>
<button type="button" id="scanBtn" class="btn">&#128246; Scan for Networks</button>
<div class="spin" id="spin">Scanning, please wait...</div>
<select id="scanList" size="5"></select>
<label>Security Type</label>
<select id="auth" name="auth_type" onchange="sync()">
<option value="personal">WPA/WPA2 Personal (Home/Office)</option>
<option value="open">Open &#8212; No Password</option>
<option value="portal">Open &#8212; Captive Portal (Hotel/Cafe)</option>
<option value="enterprise">WPA2 Enterprise (School/University/Corporate)</option>
</select>
<div id="identRow" style="display:none">
<label>Identity / Username</label>
<input type="text" id="ident" name="identity" placeholder="user@school.edu or DOMAIN\user">
</div>
<div id="passRow">
<label>Password</label>
<input type="password" id="pass" name="password" placeholder="WiFi password">
</div>
<div class="note" id="portalNote" style="display:none">
&#9432; <b>Hotel/Cafe WiFi:</b> The ESP32 cannot complete a browser login page itself.
After connecting, finish the portal login on your phone or PC &#8212;
the device will work once the network allows open internet access.
</div>
<button type="submit" id="connectBtn" class="btn">Connect</button>
</form>
<div id="result"></div>
</div>
<script>
function sync(){
  var t=document.getElementById('auth').value;
  document.getElementById('passRow').style.display=(t==='open'||t==='portal')?'none':'block';
  document.getElementById('identRow').style.display=(t==='enterprise')?'block':'none';
  document.getElementById('portalNote').style.display=(t==='portal')?'block':'none';
}
document.getElementById('scanBtn').onclick=function(){
  var sp=document.getElementById('spin'),sl=document.getElementById('scanList');
  sp.style.display='block'; sl.style.display='none';
  fetch('/scan').then(function(r){return r.json();}).then(function(nets){
    sp.style.display='none';
    sl.innerHTML='';
    if(!nets.length) return;
    nets.forEach(function(n){
      var o=document.createElement('option');
      o.value=n.ssid;
      o.textContent=n.ssid+' ('+n.rssi+' dBm)'+(n.encrypted?' \uD83D\uDD12':' \uD83D\uDD13');
      sl.appendChild(o);
    });
    sl.style.display='block';
    sl.onchange=function(){
      var sel=nets.find(function(n){return n.ssid===sl.value;});
      document.getElementById('ssid').value=sl.value;
      document.getElementById('auth').value=(sel&&!sel.encrypted)?'open':'personal';
      sync();
    };
  }).catch(function(){sp.style.display='none';});
};
document.getElementById('frm').onsubmit=function(e){
  e.preventDefault();
  var btn=document.getElementById('connectBtn'),res=document.getElementById('result');
  btn.disabled=true; btn.textContent='Connecting...';
  res.style.display='none';
  var p='ssid='+encodeURIComponent(document.getElementById('ssid').value)
       +'&password='+encodeURIComponent(document.getElementById('pass').value)
       +'&identity='+encodeURIComponent(document.getElementById('ident').value)
       +'&auth_type='+encodeURIComponent(document.getElementById('auth').value);
  fetch('/connect',{method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},body:p})
  .then(function(r){return r.text();})
  .then(function(t){
    res.innerHTML=t; res.className=t.indexOf('Connected')>=0?'ok':'err';
    res.style.display='block';
    btn.disabled=false; btn.textContent='Connect';
  }).catch(function(){
    res.textContent='Network error \u2014 try again.';
    res.className='err'; res.style.display='block';
    btn.disabled=false; btn.textContent='Connect';
  });
};
</script></body></html>)html";

// ── File-scope deferred restart ────────────────────────────────────────────────
// Set by the /connect success handler; executed in loop() after 3 s.
static unsigned long s_restartAt = 0;

// ── registerRoutes ────────────────────────────────────────────────────────────
// Attaches all AsyncWebServer endpoints. Call once from startPairingMode().
void WiFiManager::registerRoutes() {
    // GET / — serve captive portal page
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", PORTAL_HTML);
    });

    // GET /scan — synchronous WiFi scan, returns JSON array
    _server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
        int n = WiFi.scanNetworks();
        String json = "[";
        for (int i = 0; i < n; i++) {
            if (i > 0) json += ",";
            // Escape backslash and double-quote so SSID is safe in JSON
            String ssid = WiFi.SSID(i);
            ssid.replace("\\", "\\\\");
            ssid.replace("\"", "\\\"");
            bool enc = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
            json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" +
                    String(WiFi.RSSI(i)) + ",\"encrypted\":" +
                    (enc ? "true" : "false") + "}";
        }
        json += "]";
        // FIX (memory leak): scanNetworks() allocates a heap buffer for results that
        // persists until scanDelete() is called; omitting this leaks memory every time
        // the user presses "Scan for Networks" in the captive portal.
        WiFi.scanDelete();
        request->send(200, "application/json", json);
    });

    // POST /connect — attempt connection with submitted credentials
    _server.on("/connect", HTTP_POST, [this](AsyncWebServerRequest *request) {
        String ssid     = request->arg("ssid");
        String password = request->arg("password");
        String identity = request->arg("identity");
        String authType = request->arg("auth_type");

        if (ssid.isEmpty()) {
            request->send(400, "text/html",
                "<p style='color:#ff9090'>Error: Network name cannot be empty.</p>");
            return;
        }
        if (authType.isEmpty()) authType = "personal";

        bool ok = attemptConnect(ssid, password, identity, authType);

        if (ok) {
            saveCredentials(ssid, password, identity, authType);
            String ip = WiFi.localIP().toString();
            oledStatus("Connected!", ip, "Restarting...");
            request->send(200, "text/html",
                "<b>&#10003; Connected!</b><br>IP: " + ip +
                "<br>Device restarting in 3 seconds...");
            s_restartAt = millis() + 3000;
        } else {
            oledStatus("Failed", "Try again");
            request->send(200, "text/html",
                "<b>&#10007; Connection failed.</b><br>"
                "Check credentials and try again.");
        }
    });

    // Android captive-portal detection — must return 204 No Content
    _server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(204);
    });

    // iOS captive-portal detection
    _server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1");
    });

    // Windows captive-portal detection (NCSI / fwlink)
    _server.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1");
    });

    // Catch-all: redirect any unrecognised path back to the portal
    _server.onNotFound([](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1");
    });
}

// ── startPairingMode ──────────────────────────────────────────────────────────
// Generates a new PIN, opens the softAP, starts DNS redirect + web server,
// and draws the pairing screen. Always erases credentials before calling.
void WiFiManager::startPairingMode() {
    generatePin();

    WiFi.mode(WIFI_AP);
    // Explicit softAPConfig ensures a predictable gateway/subnet on all SDK versions.
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1),
                      IPAddress(192, 168, 4, 1),
                      IPAddress(255, 255, 255, 0));
    WiFi.softAP(WM_AP_SSID, _generatedPin);
    delay(100);   // brief hardware settle after softAP start

    _dns.start(53, "*", IPAddress(192, 168, 4, 1));
    registerRoutes();
    _server.begin();

    _pairingMode = true;
    oledPairingScreen();
}

// ── stopPairingMode ───────────────────────────────────────────────────────────
// Tears down the web server, DNS redirector, and softAP cleanly.
void WiFiManager::stopPairingMode() {
    _server.end();
    _dns.stop();
    WiFi.softAPdisconnect(true);
    _pairingMode = false;
}

// ── begin ─────────────────────────────────────────────────────────────────────
// Called once from setup(). Decides whether to auto-connect or enter pairing.
void WiFiManager::begin() {
    _display.begin();
    pinMode(WM_PIN_BTN_C, INPUT_PULLUP);
    pinMode(WM_PIN_BTN_D, INPUT_PULLUP);

    oledStatus("Booting...", "Please wait");

    // Boot-time forced pairing: hold C+D before power-on
    if (bothButtonsHeld()) {
        eraseCredentials();
        startPairingMode();
        return;
    }

    // No saved credentials → go straight to pairing
    if (!loadCredentials()) {
        startPairingMode();
        return;
    }

    // Try auto-connecting with saved credentials
    oledStatus("Connecting...", _savedSSID);
    if (attemptConnect(_savedSSID, _savedPassword, _savedIdentity, _savedAuthType)) {
        _connected = true;
        oledStatus("Connected!", WiFi.localIP().toString());
    } else {
        oledStatus("Auto-connect", "failed", "Entering setup...");
        delay(2000);
        startPairingMode();
    }
}

// ── loop ──────────────────────────────────────────────────────────────────────
// Call every iteration of loop(). Handles two things:
//   1. Deferred ESP.restart() scheduled by the /connect success handler.
//   2. Non-blocking detection of the C+D 3-second hold for mid-session re-pair.
void WiFiManager::loop() {
    // Execute deferred restart once the timer expires
    if (s_restartAt > 0 && millis() >= s_restartAt) {
        s_restartAt = 0;
        ESP.restart();
    }

    // C+D hold detection — fully non-blocking using millis()
    if (bothButtonsHeld()) {
        if (!_holdActive) {
            _holdActive = true;
            _holdStart  = millis();
        } else if (millis() - _holdStart >= WM_HOLD_DURATION) {
            // Hold confirmed — erase, stop current mode, re-enter pairing
            _holdActive = false;
            eraseCredentials();
            if (_pairingMode) stopPairingMode();
            _connected = false;
            startPairingMode();
        }
    } else {
        _holdActive = false;
        _holdStart  = 0;
    }
}

// ── isConnected / isPairing / getIP ───────────────────────────────────────────
bool   WiFiManager::isConnected() const { return _connected; }
bool   WiFiManager::isPairing()   const { return _pairingMode; }
String WiFiManager::getIP()       const { return WiFi.localIP().toString(); }
