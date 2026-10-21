#include <Arduino.h>
#include "BotMqttSettings.h"
#include "helpers/MQTTPrefsStorage.h"
#include "helpers/MQTTPresets.h"
#include "helpers/MQTTReplyFormat.h"
#include <Preferences.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef MQTT_DEFAULT_SLOT1_PRESET
#define MQTT_DEFAULT_SLOT1_PRESET "none"
#endif
#ifndef MQTT_DEFAULT_SLOT2_PRESET
#define MQTT_DEFAULT_SLOT2_PRESET "none"
#endif
#ifndef MQTT_DEFAULT_IATA
#define MQTT_DEFAULT_IATA ""
#endif

static const char* NVS_NS = "botmqtt";

namespace {

struct SlotCfg {
  char     preset[24];
  char     host[64];
  uint16_t port;
  char     user[32];
  char     pass[64];
  char     aud[64];
};

struct Cfg {
  char    iata[8];
  char    origin[32];
  SlotCfg slot[BotMqttSettings::SLOTS];
};

uint32_t s_boot_hash = 0;
bool     s_boot_hashed = false;

void copy(char* dst, size_t n, const char* src) {
  strncpy(dst, src, n - 1);
  dst[n - 1] = '\0';
}

// NVS key for slot field, e.g. ("s1", 'h') -> "s1h".
void slot_key(char* out, int slot, char field) {
  out[0] = 's'; out[1] = (char)('1' + slot); out[2] = field; out[3] = '\0';
}

void load(Cfg* c) {
  memset(c, 0, sizeof(*c));
  copy(c->iata, sizeof(c->iata), MQTT_DEFAULT_IATA);
  copy(c->slot[0].preset, sizeof(c->slot[0].preset), MQTT_DEFAULT_SLOT1_PRESET);
  copy(c->slot[1].preset, sizeof(c->slot[1].preset), MQTT_DEFAULT_SLOT2_PRESET);

  Preferences p;
  if (!p.begin(NVS_NS, false)) return;
  if (p.isKey("iata")) p.getString("iata", c->iata, sizeof(c->iata));
  if (p.isKey("org"))  p.getString("org", c->origin, sizeof(c->origin));
  for (int i = 0; i < BotMqttSettings::SLOTS; i++) {
    char k[4];
    SlotCfg& s = c->slot[i];
    slot_key(k, i, 'p'); if (p.isKey(k)) p.getString(k, s.preset, sizeof(s.preset));
    slot_key(k, i, 'h'); if (p.isKey(k)) p.getString(k, s.host, sizeof(s.host));
    slot_key(k, i, 'n'); s.port = p.getUShort(k, 0);
    slot_key(k, i, 'u'); if (p.isKey(k)) p.getString(k, s.user, sizeof(s.user));
    slot_key(k, i, 'w'); if (p.isKey(k)) p.getString(k, s.pass, sizeof(s.pass));
    slot_key(k, i, 'a'); if (p.isKey(k)) p.getString(k, s.aud, sizeof(s.aud));
  }
  p.end();
}

uint32_t fnv(uint32_t h, const char* s) {
  for (; *s; s++) { h ^= (uint8_t)*s; h *= 16777619u; }
  return (h ^ 0xff) * 16777619u;   // field separator
}

uint32_t fingerprint(const Cfg& c) {
  uint32_t h = 2166136261u;
  h = fnv(h, c.iata);
  h = fnv(h, c.origin);
  for (int i = 0; i < BotMqttSettings::SLOTS; i++) {
    const SlotCfg& s = c.slot[i];
    char port[8];
    snprintf(port, sizeof(port), "%u", (unsigned)s.port);
    h = fnv(h, s.preset); h = fnv(h, s.host); h = fnv(h, port);
    h = fnv(h, s.user);   h = fnv(h, s.pass); h = fnv(h, s.aud);
  }
  return h;
}

bool fail(char* err, size_t n, const char* msg) {
  if (err && n) copy(err, n, msg);
  return false;
}

// Visible ASCII, no spaces.
bool token_text(const char* v) {
  for (; *v; v++) if ((unsigned char)*v <= 0x20 || (unsigned char)*v >= 0x7f) return false;
  return true;
}
bool plain_text(const char* v) {
  for (; *v; v++) if ((unsigned char)*v < 0x20 || *v == 0x7f) return false;
  return true;
}

bool valid_preset(const char* v) {
  return !strcmp(v, "none") || !strcmp(v, "custom") || findMQTTPreset(v) != nullptr;
}

void json_str(char* buf, size_t n, int* pos, const char* s) {
  replyAppendf(buf, n, pos, "\"");
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') replyAppendf(buf, n, pos, "\\%c", c);
    else if (c < 0x20)        replyAppendf(buf, n, pos, "\\u%04x", c);
    else                      replyAppendf(buf, n, pos, "%c", c);
  }
  replyAppendf(buf, n, pos, "\"");
}

}  // namespace

