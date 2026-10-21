#include "BotWifi.h"
#include "BotDebug.h"
#include "helpers/MQTTReplyFormat.h"
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <esp_mac.h>
#include <string.h>

#ifndef BOT_WEB_USER
#define BOT_WEB_USER "admin"
#endif
#ifndef BOT_WEB_PASS
#define BOT_WEB_PASS "changeme"
#endif

// How long a saved/compiled-in network may fail to connect at boot before the
// setup AP opens as a fallback (router replaced, moved house, wrong password).
#define BOT_WIFI_FALLBACK_MS   180000UL
#define BOT_WIFI_JOIN_MS       20000UL
#define BOT_WIFI_FORGET_HOLD_MS 10000UL

static const char* NVS_NS = "botwifi";

static char s_ssid[33];
static char s_pass[65];
static char s_wpass[65];     // effective web password
static bool s_wpass_saved;   // a password chosen through setup exists in NVS
static bool s_loaded = false;

enum StaState : uint8_t { STA_IDLE, STA_TRYING, STA_UP };
static StaState s_sta = STA_IDLE;
static unsigned long s_sta_start = 0;

static bool s_setup = false;
static char s_ap_name[24];
static char s_ap_pin[12];
static DNSServer* s_dns = nullptr;
static AsyncWebServer* s_srv = nullptr;

enum JoinState : uint8_t { J_IDLE, J_JOINING, J_OK, J_FAIL };
static volatile JoinState s_join = J_IDLE;
static volatile bool s_join_req = false;
static char s_j_ssid[33], s_j_pass[65], s_j_wpass[65];
static unsigned long s_join_start = 0;
static wl_status_t s_last_bad = WL_IDLE_STATUS;
static char s_fail_msg[24];
static unsigned long s_reboot_at = 0;

static void load() {
  if (s_loaded) return;
  s_loaded = true;
  memset(s_ssid, 0, sizeof(s_ssid));
  memset(s_pass, 0, sizeof(s_pass));
  memset(s_wpass, 0, sizeof(s_wpass));
  Preferences p;
  if (p.begin(NVS_NS, false)) {
    p.getString("ssid", s_ssid, sizeof(s_ssid));
    p.getString("pass", s_pass, sizeof(s_pass));
    p.getString("wpass", s_wpass, sizeof(s_wpass));
    p.end();
  }
  s_wpass_saved = s_wpass[0] != 0;
#ifdef WIFI_SSID
  if (!s_ssid[0]) {
    strncpy(s_ssid, WIFI_SSID, sizeof(s_ssid) - 1);
    strncpy(s_pass, WIFI_PWD, sizeof(s_pass) - 1);
  }
#endif
  if (!s_wpass[0]) strncpy(s_wpass, BOT_WEB_PASS, sizeof(s_wpass) - 1);
}

const char* BotWifi::ssid() { load(); return s_ssid; }
const char* BotWifi::password() { load(); return s_pass; }
const char* BotWifi::webUser() { return BOT_WEB_USER; }
const char* BotWifi::webPass() { load(); return s_wpass; }
bool BotWifi::setupActive() { return s_setup; }
const char* BotWifi::apName() { return s_ap_name; }
const char* BotWifi::apPin() { return s_ap_pin; }

const char* BotWifi::setupStatus() {
  switch (s_join) {
    case J_JOINING: return "Connecting...";
    case J_OK:      return "Connected! Restarting";
    case J_FAIL:    return s_fail_msg;
    default:        return "";
  }
}

void BotWifi::forget() {
  Preferences p;
  if (p.begin(NVS_NS, false)) {
    p.remove("ssid");
    p.remove("pass");
    p.end();
  }
  delay(100);
  ESP.restart();
}

// ---------------------------------------------------------------------------
// Setup page
// ---------------------------------------------------------------------------

