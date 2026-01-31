#include "WebManager.h"
#include "Bridge.h"
#include "Config.h"
#include "ModeManager.h"

AsyncWebServer WebManager::_server(WEB_SERVER_PORT);
BLEManager *WebManager::_bleManager = nullptr;
USBHIDManager *WebManager::_usbHidManager = nullptr;
bool WebManager::_initialized = false;

void WebManager::begin()
{
    Serial.println("[Web] Starting WiFi AP...");

    WiFi.mode(WIFI_AP);
    bool apStarted = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);

    if (!apStarted)
    {
        Serial.println("[Web] ERROR: Failed to start WiFi AP!");
        return;
    }

    IPAddress IP = WiFi.softAPIP();
    Serial.printf("[Web] WiFi AP started: %s\n", WIFI_AP_SSID);
    Serial.printf("[Web] IP address: %s\n", IP.toString().c_str());

    if (!SPIFFS.begin(true))
    {
        Serial.println("[Web] ERROR: SPIFFS mount failed!");
    }
    else
    {
        Serial.println("[Web] SPIFFS mounted successfully");

        File root = SPIFFS.open("/");
        File file = root.openNextFile();
        Serial.println("[Web] SPIFFS files:");
        while (file)
        {
            Serial.printf("  - %s (%d bytes)\n", file.name(), file.size());
            file = root.openNextFile();
        }
    }

    _server.on("/", HTTP_GET, handleRoot);
    _server.on("/script.js", HTTP_GET, handleScript);
    _server.on("/api/key", HTTP_POST, handleKey);
    _server.on("/api/status", HTTP_GET, handleStatus);
    _server.on("/api/slot", HTTP_POST, handleSlot);
    _server.onNotFound(handleNotFound);

    _server.begin();
    Serial.println("[Web] AsyncWebServer started");
    _initialized = true;
}

void WebManager::end() {
    if (!_initialized) {
        Serial.println("[Web] Not initialized, skipping cleanup");
        return;
    }

    Serial.println("[Web] Beginning cleanup...");

    // 1. Stop the web server
    _server.end();
    Serial.println("[Web] Web server stopped");

    // 2. Disconnect WiFi AP clients and stop AP
    WiFi.softAPdisconnect(true);
    Serial.println("[Web] WiFi AP disconnected");

    // 3. Turn off WiFi completely
    WiFi.mode(WIFI_OFF);
    Serial.println("[Web] WiFi turned off");

    // 4. Unmount SPIFFS
    SPIFFS.end();
    Serial.println("[Web] SPIFFS unmounted");

    // 5. Clear references
    _bleManager = nullptr;
    _usbHidManager = nullptr;

    _initialized = false;
    Serial.println("[Web] Cleanup complete");
}

void WebManager::loop()
{
}

BLEManager *WebManager::getBLEManager()
{
    return _bleManager;
}

void WebManager::setBLEManager(BLEManager *bleManager)
{
    _bleManager = bleManager;
    _usbHidManager = nullptr;  // Clear USB when setting BLE
}

USBHIDManager *WebManager::getUSBHIDManager()
{
    return _usbHidManager;
}

void WebManager::setUSBHIDManager(USBHIDManager *usbHidManager)
{
    _usbHidManager = usbHidManager;
    _bleManager = nullptr;  // Clear BLE when setting USB
}

void WebManager::handleRoot(AsyncWebServerRequest *request)
{
    if (SPIFFS.exists("/index.html"))
    {
        Serial.println("[Web] Serving index.html from SPIFFS");
        request->send(SPIFFS, "/index.html", "text/html");
    }
    else
    {
        Serial.println("[Web] Serving embedded HTML fallback");
        request->send(200, "text/html", getEmbeddedHTML());
    }
}