void BotMqttSettings::applyTo(MQTTPrefs* p) {
  Cfg c;
  load(&c);
  copy(p->mqtt_iata, sizeof(p->mqtt_iata), c.iata);
  copy(p->mqtt_origin, sizeof(p->mqtt_origin), c.origin);
  for (int i = 0; i < SLOTS; i++) {
    const SlotCfg& s = c.slot[i];
    copy(p->mqtt_slot_preset[i], sizeof(p->mqtt_slot_preset[i]), s.preset);
    copy(p->mqtt_slot_host[i], sizeof(p->mqtt_slot_host[i]), s.host);
    p->mqtt_slot_port[i] = s.port;
    copy(p->mqtt_slot_username[i], sizeof(p->mqtt_slot_username[i]), s.user);
    copy(p->mqtt_slot_password[i], sizeof(p->mqtt_slot_password[i]), s.pass);
    copy(p->mqtt_slot_audience[i], sizeof(p->mqtt_slot_audience[i]), s.aud);
  }
  // Slots beyond the cap stay off whatever the build defaults say.
  for (int i = SLOTS; i < MQTT_PREFS_SLOT_COUNT; i++) copy(p->mqtt_slot_preset[i], sizeof(p->mqtt_slot_preset[i]), "none");
  s_boot_hash = fingerprint(c);
  s_boot_hashed = true;
}

bool BotMqttSettings::restartNeeded() {
  if (!s_boot_hashed) return false;
  Cfg c;
  load(&c);
  return fingerprint(c) != s_boot_hash;
}

void BotMqttSettings::reset() {
  Preferences p;
  if (p.begin(NVS_NS, false)) {
    p.clear();
    p.end();
  }
}

