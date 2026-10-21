#pragma once

// WiFi onboarding: runtime WiFi credentials (NVS) plus a setup access point
// with a captive portal for first-time / lost-network configuration.
//
// Credential precedence: NVS (saved by the setup page) wins, then the
// compile-time WIFI_SSID / WIFI_PWD flags (dev builds), else the device starts
// the setup AP "MeshBot-XXXXXX" (WPA2, 8-digit PIN shown on the OLED).
// Leaving setup always reboots, so the AP/DNS/server never need teardown.
namespace BotWifi {
  // Call from setup() in place of WiFi.begin(): joins the saved network, or
  // starts the setup AP when there is none.
  void begin();
  // Call every loop(): setup-AP fallback timer, captive-portal DNS, join test,
  // and the hold-BOOT-to-forget gesture.
  void loop();

  // Effective station credentials (lazy-loaded; safe to call before begin()).
  const char* ssid();
  const char* password();
  // Web UI / OTA basic-auth credentials (NVS password wins over the build one).
  const char* webUser();
  const char* webPass();

  bool setupActive();          // setup AP is up
  const char* apName();        // "MeshBot-XXXXXX"
  const char* apPin();         // 8-digit AP password, fresh per setup session
  // Short status for the OLED: "", "Connecting...", "Connected", "Wrong password"...
  const char* setupStatus();

  void forget();               // wipe saved WiFi (keeps the web password) and reboot
}
