#include "BotSettings.h"
#include "BotConfig.h"
#include <Preferences.h>
#include <stdlib.h>
#include <string.h>

static const char* NVS_NS = "botcfg";

static BotSettings::Data s_live;
static BotSettings::Data s_pending;
static bool s_loaded = false;

static void copy(char* dst, size_t n, const char* src) {
  strncpy(dst, src, n - 1);
  dst[n - 1] = '\0';
}

static void load() {
  if (s_loaded) return;
  s_loaded = true;

  BotSettings::Data d;
  memset(&d, 0, sizeof(d));
  Preferences p;
  bool ok = p.begin(NVS_NS, false);
  d.lat        = ok ? p.getFloat("lat", (float)BOT_LAT) : (float)BOT_LAT;
  d.lon        = ok ? p.getFloat("lon", (float)BOT_LON) : (float)BOT_LON;
  d.tz_min     = ok ? p.getShort("tz", BOT_TZ_OFFSET_MIN) : BOT_TZ_OFFSET_MIN;
  d.answer_dm  = ok ? p.getUChar("dm", BOT_ANSWER_DM) : BOT_ANSWER_DM;
  d.greeter    = ok ? p.getUChar("grt", BOT_GREETER_ENABLED) : BOT_GREETER_ENABLED;
  d.wx_metric  = ok ? p.getUChar("wxm", BOT_WX_METRIC) : BOT_WX_METRIC;
  d.rl_global  = ok ? p.getUChar("rlg", BOT_RL_GLOBAL_PER_MIN) : BOT_RL_GLOBAL_PER_MIN;
  d.rl_channel = ok ? p.getUChar("rlc", BOT_RL_CHANNEL_PER_MIN) : BOT_RL_CHANNEL_PER_MIN;
  d.rl_sender  = ok ? p.getUChar("rls", BOT_RL_SENDER_PER_MIN) : BOT_RL_SENDER_PER_MIN;
  copy(d.cmd_prefix, sizeof(d.cmd_prefix), BOT_CMD_PREFIX);
  copy(d.allowlist, sizeof(d.allowlist), BOT_CHANNEL_ALLOWLIST);
  d.disabled_cmds[0] = '\0';
  if (ok) {
    if (p.isKey("pfx"))   p.getString("pfx", d.cmd_prefix, sizeof(d.cmd_prefix));
    if (p.isKey("allow")) p.getString("allow", d.allowlist, sizeof(d.allowlist));
    if (p.isKey("dis"))   p.getString("dis", d.disabled_cmds, sizeof(d.disabled_cmds));
    p.end();
  }
  s_live = d;
  s_pending = d;
}

const BotSettings::Data& BotSettings::live() { load(); return s_live; }
const BotSettings::Data& BotSettings::pending() { load(); return s_pending; }
bool BotSettings::restartNeeded() { load(); return memcmp(&s_live, &s_pending, sizeof(Data)) != 0; }

bool BotSettings::commandDisabled(const char* keyword) {
  load();
  const char* list = s_live.disabled_cmds;
  size_t klen = strlen(keyword);
  if (!klen) return false;
  const char* p = list;
  while (*p) {
    const char* comma = strchr(p, ',');
    size_t seg = comma ? (size_t)(comma - p) : strlen(p);
    if (seg == klen && strncasecmp(p, keyword, seg) == 0) return true;
    if (!comma) break;
    p = comma + 1;
  }
  return false;
}

static bool fail(char* err, size_t n, const char* msg) {
  if (err && n) copy(err, n, msg);
  return false;
}

static bool parse_float(const char* v, float lo, float hi, float* out) {
  if (!*v) return false;
  char* end = nullptr;
  float f = strtof(v, &end);
  if (end == v || *end != '\0' || f < lo || f > hi) return false;
  *out = f;
  return true;
}

static bool parse_int(const char* v, int lo, int hi, int* out) {
  if (!*v) return false;
  char* end = nullptr;
  long l = strtol(v, &end, 10);
  if (end == v || *end != '\0' || l < lo || l > hi) return false;
  *out = (int)l;
  return true;
}

// Printable ASCII only (no control chars); used for the free-text list fields.
static bool plain_text(const char* v) {
  for (; *v; v++) if ((unsigned char)*v < 0x20 || *v == 0x7f) return false;
  return true;
}