bool BotMqttSettings::set(const char* key, const char* value, char* err, size_t err_len) {
  Preferences p;
  if (!p.begin(NVS_NS, false)) return fail(err, err_len, "Settings storage unavailable");
  bool ok = true;
  const size_t vlen = strlen(value);

  if (!strcmp(key, "mqtt_iata")) {
    bool good = vlen == 0 || (vlen >= 3 && vlen <= 4);
    for (size_t i = 0; good && i < vlen; i++) if (value[i] < 'A' || value[i] > 'Z') good = false;
    if (!good) ok = fail(err, err_len, "Location code is 3 or 4 capital letters, e.g. YYC");
    else p.putString("iata", value);
  } else if (!strcmp(key, "mqtt_origin")) {
    if (vlen >= 32 || !plain_text(value)) ok = fail(err, err_len, "Observer name is up to 31 characters");
    else p.putString("org", value);
  } else if (!strncmp(key, "mqtt", 4) && (key[4] == '1' || key[4] == '2') && key[5] == '_') {
    int slot = key[4] - '1';
    const char* f = key + 6;
    char nk[4];
    if (!strcmp(f, "preset")) {
      if (vlen >= 24 || !valid_preset(value)) ok = fail(err, err_len, "Unknown broker preset");
      else { slot_key(nk, slot, 'p'); p.putString(nk, value); }
    } else if (!strcmp(f, "host")) {
      bool good = vlen < 64 && token_text(value);
      if (good && strstr(value, "://")) {
        good = !strncmp(value, "mqtt://", 7) || !strncmp(value, "mqtts://", 8) ||
               !strncmp(value, "ws://", 5) || !strncmp(value, "wss://", 6);
      }
      if (!good) ok = fail(err, err_len, "Broker address: a host name, or a full mqtt://, mqtts://, ws:// or wss:// address (63 characters max)");
      else { slot_key(nk, slot, 'h'); p.putString(nk, value); }
    } else if (!strcmp(f, "port")) {
      char* end = nullptr;
      long l = vlen ? strtol(value, &end, 10) : 0;
      bool good = vlen == 0 || (end != value && *end == '\0' && l >= 0 && l <= 65535);
      if (!good) ok = fail(err, err_len, "Port must be 0 to 65535");
      else { slot_key(nk, slot, 'n'); p.putUShort(nk, (uint16_t)l); }
    } else if (!strcmp(f, "user")) {
      if (vlen >= 32 || !plain_text(value)) ok = fail(err, err_len, "Username is up to 31 characters");
      else { slot_key(nk, slot, 'u'); p.putString(nk, value); }
    } else if (!strcmp(f, "pass")) {
      if (vlen) {   // empty = keep the saved password
        if (vlen >= 64 || !plain_text(value)) ok = fail(err, err_len, "Password is up to 63 characters");
        else { slot_key(nk, slot, 'w'); p.putString(nk, value); }
      }
    } else if (!strcmp(f, "pass_clear")) {
      if (value[0] == '1') { slot_key(nk, slot, 'w'); p.remove(nk); }
    } else if (!strcmp(f, "aud")) {
      if (vlen >= 64 || !token_text(value)) ok = fail(err, err_len, "Token audience is up to 63 characters, no spaces");
      else { slot_key(nk, slot, 'a'); p.putString(nk, value); }
    } else {
      ok = fail(err, err_len, "Unknown setting");
    }
  } else {
    ok = fail(err, err_len, "Unknown setting");
  }
  p.end();
  return ok;
}

void BotMqttSettings::presetsJson(char* buf, size_t n) {
  int pos = 0;
  replyAppendf(buf, n, &pos, "[");
  for (int i = 0; i < MQTT_PRESET_COUNT; i++) {
    if (i) replyAppendf(buf, n, &pos, ",");
    replyAppendf(buf, n, &pos, "{\"n\":");
    json_str(buf, n, &pos, MQTT_PRESETS[i].name);
    replyAppendf(buf, n, &pos, ",\"c\":%d}", mqttPresetNeedsSlotCredentials(&MQTT_PRESETS[i]) ? 1 : 0);
  }
  replyAppendf(buf, n, &pos, "]");
}

void BotMqttSettings::configJson(char* buf, size_t n) {
  Cfg c;
  load(&c);
  int pos = 0;
  replyAppendf(buf, n, &pos, "{\"iata\":");
  json_str(buf, n, &pos, c.iata);
  replyAppendf(buf, n, &pos, ",\"origin\":");
  json_str(buf, n, &pos, c.origin);
  replyAppendf(buf, n, &pos, ",\"slots\":[");
  for (int i = 0; i < SLOTS; i++) {
    const SlotCfg& s = c.slot[i];
    if (i) replyAppendf(buf, n, &pos, ",");
    replyAppendf(buf, n, &pos, "{\"preset\":");
    json_str(buf, n, &pos, s.preset);
    replyAppendf(buf, n, &pos, ",\"host\":");
    json_str(buf, n, &pos, s.host);
    replyAppendf(buf, n, &pos, ",\"port\":%u,\"user\":", (unsigned)s.port);
    json_str(buf, n, &pos, s.user);
    replyAppendf(buf, n, &pos, ",\"pass_set\":%s,\"aud\":", s.pass[0] ? "true" : "false");
    json_str(buf, n, &pos, s.aud);
    replyAppendf(buf, n, &pos, "}");
  }
  replyAppendf(buf, n, &pos, "],\"restart_needed\":%s}", restartNeeded() ? "true" : "false");
}
