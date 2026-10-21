#include "BotWebUI.h"
#include "BotMesh.h"
#include "BotConfig.h"
#include "BotDebug.h"
#include "BotStore.h"
#include "BotHeapLog.h"
#include "BotWifi.h"
#include "BotSettings.h"
#include "BotMqttSettings.h"
#include "BotSettingsPage.h"
#include "BotNodesPage.h"
#include "CommandRouter.h"
#include "PsramJsonAlloc.h"
#include "helpers/MQTTReplyFormat.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#if BOT_DEV_FEATURES
#include <esp_core_dump.h>
#include <esp_flash.h>
#endif

#ifndef BOT_WEB_USER
#define BOT_WEB_USER "admin"
#endif
#ifndef BOT_WEB_PASS
#define BOT_WEB_PASS "changeme"
#endif

extern MESH_CLASS the_mesh;

static AsyncWebServer s_server(80);
static bool s_started = false;

// ---- settings page ---------------------------------------------------------

static bool s_name_changed = false;     // bot name edited this boot: re-advertises after a restart

static void json_escape(char* buf, size_t n, int* pos, const char* s) {
  replyAppendf(buf, n, pos, "\"");
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') replyAppendf(buf, n, pos, "\\%c", c);
    else if (c < 0x20)        replyAppendf(buf, n, pos, "\\u%04x", c);
    else                      replyAppendf(buf, n, pos, "%c", c);
  }
  replyAppendf(buf, n, pos, "\"");
}

static void settings_json(char* b, size_t n) {
  const BotSettings::Data& d = BotSettings::pending();
  int pos = 0;
  replyAppendf(b, n, &pos, "{\"name\":");
  json_escape(b, n, &pos, the_mesh.getNodeName());
  replyAppendf(b, n, &pos, ",\"lat\":%.5f,\"lon\":%.5f,\"tz_min\":%d,", (double)d.lat, (double)d.lon, (int)d.tz_min);
  replyAppendf(b, n, &pos, "\"answer_dm\":%u,\"greeter\":%u,\"wx_metric\":%u,", d.answer_dm, d.greeter, d.wx_metric);
  replyAppendf(b, n, &pos, "\"rl_global\":%u,\"rl_channel\":%u,\"rl_sender\":%u,", d.rl_global, d.rl_channel, d.rl_sender);
  replyAppendf(b, n, &pos, "\"cmd_prefix\":");
  json_escape(b, n, &pos, d.cmd_prefix);
  replyAppendf(b, n, &pos, ",\"allowlist\":");
  json_escape(b, n, &pos, d.allowlist);
  replyAppendf(b, n, &pos, ",\"disabled_cmds\":");
  json_escape(b, n, &pos, d.disabled_cmds);
  replyAppendf(b, n, &pos, ",\"commands\":[");
  for (int i = 0; i < CommandRouter::totalCommands(); i++) {
    if (i) replyAppendf(b, n, &pos, ",");
    json_escape(b, n, &pos, CommandRouter::commandKeyword(i));
  }
  replyAppendf(b, n, &pos, "],\"restart_needed\":%s}", (BotSettings::restartNeeded() || BotMqttSettings::restartNeeded() || s_name_changed) ? "true" : "false");
}

static const char* const SETTING_KEYS[] = {
  "lat", "lon", "tz_min", "answer_dm", "greeter", "wx_metric",
  "rl_global", "rl_channel", "rl_sender", "cmd_prefix", "allowlist", "disabled_cmds",
};

static void settings_post(AsyncWebServerRequest* r) {
  char err[96] = "";

  if (r->hasParam("name", true)) {
    const String& v = r->getParam("name", true)->value();
    bool good = v.length() >= 1 && v.length() < 32;
    for (size_t i = 0; good && i < v.length(); i++) if ((unsigned char)v[i] < 0x20) good = false;
    if (!good) { r->send(400, "text/plain", "Bot name must be 1 to 31 characters."); return; }
    NodePrefs* np = the_mesh.getNodePrefs();
    if (np && strcmp(np->node_name, v.c_str()) != 0) {
      strncpy(np->node_name, v.c_str(), sizeof(np->node_name) - 1);
      np->node_name[sizeof(np->node_name) - 1] = '\0';
      the_mesh.savePrefs();
      s_name_changed = true;
    }
  }
  // MQTT fields: everything the page posts that starts with "mqtt".
  for (size_t i = 0; i < r->params(); i++) {
    const AsyncWebParameter* ap = r->getParam(i);
    if (!ap->isPost() || strncmp(ap->name().c_str(), "mqtt", 4) != 0) continue;
    if (!BotMqttSettings::set(ap->name().c_str(), ap->value().c_str(), err, sizeof(err))) {
      r->send(400, "text/plain", err);
      return;
    }
  }
  for (const char* key : SETTING_KEYS) {
    if (!r->hasParam(key, true)) continue;
    if (!BotSettings::set(key, r->getParam(key, true)->value().c_str(), err, sizeof(err))) {
      r->send(400, "text/plain", err);
      return;
    }
  }
  r->send(200, "application/json", "{\"ok\":true}");
}

