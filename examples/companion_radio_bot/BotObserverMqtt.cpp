#include "BotObserverMqtt.h"
#include "BotConfig.h"
#include "BotDebug.h"
#include "helpers/bridges/MQTTBridge.h"
#include "helpers/MQTTDefaults.h"
#include "target.h"   // extern HeltecV4Board board;
#include <Utils.h>
#include <string.h>

// File-static, same reasoning as BotMesh.cpp's s_outq: kept out of the_mesh,
// heap-allocating the bridge at static-init time crashes on ESP32 classic
// (per upstream's own note), so it's constructed lazily in begin() instead.
static MQTTPrefs s_mqtt_prefs;
static NodePrefs s_node_prefs;   // CommonCLI.h's type - see BotObserverMqtt.h
static MQTTBridge* s_bridge = nullptr;

void BotObserverMqtt::begin(const char* node_name, float freq, uint8_t sf, uint8_t cr, float bw,
                            bool disable_fwd, mesh::PacketManager* mgr, mesh::RTCClock* rtc,
                            mesh::LocalIdentity* identity, mesh::Dispatcher* dispatcher,
                            mesh::Radio* radio, mesh::MillisecondClock* ms) {
  applyMQTTDefaults(&s_mqtt_prefs);   // MQTT_DEFAULT_* build flags (platformio.ini)

  // Not covered by applyMQTTDefaults() - the bridge manages its own WiFi
  // station connection (falls back to a no-op if already connected, since
  // BotMesh's own WiFi.begin() in main.cpp already brought the link up).
  strncpy(s_mqtt_prefs.wifi_ssid, WIFI_SSID, sizeof(s_mqtt_prefs.wifi_ssid) - 1);
  strncpy(s_mqtt_prefs.wifi_password, WIFI_PWD, sizeof(s_mqtt_prefs.wifi_password) - 1);

  // Only used by MQTTBridge for status-text display (radio config, repeat
  // on/off) - never read for actual behavior, which stays governed by our own
  // allowPacketForward() override.
  strncpy(s_node_prefs.node_name, node_name ? node_name : "", sizeof(s_node_prefs.node_name) - 1);
  s_node_prefs.node_name[sizeof(s_node_prefs.node_name) - 1] = '\0';
  s_node_prefs.freq = freq;
  s_node_prefs.sf = sf;
  s_node_prefs.cr = cr;
  s_node_prefs.bw = bw;
  s_node_prefs.disable_fwd = disable_fwd ? 1 : 0;

  s_bridge = new MQTTBridge(&s_node_prefs, &s_mqtt_prefs, mgr, rtc, identity);

  char device_id[65];
  mesh::Utils::toHex(device_id, identity->pub_key, PUB_KEY_SIZE);
  s_bridge->setDeviceID(device_id);
  s_bridge->setFirmwareVersion(BOT_VERSION);
  s_bridge->setBoardModel(board.getManufacturerName());
  s_bridge->setBuildDate(__DATE__);
  s_bridge->setStatsSources(dispatcher, radio, &board, ms);
  s_bridge->begin();

  BOT_DEBUG_PRINTLN("MQTT observer bridge started (IATA %s, device %s)",
                    s_mqtt_prefs.mqtt_iata, device_id);
}

void BotObserverMqtt::loop() {
  if (s_bridge) s_bridge->loop();
}

void BotObserverMqtt::onPacketReceived(mesh::Packet* pkt) {
  if (s_bridge) s_bridge->onPacketReceived(pkt);
}

void BotObserverMqtt::storeRawRadioData(const uint8_t* raw, int len, float snr, float rssi) {
  if (s_bridge) s_bridge->storeRawRadioData(raw, len, snr, rssi);
}

void BotObserverMqtt::sendPacket(mesh::Packet* pkt) {
  if (s_bridge) s_bridge->sendPacket(pkt);
}

bool BotObserverMqtt::isReady() const { return s_bridge && s_bridge->isReady(); }
int BotObserverMqtt::connectedBrokers() const { return s_bridge ? s_bridge->getConnectedBrokers() : 0; }

void BotObserverMqtt::statsReply(char* buf, size_t bufsize) const {
  MQTTBridge::formatMqttStatsReply(buf, bufsize);   // static; "(bridge not running)" if unset
}

unsigned long BotObserverMqtt::publishAttempts() const { return s_bridge ? s_bridge->getPublishAttempts() : 0; }
int BotObserverMqtt::skippedPublishes() const { return s_bridge ? s_bridge->getSkippedPublishes() : 0; }
size_t BotObserverMqtt::skipThreshold() const { return MQTTBridge::publishSkipThreshold(); }
