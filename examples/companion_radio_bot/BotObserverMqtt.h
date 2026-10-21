#pragma once

#include <Mesh.h>

// Thin wrapper around the vendored MQTTBridge (agessaman/MeshCore,
// mqtt-bridge-implementation-flex @ c2c4cb59e3) - passively publishes observed
// packet telemetry to the meshcore-ca MQTT brokers alongside the bot's normal
// command-answering behavior. Configured entirely at compile time (see the
// MQTT_DEFAULT_* build flags in variants/heltec_v4/platformio.ini) rather than
// via the upstream runtime CLI, which we don't port.
//
// begin() takes what it needs explicitly rather than reaching for BotMesh's
// protected _mgr/_radio/_ms, so BotMesh::begin() (which already has access to
// those as inherited members) is the only caller.
//
// Radio/name params are plain scalars, not our own examples/companion_radio
// NodePrefs* - MQTTBridge's constructor wants CommonCLI.h's differently-shaped
// NodePrefs (same name, unrelated class; the two can't coexist in one
// translation unit), and only reads a handful of fields from it purely for
// status-text display. BotObserverMqtt.cpp builds a throwaway instance of
// that type internally instead of exposing it here.
class BotObserverMqtt {
public:
  void begin(const char* node_name, float freq, uint8_t sf, uint8_t cr, float bw, bool disable_fwd,
            mesh::PacketManager* mgr, mesh::RTCClock* rtc, mesh::LocalIdentity* identity,
            mesh::Dispatcher* dispatcher, mesh::Radio* radio, mesh::MillisecondClock* ms);
  void loop();
  void end();   // stop the uplink (tears down broker connections; blocks a few seconds)

  // Fed from BotMesh's logRx/logRxRaw overrides and its outbound send path.
  void onPacketReceived(mesh::Packet* pkt);
  void storeRawRadioData(const uint8_t* raw, int len, float snr, float rssi);
  void sendPacket(mesh::Packet* pkt);

  // For the web UI status page - is the bridge up, and how many of the
  // configured broker slots are currently connected.
  bool isReady() const;
  int connectedBrokers() const;

  // Compact diagnostic line: free/max heap, packet queue depth, per-slot
  // outbox size + drops, skipped-publish count (memory-pressure throttling),
  // filtered-packet count, per-slot publish ok/err. Same string the
  // reference firmware's `get mqtt.stats` CLI command produces.
  void statsReply(char* buf, size_t bufsize) const;

  // Memory-pressure loss accounting (BOT_DEV_FEATURES status rows): packets
  // handed to the publisher (counted once, not per retry) and how many were
  // dropped because every retry hit the largest-free-block < skipThreshold() guard.
  unsigned long packetsSeen() const;
  unsigned long packetsLostToPressure() const;
  size_t skipThreshold() const;

  // Per-broker-slot (0-based) reconnect counters and a hook that forces the
  // JWT-renewal reconnect now (BOT_DEV_FEATURES heap-fragmentation probing).
  uint32_t slotDisconnects(int slot) const;
  uint32_t slotRenewalReconnects(int slot) const;
  void forceTokenRenewal(int slot);
};
