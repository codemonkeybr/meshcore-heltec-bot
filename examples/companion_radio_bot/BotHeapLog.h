#pragma once

#include <stddef.h>
#include <stdint.h>

// BOT_DEV_FEATURES heap-fragmentation probe. Samples the internal heap once a
// second and logs an entry whenever the largest free block settles at a new
// value (>= BOT_HEAPLOG_STEP bytes away from the last entry, held for
// BOT_HEAPLOG_SETTLE_S samples) - i.e. step changes, not the ~1 s transient dips
// HTTPS fetches/web requests cause. Each entry snapshots event counters
// (per-slot MQTT disconnects/renewal bounces, HTTP fetches, web requests, WiFi
// disconnects, bot replies), so a step can be pinned on whatever changed since
// the previous entry. Ring buffer lives in PSRAM; served as /api/heaplog.
//
// Alpha-only: remove with BOT_DEV_FEATURES.

namespace BotHeapLog {
  static const int MQTT_SLOTS = 2;

  // Counters owned by other modules, passed in on each sample.
  struct Counters {
    uint32_t mqtt_dc[MQTT_SLOTS];
    uint32_t mqtt_renew[MQTT_SLOTS];
    uint32_t replies;
  };

  void begin();                         // allocate ring/JSON buffers, hook WiFi events
  void loop(const Counters& c, uint32_t uptime_s, uint32_t clock);

  // Event counters bumped from their own tasks (diagnostic; races are harmless).
  void noteHttpFetch();
  void noteWebRequest();

  // Internal-heap block stats for /api/status.
  void heapInfo(uint32_t* free_blocks, uint32_t* alloc_blocks, uint32_t* largest);
  uint32_t httpFetches();
  uint32_t webRequests();
  uint32_t wifiDisconnects();
  uint32_t entries();                   // total logged since boot (ring may have wrapped)

  // JSON of the newest `n` entries into the PSRAM buffer; returns it (nullptr
  // if the buffer couldn't be allocated).
  const char* json(int n);
}
