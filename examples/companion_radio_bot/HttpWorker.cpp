#include "HttpWorker.h"
#include "BotHeapLog.h"
#include "BotDebug.h"
#include "PsramJsonAlloc.h"
#include "BotWifi.h"
#include "BotSettings.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static QueueHandle_t s_jobs = nullptr;
static QueueHandle_t s_results = nullptr;
static TaskHandle_t  s_task = nullptr;
static volatile bool s_busy = false;

// Shared by every JsonDocument constructed in this file - HttpWorker only
// ever runs one job at a time (see busy()/s_busy below), so no concurrent use.
static PsramJsonAllocator s_json_alloc;

// Fixed-capacity sink for HTTPClient::writeToStream(), replacing a dynamic
// String body. Never overflows: write() copies only what still fits and
// returns a short count once full, which HTTPClient treats as a stream-write
// error (HTTPC_ERROR_STREAM_WRITE) and aborts cleanly - same failure class as
// the pre-existing response-size cap below.
class FixedBufStream : public Stream {
public:
  FixedBufStream(char* buf, size_t cap) : _buf(buf), _cap(cap) {}
  size_t write(const uint8_t* data, size_t size) override {
    size_t room = _len < _cap - 1 ? _cap - 1 - _len : 0;
    size_t n = size < room ? size : room;
    memcpy(_buf + _len, data, n);
    _len += n;
    _buf[_len] = '\0';
    return n;
  }
  size_t write(uint8_t b) override { return write(&b, 1); }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  size_t length() const { return _len; }
private:
  char* _buf; size_t _cap; size_t _len = 0;
};

// PSRAM-backed, allocated once in HttpWorker::begin(); matches the existing
// 24000-byte response-size cap in fetch_json() below.
static const size_t HTTP_BODY_BUF_SIZE = 24001;
static char* s_http_body = nullptr;

HttpWorker& HttpWorker::instance() { static HttpWorker w; return w; }

void HttpWorker::begin() {
  if (s_task) return;
  if (!s_http_body) s_http_body = (char*)psram_malloc(HTTP_BODY_BUF_SIZE);
  s_jobs = xQueueCreate(2, sizeof(HttpJob));
  s_results = xQueueCreate(3, sizeof(HttpResult));
  xTaskCreatePinnedToCore(taskEntry, "bot_http", BOT_HTTP_TASK_STACK, this, 1, &s_task, 0);
  BOT_DEBUG_PRINTLN("http worker up");
}

bool HttpWorker::busy() const { return s_busy || (s_jobs && uxQueueMessagesWaiting(s_jobs) > 0); }

bool HttpWorker::submit(const HttpJob& job) {
  if (!s_jobs || busy()) return false;
  return xQueueSend(s_jobs, &job, 0) == pdTRUE;
}

bool HttpWorker::pollResult(HttpResult& out) {
  if (!s_results) return false;
  return xQueueReceive(s_results, &out, 0) == pdTRUE;
}

void HttpWorker::taskEntry(void* self) { static_cast<HttpWorker*>(self)->taskLoop(); }

// ---- helpers ----------------------------------------------------------

static const char* wmo_desc(int c) {
  switch (c) {
    case 0: return "clear"; case 1: return "mostly clear"; case 2: return "partly cloudy";
    case 3: return "overcast"; case 45: case 48: return "fog";
    case 51: return "light drizzle"; case 53: return "drizzle"; case 55: return "heavy drizzle";
    case 56: case 57: return "freezing drizzle";
    case 61: return "light rain"; case 63: return "rain"; case 65: return "heavy rain";
    case 66: case 67: return "freezing rain";
    case 71: return "light snow"; case 73: return "snow"; case 75: return "heavy snow";
    case 77: return "snow grains";
    case 80: return "light showers"; case 81: return "showers"; case 82: return "heavy showers";
    case 85: return "light snow showers"; case 86: return "snow showers";
    case 95: return "thunderstorm"; case 96: return "t-storm w/hail"; case 99: return "severe t-storm";
    default: return "?";
  }
}