static bool auth_ok(AsyncWebServerRequest* r) {
#if BOT_DEV_FEATURES
  BotHeapLog::noteWebRequest();
#endif
  if (r->authenticate(BotWifi::webUser(), BotWifi::webPass())) return true;
  r->requestAuthentication();
  return false;
}

static const size_t STATUS_JSON_BUF_SIZE = 1280;

static void status_json(char* buf, size_t bufsize) {
  const BotMesh::Stats& s = the_mesh.stats;
  NodePrefs* p = the_mesh.getNodePrefs();
  const char* rst = BotMesh::lastResetReason();
  uint32_t up = the_mesh.uptimeSecs();

  int pos = 0;
  replyAppendf(buf, bufsize, &pos, "{");
  replyAppendf(buf, bufsize, &pos, "\"name\":\"%s\",", the_mesh.getNodeName());
  replyAppendf(buf, bufsize, &pos, "\"version\":\"" BOT_VERSION " " __DATE__ " " __TIME__ "\",");
  replyAppendf(buf, bufsize, &pos, "\"uptime_s\":%u,", (unsigned)up);
  replyAppendf(buf, bufsize, &pos, "\"heap_free\":%u,", (unsigned)ESP.getFreeHeap());
  replyAppendf(buf, bufsize, &pos, "\"heap_min\":%u,", (unsigned)ESP.getMinFreeHeap());
  replyAppendf(buf, bufsize, &pos, "\"reset\":\"%s\",", rst ? rst : "clean");
  replyAppendf(buf, bufsize, &pos, "\"clock\":%u,", (unsigned)the_mesh.nowEpoch());
  replyAppendf(buf, bufsize, &pos, "\"clock_ntp\":%s,", BotMesh::clockSynced() ? "true" : "false");
  replyAppendf(buf, bufsize, &pos, "\"wifi_rssi\":%d,", (int)WiFi.RSSI());
  replyAppendf(buf, bufsize, &pos, "\"ip\":\"%s\",", WiFi.localIP().toString().c_str());
  replyAppendf(buf, bufsize, &pos, "\"cache_nodes\":%d,", the_mesh.store().count());
  replyAppendf(buf, bufsize, &pos, "\"msgs\":%u,", (unsigned)s.msgs);
  replyAppendf(buf, bufsize, &pos, "\"matched\":%u,", (unsigned)s.matched);
  replyAppendf(buf, bufsize, &pos, "\"replies\":%u,", (unsigned)s.replies);
  replyAppendf(buf, bufsize, &pos, "\"drop_rate\":%u,", (unsigned)s.dropped_rate);
  replyAppendf(buf, bufsize, &pos, "\"drop_gap\":%u,", (unsigned)s.dropped_gap);
  replyAppendf(buf, bufsize, &pos, "\"drop_full\":%u,", (unsigned)s.dropped_full);
  replyAppendf(buf, bufsize, &pos, "\"send_fail\":%u,", (unsigned)s.send_fail);
  replyAppendf(buf, bufsize, &pos, "\"mqtt_ready\":%s,", the_mesh.mqtt().isReady() ? "true" : "false");
  replyAppendf(buf, bufsize, &pos, "\"mqtt_brokers\":%d,", the_mesh.mqtt().connectedBrokers());
  {
    char mqs[160];
    the_mesh.mqtt().statsReply(mqs, sizeof(mqs));
    replyAppendf(buf, bufsize, &pos, "\"mqtt_stats\":\"%s\",", mqs);
  }
#if BOT_DEV_FEATURES
  replyAppendf(buf, bufsize, &pos, "\"mqtt_pkts\":%lu,", the_mesh.mqtt().packetsSeen());
  replyAppendf(buf, bufsize, &pos, "\"mqtt_lost\":%lu,", the_mesh.mqtt().packetsLostToPressure());
  replyAppendf(buf, bufsize, &pos, "\"mqtt_max_alloc\":%u,", (unsigned)ESP.getMaxAllocHeap());
  replyAppendf(buf, bufsize, &pos, "\"mqtt_skip_threshold\":%u,", (unsigned)the_mesh.mqtt().skipThreshold());
  {
    uint32_t fb, ab, lg;
    BotHeapLog::heapInfo(&fb, &ab, &lg);
    replyAppendf(buf, bufsize, &pos, "\"int_free_blocks\":%lu,\"int_alloc_blocks\":%lu,",
                 (unsigned long)fb, (unsigned long)ab);
    replyAppendf(buf, bufsize, &pos, "\"psram_free\":%lu,",
                 (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    replyAppendf(buf, bufsize, &pos, "\"mqtt_dc\":[%lu,%lu],\"mqtt_renew\":[%lu,%lu],",
                 (unsigned long)the_mesh.mqtt().slotDisconnects(0), (unsigned long)the_mesh.mqtt().slotDisconnects(1),
                 (unsigned long)the_mesh.mqtt().slotRenewalReconnects(0), (unsigned long)the_mesh.mqtt().slotRenewalReconnects(1));
    replyAppendf(buf, bufsize, &pos, "\"http_fetches\":%lu,\"web_reqs\":%lu,\"wifi_dc\":%lu,\"heaplog_n\":%lu,",
                 (unsigned long)BotHeapLog::httpFetches(), (unsigned long)BotHeapLog::webRequests(),
                 (unsigned long)BotHeapLog::wifiDisconnects(), (unsigned long)BotHeapLog::entries());
  }
#endif
  if (p) {
    replyAppendf(buf, bufsize, &pos, "\"lora\":\"%.3fMHz BW%.1f SF%u CR%u\",",
                p->freq, p->bw, (unsigned)p->sf, (unsigned)p->cr);
    replyAppendf(buf, bufsize, &pos, "\"loc\":\"%.5f,%.5f\"", p->node_lat, p->node_lon);
  } else {
    replyAppendf(buf, bufsize, &pos, "\"lora\":\"?\",\"loc\":\"?\"");
  }
  replyAppendf(buf, bufsize, &pos, "}");
}

// Full dump of the persistent node/repeater cache (BotStore), for debugging
// name-resolution issues (e.g. two nodes sharing a 1-byte hop hash prefix) -
// the same cache `path`/`prefix` resolve against, without needing a mesh
// command relayed back by hand.
// PSRAM-backed, allocated once in BotWebUI::begin() - 400 records * ~212B
// worst case (escaped 31-char name + full lat/lon + hash + last_advert/heard) fits
// comfortably with margin, so no truncation logic is needed.
static const size_t NODES_JSON_BUF_SIZE = 98304;
static char* s_nodes_json_buf = nullptr;

static size_t nodes_json(char* buf, size_t bufsize) {
  static const NodeRec* recs[BOT_LOCCACHE_MAX];
  int n = the_mesh.store().listAll(recs, BOT_LOCCACHE_MAX);

  int pos = 0;
  replyAppendf(buf, bufsize, &pos, "{\"total\":%d,\"nodes\":[", n);
  for (int i = 0; i < n; i++) {
    const NodeRec* r = recs[i];
    if (i) replyAppendf(buf, bufsize, &pos, ",");
    replyAppendf(buf, bufsize, &pos, "{\"hash\":\"");
    for (int b = 0; b < 6; b++)
      replyAppendf(buf, bufsize, &pos, "%02x", r->pubkey[b]);
    replyAppendf(buf, bufsize, &pos, "\",\"name\":\"");
    for (const char* pch = r->name; *pch; pch++) {
      if (*pch == '"' || *pch == '\\') replyAppendf(buf, bufsize, &pos, "\\");
      replyAppendf(buf, bufsize, &pos, "%c", *pch);
    }
    replyAppendf(buf, bufsize, &pos, "\",\"type\":%u,\"has_loc\":%s",
                (unsigned)r->type, r->hasLoc() ? "true" : "false");
    if (r->hasLoc())
      replyAppendf(buf, bufsize, &pos, ",\"lat\":%.6f,\"lon\":%.6f",
                  r->lat_e6 / 1e6, r->lon_e6 / 1e6);
    replyAppendf(buf, bufsize, &pos, ",\"last_advert\":%u,\"last_heard\":%u}",
                (unsigned)r->last_advert, (unsigned)r->last_heard);
  }
  replyAppendf(buf, bufsize, &pos, "]}");
  return (size_t)pos;
}

static const char PAGE[] = R"HTML(<!doctype html><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>D4rl3k</title>
<style>
body{font:14px/1.5 system-ui,sans-serif;margin:0;background:#0e1413;color:#dfe5e2}
.w{max-width:640px;margin:0 auto;padding:1.4rem}
h1{font-size:1.2rem;margin:0 0 1rem;color:#3fbaa3}
table{width:100%;border-collapse:collapse}
td{padding:.35rem .2rem;border-bottom:1px solid #263230}
td:first-child{color:#7d8a85;width:40%}
td:last-child{font-variant-numeric:tabular-nums}
a{color:#3fbaa3}
.bad{color:#e07a7a}
.warn{color:#e0c07a}
</style>
<div class=w>
<h1 id=name>D4rl3k</h1>
<p><a href="/nodes">Nodes</a> &middot; <a href="/settings">Settings</a></p>
<table id=t></table>
<p style=color:#7d8a85>auto-refresh 3s</p>
</div>
<script>
const rows=[["version","version",v=>v],
["uptime","uptime_s",v=>{let s=v|0,d=s/86400|0,h=s%86400/3600|0,m=s%3600/60|0;return `${d}d ${h}h ${m}m`}],
["free heap","heap_free",v=>(v/1024|0)+" k"],["min heap","heap_min",v=>(v/1024|0)+" k"],
["last reset","reset",v=>v],
["clock (UTC)","_clock",d=>new Date(d.clock*1000).toISOString().slice(0,19).replace("T"," ")+(d.clock_ntp?" (NTP)":" (no NTP)")],
["wifi","wifi_rssi",v=>v+" dBm"],["ip","ip",v=>v],
["cached nodes","cache_nodes",v=>v],["lora","lora",v=>v],["location","loc",v=>v],
["msgs seen","msgs",v=>v],["matched","matched",v=>v],["replies sent","replies",v=>v],
["drop rate/gap/full","_drop",d=>`${d.drop_rate}/${d.drop_gap}/${d.drop_full}`],
["send fail","send_fail",v=>v],
["mqtt","_mqtt",d=>d.mqtt_ready?`up · ${d.mqtt_brokers} broker${d.mqtt_brokers==1?"":"s"}`:"down"],
["mqtt stats","mqtt_stats",v=>v]];
)HTML"
#if BOT_DEV_FEATURES
R"HTML(rows.push(
["mqtt loss","_loss",d=>d.mqtt_pkts?`${(100*d.mqtt_lost/d.mqtt_pkts).toFixed(2)}% (${d.mqtt_lost} / ${d.mqtt_pkts})`:"-"],
["heap headroom","_room",d=>`${d.mqtt_max_alloc-d.mqtt_skip_threshold} B`]);
)HTML"
#endif
R"HTML(
async function tick(){
 try{
  let d=await (await fetch("/api/status")).json();
  document.getElementById("name").textContent=d.name;
  document.getElementById("t").innerHTML=rows.map(([k,f,fn])=>{
   let v=f[0]==="_"?fn(d):fn(d[f]);
   let cls=(f==="reset"&&d.reset!=="clean")||(f==="send_fail"&&d.send_fail>0)||(f==="_mqtt"&&!d.mqtt_ready)
    ||(f==="_loss"&&d.mqtt_pkts&&d.mqtt_lost*100>=d.mqtt_pkts)||(f==="_room"&&d.mqtt_max_alloc<d.mqtt_skip_threshold)?" class=bad"
    :(f==="_room"&&d.mqtt_max_alloc-d.mqtt_skip_threshold<1024)?" class=warn":"";
   return `<tr><td>${k}</td><td${cls}>${v}</td></tr>`}).join("");
 }catch(e){}
}
tick();setInterval(tick,3000);
</script>
)HTML";

void BotWebUI::begin() {
  if (s_started || BotWifi::setupActive()) return;   // setup AP owns port 80

  if (!s_nodes_json_buf) s_nodes_json_buf = (char*)psram_malloc(NODES_JSON_BUF_SIZE);

  s_server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth_ok(r)) return;
    r->send(200, "text/html", PAGE);
  });
  s_server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth_ok(r)) return;
    char buf[STATUS_JSON_BUF_SIZE];
    status_json(buf, sizeof(buf));
    r->send(200, "application/json", buf);
  });
  s_server.on("/nodes", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth_ok(r)) return;
    r->send(200, "text/html", NODES_PAGE);
  });
  s_server.on("/settings", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth_ok(r)) return;
    r->send(200, "text/html", SETTINGS_PAGE);
  });
  s_server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth_ok(r)) return;
    char buf[1536];   // stack, like status_json: no permanent RAM for a rarely used page
    settings_json(buf, sizeof(buf));
    r->send(200, "application/json", buf);
  });
  s_server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!auth_ok(r)) return;
    settings_post(r);
  });
  s_server.on("/api/mqtt/config", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth_ok(r)) return;
    char buf[1280];
    BotMqttSettings::configJson(buf, sizeof(buf));
    r->send(200, "application/json", buf);
  });
  s_server.on("/api/mqtt/presets", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth_ok(r)) return;
    char buf[1792];
    BotMqttSettings::presetsJson(buf, sizeof(buf));
    r->send(200, "application/json", buf);
  });
  s_server.on("/api/mqtt/status", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth_ok(r)) return;
    char buf[400];
    int pos = 0;
    replyAppendf(buf, sizeof(buf), &pos, "{\"slots\":[");
    for (int i = 0; i < BotMqttSettings::SLOTS; i++) {
      const char *name, *state;
      unsigned long ok, err;
      if (i) replyAppendf(buf, sizeof(buf), &pos, ",");
      if (the_mesh.mqtt().slotStatus(i, &name, &state, &ok, &err))
        replyAppendf(buf, sizeof(buf), &pos, "{\"state\":\"%s\",\"ok\":%lu,\"err\":%lu}", state, ok, err);
      else
        replyAppendf(buf, sizeof(buf), &pos, "{\"state\":\"off\",\"ok\":0,\"err\":0}");
    }
    replyAppendf(buf, sizeof(buf), &pos, "]}");
    r->send(200, "application/json", buf);
  });
  s_server.on("/api/mqtt/reset", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!auth_ok(r)) return;
    BotMqttSettings::reset();
    r->send(200, "application/json", "{\"ok\":true}");
  });
  s_server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!auth_ok(r)) return;
    r->send(200, "application/json", "{\"ok\":true}");
    delay(300);
    ESP.restart();
  });
  // Forget the saved WiFi and reopen the setup AP (the config page will own a button for this).
  s_server.on("/api/wifi/forget", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!auth_ok(r)) return;
    r->send(200, "application/json", "{\"ok\":true}");
    BotWifi::forget();
  });
  s_server.on("/api/nodes", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth_ok(r)) return;
    if (!s_nodes_json_buf) { r->send(503, "text/plain", "buffer unavailable"); return; }
    nodes_json(s_nodes_json_buf, NODES_JSON_BUF_SIZE);
    r->send(200, "application/json", s_nodes_json_buf);
  });

