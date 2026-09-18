#include "web_config.h"

#include <ESPmDNS.h>
#include <Update.h>
#include <WiFi.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace camper {
namespace {

const char kPageStart[] PROGMEM = R"HTML(<!doctype html>
<html lang="es"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Camper Level</title><style>
:root{color-scheme:dark;--bg:#0d1117;--card:#161b22;--line:#30363d;--blue:#2f81f7;--ok:#3fb950;--warn:#d29922;--bad:#f85149}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:#f0f6fc;font:16px system-ui,sans-serif}main{max-width:720px;margin:auto;padding:18px}
h1{margin:4px 0 2px}h2{font-size:1.1rem;margin:26px 0 10px}.muted{color:#8b949e}.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin:14px 0}
.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}.reading{text-align:center;padding:12px;background:#0d1117;border-radius:10px}.reading b{display:block;font-size:1.55rem}
label{display:block;margin:13px 0 5px}input,select,button{width:100%;font:inherit;border-radius:9px;padding:11px;border:1px solid var(--line);background:#0d1117;color:#f0f6fc}
button{border:0;background:var(--blue);font-weight:700;margin-top:12px;cursor:pointer}.secondary{background:#30363d}.danger{background:#8e2724}.inline{display:flex;gap:12px}.inline>*{flex:1}
details{margin-top:16px;border-top:1px solid var(--line);padding-top:12px}summary{cursor:pointer;font-weight:700}.check{display:flex;gap:9px;align-items:center}.check input{width:auto}
#quality{font-weight:800}.LEVEL{color:var(--ok)}.SLIGHTLY_UNLEVEL{color:var(--warn)}.UNLEVEL{color:var(--bad)}
@media(max-width:520px){.inline{display:block}.grid{grid-template-columns:1fr 1fr}}
</style></head><body><main><h1>Camper Level</h1><p class="muted">Nivel práctico para camper y autocaravana</p>
<section class="card"><div id="quality">Cargando…</div><div class="grid">
<div class="reading">Pitch<b id="pitch">--</b></div><div class="reading">Roll<b id="roll">--</b></div>
<div class="reading">FL<b id="fl">--</b></div><div class="reading">FR<b id="fr">--</b></div>
<div class="reading">RL<b id="rl">--</b></div><div class="reading">RR<b id="rr">--</b></div></div>
<form method="post" action="/zero"><button class="secondary" type="submit">Establecer posición actual como NIVEL 0</button></form></section>
<form class="card" method="post" action="/save"><h2>Conexión Wi-Fi</h2>
<label>Nombre de la red</label><input required name="wifi_ssid" list="wifi-list" value=")HTML";

const char kPageMiddle[] PROGMEM = R"HTML("><datalist id="wifi-list"></datalist>
<label>Contraseña Wi-Fi</label><input name="wifi_password" type="password" autocomplete="new-password" placeholder="Déjala vacía para conservarla">
<h2>Conexión con Venus OS</h2>
<p class="muted">Activa <b>Ajustes → Integraciones → Acceso MQTT</b> en Venus OS. Después introduce su IP local, la misma que aparece en Conectividad → Wi-Fi/Ethernet.</p>
<label>IP de Venus OS</label><input required name="mqtt_server" placeholder="Ejemplo: 192.168.1.50" value=")HTML";

const char kPageDimensions[] PROGMEM = R"HTML("><details><summary>MQTT avanzado / broker externo</summary><p class="muted">Con Venus OS normal no cambies estos campos.</p><div class="inline"><div><label>Puerto</label><input required name="mqtt_port" type="number" min="1" max="65535" value=")HTML";

const char kPageTailA[] PROGMEM = R"HTML("></div><div><label>Usuario</label><input name="mqtt_username" value=")HTML";

const char kPageTailB[] PROGMEM = R"HTML("></div></div><label>Contraseña MQTT</label><input name="mqtt_password" type="password" autocomplete="new-password" placeholder="Déjala vacía para conservarla">
</details><h2>Medidas de la camper</h2><div class="inline"><div><label>Batalla (mm)</label><input required name="wheelbase_mm" type="number" min="500" max="12000" step="10" value=")HTML";

const char kPageTailC[] PROGMEM = R"HTML("></div><div><label>Vía delantera (mm)</label><input required name="front_track_mm" type="number" min="500" max="4000" step="10" value=")HTML";

const char kPageTailD[] PROGMEM = R"HTML("></div></div><label>Vía trasera (mm)</label><input required name="rear_track_mm" type="number" min="500" max="4000" step="10" value=")HTML";

const char kPageTailE[] PROGMEM = R"HTML("><h2>Sensibilidad</h2><select name="sensitivity">
<option value="precise">Precisa (±0,25°)</option><option value="camper">Burbuja camper (±0,5°)</option><option value="relaxed">Relajada (±1,0°)</option></select>
<details><summary>Opciones avanzadas</summary>
<label>Nombre del dispositivo</label><input name="device_name" value=")HTML";

const char kPageTailF[] PROGMEM = R"HTML("><label>Topic MQTT</label><input name="mqtt_base_topic" value=")HTML";

const char kPageTailG[] PROGMEM = R"HTML("><label>Nueva contraseña de esta web</label><input name="web_password" type="password" minlength="8" placeholder="Mínimo 8 caracteres; vacía para conservarla">
<label class="check"><input type="checkbox" name="swap_axes" value="1"> Intercambiar pitch y roll</label>
<label class="check"><input type="checkbox" name="invert_pitch" value="1"> Invertir pitch</label>
<label class="check"><input type="checkbox" name="invert_roll" value="1"> Invertir roll</label></details>
<button type="submit">Guardar y reiniciar</button></form>
<section class="card"><b>Acceso posterior</b><p>Abre <code>http://camper-level.local</code>. Usuario: <code>admin</code>.</p>
<form method="post" action="/reboot"><button class="danger" type="submit">Reiniciar ESP32</button></form>
<details><summary>Actualizar firmware OTA</summary><p class="muted">Selecciona el archivo <code>firmware.bin</code> oficial. La configuración y el nivel cero se conservan.</p>
<form method="post" action="/update" enctype="multipart/form-data"><input required type="file" name="firmware" accept=".bin"><button type="submit">Instalar actualización</button></form></details></section>
<p class="muted">La precisión completa se conserva internamente. La pantalla redondea las ruedas a centímetros.</p></main>
<script>
const q=document.getElementById('quality');function update(){fetch('/api/status').then(r=>r.json()).then(s=>{q.textContent=s.valid?s.quality+' · '+s.stability:'IMU SIN DATOS';q.className=s.quality||'';
for(const k of ['pitch','roll'])document.getElementById(k).textContent=s.valid?Number(s[k]).toFixed(1)+'°':'--';for(const k of ['fl','fr','rl','rr'])document.getElementById(k).textContent=s.valid?'+'+Math.round(s[k])+' mm':'--';}).catch(()=>{});}update();setInterval(update,1000);
function networks(){fetch('/api/networks').then(r=>r.json()).then(x=>{const d=document.getElementById('wifi-list');d.innerHTML='';for(const s of x.networks||[]){const o=document.createElement('option');o.value=s;d.appendChild(o)}if(x.scanning)setTimeout(networks,1500)}).catch(()=>{});}networks();
const sensitivity=')HTML";

const char kPageEnd[] PROGMEM = R"HTML(';document.querySelector('[name=sensitivity]').value=sensitivity;
</script></body></html>)HTML";

const char* sensitivityName(const LevelConfig& level) {
  if (fabsf(level.perfectToleranceDeg - 0.25f) < 0.02f) return "precise";
  if (fabsf(level.perfectToleranceDeg - 1.0f) < 0.02f) return "relaxed";
  return "camper";
}

}  // namespace

constexpr char WebConfig::kApPassword[];

void WebConfig::begin(ConfigManager& config, uint32_t now) {
  config_ = &config;
  const uint32_t suffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFULL);
  snprintf(apName_, sizeof(apName_), "CamperLevel-%06lX", static_cast<unsigned long>(suffix));
  registerRoutes();
  server_.begin();
  serverStarted_ = true;
  wifiFallbackAtMs_ = now + kFallbackDelayMs;
  if (!networkConfigured()) startAccessPoint();
  WiFi.scanNetworks(true, true);
}

void WebConfig::update(uint32_t now, const WebLevelStatus& status) {
  status_ = status;
  if (apActive_) dns_.processNextRequest();
  if (serverStarted_) server_.handleClient();

  if (WiFi.status() == WL_CONNECTED) {
    wifiFallbackAtMs_ = now + kFallbackDelayMs;
    if (!mdnsStarted_ && MDNS.begin(config_->network().deviceName)) {
      MDNS.addService("http", "tcp", 80);
      mdnsStarted_ = true;
    }
  } else if (!apActive_ && static_cast<int32_t>(now - wifiFallbackAtMs_) >= 0) {
    startAccessPoint();
  }

  if (rebootAtMs_ && static_cast<int32_t>(now - rebootAtMs_) >= 0) {
    delay(50);
    ESP.restart();
  }
}

bool WebConfig::takeLevelZeroApplied() {
  const bool result = levelZeroApplied_;
  levelZeroApplied_ = false;
  return result;
}

bool WebConfig::networkConfigured() const {
  return config_ && config_->network().wifiSsid[0] && config_->network().mqttServer[0];
}

void WebConfig::startAccessPoint() {
  if (apActive_) return;
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(apName_, kApPassword)) return;
  dns_.start(53, "*", WiFi.softAPIP());
  apActive_ = true;
  Serial.printf("INFO setup portal: Wi-Fi %s password %s, open http://192.168.4.1\n", apName_, kApPassword);
}

bool WebConfig::authorize() {
  if (!networkConfigured()) return true;
  if (server_.authenticate("admin", config_->network().webPassword)) return true;
  server_.requestAuthentication(BASIC_AUTH, "Camper Level", "Introduce la contraseña del panel");
  return false;
}

void WebConfig::registerRoutes() {
  server_.on("/", HTTP_GET, [this] { handleRoot(); });
  server_.on("/api/status", HTTP_GET, [this] { handleStatus(); });
  server_.on("/api/networks", HTTP_GET, [this] { handleNetworks(); });
  server_.on("/save", HTTP_POST, [this] { handleSave(); });
  server_.on("/zero", HTTP_POST, [this] { handleLevelZero(); });
  server_.on("/reboot", HTTP_POST, [this] { handleReboot(); });
  server_.on("/update", HTTP_POST, [this] { handleUpdateFinished(); }, [this] { handleUpdateUpload(); });
  server_.on("/generate_204", HTTP_ANY, [this] { handleCaptivePortal(); });
  server_.on("/hotspot-detect.html", HTTP_ANY, [this] { handleCaptivePortal(); });
  server_.on("/ncsi.txt", HTTP_ANY, [this] { handleCaptivePortal(); });
  server_.onNotFound([this] { handleCaptivePortal(); });
}

void WebConfig::handleRoot() {
  if (!authorize()) return;
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "text/html; charset=utf-8", renderPage());
}