bool BotSettings::set(const char* key, const char* value, char* err, size_t err_len) {
  load();
  Data d = s_pending;
  Preferences p;
  if (!p.begin(NVS_NS, false)) return fail(err, err_len, "Settings storage unavailable");

  int i = 0;
  float f = 0;
  bool ok = true;

  if (!strcmp(key, "lat")) {
    if (!parse_float(value, -90, 90, &f)) ok = fail(err, err_len, "Latitude must be between -90 and 90");
    else { d.lat = f; p.putFloat("lat", f); }
  } else if (!strcmp(key, "lon")) {
    if (!parse_float(value, -180, 180, &f)) ok = fail(err, err_len, "Longitude must be between -180 and 180");
    else { d.lon = f; p.putFloat("lon", f); }
  } else if (!strcmp(key, "tz_min")) {
    if (!parse_int(value, -720, 840, &i) || (i % 15) != 0) ok = fail(err, err_len, "UTC offset must be -12 to +14 hours in 15-minute steps");
    else { d.tz_min = (int16_t)i; p.putShort("tz", d.tz_min); }
  } else if (!strcmp(key, "answer_dm")) {
    if (!parse_int(value, 0, 1, &i)) ok = fail(err, err_len, "Answer DMs must be on or off");
    else { d.answer_dm = (uint8_t)i; p.putUChar("dm", d.answer_dm); }
  } else if (!strcmp(key, "greeter")) {
    if (!parse_int(value, 0, 1, &i)) ok = fail(err, err_len, "Greeter must be on or off");
    else { d.greeter = (uint8_t)i; p.putUChar("grt", d.greeter); }
  } else if (!strcmp(key, "wx_metric")) {
    if (!parse_int(value, 0, 1, &i)) ok = fail(err, err_len, "Units must be metric or imperial");
    else { d.wx_metric = (uint8_t)i; p.putUChar("wxm", d.wx_metric); }
  } else if (!strcmp(key, "rl_global")) {
    if (!parse_int(value, 1, 60, &i)) ok = fail(err, err_len, "Rate limits are 1 to 60 per minute");
    else { d.rl_global = (uint8_t)i; p.putUChar("rlg", d.rl_global); }
  } else if (!strcmp(key, "rl_channel")) {
    if (!parse_int(value, 1, 60, &i)) ok = fail(err, err_len, "Rate limits are 1 to 60 per minute");
    else { d.rl_channel = (uint8_t)i; p.putUChar("rlc", d.rl_channel); }
  } else if (!strcmp(key, "rl_sender")) {
    if (!parse_int(value, 1, 60, &i)) ok = fail(err, err_len, "Rate limits are 1 to 60 per minute");
    else { d.rl_sender = (uint8_t)i; p.putUChar("rls", d.rl_sender); }
  } else if (!strcmp(key, "cmd_prefix")) {
    size_t n = strlen(value);
    bool good = n < sizeof(d.cmd_prefix);
    for (size_t k = 0; good && k < n; k++) if (value[k] <= ' ' || value[k] >= 0x7f) good = false;
    if (!good) ok = fail(err, err_len, "Command prefix is up to 3 visible characters, no spaces");
    else { copy(d.cmd_prefix, sizeof(d.cmd_prefix), value); p.putString("pfx", d.cmd_prefix); }
  } else if (!strcmp(key, "allowlist")) {
    if (strlen(value) >= sizeof(d.allowlist) || !plain_text(value)) ok = fail(err, err_len, "Channel list is too long (95 characters max)");
    else { copy(d.allowlist, sizeof(d.allowlist), value); p.putString("allow", d.allowlist); }
  } else if (!strcmp(key, "disabled_cmds")) {
    bool good = strlen(value) < sizeof(d.disabled_cmds);
    for (const char* c = value; good && *c; c++) {
      if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == ',' || *c == '_' || *c == '-')) good = false;
    }
    if (!good) ok = fail(err, err_len, "Invalid command list");
    else { copy(d.disabled_cmds, sizeof(d.disabled_cmds), value); p.putString("dis", d.disabled_cmds); }
  } else {
    ok = fail(err, err_len, "Unknown setting");
  }

  p.end();
  if (ok) s_pending = d;
  return ok;
}