static const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>MeshBot setup</title>
<style>
:root{--bg:#f6f7f9;--card:#fff;--fg:#14181f;--mut:#667085;--line:#d9dde3;--acc:#1f6feb;--err:#c0392b}
@media (prefers-color-scheme:dark){:root{--bg:#0f1218;--card:#181c24;--fg:#e8ebf0;--mut:#98a2b3;--line:#2a303b;--acc:#58a6ff;--err:#ff7b72}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:16px/1.45 system-ui,sans-serif}
main{max-width:460px;margin:0 auto;padding:20px 16px 40px}
h1{font-size:1.4rem;margin:8px 0 4px}.sub,.hint,.muted{color:var(--mut)}.hint{font-size:.85rem;margin:4px 0 14px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px;margin:14px 0}
.row{display:flex;justify-content:space-between;align-items:center}
ul{list-style:none;margin:8px 0;padding:0}
li{padding:11px 4px;border-top:1px solid var(--line);display:flex;justify-content:space-between;cursor:pointer}
li:first-child{border-top:0}li.muted{cursor:default}li.sel{color:var(--acc);font-weight:600}
label{display:block;margin:12px 0 0;font-size:.9rem;color:var(--mut)}
input{width:100%;margin-top:4px;padding:12px;border:1px solid var(--line);border-radius:8px;background:var(--bg);color:var(--fg);font-size:1rem}
button{width:100%;margin-top:16px;padding:13px;border:0;border-radius:8px;background:var(--acc);color:#fff;font-size:1rem;font-weight:600}
button.link{width:auto;margin:0;padding:4px 8px;background:none;color:var(--acc);font-weight:500}
#err{color:var(--err);min-height:1.4em;margin:10px 0 0}
</style></head><body><main>
<h1>MeshBot WiFi setup</h1>
<p class="sub">Choose your home WiFi so this device can get online.</p>
<div id="form">
<div class="card"><div class="row"><b>Nearby networks</b><button class="link" id="rs">Rescan</button></div>
<ul id="nets"><li class="muted">Scanning...</li></ul></div>
<div class="card">
<label>Network name<input id="ssid" maxlength="32" autocapitalize="none" autocorrect="off" spellcheck="false"></label>
<label>WiFi password<input id="pw" type="password" maxlength="63" autocomplete="off"></label>
<label>Device page password<input id="wp" type="password" maxlength="63" placeholder="8 or more characters" autocomplete="new-password"></label>
<p class="hint">You will use it to open this device's status page (user: admin).</p>
<div id="err"></div>
<button id="go">Connect</button></div>
</div>
<div id="wait" class="card" hidden><p id="st"></p></div>
<script>
const $=i=>document.getElementById(i);
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
async function scan(){
  $('nets').innerHTML='<li class="muted">Scanning...</li>';
  for(let i=0;i<10;i++){
    try{const d=await(await fetch('/api/scan')).json();if(!d.scanning){render(d.nets);return;}}catch(e){}
    await sleep(1500);
  }
  $('nets').innerHTML='<li class="muted">Nothing found. Type the network name below.</li>';
}
function render(n){
  const ul=$('nets');ul.textContent='';
  if(!n.length){ul.innerHTML='<li class="muted">Nothing found. Type the network name below.</li>';return;}
  n.forEach(x=>{
    const li=document.createElement('li');
    const a=document.createElement('span');a.textContent=x.s;
    const b=document.createElement('span');b.className='muted';
    b.textContent=(x.r>-60?'▂▄▆█':x.r>-72?'▂▄▆':x.r>-82?'▂▄':'▂')+(x.o?' open':'');
    li.append(a,b);
    li.onclick=()=>{$('ssid').value=x.s;[...ul.children].forEach(c=>c.classList.remove('sel'));li.classList.add('sel');$('pw').focus();};
    ul.appendChild(li);
  });
}
function show(wait,msg){$('form').hidden=wait;$('wait').hidden=!wait;if(msg!==undefined)$('st').textContent=msg;}
let fails=0;
async function poll(){
  try{
    const d=await(await fetch('/api/join/status')).json();fails=0;
    if(d.state=='ok'){show(true,'Connected! The device is restarting and joining your WiFi. Reconnect your phone to your home WiFi; the device\'s address is shown on its screen.');return;}
    if(d.state=='fail'){show(false);$('err').textContent=d.msg+'. Check the details and try again.';return;}
  }catch(e){
    if(++fails>=4)$('st').textContent='Lost contact with the device. If it connected, your phone will rejoin your home WiFi and the device\'s screen shows its address. If you are still on MeshBot, refresh this page.';
  }
  setTimeout(poll,2000);
}
$('rs').onclick=scan;
$('go').onclick=async()=>{
  const s=$('ssid').value.trim();
  if(!s){$('err').textContent='Pick a network or type its name.';return;}
  $('err').textContent='';
  show(true,'Connecting to '+s+'...');
  let r;
  try{r=await fetch('/api/join',{method:'POST',body:new URLSearchParams({ssid:s,pass:$('pw').value,wpass:$('wp').value})});}
  catch(e){fails=4;poll();return;}
  if(!r.ok){show(false);$('err').textContent=await r.text();return;}
  fails=0;poll();
};
scan();
</script></main></body></html>)HTML";

static void json_str(char* buf, size_t n, int* pos, const char* s) {
  replyAppendf(buf, n, pos, "\"");
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') replyAppendf(buf, n, pos, "\\%c", c);
    else if (c < 0x20)        replyAppendf(buf, n, pos, "\\u%04x", c);
    else                      replyAppendf(buf, n, pos, "%c", c);
  }
  replyAppendf(buf, n, pos, "\"");
}

static char* s_scan_buf = nullptr;   // PSRAM, allocated only while setup is active
static const size_t SCAN_BUF_SIZE = 2048;

// Async scan: the first call starts it; the page polls until it completes.
// Results are deduplicated by SSID (strongest wins), strongest first.
static const char* scan_json() {
  if (!s_scan_buf) return "{\"nets\":[]}";
  int c = WiFi.scanComplete();
  if (c == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);
    return "{\"scanning\":true}";
  }
  if (c == WIFI_SCAN_RUNNING) return "{\"scanning\":true}";

  if (c > 40) c = 40;
  bool used[40] = {false};
  int pos = 0;
  replyAppendf(s_scan_buf, SCAN_BUF_SIZE, &pos, "{\"nets\":[");
  int emitted = 0;
  for (int k = 0; k < 20; k++) {
    int best = -1;
    for (int i = 0; i < c; i++) {
      if (used[i] || WiFi.SSID(i).length() == 0) continue;
      if (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best)) best = i;
    }
    if (best < 0) break;
    String name = WiFi.SSID(best);
    for (int i = 0; i < c; i++) if (WiFi.SSID(i) == name) used[i] = true;
    if (emitted++) replyAppendf(s_scan_buf, SCAN_BUF_SIZE, &pos, ",");
    replyAppendf(s_scan_buf, SCAN_BUF_SIZE, &pos, "{\"s\":");
    json_str(s_scan_buf, SCAN_BUF_SIZE, &pos, name.c_str());
    replyAppendf(s_scan_buf, SCAN_BUF_SIZE, &pos, ",\"r\":%d,\"o\":%s}", (int)WiFi.RSSI(best),
                 WiFi.encryptionType(best) == WIFI_AUTH_OPEN ? "true" : "false");
  }
  replyAppendf(s_scan_buf, SCAN_BUF_SIZE, &pos, "]}");
  WiFi.scanDelete();
  return s_scan_buf;
}