// Emoji for a WMO weather code, matching the Python bot's condition emoji.
static const char* wmo_emoji(int c) {
  if (c < 0)                return "";
  if (c == 0)               return "\xE2\x98\x80\xEF\xB8\x8F";          // sun
  if (c == 1)               return "\xF0\x9F\x8C\xA4\xEF\xB8\x8F";      // sun behind small cloud
  if (c == 2)               return "\xE2\x9B\x85";                      // sun behind cloud
  if (c == 3)               return "\xE2\x98\x81\xEF\xB8\x8F";          // cloud
  if (c == 45 || c == 48)   return "\xF0\x9F\x8C\xAB\xEF\xB8\x8F";      // fog
  if (c >= 51 && c <= 57)   return "\xF0\x9F\x8C\xA6\xEF\xB8\x8F";      // sun behind rain cloud
  if (c >= 61 && c <= 67)   return "\xF0\x9F\x8C\xA7\xEF\xB8\x8F";      // cloud with rain
  if (c >= 71 && c <= 77)   return "\xE2\x9D\x84\xEF\xB8\x8F";          // snowflake
  if (c >= 80 && c <= 82)   return "\xF0\x9F\x8C\xA6\xEF\xB8\x8F";      // sun behind rain cloud
  if (c >= 85 && c <= 86)   return "\xF0\x9F\x8C\xA8\xEF\xB8\x8F";      // cloud with snow
  if (c >= 95)              return "\xE2\x9B\x88\xEF\xB8\x8F";          // cloud with lightning + rain
  return "";
}

static const char* compass(double deg) {
  static const char* d[] = {"N","NE","E","SE","S","SW","W","NW"};
  int i = (int)floor((fmod(deg, 360.0) + 22.5) / 45.0) & 7;
  return d[i];
}

extern const uint8_t x509_crt_bundle[] asm("_binary_x509_crt_bundle_start");