#if BOT_DEV_FEATURES
  // Heap-fragmentation probe: step-change log (?n=newest N entries) and a hook
  // that forces one broker slot's JWT-renewal reconnect (?slot=1|2).
  s_server.on("/api/heaplog", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth_ok(r)) return;
    int n = r->hasParam("n") ? r->getParam("n")->value().toInt() : 0;
    const char* j = BotHeapLog::json(n);
    if (!j) { r->send(503, "text/plain", "buffer unavailable"); return; }
    r->send(200, "application/json", j);
  });
  // Last crash's ELF core dump from the coredump flash partition (raw image
  // for `esp-coredump info_corefile -t raw -c <file> firmware.elf`); 404 if none.
  s_server.on("/api/dev/coredump", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth_ok(r)) return;
    size_t addr = 0, size = 0;
    if (esp_core_dump_image_get(&addr, &size) != ESP_OK || size == 0) {
      r->send(404, "text/plain", "no core dump");
      return;
    }
    AsyncWebServerResponse* resp = r->beginResponse("application/octet-stream", size,
        [addr, size](uint8_t* out, size_t max_len, size_t index) -> size_t {
          size_t n = size - index < max_len ? size - index : max_len;
          if (n && esp_flash_read(esp_flash_default_chip, out, addr + index, n) != ESP_OK) return 0;
          return n;
        });
    r->send(resp);
  });
  s_server.on("/api/dev/mqtt_renew", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth_ok(r)) return;
    int slot = r->hasParam("slot") ? r->getParam("slot")->value().toInt() : 0;
    if (slot < 1 || slot > BotHeapLog::MQTT_SLOTS) { r->send(400, "text/plain", "slot=1|2"); return; }
    the_mesh.mqtt().forceTokenRenewal(slot - 1);
    r->send(200, "application/json", "{\"ok\":true}");
  });
#endif

  AsyncElegantOTA.begin(&s_server, BotWifi::webUser(), BotWifi::webPass());
  s_server.begin();
  s_started = true;
  BOT_DEBUG_PRINTLN("web UI on http://%s/", WiFi.localIP().toString().c_str());
}

bool BotWebUI::started() { return s_started; }
