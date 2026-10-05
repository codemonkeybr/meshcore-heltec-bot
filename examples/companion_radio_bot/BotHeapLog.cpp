#include "BotHeapLog.h"
#include "BotConfig.h"

#if BOT_DEV_FEATURES

#include "PsramJsonAlloc.h"
#include "helpers/MQTTReplyFormat.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#ifndef BOT_HEAPLOG_ENTRIES
#define BOT_HEAPLOG_ENTRIES   1024
#endif
#ifndef BOT_HEAPLOG_STEP
#define BOT_HEAPLOG_STEP      512     // bytes the largest block must move to count
#endif
#ifndef BOT_HEAPLOG_SETTLE_S
#define BOT_HEAPLOG_SETTLE_S  10      // seconds it must hold there before logging
#endif

namespace BotHeapLog {

struct Entry {
  uint32_t up, clock;
  uint32_t largest, free_bytes, dip_min;
  uint16_t free_blocks, alloc_blocks;
  uint16_t mqtt_dc[MQTT_SLOTS], mqtt_renew[MQTT_SLOTS];
  uint16_t http, web, wifi_dc, replies;
};

static const size_t JSON_BUF_SIZE = 98304;   // 1024 rows * ~90 B
static Entry* s_ring = nullptr;
static char* s_json = nullptr;
static uint32_t s_count = 0;                 // entries logged since boot

static volatile uint32_t s_http = 0, s_web = 0, s_wifi_dc = 0;

static unsigned long s_last_sample_ms = 0;
static uint32_t s_logged_largest = 0;        // largest block at the last entry
static uint32_t s_candidate = 0;             // new value waiting to settle
static int s_settle = 0;
static uint32_t s_dip_min = UINT32_MAX;      // lowest largest block since last entry

void noteHttpFetch() { s_http++; }
void noteWebRequest() { s_web++; }
uint32_t httpFetches() { return s_http; }
uint32_t webRequests() { return s_web; }
uint32_t wifiDisconnects() { return s_wifi_dc; }
uint32_t entries() { return s_count; }

void heapInfo(uint32_t* free_blocks, uint32_t* alloc_blocks, uint32_t* largest) {
  multi_heap_info_t info;
  heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
  *free_blocks = info.free_blocks;
  *alloc_blocks = info.allocated_blocks;
  *largest = info.largest_free_block;
}

void begin() {
  if (!s_ring) s_ring = (Entry*)psram_malloc(sizeof(Entry) * BOT_HEAPLOG_ENTRIES);
  if (!s_json) s_json = (char*)psram_malloc(JSON_BUF_SIZE);
  WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) { s_wifi_dc++; },
               ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}

static void logEntry(const multi_heap_info_t& info, const Counters& c, uint32_t up, uint32_t clock) {
  if (!s_ring) return;
  Entry& e = s_ring[s_count % BOT_HEAPLOG_ENTRIES];
  e.up = up;
  e.clock = clock;
  e.largest = info.largest_free_block;
  e.free_bytes = info.total_free_bytes;
  e.dip_min = s_dip_min == UINT32_MAX ? info.largest_free_block : s_dip_min;
  e.free_blocks = (uint16_t)info.free_blocks;
  e.alloc_blocks = (uint16_t)info.allocated_blocks;
  for (int i = 0; i < MQTT_SLOTS; i++) {
    e.mqtt_dc[i] = (uint16_t)c.mqtt_dc[i];
    e.mqtt_renew[i] = (uint16_t)c.mqtt_renew[i];
  }
  e.http = (uint16_t)s_http;
  e.web = (uint16_t)s_web;
  e.wifi_dc = (uint16_t)s_wifi_dc;
  e.replies = (uint16_t)c.replies;
  s_count++;
  s_logged_largest = info.largest_free_block;
  s_dip_min = UINT32_MAX;
}

void loop(const Counters& c, uint32_t uptime_s, uint32_t clock) {
  unsigned long now = millis();
  if (s_last_sample_ms != 0 && now - s_last_sample_ms < 1000) return;
  s_last_sample_ms = now;

  multi_heap_info_t info;
  heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
  uint32_t largest = info.largest_free_block;
  if (largest < s_dip_min) s_dip_min = largest;

  if (s_count == 0) { logEntry(info, c, uptime_s, clock); return; }   // boot baseline

  uint32_t diff = largest > s_logged_largest ? largest - s_logged_largest : s_logged_largest - largest;
  if (diff < BOT_HEAPLOG_STEP) { s_settle = 0; return; }              // back at the logged level

  uint32_t cdiff = largest > s_candidate ? largest - s_candidate : s_candidate - largest;
  if (s_settle == 0 || cdiff >= BOT_HEAPLOG_STEP) { s_candidate = largest; s_settle = 1; return; }
  if (++s_settle >= BOT_HEAPLOG_SETTLE_S) { logEntry(info, c, uptime_s, clock); s_settle = 0; }
}

const char* json(int n) {
  if (!s_json) return nullptr;
  uint32_t have = s_count < BOT_HEAPLOG_ENTRIES ? s_count : BOT_HEAPLOG_ENTRIES;
  if (n <= 0 || (uint32_t)n > have) n = (int)have;

  int pos = 0;
  replyAppendf(s_json, JSON_BUF_SIZE, &pos,
               "{\"count\":%lu,\"fields\":[\"up\",\"clock\",\"largest\",\"dip_min\",\"free\","
               "\"free_blocks\",\"alloc_blocks\",\"dc1\",\"dc2\",\"renew1\",\"renew2\","
               "\"http\",\"web\",\"wifi_dc\",\"replies\"],\"entries\":[",
               (unsigned long)s_count);
  for (uint32_t k = s_count - n; k < s_count; k++) {
    const Entry& e = s_ring[k % BOT_HEAPLOG_ENTRIES];
    replyAppendf(s_json, JSON_BUF_SIZE, &pos,
                 "%s[%lu,%lu,%lu,%lu,%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u]",
                 k == s_count - n ? "" : ",",
                 (unsigned long)e.up, (unsigned long)e.clock, (unsigned long)e.largest,
                 (unsigned long)e.dip_min, (unsigned long)e.free_bytes,
                 e.free_blocks, e.alloc_blocks, e.mqtt_dc[0], e.mqtt_dc[1],
                 e.mqtt_renew[0], e.mqtt_renew[1], e.http, e.web, e.wifi_dc, e.replies);
  }
  replyAppendf(s_json, JSON_BUF_SIZE, &pos, "]}");
  return s_json;
}

}  // namespace BotHeapLog

#endif  // BOT_DEV_FEATURES