void WebManager::handleScript(AsyncWebServerRequest *request)
{
    if (SPIFFS.exists("/script.js"))
    {
        Serial.println("[Web] Serving script.js from SPIFFS");
        request->send(SPIFFS, "/script.js", "application/javascript");
    }
    else
    {
        Serial.println("[Web] Serving embedded JS fallback");
        request->send(200, "application/javascript", getEmbeddedJS());
    }
}

void WebManager::handleKey(AsyncWebServerRequest *request)
{
    // Check if any output is configured
    if (!_bleManager && !_usbHidManager)
    {
        request->send(500, "application/json", "{\"error\":\"No output configured\"}");
        return;
    }

    if (!request->hasParam("keycode", true))
    {
        request->send(400, "application/json", "{\"error\":\"Missing keycode\"}");
        return;
    }

    uint8_t keycode = request->getParam("keycode", true)->value().toInt();
    uint8_t modifiers = 0;

    if (request->hasParam("modifiers", true))
    {
        modifiers = request->getParam("modifiers", true)->value().toInt();
    }

    // Check connection based on output type
    bool connected = false;
    const char* outputType = "Unknown";

    if (_usbHidManager) {
        connected = _usbHidManager->isConnected();
        outputType = "USB";
    } else if (_bleManager) {
        connected = _bleManager->isConnected();
        outputType = "BLE";
    }

    if (!connected) {
        char error[64];
        snprintf(error, sizeof(error), "{\"error\":\"%s not connected\"}", outputType);
        request->send(503, "application/json", error);
        return;
    }

    // Send key to appropriate output
    if (_usbHidManager) {
        _usbHidManager->sendKey(keycode, modifiers);
    } else if (_bleManager) {
        _bleManager->sendKey(keycode, modifiers);
    }

    request->send(200, "application/json", "{\"status\":\"ok\"}");
}

void WebManager::handleStatus(AsyncWebServerRequest *request)
{
    // Check if any output is configured
    if (!_bleManager && !_usbHidManager)
    {
        request->send(500, "application/json", "{\"error\":\"No output configured\"}");
        return;
    }

    bool connected = false;
    const char* outputType = "Unknown";

    if (_usbHidManager) {
        connected = _usbHidManager->isConnected();
        outputType = "USB";
    } else if (_bleManager) {
        connected = _bleManager->isConnected();
        outputType = "BLE";
    }

    uint8_t slot = Bridge::getCurrentSlot();
    const char* mode = ModeManager::getModeName();

    char response[192];
    snprintf(response, sizeof(response),
             "{\"connected\":%s,\"slot\":%d,\"mode\":\"%s\",\"output\":\"%s\"}",
             connected ? "true" : "false",
             slot + 1,
             mode,
             outputType);

    request->send(200, "application/json", response);
}

void WebManager::handleSlot(AsyncWebServerRequest *request)
{
    if (!request->hasParam("slot", true))
    {
        request->send(400, "application/json", "{\"error\":\"Missing slot\"}");
        return;
    }

    uint8_t slot = request->getParam("slot", true)->value().toInt() - 1;

    if (slot >= NUM_DEVICE_SLOTS)
    {
        request->send(400, "application/json", "{\"error\":\"Invalid slot\"}");
        return;
    }

    Bridge::switchToSlot(slot);
    request->send(200, "application/json", "{\"status\":\"ok\"}");
}

void WebManager::handleNotFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