void WebConfig::handleStatus() {
  if (!authorize()) return;
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", statusJson());
}

void WebConfig::handleNetworks() {
  if (!authorize()) return;
  const int count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING) {
    server_.send(200, "application/json", "{\"scanning\":true,\"networks\":[]}");
    return;
  }
  if (count < 0) {
    WiFi.scanNetworks(true, true);
    server_.send(200, "application/json", "{\"scanning\":true,\"networks\":[]}");
    return;
  }
  String result = "{\"scanning\":false,\"networks\":[";
  for (int i = 0; i < count; ++i) {
    const String ssid = WiFi.SSID(i);
    bool duplicate = false;
    for (int previous = 0; previous < i; ++previous) if (WiFi.SSID(previous) == ssid) duplicate = true;
    if (duplicate || !ssid.length()) continue;
    if (result[result.length() - 1] != '[') result += ',';
    result += '"'; result += jsonEscape(ssid); result += '"';
  }
  result += "]}";
  WiFi.scanDelete();
  server_.send(200, "application/json", result);
}

void WebConfig::handleSave() {
  if (!authorize()) return;
  LevelConfig level = config_->level();
  NetworkConfig network = config_->network();
  String error;
  const String newSsid = server_.arg("wifi_ssid");
  const bool ssidChanged = newSsid != network.wifiSsid;
  if (!copyArg(newSsid, network.wifiSsid, sizeof(network.wifiSsid), error) || !newSsid.length()) {
    server_.send(400, "text/plain; charset=utf-8", error.length() ? error : "Falta la red Wi-Fi"); return;
  }
  const String wifiPassword = server_.arg("wifi_password");
  if ((wifiPassword.length() || ssidChanged || !network.wifiPassword[0]) &&
      !copyArg(wifiPassword, network.wifiPassword, sizeof(network.wifiPassword), error)) {
    server_.send(400, "text/plain; charset=utf-8", error); return;
  }
  if (!copyArg(server_.arg("mqtt_server"), network.mqttServer, sizeof(network.mqttServer), error) || !network.mqttServer[0] ||
      !copyArg(server_.arg("mqtt_username"), network.mqttUsername, sizeof(network.mqttUsername), error) ||
      !copyArg(server_.arg("mqtt_base_topic"), network.mqttBaseTopic, sizeof(network.mqttBaseTopic), error) ||
      !copyArg(server_.arg("device_name"), network.deviceName, sizeof(network.deviceName), error)) {
    server_.send(400, "text/plain; charset=utf-8", error.length() ? error : "Falta el servidor MQTT"); return;
  }
  const String mqttPassword = server_.arg("mqtt_password");
  if (mqttPassword.length() && !copyArg(mqttPassword, network.mqttPassword, sizeof(network.mqttPassword), error)) {
    server_.send(400, "text/plain; charset=utf-8", error); return;
  }
  if (!network.mqttUsername[0] && !mqttPassword.length()) network.mqttPassword[0] = '\0';
  const String webPassword = server_.arg("web_password");
  if (webPassword.length() && !copyArg(webPassword, network.webPassword, sizeof(network.webPassword), error)) {
    server_.send(400, "text/plain; charset=utf-8", error); return;
  }
  bool ok = true;
  const uint32_t mqttPort = parseUnsignedArg(server_.arg("mqtt_port"), network.mqttPort, ok);
  if (mqttPort < 1 || mqttPort > 65535) ok = false;
  network.mqttPort = static_cast<uint16_t>(mqttPort);
  level.wheelbaseMm = parseFloatArg(server_.arg("wheelbase_mm"), level.wheelbaseMm, ok);
  level.frontTrackMm = parseFloatArg(server_.arg("front_track_mm"), level.frontTrackMm, ok);
  level.rearTrackMm = parseFloatArg(server_.arg("rear_track_mm"), level.rearTrackMm, ok);
  if (!ok) { server_.send(400, "text/plain; charset=utf-8", "Hay un número no válido"); return; }

  const String sensitivity = server_.arg("sensitivity");
  if (sensitivity == "precise") { level.perfectToleranceDeg = 0.25f; level.acceptableToleranceDeg = 0.5f; level.stableVariationDeg = 0.12f; }
  else if (sensitivity == "relaxed") { level.perfectToleranceDeg = 1.0f; level.acceptableToleranceDeg = 1.5f; level.stableVariationDeg = 0.25f; }
  else { level.perfectToleranceDeg = 0.5f; level.acceptableToleranceDeg = 1.0f; level.stableVariationDeg = 0.2f; }
  level.swapAxes = server_.hasArg("swap_axes");
  level.invertPitch = server_.hasArg("invert_pitch");
  level.invertRoll = server_.hasArg("invert_roll");

  if (!config_->replace(level, network, error)) {
    server_.send(400, "text/plain; charset=utf-8", "No se pudo guardar: " + error); return;
  }
  server_.send(200, "text/html; charset=utf-8", "<meta name=viewport content='width=device-width'><h2>Configuración guardada</h2><p>El ESP32 se reiniciará y conectará a la red.</p>");
  rebootAtMs_ = millis() + 1500;
}