static void on_join(AsyncWebServerRequest* r) {
  if (s_join == J_JOINING || s_join == J_OK) { r->send(409, "text/plain", "Already connecting."); return; }
  auto field = [&](const char* k) -> String {
    return r->hasParam(k, true) ? r->getParam(k, true)->value() : String();
  };
  String ssid = field("ssid"), pass = field("pass"), wpass = field("wpass");
  if (ssid.length() < 1 || ssid.length() > 32) { r->send(400, "text/plain", "Network name must be 1-32 characters."); return; }
  if (pass.length() != 0 && (pass.length() < 8 || pass.length() > 63)) {
    r->send(400, "text/plain", "WiFi passwords are 8-63 characters (leave empty for an open network)."); return;
  }
  if (wpass.length() == 0 && !s_wpass_saved) { r->send(400, "text/plain", "Choose a device page password (8 or more characters)."); return; }
  if (wpass.length() != 0 && (wpass.length() < 8 || wpass.length() > 63)) {
    r->send(400, "text/plain", "The device page password needs 8-63 characters."); return;
  }
  strncpy(s_j_ssid, ssid.c_str(), sizeof(s_j_ssid) - 1);   s_j_ssid[sizeof(s_j_ssid) - 1] = 0;
  strncpy(s_j_pass, pass.c_str(), sizeof(s_j_pass) - 1);   s_j_pass[sizeof(s_j_pass) - 1] = 0;
  strncpy(s_j_wpass, wpass.c_str(), sizeof(s_j_wpass) - 1); s_j_wpass[sizeof(s_j_wpass) - 1] = 0;
  s_join_req = true;
  s_join = J_JOINING;
  r->send(200, "text/plain", "ok");
}

static void on_join_status(AsyncWebServerRequest* r) {
  char b[96];
  int pos = 0;
  const char* st = s_join == J_JOINING ? "joining" : s_join == J_OK ? "ok" : s_join == J_FAIL ? "fail" : "idle";
  replyAppendf(b, sizeof(b), &pos, "{\"state\":\"%s\",\"msg\":\"%s\"}", st, s_join == J_FAIL ? s_fail_msg : "");
  r->send(200, "application/json", b);
}

