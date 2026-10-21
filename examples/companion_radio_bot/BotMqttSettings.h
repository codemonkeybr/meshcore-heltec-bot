#pragma once

#include <stddef.h>

struct MQTTPrefs;

// User-editable MQTT uplink settings (two broker slots). Same contract as
// BotSettings: edits are saved to NVS (namespace "botmqtt") immediately but only
// applied at the next boot, when applyTo() overlays them on the compile-time
// MQTT_DEFAULT_* values. Nothing is kept resident - every call reads NVS into a
// small stack struct - so this adds no permanent RAM.
namespace BotMqttSettings {
  static const int SLOTS = 2;   // deliberate cap: each extra TLS broker means more reconnect heap churn

  // Boot: overlay saved values on `p` (already filled by applyMQTTDefaults) and
  // remember a fingerprint of what was applied.
  void applyTo(MQTTPrefs* p);
  bool restartNeeded();                 // saved settings differ from what booted

  // Validate and save one field. Keys: mqtt_iata, mqtt_origin, mqttN_preset|host|port|user|pass|aud
  // (N = 1..2). An empty password is ignored (keep the saved one); mqttN_pass_clear=1 erases it.
  bool set(const char* key, const char* value, char* err, size_t err_len);
  void reset();                         // back to the build defaults

  void presetsJson(char* buf, size_t bufsize);   // [{"n":"name","c":needsCredentials}, ...]
  void configJson(char* buf, size_t bufsize);    // saved values; passwords only as *_pass_set flags
}