const char *WebManager::getEmbeddedHTML()
{
    return R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="mobile-web-app-capable" content="yes">
    <title>EKey</title>
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css" rel="stylesheet">
    <style>
        body { background-color: #f8f9fa; padding-bottom: 20px; }
        .keyboard-key { min-width: 40px; min-height: 45px; margin: 2px; font-size: 12px; font-weight: bold; touch-action: manipulation; -webkit-tap-highlight-color: transparent; }
        @media (max-width: 576px) { .keyboard-key { min-width: 30px; min-height: 40px; font-size: 11px; margin: 1px; padding: 4px 6px; } }
        @media (min-width: 768px) { .keyboard-key { min-width: 50px; min-height: 50px; font-size: 14px; margin: 3px; } }
        .keyboard-row { display: flex; justify-content: center; flex-wrap: wrap; margin-bottom: 4px; }
        @media (max-width: 576px) { .keyboard-row { margin-bottom: 2px; } }
        .keyboard-container { overflow-x: auto; -webkit-overflow-scrolling: touch; }
        .status-indicator { width: 12px; height: 12px; border-radius: 50%; display: inline-block; margin-right: 8px; flex-shrink: 0; }
        .status-connected { background-color: #28a745; box-shadow: 0 0 8px rgba(40, 167, 69, 0.6); }
        .status-disconnected { background-color: #dc3545; }
        .card { box-shadow: 0 2px 4px rgba(0,0,0,0.1); border: none; }
        .slot-btn { font-weight: 500; }
        .slot-btn.active { font-weight: bold; }
        h1 { font-size: 1.75rem; }
        @media (max-width: 576px) { h1 { font-size: 1.5rem; } }
        .status-row { display: flex; align-items: center; gap: 8px; }
        .mode-badge { font-size: 0.75rem; padding: 2px 6px; }
    </style>
</head>
<body>
    <div class="container-fluid px-3 mt-3">
        <h1 class="text-center mb-3">EKey</h1>
        <div id="statusPanel" class="card mb-3">
            <div class="card-body py-2">
                <div class="status-row mb-2">
                    <span id="bleStatus" class="status-indicator status-disconnected"></span>
                    <span id="statusText" class="fw-bold">Disconnected</span>
                    <span id="modeBadge" class="badge bg-secondary mode-badge">WEB</span>
                    <span id="outputBadge" class="badge bg-info mode-badge">BLE</span>
                </div>
                <div class="text-muted small">Slot: <span id="currentSlot" class="fw-bold">1</span></div>
            </div>
        </div>
        <div class="card mb-3">
            <div class="card-body py-2">
                <h6 class="card-title mb-2">Slot Selection</h6>
                <div class="btn-group w-100" role="group">
                    <button type="button" class="btn btn-outline-primary slot-btn" data-slot="1">1</button>
                    <button type="button" class="btn btn-outline-primary slot-btn" data-slot="2">2</button>
                    <button type="button" class="btn btn-outline-primary slot-btn" data-slot="3">3</button>
                </div>
            </div>
        </div>
        <div class="card">
            <div class="card-body py-2">
                <h6 class="card-title mb-2">Virtual Keyboard</h6>
                <div class="keyboard-container">
                    <div id="keyboard"></div>
                </div>
            </div>
        </div>
    </div>
    <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/js/bootstrap.bundle.min.js"></script>
    <script src="/script.js"></script>
</body>
</html>)";
}

const char *WebManager::getEmbeddedJS()
{
    return R"(const HID_KEYCODES={'a':4,'b':5,'c':6,'d':7,'e':8,'f':9,'g':10,'h':11,'i':12,'j':13,'k':14,'l':15,'m':16,'n':17,'o':18,'p':19,'q':20,'r':21,'s':22,'t':23,'u':24,'v':25,'w':26,'x':27,'y':28,'z':29,'1':30,'2':31,'3':32,'4':33,'5':34,'6':35,'7':36,'8':37,'9':38,'0':39,'Enter':40,'Escape':41,'Backspace':42,'Tab':43,'Space':44,'-':45,'=':46,'[':47,']':48,'\\':49,';':51,"'":52,'`':53,',':54,'.':55,'/':56,'CapsLock':57,'F1':58,'F2':59,'F3':60,'F4':61,'F5':62,'F6':63,'F7':64,'F8':65,'F9':66,'F10':67,'F11':68,'F12':69,'Insert':73,'Home':74,'PageUp':75,'Delete':76,'End':77,'PageDown':78,'ArrowRight':79,'ArrowLeft':80,'ArrowDown':81,'ArrowUp':82};let shiftPressed=false;let lastKeyPress=0;const DEBOUNCE_MS=50;let statusUpdateInterval=null;function createKeyboard(){const keyboard=document.getElementById('keyboard');keyboard.innerHTML='';const layout=[['`','1','2','3','4','5','6','7','8','9','0','-','=','Backspace'],['Tab','q','w','e','r','t','y','u','i','o','p','[',']','\\'],['CapsLock','a','s','d','f','g','h','j','k','l',';',"'",'Enter'],['Shift','z','x','c','v','b','n','m',',','.','/','Shift'],['Space']];layout.forEach((row)=>{const rowDiv=document.createElement('div');rowDiv.className='keyboard-row';row.forEach(key=>{const btn=document.createElement('button');btn.className='btn btn-secondary keyboard-key';btn.textContent=key;btn.dataset.key=key.toLowerCase();btn.type='button';if(key==='Shift'){const handleShiftStart=(e)=>{e.preventDefault();shiftPressed=true;btn.classList.add('active');};const handleShiftEnd=(e)=>{e.preventDefault();shiftPressed=false;btn.classList.remove('active');};btn.addEventListener('mousedown',handleShiftStart);btn.addEventListener('mouseup',handleShiftEnd);btn.addEventListener('mouseleave',handleShiftEnd);btn.addEventListener('touchstart',handleShiftStart,{passive:false});btn.addEventListener('touchend',handleShiftEnd,{passive:false});btn.addEventListener('touchcancel',handleShiftEnd,{passive:false});}else{const handleKeyPress=(e)=>{e.preventDefault();sendKey(key);};btn.addEventListener('click',handleKeyPress);btn.addEventListener('touchend',handleKeyPress,{passive:false});}rowDiv.appendChild(btn);});keyboard.appendChild(rowDiv);});}function sendKey(key){const now=Date.now();if(now-lastKeyPress<DEBOUNCE_MS)return;lastKeyPress=now;const keyLower=key.toLowerCase();let keycode=HID_KEYCODES[keyLower]||HID_KEYCODES[key];if(!keycode){console.warn('Unknown key:',key);return;}let modifiers=0;if(shiftPressed||(key>='A'&&key<='Z'&&key!==keyLower)){modifiers=0x02;if(keycode>=4&&keycode<=29){keycode=HID_KEYCODES[keyLower];}}fetch('/api/key',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:`keycode=${keycode}&modifiers=${modifiers}`}).catch(err=>{console.error('Error sending key:',err);});}function updateStatus(){fetch('/api/status').then(response=>{if(!response.ok)throw new Error('Network error');return response.json();}).then(data=>{const statusIndicator=document.getElementById('bleStatus');const statusText=document.getElementById('statusText');const currentSlot=document.getElementById('currentSlot');const modeBadge=document.getElementById('modeBadge');const outputBadge=document.getElementById('outputBadge');if(data.connected){statusIndicator.className='status-indicator status-connected';statusText.textContent='Connected';}else{statusIndicator.className='status-indicator status-disconnected';statusText.textContent='Disconnected';}currentSlot.textContent=data.slot;if(data.mode){modeBadge.textContent=data.mode;}if(data.output){outputBadge.textContent=data.output;outputBadge.className='badge mode-badge '+(data.output==='USB'?'bg-success':'bg-info');}document.querySelectorAll('.slot-btn').forEach(btn=>{if(parseInt(btn.dataset.slot)===data.slot){btn.classList.add('active');}else{btn.classList.remove('active');}});}).catch(err=>{console.error('Error fetching status:',err);});}document.querySelectorAll('.slot-btn').forEach(btn=>{btn.addEventListener('click',()=>{const slot=btn.dataset.slot;fetch('/api/slot',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:`slot=${slot}`}).then(()=>{setTimeout(updateStatus,500);}).catch(err=>{console.error('Error switching slot:',err);});});});createKeyboard();updateStatus();if(statusUpdateInterval)clearInterval(statusUpdateInterval);statusUpdateInterval=setInterval(updateStatus,2000);)";
}