static void start_setup() {
  if (s_setup) return;
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(s_ap_name, sizeof(s_ap_name), "MeshBot-%02X%02X%02X", mac[3], mac[4], mac[5]);
  snprintf(s_ap_pin, sizeof(s_ap_pin), "%08lu", (unsigned long)(10000000UL + esp_random() % 90000000UL));

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(s_ap_name, s_ap_pin, 1, 0, 4);
  IPAddress ip = WiFi.softAPIP();

  s_scan_buf = (char*)ps_malloc(SCAN_BUF_SIZE);
  s_dns = new DNSServer();
  s_dns->start(53, "*", ip);

  s_srv = new AsyncWebServer(80);
  s_srv->on("/", HTTP_GET, [](AsyncWebServerRequest* r) { r->send(200, "text/html", PAGE); });
  s_srv->on("/api/scan", HTTP_GET, [](AsyncWebServerRequest* r) { r->send(200, "application/json", scan_json()); });
  s_srv->on("/api/join", HTTP_POST, on_join);
  s_srv->on("/api/join/status", HTTP_GET, on_join_status);
  // Captive-portal probes (Android /generate_204, iOS /hotspot-detect.html,
  // Windows /connecttest.txt ...) and any other URL land on the setup page.
  s_srv->onNotFound([](AsyncWebServerRequest* r) { r->redirect("http://192.168.4.1/"); });
  s_srv->begin();
  s_setup = true;
  BOT_DEBUG_PRINTLN("WiFi setup AP '%s' up at %s", s_ap_name, ip.toString().c_str());
}

static void handle_join() {
  if (s_join_req) {
    s_join_req = false;
    s_last_bad = WL_IDLE_STATUS;
    s_join_start = millis();
    WiFi.disconnect();
    WiFi.begin(s_j_ssid, s_j_pass);
    return;
  }
  if (s_join != J_JOINING) return;

  wl_status_t st = WiFi.status();
  if (st == WL_CONNECTED) {
    Preferences p;
    if (p.begin(NVS_NS, false)) {
      p.putString("ssid", s_j_ssid);
      p.putString("pass", s_j_pass);
      if (s_j_wpass[0]) p.putString("wpass", s_j_wpass);
      p.end();
    }
    s_join = J_OK;
    s_reboot_at = millis() + 5000;   // let the page poll "ok" once if the link survives
  } else {
    if (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL) s_last_bad = st;
    if (millis() - s_join_start > BOT_WIFI_JOIN_MS) {
      const char* why = s_last_bad == WL_NO_SSID_AVAIL  ? "Network not found"
                      : s_last_bad == WL_CONNECT_FAILED ? "Wrong password"
                                                        : "Could not connect";
      strncpy(s_fail_msg, why, sizeof(s_fail_msg) - 1);
      s_fail_msg[sizeof(s_fail_msg) - 1] = 0;
      WiFi.disconnect();
      s_join = J_FAIL;
    }
  }
}

void BotWifi::begin() {
  load();
  if (s_ssid[0]) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(s_ssid, s_pass);
    s_sta = STA_TRYING;
    s_sta_start = millis();
  } else {
    start_setup();
  }
}

void BotWifi::loop() {
#ifdef PIN_USER_BTN
  // Hold the BOOT button for 10 s to forget the saved WiFi and reopen setup.
  // (Held at power-up it would enter the ROM downloader instead, so this only
  // counts while running.)
  static unsigned long hold_start = 0;
  if (digitalRead(PIN_USER_BTN) == LOW) {
    if (!hold_start) hold_start = millis();
    else if (millis() - hold_start > BOT_WIFI_FORGET_HOLD_MS) forget();
  } else {
    hold_start = 0;
  }
#endif

  if (s_sta == STA_TRYING) {
    if (WiFi.status() == WL_CONNECTED) s_sta = STA_UP;
    else if (millis() - s_sta_start > BOT_WIFI_FALLBACK_MS) start_setup();
  }

  if (!s_setup) return;
  s_dns->processNextRequest();
  handle_join();

  // Fallback mode: the original network came back on its own - just restart.
  if (s_join == J_IDLE && !s_reboot_at && s_sta == STA_UP && WiFi.status() == WL_CONNECTED) {
    s_reboot_at = millis() + 2000;
  }
  if (s_reboot_at && (long)(millis() - s_reboot_at) >= 0) ESP.restart();
}