// GET url into a JsonDocument. Returns HTTP status (or a negative error).
// If `filter` is non-null, only keys present in *filter are kept.
static int fetch_json(const char* url, JsonDocument& doc, bool accept_json = true,
                      JsonDocument* filter = nullptr) {
#if BOT_DEV_FEATURES
  BotHeapLog::noteHttpFetch();
#endif
  WiFiClientSecure client;
#if BOT_HTTP_INSECURE
  client.setInsecure();
#else
  // ESP-IDF's built-in Mozilla root bundle, already linked in for the MQTT
  // bridge (CONFIG_MBEDTLS_CERTIFICATE_BUNDLE); same blob format the Arduino
  // core expects. Verification needs a correct clock, which NTP provides.
  client.setCACertBundle(x509_crt_bundle);
#endif

  HTTPClient http;
  http.setConnectTimeout(BOT_HTTP_TIMEOUT_MS);
  http.setTimeout(BOT_HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setReuse(false);
  if (!http.begin(client, url)) return -1;
  http.addHeader("User-Agent", "meshcore-heltec-bot/" BOT_VERSION " (+github.com/codemonkeybr)");
  if (accept_json) http.addHeader("Accept", "application/json");

  int code = http.GET();
  if (code == 200) {
    int len = http.getSize();
    if (len > (int)(HTTP_BODY_BUF_SIZE - 1)) { http.end(); BOT_DEBUG_PRINTLN("resp too big: %d", len); return -3; }
    if (!s_http_body) { http.end(); return -4; }
    // writeToStream() de-chunks internally (same as getString(); getStream()
    // alone would leave chunk framing in the bytes) but writes into our fixed
    // buffer instead of growing a String.
    FixedBufStream sink(s_http_body, HTTP_BODY_BUF_SIZE);
    int wrc = http.writeToStream(&sink);
    if (wrc < 0) { BOT_DEBUG_PRINTLN("http body read err: %d", wrc); http.end(); return -5; }
    DeserializationError err = filter
        ? deserializeJson(doc, s_http_body, sink.length(), DeserializationOption::Filter(*filter))
        : deserializeJson(doc, s_http_body, sink.length());
    if (err) {
      BOT_DEBUG_PRINTLN("json err: %s (body %u b)", err.c_str(), (unsigned)sink.length());
      code = -2;
    }
  }
  http.end();
  return code;
}

// ---- per-kind ---------------------------------------------------------

static void do_wx(const HttpJob& j, char* out, size_t n) {
  char url[280];
  snprintf(url, sizeof(url),
    "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
    "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,"
    "wind_speed_10m,wind_direction_10m,wind_gusts_10m,surface_pressure,dew_point_2m"
    "&temperature_unit=%s&wind_speed_unit=%s",
    j.lat, j.lon,
    BotSettings::live().wx_metric ? "celsius" : "fahrenheit",
    BotSettings::live().wx_metric ? "kmh" : "mph");

  JsonDocument doc(&s_json_alloc);
  int code = fetch_json(url, doc, false, nullptr);
  if (code != 200) { snprintf(out, n, "weather unavailable (%d)", code); return; }

  JsonObject c = doc["current"];
  double t = c["temperature_2m"] | 0.0;
  double feels = c["apparent_temperature"] | t;
  int rh = c["relative_humidity_2m"] | 0;
  int wc = c["weather_code"] | -1;
  double ws = c["wind_speed_10m"] | 0.0;
  double wd = c["wind_direction_10m"] | 0.0;
  double gust = c["wind_gusts_10m"] | 0.0;
  double dew = c["dew_point_2m"] | 0.0;
  double press = c["surface_pressure"] | 0.0;
  const char* tu = BotSettings::live().wx_metric ? "C" : "F";

  char desc[24];
  strlcpy(desc, wmo_desc(wc), sizeof(desc));
  if (desc[0] >= 'a' && desc[0] <= 'z') desc[0] -= 32;   // capitalise

  // <emoji><desc> <t>°<U> feels <f>° <dir><spd> <rh>%RH 💧<dew>° [💨<gust>] 📊<p>hPa
  size_t k = snprintf(out, n,
    "%s%s %.0f\xC2\xB0%s feels %.0f\xC2\xB0 %s%.0f %d%%RH \xF0\x9F\x92\xA7%.0f\xC2\xB0",
    wmo_emoji(wc), desc, t, tu, feels, compass(wd), ws, rh, dew);
  if (gust > ws + 1.0 && k < n)
    k += snprintf(out + k, n - k, " \xF0\x9F\x92\xA8%.0f", gust);
  if (press > 0.0 && k < n)
    snprintf(out + k, n - k, " \xF0\x9F\x93\x8A%.0fhPa", press);
}

static void do_aqi(const HttpJob& j, char* out, size_t n) {
  char url[220];
  snprintf(url, sizeof(url),
    "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.4f&longitude=%.4f"
    "&current=us_aqi,pm2_5,pm10,ozone", j.lat, j.lon);
  JsonDocument doc(&s_json_alloc);
  int code = fetch_json(url, doc, false, nullptr);
  if (code != 200) { snprintf(out, n, "Air quality unavailable (%d)", code); return; }

  JsonObject c = doc["current"];
  int aqi = c["us_aqi"] | -1;
  double pm25 = c["pm2_5"] | 0.0;
  double pm10 = c["pm10"] | 0.0;
  const char* cat = aqi < 0 ? "?" : aqi <= 50 ? "good" : aqi <= 100 ? "moderate" :
                    aqi <= 150 ? "unhealthy (sensitive)" : aqi <= 200 ? "unhealthy" :
                    aqi <= 300 ? "very unhealthy" : "hazardous";
  snprintf(out, n, "Air quality %d AQI (%s), PM2.5 %.1f, PM10 %.1f", aqi, cat, pm25, pm10);
}

// Precipitation type of one 15-min slot. Thresholds are per slot (mm rain / cm snow).
enum { P_DRY = 0, P_RAIN, P_SNOW, P_MIX };
static int precip_type(double rain_slot, double snow_slot) {
  bool r = rain_slot > 0.05, s = snow_slot > 0.05;
  return (r && s) ? P_MIX : s ? P_SNOW : r ? P_RAIN : P_DRY;
}
static const char* const kPrecipName[]  = { "", "Rain", "Snow", "Rain/snow mix" };
static const char* const kPrecipLower[] = { "", "rain", "snow", "rain/snow mix" };

// Rates are per hour (slot value * 4). Rain bands are the usual mm/h ones; snow
// bands are the same idea in cm/h.
static void fmt_amount(char* out, size_t n, int type, double rain_mmh, double snow_cmh) {
  if (type == P_RAIN) {
    snprintf(out, n, "%s (%.1f mm/h)",
             rain_mmh < 2.5 ? "light" : rain_mmh < 7.6 ? "moderate" : "heavy", rain_mmh);
  } else if (type == P_SNOW) {
    snprintf(out, n, "%s (%.1f cm/h)",
             snow_cmh < 1.0 ? "light" : snow_cmh < 2.5 ? "moderate" : "heavy", snow_cmh);
  } else {
    snprintf(out, n, "%.1f mm/h rain, %.1f cm/h snow", rain_mmh, snow_cmh);
  }
}

static void do_rain(const HttpJob& j, char* out, size_t n) {
  char url[240];
  snprintf(url, sizeof(url),
    "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
    "&minutely_15=rain,showers,snowfall&forecast_minutely_15=8", j.lat, j.lon);
  JsonDocument doc(&s_json_alloc);
  int code = fetch_json(url, doc, false, nullptr);
  if (code != 200) { snprintf(out, n, "Nowcast unavailable (%d)", code); return; }

  JsonArray ra = doc["minutely_15"]["rain"];
  JsonArray sh = doc["minutely_15"]["showers"];
  JsonArray sn = doc["minutely_15"]["snowfall"];
  size_t cnt = ra.size();
  if (sh.size() < cnt) cnt = sh.size();
  if (sn.size() < cnt) cnt = sn.size();
  if (cnt == 0) { snprintf(out, n, "no nowcast data"); return; }
  if (cnt > 16) cnt = 16;

  int type[16];
  double rate_r[16], rate_s[16];            // per hour
  for (size_t i = 0; i < cnt; i++) {
    double r = ra[i].as<double>() + sh[i].as<double>();
    double s = sn[i].as<double>();
    type[i] = precip_type(r, s);
    rate_r[i] = r * 4.0;
    rate_s[i] = s * 4.0;
  }

  int change = -1;                          // first slot whose type differs from now
  for (size_t i = 1; i < cnt; i++) {
    if (type[i] != type[0]) { change = (int)i; break; }
  }

  char amt[64];
  if (type[0] == P_DRY) {
    if (change < 0) { snprintf(out, n, "dry, no precipitation in the next 2h"); return; }
    fmt_amount(amt, sizeof(amt), type[change], rate_r[change], rate_s[change]);
    snprintf(out, n, "%s: starting in ~%d min, %s", kPrecipName[type[change]], change * 15, amt);
  } else if (change < 0) {
    double pr = 0, ps = 0;
    for (size_t i = 0; i < cnt; i++) {
      if (rate_r[i] > pr) pr = rate_r[i];
      if (rate_s[i] > ps) ps = rate_s[i];
    }
    fmt_amount(amt, sizeof(amt), type[0], pr, ps);
    snprintf(out, n, "%s, no letup in the next 2h, peak %s", kPrecipName[type[0]], amt);
  } else if (type[change] == P_DRY) {
    fmt_amount(amt, sizeof(amt), type[0], rate_r[0], rate_s[0]);
    snprintf(out, n, "%s: easing in ~%d min, now %s", kPrecipName[type[0]], change * 15, amt);
  } else {                                  // wet -> different wet type, no dry gap
    fmt_amount(amt, sizeof(amt), type[change], rate_r[change], rate_s[change]);
    snprintf(out, n, "Turning to %s in ~%d min (%s now), %s",
             kPrecipLower[type[change]], change * 15, kPrecipLower[type[0]], amt);
  }
}

// Reply must not start with the word "wind" (or any keyword): another bot would
// read it as a command and answer it.
static void do_wind(const HttpJob& j, char* out, size_t n) {
  char url[240];
  snprintf(url, sizeof(url),
    "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
    "&current=wind_speed_10m,wind_direction_10m,wind_gusts_10m&wind_speed_unit=%s",
    j.lat, j.lon, BotSettings::live().wx_metric ? "kmh" : "mph");
  JsonDocument doc(&s_json_alloc);
  int code = fetch_json(url, doc, false, nullptr);
  if (code != 200) { snprintf(out, n, "Conditions unavailable (%d)", code); return; }

  JsonObject c = doc["current"];
  if (c.isNull()) { snprintf(out, n, "no wind data"); return; }
  double ws = c["wind_speed_10m"] | 0.0;
  double wd = c["wind_direction_10m"] | 0.0;
  double gust = c["wind_gusts_10m"] | 0.0;
  const char* u = BotSettings::live().wx_metric ? "km/h" : "mph";

  if (ws < 1.0) { snprintf(out, n, "Calm, under 1 %s", u); return; }
  size_t k = snprintf(out, n, "From the %s (%.0f\xC2\xB0) at %.0f %s", compass(wd), wd, ws, u);
  if (gust > ws + 1.0 && k < n) snprintf(out + k, n - k, ", gusts to %.0f %s", gust, u);
}

static bool fetch_noaa_val(const char* url, const char* const* keys, char* val, size_t vn) {
  JsonDocument doc(&s_json_alloc);
  if (fetch_json(url, doc, false) != 200) return false;
  for (int i = 0; keys[i]; i++) {
    JsonVariant v = doc[keys[i]];
    if (!v.isNull()) {
      if (v.is<const char*>()) strlcpy(val, v.as<const char*>(), vn);
      else snprintf(val, vn, "%g", v.as<double>());
      return true;
    }
  }
  return false;
}

static void do_solar(const HttpJob&, char* out, size_t n) {
  char flux[16] = "?", kp[16] = "?", ssn[16] = "?";
  const char* flux_keys[] = {"Flux", "flux", nullptr};
  const char* kp_keys[]   = {"kp", "kp_index", "k_index", nullptr};
  const char* ssn_keys[]  = {"SSN", "ssn", "sunspot_number", nullptr};
  fetch_noaa_val("https://services.swpc.noaa.gov/products/summary/10cm-flux.json", flux_keys, flux, sizeof(flux));
  fetch_noaa_val("https://services.swpc.noaa.gov/products/summary/planetary-k-index.json", kp_keys, kp, sizeof(kp));
  fetch_noaa_val("https://services.swpc.noaa.gov/products/summary/sunspot-number.json", ssn_keys, ssn, sizeof(ssn));

  int sfi = atoi(flux);
  double k = atof(kp);
  const char* hf = (sfi >= 150 && k < 3) ? "HF good on 20-10m"
                 : (sfi >= 110 && k < 4) ? "HF fair, 40-20m best"
                 : (k >= 5)              ? "HF poor - geomag storm"
                                         : "HF marginal";
  snprintf(out, n, "SFI %s, Kp %s, SSN %s - %s", flux, kp, ssn, hf);
}

static void do_joke(const HttpJob&, char* out, size_t n) {
  JsonDocument filter(&s_json_alloc);
  filter["joke"] = true;
  JsonDocument doc(&s_json_alloc);
  int code = fetch_json("https://v2.jokeapi.dev/joke/Any?safe-mode&type=single&format=json",
                        doc, true, &filter);
  if (code != 200 || doc["joke"].isNull()) { snprintf(out, n, "no joke right now (%d)", code); return; }
  strlcpy(out, doc["joke"] | "?", n);
}

static void do_dadjoke(const HttpJob&, char* out, size_t n) {
  JsonDocument filter(&s_json_alloc); filter["joke"] = true;
  JsonDocument doc(&s_json_alloc);
  int code = fetch_json("https://icanhazdadjoke.com/", doc, true, &filter);
  if (code != 200 || doc["joke"].isNull()) { snprintf(out, n, "no dad joke right now (%d)", code); return; }
  strlcpy(out, doc["joke"] | "?", n);
}

// ---- task loop ------------------------------------------------------

void HttpWorker::taskLoop() {
  HttpJob job;
  for (;;) {
    if (xQueueReceive(s_jobs, &job, portMAX_DELAY) != pdTRUE) continue;
    s_busy = true;

    HttpResult res;
    res.target = job.target;
    res.text[0] = '\0';

    if (BotWifi::setupActive()) {
      strlcpy(res.text, "offline - WiFi setup mode", sizeof(res.text));
    } else if (WiFi.status() != WL_CONNECTED) {
      strlcpy(res.text, "offline - no WiFi", sizeof(res.text));
    } else {
      switch (job.kind) {
        case HTTP_WX:      do_wx(job, res.text, sizeof(res.text)); break;
        case HTTP_AQI:     do_aqi(job, res.text, sizeof(res.text)); break;
        case HTTP_RAIN:    do_rain(job, res.text, sizeof(res.text)); break;
        case HTTP_WIND:    do_wind(job, res.text, sizeof(res.text)); break;
        case HTTP_SOLAR:   do_solar(job, res.text, sizeof(res.text)); break;
        case HTTP_JOKE:    do_joke(job, res.text, sizeof(res.text)); break;
        case HTTP_DADJOKE: do_dadjoke(job, res.text, sizeof(res.text)); break;
        default:           strlcpy(res.text, "?", sizeof(res.text)); break;
      }
    }

    xQueueSend(s_results, &res, pdMS_TO_TICKS(1000));
    s_busy = false;
  }
}
