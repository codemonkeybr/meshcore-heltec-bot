#pragma once

#include <ArduinoJson.h>
#include <stdlib.h>
#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

// PSRAM-first alloc/realloc/free, independent copy of MQTTBridge's private
// psram_malloc/psram_realloc/psram_free (src/helpers/bridges/MQTTBridge.cpp,
// ~line 148) - that file is vendored to match a third-party reference
// firmware exactly and isn't touched for bot-only needs like this one.
inline void* psram_malloc(size_t size) {
  if (size == 0) return nullptr;
#if defined(ESP_PLATFORM) && defined(BOARD_HAS_PSRAM)
  void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  if (p != nullptr) return p;
  return heap_caps_malloc(size, MALLOC_CAP_INTERNAL);
#else
  return malloc(size);
#endif
}

inline void psram_free(void* ptr) {
  if (ptr == nullptr) return;
#if defined(ESP_PLATFORM)
  heap_caps_free(ptr);
#else
  free(ptr);
#endif
}

inline void* psram_realloc(void* ptr, size_t new_size) {
  if (new_size == 0) {
    psram_free(ptr);
    return nullptr;
  }
#if defined(ESP_PLATFORM) && defined(BOARD_HAS_PSRAM)
  void* p = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
  if (p != nullptr) return p;
  return heap_caps_realloc(ptr, new_size, MALLOC_CAP_INTERNAL);
#else
  return realloc(ptr, new_size);
#endif
}

// ArduinoJson v7 allocator wrapping the above, same shape as MQTTBridge's own
// JsonScratchAllocator (MQTTBridge.h ~line 342). Stateless, so one shared
// static instance is safe across every JsonDocument that uses it as long as
// they're not used concurrently from different tasks.
class PsramJsonAllocator : public ArduinoJson::Allocator {
public:
  void* allocate(size_t size) override { return psram_malloc(size); }
  void deallocate(void* ptr) override { psram_free(ptr); }
  void* reallocate(void* ptr, size_t new_size) override { return psram_realloc(ptr, new_size); }
};