void WebConfig::handleLevelZero() {
  if (!authorize()) return;
  if (!status_.valid || !config_->setLevelZero(status_.orientedPitchDeg, status_.orientedRollDeg)) {
    server_.send(409, "text/plain; charset=utf-8", "No hay una lectura válida o no se pudo guardar"); return;
  }
  levelZeroApplied_ = true;
  server_.sendHeader("Location", "/", true);
  server_.send(303, "text/plain", "Nivel cero guardado");
}

void WebConfig::handleReboot() {
  if (!authorize()) return;
  server_.send(200, "text/html; charset=utf-8", "<meta name=viewport content='width=device-width'><h2>Reiniciando…</h2>");
  rebootAtMs_ = millis() + 1000;
}

void WebConfig::handleUpdateUpload() {
  HTTPUpload& upload = server_.upload();
  if (upload.status == UPLOAD_FILE_START) {
    updateAuthorized_ = authorize();
    updateFailed_ = !updateAuthorized_ || !Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (upload.status == UPLOAD_FILE_WRITE && updateAuthorized_ && !updateFailed_) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) updateFailed_ = true;
  } else if (upload.status == UPLOAD_FILE_END && updateAuthorized_ && !updateFailed_) {
    if (!Update.end(true)) updateFailed_ = true;
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort(); updateFailed_ = true;
  }
}

