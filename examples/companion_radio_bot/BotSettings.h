#pragma once

#include <stddef.h>
#include <stdint.h>

// User-editable bot settings, persisted in NVS (namespace "botcfg", one key per
// field so adding a field never invalidates what is already saved).
//
// "Reboot to apply" by design: the running firmware reads live(), a snapshot
// taken at boot. Edits go to pending() and NVS; restartNeeded() says whether the
// two differ. Nothing is re-initialised at runtime, so there are no races
// between the web task and the mesh loop and no heap churn from re-starting
// subsystems. Defaults come from the compile-time BotConfig.h values.
namespace BotSettings {

  struct Data {
    float   lat;
    float   lon;
    int16_t tz_min;            // minutes east of UTC (sun/moon/test local time)
    uint8_t answer_dm;
    uint8_t greeter;
    uint8_t wx_metric;         // 1 = C / km/h, 0 = F / mph
    uint8_t rl_global;         // replies per minute, whole bot
    uint8_t rl_channel;        // per channel
    uint8_t rl_sender;         // per sender
    char    cmd_prefix[4];     // optional leading character(s) for commands
    char    allowlist[96];     // comma-separated channel names; empty = all channels
    char    disabled_cmds[96]; // comma-separated command keywords that are switched off
  };

  const Data& live();          // what the firmware is running with (loaded once)
  const Data& pending();       // what is saved (applies after the next restart)
  bool restartNeeded();        // pending() != live()

  // Validate and stage one field (key names match the settings page). On success the value is
  // written to pending() and NVS. On failure err is filled and nothing changes.
  bool set(const char* key, const char* value, char* err, size_t err_len);

  // True if `keyword` is listed in the live disabled-commands list.
  bool commandDisabled(const char* keyword);
}