void WebConfig::handleUpdateFinished() {
  if (!updateAuthorized_) return;
  if (updateFailed_ || Update.hasError()) {
    server_.send(500, "text/plain; charset=utf-8", "La actualización ha fallado; el firmware anterior sigue activo");
    return;
  }
  server_.send(200, "text/html; charset=utf-8", "<meta name=viewport content='width=device-width'><h2>Actualización instalada</h2><p>Reiniciando…</p>");
  rebootAtMs_ = millis() + 1200;
}

void WebConfig::handleCaptivePortal() {
  if (apActive_) {
    server_.sendHeader("Location", "http://192.168.4.1/", true);
    server_.send(302, "text/plain", "Camper Level");
  } else {
    server_.send(404, "text/plain", "Not found");
  }
}

String WebConfig::renderPage() const {
  const auto& network = config_->network();
  const auto& level = config_->level();
  String page; page.reserve(10500);
  page += FPSTR(kPageStart); page += htmlEscape(network.wifiSsid);
  page += FPSTR(kPageMiddle); page += htmlEscape(network.mqttServer);
  page += FPSTR(kPageDimensions); page += network.mqttPort;
  page += FPSTR(kPageTailA); page += htmlEscape(network.mqttUsername);
  page += FPSTR(kPageTailB); page += String(level.wheelbaseMm, 0);
  page += FPSTR(kPageTailC); page += String(level.frontTrackMm, 0);
  page += FPSTR(kPageTailD); page += String(level.rearTrackMm, 0);
  page += FPSTR(kPageTailE); page += htmlEscape(network.deviceName);
  page += FPSTR(kPageTailF); page += htmlEscape(network.mqttBaseTopic);
  page += FPSTR(kPageTailG); page += sensitivityName(level); page += FPSTR(kPageEnd);
  const String selectedProfile = String("value=\"") + sensitivityName(level) + "\"";
  page.replace(selectedProfile, selectedProfile + " selected");
  if (level.swapAxes) page.replace("name=\"swap_axes\" value=\"1\"", "name=\"swap_axes\" value=\"1\" checked");
  if (level.invertPitch) page.replace("name=\"invert_pitch\" value=\"1\"", "name=\"invert_pitch\" value=\"1\" checked");
  if (level.invertRoll) page.replace("name=\"invert_roll\" value=\"1\"", "name=\"invert_roll\" value=\"1\" checked");
  return page;
}

String WebConfig::statusJson() const {
  char output[512];
  snprintf(output, sizeof(output),
      "{\"valid\":%s,\"pitch\":%.2f,\"roll\":%.2f,\"fl\":%.0f,\"fr\":%.0f,\"rl\":%.0f,\"rr\":%.0f,\"stability\":\"%s\",\"quality\":\"%s\",\"wifi\":\"%s\",\"rssi\":%d,\"ap\":%s}",
      status_.valid ? "true" : "false", status_.pitchDeg, status_.rollDeg,
      status_.practicalCorrectionMm.frontLeft, status_.practicalCorrectionMm.frontRight,
      status_.practicalCorrectionMm.rearLeft, status_.practicalCorrectionMm.rearRight,
      StabilityDetector::stateName(status_.stability), LevelCalculator::qualityName(status_.quality),
      WiFi.status() == WL_CONNECTED ? "connected" : "disconnected", WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0,
      apActive_ ? "true" : "false");
  return String(output);
}

String WebConfig::htmlEscape(const char* value) {
  String result = value ? value : "";
  result.replace("&", "&amp;"); result.replace("\"", "&quot;");
  result.replace("<", "&lt;"); result.replace(">", "&gt;");
  return result;
}

String WebConfig::jsonEscape(const String& value) {
  String result = value;
  result.replace("\\", "\\\\"); result.replace("\"", "\\\"");
  result.replace("\r", "\\r"); result.replace("\n", "\\n");
  return result;
}

bool WebConfig::copyArg(const String& value, char* target, size_t capacity, String& error) {
  if (!target || !capacity || value.length() >= capacity) { error = "Un texto es demasiado largo"; return false; }
  value.toCharArray(target, capacity);
  return true;
}

float WebConfig::parseFloatArg(const String& value, float fallback, bool& ok) {
  if (!ok || !value.length()) { ok = false; return fallback; }
  char* end = nullptr; const float parsed = strtof(value.c_str(), &end);
  if (end == value.c_str() || *end || !isfinite(parsed)) { ok = false; return fallback; }
  return parsed;
}

uint32_t WebConfig::parseUnsignedArg(const String& value, uint32_t fallback, bool& ok) {
  if (!ok || !value.length()) { ok = false; return fallback; }
  char* end = nullptr; const unsigned long parsed = strtoul(value.c_str(), &end, 10);
  if (end == value.c_str() || *end || parsed > 0xFFFFFFFFUL) { ok = false; return fallback; }
  return static_cast<uint32_t>(parsed);
}

}  // namespace camper
